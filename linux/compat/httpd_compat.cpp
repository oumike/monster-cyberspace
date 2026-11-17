// SPDX-License-Identifier: GPL-3.0-or-later
// esp_http_server over POSIX sockets (see esp_http_server.h): one thread
// accepts, reads one request, runs its handler, answers and closes.
#include "esp_http_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0  // macOS: SO_NOSIGPIPE below instead
#endif

namespace {

constexpr size_t kMaxHead = 16384;

struct Server {
  int fd = -1;
  uint16_t port = 0;
  size_t maxHandlers = 8;
  int recvTimeoutS = 5, sendTimeoutS = 5;
  std::vector<httpd_uri_t> routes;
  std::mutex mu;  // routes
  std::thread th;
  std::atomic<bool> stopping{false};
};

// The request's connection and its reply as it is built.
struct Conn {
  int fd = -1;
  std::string query;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string pending;  // body bytes read with the head
  size_t bodyLeft = 0;
  std::string status = "200 OK", type = "text/html";
  std::vector<std::pair<std::string, std::string>> respHeaders;
  bool sent = false, chunked = false;
};

thread_local Server* inHandler = nullptr;

Conn* conn(httpd_req_t* r) { return static_cast<Conn*>(r->aux); }

bool iequals(const std::string& a, const char* b) {
  const size_t n = std::strlen(b);
  if (a.size() != n) return false;
  for (size_t i = 0; i < n; ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  return true;
}

std::string trim(const std::string& s) {
  const size_t b = s.find_first_not_of(" \t"), e = s.find_last_not_of(" \t\r");
  return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}

bool sendAll(int fd, const char* p, size_t n) {
  while (n > 0) {
    const ssize_t w = ::send(fd, p, n, MSG_NOSIGNAL);
    if (w <= 0) return false;
    p += w;
    n -= size_t(w);
  }
  return true;
}

std::string head(const Conn& c, const std::string& extra) {
  std::string h = "HTTP/1.1 " + c.status + "\r\nContent-Type: " + c.type + "\r\n";
  for (const auto& kv : c.respHeaders) h += kv.first + ": " + kv.second + "\r\n";
  return h + extra + "Connection: close\r\n\r\n";
}

void plain(int fd, const char* status, const char* text) {
  Conn c;
  c.status = status;
  c.type = "text/plain";
  const std::string h = head(c, "Content-Length: " + std::to_string(std::strlen(text)) + "\r\n");
  sendAll(fd, h.data(), h.size());
  sendAll(fd, text, std::strlen(text));
}

int methodOf(const std::string& m) {
  if (m == "GET") return HTTP_GET;
  if (m == "POST") return HTTP_POST;
  if (m == "DELETE") return HTTP_DELETE;
  if (m == "HEAD") return HTTP_HEAD;
  if (m == "PUT") return HTTP_PUT;
  return -1;
}

void serve(Server* s, int fd) {
  timeval tv{s->recvTimeoutS, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  tv.tv_sec = s->sendTimeoutS;
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
#ifdef SO_NOSIGPIPE
  const int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on);
#endif

  std::string buf;
  size_t end;
  char tmp[2048];
  while ((end = buf.find("\r\n\r\n")) == std::string::npos) {
    if (buf.size() > kMaxHead) return plain(fd, "431 Request Header Fields Too Large", "Header too large");
    const ssize_t n = ::recv(fd, tmp, sizeof tmp, 0);
    if (n <= 0) return;
    buf.append(tmp, size_t(n));
  }

  Conn c;
  c.fd = fd;
  c.pending = buf.substr(end + 4);
  const std::string headText = buf.substr(0, end);
  std::fill(buf.begin(), buf.end(), '\0');
  const size_t lineEnd = headText.find("\r\n");
  const std::string line = headText.substr(0, lineEnd);
  const size_t sp1 = line.find(' '), sp2 = line.rfind(' ');
  if (sp1 == std::string::npos || sp2 == sp1 || line.compare(sp2 + 1, 5, "HTTP/") != 0)
    return plain(fd, "400 Bad Request", "Bad request");
  const int method = methodOf(line.substr(0, sp1));
  const std::string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
  if (target.size() > HTTPD_MAX_URI_LEN) return plain(fd, "414 URI Too Long", "URI too long");
  for (size_t p = lineEnd; p != std::string::npos && p < headText.size();) {
    const size_t q = headText.find("\r\n", p + 2);
    const std::string h = headText.substr(p + 2, q == std::string::npos ? std::string::npos : q - p - 2);
    const size_t colon = h.find(':');
    if (colon != std::string::npos) c.headers.emplace_back(trim(h.substr(0, colon)), trim(h.substr(colon + 1)));
    p = q;
  }
  size_t contentLen = 0;
  for (const auto& kv : c.headers)
    if (iequals(kv.first, "Content-Length")) contentLen = std::strtoul(kv.second.c_str(), nullptr, 10);
  if (c.pending.size() > contentLen) c.pending.resize(contentLen);
  c.bodyLeft = contentLen - c.pending.size();

  std::string path = target;
  const size_t qm = target.find('?');
  if (qm != std::string::npos) {
    path = target.substr(0, qm);
    c.query = target.substr(qm + 1);
  }

  httpd_uri_t found = {};
  bool pathKnown = false;
  {
    std::lock_guard<std::mutex> l(s->mu);
    for (const httpd_uri_t& u : s->routes) {
      if (path != u.uri) continue;
      pathKnown = true;
      if (int(u.method) == method) found = u;
    }
  }
  if (!found.handler)
    return pathKnown ? plain(fd, "405 Method Not Allowed", "Method not allowed")
                     : plain(fd, "404 Not Found", "Nothing here");

  httpd_req_t req = {};
  req.handle = s;
  req.method = method;
  std::strncpy(req.uri, target.c_str(), HTTPD_MAX_URI_LEN);
  req.content_len = contentLen;
  req.aux = &c;
  req.user_ctx = found.user_ctx;
  inHandler = s;
  const esp_err_t r = found.handler(&req);
  inHandler = nullptr;
  if (!c.sent) {
    if (r == ESP_OK) httpd_resp_send(&req, "", 0);
    else plain(fd, "500 Internal Server Error", "Server error");
  }
  // The Authorization header and bodies can hold passwords.
  for (auto& kv : c.headers) std::fill(kv.second.begin(), kv.second.end(), '\0');
  std::fill(c.pending.begin(), c.pending.end(), '\0');
}

void loop(Server* s) {
  while (!s->stopping) {
    pollfd p{s->fd, POLLIN, 0};
    if (::poll(&p, 1, 100) <= 0) continue;
    const int c = ::accept(s->fd, nullptr, nullptr);
    if (c < 0) continue;
    serve(s, c);
    ::shutdown(c, SHUT_WR);
    ::close(c);
  }
  ::close(s->fd);
}

const std::string* header(httpd_req_t* r, const char* field) {
  for (const auto& kv : conn(r)->headers)
    if (iequals(kv.first, field)) return &kv.second;
  return nullptr;
}

esp_err_t copyOut(const std::string& v, char* out, size_t size) {
  if (!out || size == 0) return ESP_ERR_INVALID_ARG;
  const size_t n = std::min(v.size(), size - 1);
  std::memcpy(out, v.data(), n);
  out[n] = '\0';
  return n < v.size() ? ESP_ERR_HTTPD_RESULT_TRUNC : ESP_OK;
}

size_t lengthOf(const char* buf, ssize_t len) {
  return len == HTTPD_RESP_USE_STRLEN ? (buf ? std::strlen(buf) : 0) : size_t(len);
}

}  // namespace

esp_err_t httpd_start(httpd_handle_t* handle, const httpd_config_t* config) {
  if (!handle || !config) return ESP_ERR_INVALID_ARG;
  auto* s = new Server;
  s->maxHandlers = config->max_uri_handlers;
  s->recvTimeoutS = config->recv_wait_timeout;
  s->sendTimeoutS = config->send_wait_timeout;
  s->fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (s->fd >= 0) {
    const int on = 1;
    setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
  }
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(config->server_port);
  a.sin_addr.s_addr = htonl(config->loopback_only ? INADDR_LOOPBACK : INADDR_ANY);
  socklen_t len = sizeof a;
  if (s->fd < 0 || ::bind(s->fd, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || ::listen(s->fd, 8) != 0 ||
      ::getsockname(s->fd, reinterpret_cast<sockaddr*>(&a), &len) != 0) {
    if (s->fd >= 0) ::close(s->fd);
    delete s;
    return ESP_FAIL;
  }
  s->port = ntohs(a.sin_port);
  s->th = std::thread(loop, s);
  *handle = s;
  return ESP_OK;
}

esp_err_t httpd_stop(httpd_handle_t handle) {
  auto* s = static_cast<Server*>(handle);
  if (!s) return ESP_ERR_INVALID_ARG;
  s->stopping = true;
  // From a handler, the server's own thread answers, closes the socket and
  // ends; the Server is left behind (a few bytes).
  if (inHandler == s) {
    s->th.detach();
    return ESP_OK;
  }
  if (s->th.joinable()) s->th.join();
  delete s;
  return ESP_OK;
}

esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t* u) {
  auto* s = static_cast<Server*>(handle);
  if (!s || !u || !u->uri || !u->handler) return ESP_ERR_INVALID_ARG;
  std::lock_guard<std::mutex> l(s->mu);
  for (const httpd_uri_t& r : s->routes)
    if (std::strcmp(r.uri, u->uri) == 0 && r.method == u->method) return ESP_ERR_HTTPD_HANDLER_EXISTS;
  if (s->routes.size() >= s->maxHandlers) return ESP_ERR_HTTPD_HANDLERS_FULL;
  s->routes.push_back(*u);
  return ESP_OK;
}

uint16_t httpd_compat_port(httpd_handle_t handle) {
  return handle ? static_cast<Server*>(handle)->port : 0;
}

int httpd_req_to_sockfd(httpd_req_t* r) { return conn(r)->fd; }

int httpd_req_recv(httpd_req_t* r, char* buf, size_t len) {
  Conn* c = conn(r);
  if (!c->pending.empty()) {
    const size_t n = std::min(len, c->pending.size());
    std::memcpy(buf, c->pending.data(), n);
    std::fill(c->pending.begin(), c->pending.begin() + long(n), '\0');
    c->pending.erase(0, n);
    return int(n);
  }
  if (c->bodyLeft == 0) return 0;
  const ssize_t n = ::recv(c->fd, buf, std::min(len, c->bodyLeft), 0);
  if (n < 0) return (errno == EAGAIN || errno == EWOULDBLOCK) ? HTTPD_SOCK_ERR_TIMEOUT : HTTPD_SOCK_ERR_FAIL;
  if (n == 0) return HTTPD_SOCK_ERR_FAIL;
  c->bodyLeft -= size_t(n);
  return int(n);
}

size_t httpd_req_get_hdr_value_len(httpd_req_t* r, const char* field) {
  const std::string* v = header(r, field);
  return v ? v->size() : 0;
}

esp_err_t httpd_req_get_hdr_value_str(httpd_req_t* r, const char* field, char* val, size_t size) {
  const std::string* v = header(r, field);
  return v ? copyOut(*v, val, size) : ESP_ERR_NOT_FOUND;
}

size_t httpd_req_get_url_query_len(httpd_req_t* r) { return conn(r)->query.size(); }

esp_err_t httpd_req_get_url_query_str(httpd_req_t* r, char* buf, size_t len) {
  const std::string& q = conn(r)->query;
  if (q.empty()) return ESP_ERR_NOT_FOUND;
  return copyOut(q, buf, len);
}

esp_err_t httpd_query_key_value(const char* qry, const char* key, char* val, size_t size) {
  if (!qry || !key || !val) return ESP_ERR_INVALID_ARG;
  const size_t keyLen = std::strlen(key);
  for (const char* p = qry; *p;) {
    const char* amp = std::strchr(p, '&');
    const char* end = amp ? amp : p + std::strlen(p);
    const char* eq = static_cast<const char*>(std::memchr(p, '=', size_t(end - p)));
    if (eq && size_t(eq - p) == keyLen && std::strncmp(p, key, keyLen) == 0)
      return copyOut(std::string(eq + 1, end), val, size);
    if (!amp) break;
    p = amp + 1;
  }
  return ESP_ERR_NOT_FOUND;
}

esp_err_t httpd_resp_set_status(httpd_req_t* r, const char* status) {
  conn(r)->status = status;
  return ESP_OK;
}

esp_err_t httpd_resp_set_type(httpd_req_t* r, const char* type) {
  conn(r)->type = type;
  return ESP_OK;
}

esp_err_t httpd_resp_set_hdr(httpd_req_t* r, const char* field, const char* value) {
  conn(r)->respHeaders.emplace_back(field, value);
  return ESP_OK;
}

esp_err_t httpd_resp_send(httpd_req_t* r, const char* buf, ssize_t len) {
  Conn* c = conn(r);
  if (c->sent) return ESP_ERR_INVALID_STATE;
  const size_t n = lengthOf(buf, len);
  const std::string h = head(*c, "Content-Length: " + std::to_string(n) + "\r\n");
  c->sent = true;
  if (!sendAll(c->fd, h.data(), h.size()) || (n && !sendAll(c->fd, buf, n))) return ESP_FAIL;
  return ESP_OK;
}

esp_err_t httpd_resp_send_chunk(httpd_req_t* r, const char* buf, ssize_t len) {
  Conn* c = conn(r);
  if (c->sent && !c->chunked) return ESP_ERR_INVALID_STATE;
  if (!c->sent) {
    const std::string h = head(*c, "Transfer-Encoding: chunked\r\n");
    c->sent = c->chunked = true;
    if (!sendAll(c->fd, h.data(), h.size())) return ESP_FAIL;
  }
  const size_t n = lengthOf(buf, len);
  char size[24];
  std::snprintf(size, sizeof size, "%zx\r\n", n);
  if (!sendAll(c->fd, size, std::strlen(size)) || (n && !sendAll(c->fd, buf, n)) || !sendAll(c->fd, "\r\n", 2))
    return ESP_FAIL;
  return ESP_OK;
}

esp_err_t httpd_resp_send_404(httpd_req_t* r) {
  httpd_resp_set_status(r, "404 Not Found");
  httpd_resp_set_type(r, "text/plain");
  return httpd_resp_send(r, "Nothing here", HTTPD_RESP_USE_STRLEN);
}
