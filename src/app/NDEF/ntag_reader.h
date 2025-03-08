#ifndef NTAG_READER_H
#define NTAG_READER_H

#include "pn532.h"
#include <esp_log.h>
#include <esp_log_buffer.h>

// SPI Pin Configuration
#define PN532_SCK 10 // 10
#define PN532_MOSI 7 // 7
#define PN532_SS 20
#define PN532_MISO 2

// NTAG213 Memory Layout
#define NTAG_213_USER_START 4
#define NTAG_213_USER_STOP 39
#define NDEF_TEXT_RECORD_TYPE 0x54

// Self-defined User ID character limit
#define MAX_ID_LEN 64

// Debugging Macros
#define NTAG_DEBUG_EN
#define DISPLAY_TARGET_ID_EN

extern pn532_t nfc;
extern TaskHandle_t id_receiver_task_handle;

void nfc_init();
void kill_all_nfc_tasks();
bool kill_task(TaskHandle_t *task_handle);
void nfc_task_handler(char caseValue);
void ntag_write_text_task(void *pvParameters);
bool read_user_id(char *userID, uint16_t readerTimeout);
bool write_user_id(char *userID, uint16_t readerTimeout);
void ntag2xx_read_user_id_task(void *pvParameters);
void ntag2xx_memory_dump_task(void *pvParameters);
void ntag_erase_task(void *pvParameters);
void ntag_write_uri_task(void *pvParameters);
uint8_t pn532_ntag2xx_WriteNDEF_TEXT(pn532_t *obj, char *text, uint8_t dataLen);
#endif