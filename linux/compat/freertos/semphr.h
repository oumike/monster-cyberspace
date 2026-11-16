// SPDX-License-Identifier: GPL-3.0-or-later
// FreeRTOS mutexes over std::timed_mutex (the app uses no other semaphore).
#pragma once

#include "FreeRTOS.h"

typedef struct compat_mutex* SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t ticks);
BaseType_t xSemaphoreGive(SemaphoreHandle_t m);
void vSemaphoreDelete(SemaphoreHandle_t m);
