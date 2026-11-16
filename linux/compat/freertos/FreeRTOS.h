// SPDX-License-Identifier: GPL-3.0-or-later
// FreeRTOS's types and constants for the Linux build. A tick is 1 ms.
#pragma once

#include <cstdint>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;

#define pdFALSE 0
#define pdTRUE 1
#define pdFAIL pdFALSE
#define pdPASS pdTRUE

#define portMAX_DELAY ((TickType_t)0xffffffffUL)
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
// Two, so the app keeps its ESP32-S3 task layout.
#define portNUM_PROCESSORS 2
