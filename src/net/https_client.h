#pragma once

#include <string>

namespace rc {

struct HttpResult {
  int status = 0;
  std::string body;
  bool transportError = false;
};

// HTTPS to https://api.cyberspace.online over one keep-alive connection.
// Not thread-safe: owned by the network task.
class HttpsClient {
 public:
  HttpsClient() = default;
  ~HttpsClient();
  HttpsClient(const HttpsClient&) = delete;
  HttpsClient& operator=(const HttpsClient&) = delete;

  // `path` starts with "/". An empty bearer sends no Authorization header.
  // An empty jsonBody sends no body.
  HttpResult request(const char* method, const std::string& path, const std::string& jsonBody,
                     const std::string& bearer);
  void close();

 private:
  void* handle_ = nullptr;  // esp_http_client_handle_t
};

}  // namespace rc
