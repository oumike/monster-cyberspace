// SPDX-License-Identifier: GPL-3.0-or-later
// The esp_http_server shim on loopback: routes, methods, headers, Basic auth
// as web_server.cpp asks for it, the query string, request bodies, chunked
// replies, and stopping from inside a handler.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include "check.h"
#include "esp_http_server.h"

namespace {

uint16_t port = 0;
httpd_handle_t server = nullptr;

// One request on a fresh connection; the whole reply, read until close.
std::string request(const std::string& raw) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
    close(fd);
    return "";
  }
  // In pieces, so the server has to read across them.
  const size_t half = raw.size() / 2;
  send(fd, raw.data(), half, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  send(fd, raw.data() + half, raw.size() - half, 0);
  std::string out;
  char buf[1024];
  for (ssize_t n; (n = read(fd, buf, sizeof buf)) > 0;) out.append(buf, size_t(n));
  close(fd);
  return out;
}

std::string get(const std::string& path, const std::string& headers = "") {
  return request("GET " + path + " HTTP/1.1\r\nHost: x\r\n" + headers + "\r\n");
}

std::string post(const std::string& path, const std::string& body) {
  return request("POST " + path + " HTTP/1.1\r\nHost: x\r\nContent-Length: " +
                 std::to_string(body.size()) + "\r\n\r\n" + body);
}

std::string bodyOf(const std::string& reply) {
  const size_t p = reply.find("\r\n\r\n");
  return p == std::string::npos ? "" : reply.substr(p + 4);
}

bool starts(const std::string& s, const std::string& prefix) { return s.rfind(prefix, 0) == 0; }

esp_err_t hello(httpd_req_t* r) {
  httpd_resp_set_type(r, "text/plain");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return httpd_resp_sendstr(r, "hello");
}

// As web_server.cpp: 401 unless "Authorization: Basic YWRtaW46cHc=" (admin:pw).
esp_err_t secret(httpd_req_t* r) {
  char v[64] = {0};
  const size_t n = httpd_req_get_hdr_value_len(r, "authorization");
  if (n == 0 || n >= sizeof v || httpd_req_get_hdr_value_str(r, "Authorization", v, sizeof v) != ESP_OK ||
      std::strcmp(v, "Basic YWRtaW46cHc=") != 0) {
    httpd_resp_set_status(r, "401 Unauthorized");
    httpd_resp_set_hdr(r, "WWW-Authenticate", "Basic realm=\"t\"");
    return httpd_resp_sendstr(r, "Password required");
  }
  return httpd_resp_sendstr(r, "ok");
}

esp_err_t query(httpd_req_t* r) {
  char q[128] = {0}, name[16] = {0}, tiny[3] = {0};
  std::string out;
  if (httpd_req_get_url_query_str(r, q, sizeof q) != ESP_OK) return httpd_resp_sendstr(r, "none");
  out += httpd_query_key_value(q, "name", name, sizeof name) == ESP_OK ? std::string(name) : "-";
  out += "|";
  out += httpd_query_key_value(q, "missing", name, sizeof name) == ESP_ERR_NOT_FOUND ? "nf" : "?";
  out += "|";
  out += httpd_query_key_value(q, "long", tiny, sizeof tiny) == ESP_ERR_HTTPD_RESULT_TRUNC ? tiny : "?";
  return httpd_resp_sendstr(r, out.c_str());
}

esp_err_t echo(httpd_req_t* r) {
  std::string body(r->content_len, '\0');
  size_t got = 0;
  while (got < body.size()) {
    const int n = httpd_req_recv(r, &body[got], body.size() - got);
    if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (n <= 0) return ESP_FAIL;
    got += size_t(n);
  }
  httpd_resp_set_status(r, "201 Created");
  return httpd_resp_send(r, body.data(), ssize_t(body.size()));
}

esp_err_t chunks(httpd_req_t* r) {
  httpd_resp_sendstr_chunk(r, "ab");
  httpd_resp_send_chunk(r, "cde", 3);
  return httpd_resp_sendstr_chunk(r, nullptr);
}

esp_err_t local(httpd_req_t* r) {
  sockaddr_in a{};
  socklen_t len = sizeof a;
  const bool ok = getsockname(httpd_req_to_sockfd(r), reinterpret_cast<sockaddr*>(&a), &len) == 0 &&
                  ntohl(a.sin_addr.s_addr) == INADDR_LOOPBACK;
  return httpd_resp_sendstr(r, ok ? "loopback" : "?");
}

