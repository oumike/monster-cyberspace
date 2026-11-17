// SPDX-License-Identifier: GPL-3.0-or-later
// ESP-IDF's esp_http_server, as far as src/web/web_server.cpp uses it, over
// POSIX sockets: one thread serves one request per connection
// (Connection: close). Spec section 7.
#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

#include "esp_err.h"

#define ESP_ERR_HTTPD_BASE 0xb000
#define ESP_ERR_HTTPD_HANDLERS_FULL (ESP_ERR_HTTPD_BASE + 1)
#define ESP_ERR_HTTPD_HANDLER_EXISTS (ESP_ERR_HTTPD_BASE + 2)
#define ESP_ERR_HTTPD_INVALID_REQ (ESP_ERR_HTTPD_BASE + 3)
#define ESP_ERR_HTTPD_RESULT_TRUNC (ESP_ERR_HTTPD_BASE + 4)
#define ESP_ERR_HTTPD_TASK (ESP_ERR_HTTPD_BASE + 8)

#define HTTPD_SOCK_ERR_FAIL -1
#define HTTPD_SOCK_ERR_INVALID -2
#define HTTPD_SOCK_ERR_TIMEOUT -3

#define HTTPD_RESP_USE_STRLEN -1
#define HTTPD_MAX_URI_LEN 512

typedef void* httpd_handle_t;

typedef enum {
  HTTP_DELETE = 0,
  HTTP_GET = 1,
  HTTP_HEAD = 2,
  HTTP_POST = 3,
  HTTP_PUT = 4,
} httpd_method_t;

struct httpd_config_t {
  unsigned task_priority;
  size_t stack_size;
  int core_id;
  uint16_t server_port;  // 0: any free port (tests; see httpd_compat_port)
  uint16_t max_open_sockets;
  uint16_t max_uri_handlers;
  uint16_t recv_wait_timeout;  // seconds
  uint16_t send_wait_timeout;  // seconds
  bool lru_purge_enable;
  bool loopback_only;  // Linux only: listen on 127.0.0.1 (tests)
};

#define HTTPD_DEFAULT_CONFIG()                                                            \
  httpd_config_t {                                                                        \
    5, 4096, 0x7fffffff, 80, 7, 8, 5, 5, false, false                                     \
  }

struct httpd_req_t {
  httpd_handle_t handle;
  int method;
  char uri[HTTPD_MAX_URI_LEN + 1];
  size_t content_len;
  void* aux;
  void* user_ctx;
};

struct httpd_uri_t {
  const char* uri;
  httpd_method_t method;
  esp_err_t (*handler)(httpd_req_t* r);
  void* user_ctx;
};

esp_err_t httpd_start(httpd_handle_t* handle, const httpd_config_t* config);
// Stops the server and waits for its thread, unless called from a handler,
// when it stops once that request is answered.
esp_err_t httpd_stop(httpd_handle_t handle);
esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t* uri_handler);

int httpd_req_to_sockfd(httpd_req_t* r);
int httpd_req_recv(httpd_req_t* r, char* buf, size_t buf_len);
size_t httpd_req_get_hdr_value_len(httpd_req_t* r, const char* field);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t* r, const char* field, char* val, size_t val_size);
size_t httpd_req_get_url_query_len(httpd_req_t* r);
esp_err_t httpd_req_get_url_query_str(httpd_req_t* r, char* buf, size_t buf_len);
esp_err_t httpd_query_key_value(const char* qry, const char* key, char* val, size_t val_size);

esp_err_t httpd_resp_set_status(httpd_req_t* r, const char* status);
esp_err_t httpd_resp_set_type(httpd_req_t* r, const char* type);
esp_err_t httpd_resp_set_hdr(httpd_req_t* r, const char* field, const char* value);
esp_err_t httpd_resp_send(httpd_req_t* r, const char* buf, ssize_t buf_len);
esp_err_t httpd_resp_send_chunk(httpd_req_t* r, const char* buf, ssize_t buf_len);
esp_err_t httpd_resp_send_404(httpd_req_t* r);

inline esp_err_t httpd_resp_sendstr(httpd_req_t* r, const char* str) {
  return httpd_resp_send(r, str, HTTPD_RESP_USE_STRLEN);
}
inline esp_err_t httpd_resp_sendstr_chunk(httpd_req_t* r, const char* str) {
  return httpd_resp_send_chunk(r, str, str ? HTTPD_RESP_USE_STRLEN : 0);
}

// Linux only: the port a started server listens on.
uint16_t httpd_compat_port(httpd_handle_t handle);
