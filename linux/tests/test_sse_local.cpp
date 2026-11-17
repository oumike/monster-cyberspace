// SPDX-License-Identifier: GPL-3.0-or-later
// The Linux SseClient (libcurl) against a plain-HTTP server on loopback.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "check.h"
#include "net/sse_client.h"

namespace {

using Clock = std::chrono::steady_clock;

// Serves one connection: the head, then each chunk, then holds the socket
// open until release() (or the client goes).
class Server {
 public:
  Server(std::string head, std::vector<std::string> chunks) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof a);
    socklen_t len = sizeof a;
    getsockname(fd_, reinterpret_cast<sockaddr*>(&a), &len);
    port_ = ntohs(a.sin_port);
    listen(fd_, 1);
    th_ = std::thread([this, head, chunks] {
      const int c = accept(fd_, nullptr, nullptr);
      if (c < 0) return;
      std::string req;
      char buf[512];
      while (req.find("\r\n\r\n") == std::string::npos) {
        const ssize_t n = read(c, buf, sizeof buf);
        if (n <= 0) break;
        req.append(buf, size_t(n));
      }
      request_ = req;
      send(c, head.data(), head.size(), 0);
      for (const auto& s : chunks) {
        send(c, s.data(), s.size(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
      while (!release_) std::this_thread::sleep_for(std::chrono::milliseconds(10));
      close(c);
    });
  }
  ~Server() {
    release_ = true;
    shutdown(fd_, SHUT_RDWR);
    close(fd_);
    th_.join();
  }
  std::string url() const { return "http://127.0.0.1:" + std::to_string(port_) + "/.json?auth=secret"; }
  std::string request() const { return request_; }

 private:
  int fd_ = -1;
  int port_ = 0;
  std::thread th_;
  std::atomic<bool> release_{false};
  std::string request_;
};

const char* kOk = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n";

struct Seen {
  std::mutex mu;
  std::vector<std::string> events;
  std::vector<bool> states;
  size_t eventCount() {
    std::lock_guard<std::mutex> g(mu);
    return events.size();
  }
};

bool waitFor(const std::function<bool()>& f, int ms) {
  const auto end = Clock::now() + std::chrono::milliseconds(ms);
  while (Clock::now() < end) {
    if (f()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return f();
}

void start(rc::SseClient& c, const std::string& url, Seen& s) {
  CHECK(c.start(
      url,
      [&s](const rc::SseEvent& e) {
        std::lock_guard<std::mutex> g(s.mu);
        s.events.push_back(e.event + ":" + e.data);
      },
      [&s](bool live) {
        std::lock_guard<std::mutex> g(s.mu);
        s.states.push_back(live);
      }));
}

void eventsArriveInOrder() {
  // Split mid-line and mid-event: the parser takes any split.
  Server srv(kOk, {"event: put\ndata: {\"a\"", ":1}\n\nevent: patch\r\n", "data: 2\r\n\r\n",
                   ": keep-alive\n\nevent: keep-alive\ndata: null\n\n"});
  Seen s;
  rc::SseClient c;
  start(c, srv.url(), s);
  CHECK(c.running());
  CHECK(waitFor([&] { return s.eventCount() >= 3; }, 5000));
  {
    std::lock_guard<std::mutex> g(s.mu);
    CHECK_EQ(s.events.size(), 3u);
    if (s.events.size() == 3) {
      CHECK_EQ(s.events[0], std::string("put:{\"a\":1}"));
      CHECK_EQ(s.events[1], std::string("patch:2"));
      CHECK_EQ(s.events[2], std::string("keep-alive:null"));
    }
    CHECK(!s.states.empty() && s.states.front());
  }
  CHECK(srv.request().find("Accept: text/event-stream") != std::string::npos);
  c.stop();
  CHECK(!c.running());
  std::lock_guard<std::mutex> g(s.mu);
  CHECK(!s.states.empty() && !s.states.back());
}

void stopReturnsWhileTheServerHoldsTheStream() {
  Server srv(kOk, {"event: put\ndata: 1\n\n"});
  Seen s;
  rc::SseClient c;
  start(c, srv.url(), s);
  CHECK(waitFor([&] { return s.eventCount() >= 1; }, 5000));
  const auto t0 = Clock::now();
  c.stop();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
  CHECK(ms < 2000);
  CHECK(!c.running());
  // A stopped client starts again.
  Server srv2(kOk, {"event: put\ndata: 2\n\n"});
  Seen s2;
  start(c, srv2.url(), s2);
  CHECK(waitFor([&] { return s2.eventCount() >= 1; }, 5000));
  c.stop();
}

void aRefusedStreamDeliversNothing() {
  Server srv("HTTP/1.1 401 Unauthorized\r\nContent-Length: 30\r\nConnection: close\r\n\r\n",
             {"event: put\ndata: not-for-you\n\n"});
  Seen s;
  rc::SseClient c;
  start(c, srv.url(), s);
  CHECK(waitFor([&] { return !c.running(); }, 5000));
  std::lock_guard<std::mutex> g(s.mu);
  CHECK(s.events.empty());
  CHECK_EQ(s.states.size(), 1u);
  CHECK(!s.states.empty() && !s.states.front());
}

}  // namespace

int main() {
  eventsArriveInOrder();
  stopReturnsWhileTheServerHoldsTheStream();
  aRefusedStreamDeliversNothing();
  return checkResult();
}
