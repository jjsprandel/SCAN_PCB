#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

void wifi_init_sta(void);
void wifi_init_task(void *pvParameter);

extern SemaphoreHandle_t wifi_init_semaphore;