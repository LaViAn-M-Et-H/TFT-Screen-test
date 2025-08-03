#pragma once
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sntp.h"

void time_sync_notification_cb(struct timeval *tv)
void initialize_sntp(void)
void obtain_time(void);
void print_time_task(void *pvParam);
