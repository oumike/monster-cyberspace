// SPDX-License-Identifier: GPL-3.0-or-later
// ESP-IDF's error codes, as far as the Linux build needs them.
#pragma once

#include <cstdint>

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
