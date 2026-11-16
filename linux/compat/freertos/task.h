// SPDX-License-Identifier: GPL-3.0-or-later
// FreeRTOS tasks as detached threads. Core and priority are ignored; the
// stack is the platform's default thread stack (larger than any ESP one).
#pragma once

#include "FreeRTOS.h"

typedef struct compat_task* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char* name, uint32_t stackBytes, void* arg,
                                   UBaseType_t priority, TaskHandle_t* out, BaseType_t core);
BaseType_t xTaskCreate(TaskFunction_t fn, const char* name, uint32_t stackBytes, void* arg,
                       UBaseType_t priority, TaskHandle_t* out);
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount();
// The running task with this name, or nullptr once its function returned.
TaskHandle_t xTaskGetHandle(const char* name);
// Only the [mem] logs read it; a thread's stack has no watermark here.
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t t);
