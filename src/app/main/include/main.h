#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_heap_task_info.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "wifi_init.h"
#include "ota.h"
#include "firebase_utils.h"
#include "ntag_reader.h"
#include "firebase_http_client.h"

#define ID_LEN 11

typedef enum
{
    STATE_WIFI_INIT,
    STATE_WIFI_READY,
    STATE_IDLE,
    STATE_USER_DETECTED,
    STATE_DATABASE_VALIDATION,
    STATE_CHECK_IN,
    STATE_CHECK_OUT,
    STATE_ADMIN_MODE,
    STATE_VALIDATION_FAILURE,
    STATE_ERROR
} state_t;

pn532_t nfc; // Defined in ntag_reader.h

char user_id[ID_LEN];
void state_control_task(void *pvParameter);
void blink_led_1_task(void *pvParameter);
void blink_led_2_task(void *pvParameter);