esp_err_t missing(httpd_req_t* r) { return httpd_resp_send_404(r); }

esp_err_t stopper(httpd_req_t* r) {
  httpd_resp_sendstr(r, "bye");
  return httpd_stop(r->handle);
}

void route(const char* uri, httpd_method_t m, esp_err_t (*fn)(httpd_req_t*)) {
  httpd_uri_t u = {};
  u.uri = uri;
  u.method = m;
  u.handler = fn;
  CHECK_EQ(httpd_register_uri_handler(server, &u), ESP_OK);
}

}  // namespace

int main() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 0;
  cfg.loopback_only = true;
  cfg.max_uri_handlers = 8;
  CHECK_EQ(httpd_start(&server, &cfg), ESP_OK);
  port = httpd_compat_port(server);
  CHECK(port != 0);

  route("/", HTTP_GET, hello);
  route("/secret", HTTP_GET, secret);
  route("/q", HTTP_GET, query);
  route("/echo", HTTP_POST, echo);
  route("/chunks", HTTP_GET, chunks);
  route("/local", HTTP_GET, local);
  route("/gone", HTTP_GET, missing);
  route("/stop", HTTP_POST, stopper);
  httpd_uri_t extra = {"/extra", HTTP_GET, hello, nullptr};
  CHECK_EQ(httpd_register_uri_handler(server, &extra), ESP_ERR_HTTPD_HANDLERS_FULL);

  std::string r = get("/");
  CHECK(starts(r, "HTTP/1.1 200 OK\r\n"));
  CHECK(r.find("Content-Type: text/plain\r\n") != std::string::npos);
  CHECK(r.find("Cache-Control: no-store\r\n") != std::string::npos);
  CHECK(r.find("Content-Length: 5\r\n") != std::string::npos);
  CHECK(r.find("Connection: close\r\n") != std::string::npos);
  CHECK_EQ(bodyOf(r), "hello");

  r = get("/secret");
  CHECK(starts(r, "HTTP/1.1 401 Unauthorized\r\n"));
  CHECK(r.find("WWW-Authenticate: Basic realm=\"t\"\r\n") != std::string::npos);
  r = get("/secret", "Authorization: Basic YWRtaW46d3Jvbmc=\r\n");
  CHECK(starts(r, "HTTP/1.1 401"));
  r = get("/secret", "authorization:   Basic YWRtaW46cHc=\r\n");
  CHECK(starts(r, "HTTP/1.1 200"));
  CHECK_EQ(bodyOf(r), "ok");

  CHECK_EQ(bodyOf(get("/q?name=a%20b&long=xyz")), "a%20b|nf|xy");
  CHECK_EQ(bodyOf(get("/q")), "none");

  std::string big(20000, 'x');
  big[0] = '{';
  r = post("/echo", big);
  CHECK(starts(r, "HTTP/1.1 201 Created\r\n"));
  CHECK(bodyOf(r) == big);

  r = get("/chunks");
  CHECK(r.find("Transfer-Encoding: chunked\r\n") != std::string::npos);
  CHECK_EQ(bodyOf(r), "2\r\nab\r\n3\r\ncde\r\n0\r\n\r\n");

  CHECK_EQ(bodyOf(get("/local")), "loopback");
  CHECK(starts(get("/gone"), "HTTP/1.1 404"));
  CHECK(starts(get("/nowhere"), "HTTP/1.1 404"));
  CHECK(starts(post("/", "x"), "HTTP/1.1 405"));
  CHECK(starts(request("BOGUS\r\n\r\n"), "HTTP/1.1 400"));

  CHECK_EQ(bodyOf(post("/stop", "")), "bye");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  CHECK_EQ(get("/"), "");

  // A fresh server after that; httpd_stop from outside waits for it.
  CHECK_EQ(httpd_start(&server, &cfg), ESP_OK);
  port = httpd_compat_port(server);
  route("/", HTTP_GET, hello);
  CHECK_EQ(bodyOf(get("/")), "hello");
  CHECK_EQ(httpd_stop(server), ESP_OK);
  CHECK_EQ(get("/"), "");
  return checkResult();
}
