#include "ntag_reader.h"

#define NTAG_DEBUG_EN

// ESP Log Tags
static const char *CARD_READER_TAG = "ntag_reader";
static const char *INIT_TAG = "PN532 Initialization";

void nfc_init()
{
    pn532_spi_init(&nfc, PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
    pn532_begin(&nfc);

    uint32_t versiondata = pn532_getFirmwareVersion(&nfc);
    if (!versiondata)
    {
        ESP_LOGI(INIT_TAG, "Didn't find PN53x board");
        while (1)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

#ifdef NTAG_DEBUG_EN
    ESP_LOGI(INIT_TAG, "Found chip PN5 %lu", (versiondata >> 24) & 0xFF);
    ESP_LOGI(INIT_TAG, "Firmware ver. %lu.%lu", (versiondata >> 16) & 0xFF, (versiondata >> 8) & 0xFF);
#endif

    if (!pn532_SAMConfig(&nfc))
    {
#ifdef NTAG_DEBUG_EN
        ESP_LOGI(INIT_TAG, "SAMConfig failed");
#endif
        while (1)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

#ifdef NTAG_DEBUG_EN
    ESP_LOGI(INIT_TAG, "Waiting for an ISO14443A Card ...");
#endif
}

void kill_all_nfc_tasks(TaskHandle_t nfc_write_task_handle, TaskHandle_t nfc_read_task_handle, TaskHandle_t nfc_erase_task_handle, TaskHandle_t nfc_memdump_task_handle)
{
    if (nfc_read_task_handle != NULL)
    {
        vTaskDelete(nfc_read_task_handle);
        nfc_read_task_handle = NULL;
    }
    if (nfc_write_task_handle != NULL)
    {
        vTaskDelete(nfc_write_task_handle);
        nfc_write_task_handle = NULL;
    }
    if (nfc_erase_task_handle != NULL)
    {
        vTaskDelete(nfc_erase_task_handle);
        nfc_erase_task_handle = NULL;
    }
    if (nfc_memdump_task_handle != NULL)
    {
        vTaskDelete(nfc_memdump_task_handle);
        nfc_memdump_task_handle = NULL;
    }
}

bool kill_task(TaskHandle_t *task_handle)
{
    if (*task_handle != NULL)
    {
        vTaskDelete(*task_handle);
        *task_handle = NULL;
        return true;
    }
    return false;
}

void nfc_task_handler(char caseValue)
{
    TaskHandle_t nfc_write_task_handle = NULL, nfc_read_task_handle = NULL;
    TaskHandle_t nfc_erase_task_handle = NULL, nfc_memdump_task_handle = NULL;
    switch (caseValue)
    {
    case '0': // Read User ID
        if (nfc_read_task_handle == NULL)
            xTaskCreate(ntag2xx_read_user_id_task, "ntag2xx_read_user_id_task", 4096, NULL, 1, &nfc_read_task_handle);
        kill_task(&nfc_write_task_handle);
        kill_task(&nfc_erase_task_handle);
        kill_task(&nfc_memdump_task_handle);
        break;
    case '1': // Write User ID
        if (nfc_write_task_handle == NULL)
            xTaskCreate(ntag_write_text_task, "ntag_write_task", 4096, NULL, 1, &nfc_write_task_handle);
        kill_task(&nfc_read_task_handle);
        kill_task(&nfc_erase_task_handle);
        kill_task(&nfc_memdump_task_handle);
        break;
    case '2': // Erase ID Card
        if (nfc_erase_task_handle == NULL)
            xTaskCreate(ntag_erase_task, "ntag_erase_task", 4096, NULL, 1, &nfc_erase_task_handle);
        kill_task(&nfc_read_task_handle);
        kill_task(&nfc_write_task_handle);
        kill_task(&nfc_memdump_task_handle);
        break;
    case '3': // Card Memory Dump
        if (nfc_memdump_task_handle == NULL)
            xTaskCreate(ntag2xx_memory_dump_task, "ntag2xx_memory_dump_task", 4096, NULL, 1, &nfc_erase_task_handle);
        kill_task(&nfc_read_task_handle);
        kill_task(&nfc_write_task_handle);
        kill_task(&nfc_erase_task_handle);
        break;
    default:
        kill_all_nfc_tasks(nfc_write_task_handle, nfc_read_task_handle, nfc_erase_task_handle, nfc_memdump_task_handle);
        break;
    }
}

void ntag2xx_memory_dump_task(void *pvParameters)
{
    while (1)
    {
        uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
        uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

        // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
        // the UID, and uidLength will indicate the size of the UUID (normally 7)

        if (pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, uid, &uidLength, 0))
        {
#ifdef DISPLAY_TARGET_ID_EN
            // Display some basic information about the card
            ESP_LOGI(CARD_READER_TAG, "Found an ISO14443A card");
            ESP_LOGI(CARD_READER_TAG, "  UID Length: %d bytes, UID Value:", uidLength);
            esp_log_buffer_hexdump_internal(CARD_READER_TAG, uid, uidLength, ESP_LOG_INFO);
#endif
            if (uidLength == 7)
            {
                uint8_t data[32];
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "Seems to be an NTAG2xx tag (7 byte UID)");
#endif
                for (uint8_t i = NTAG_213_USER_START; i < NTAG_213_USER_STOP; i++)
                {

                    ESP_LOGI(CARD_READER_TAG, "PAGE %c%d: ", (i < 10) ? '0' : ' ', i);

                    // Display the results, depending on 'success'
                    if (pn532_ntag2xx_ReadPage(&nfc, i, data))
                    {
// Dump the page data
#ifdef NTAG_DEBUG_EN
                        esp_log_buffer_hexdump_internal(CARD_READER_TAG, data, 4, ESP_LOG_INFO);
#endif
                    }
                    else
                    {
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "Unable to read the requested page!");
#endif
                    }
                }
            }
            else
            {
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
#endif // DEBUG
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

bool write_user_id(char *userID, uint16_t readerTimeout)
{
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
    uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    uint8_t dataLength;

    // Require some user feedback before running this example!
#ifdef NTAG_DEBUG_EN
    ESP_LOGI(CARD_READER_TAG, "\r\nPlace your NDEF formatted NTAG2xx tag on the reader to update the NDEF record");
#endif

    // 1.) Wait for an NTAG203 card.  When one is found 'uid' will be populated with
    // the UID, and uidLength will indicate the size of the UID (normally 7)

    // It seems we found a valid ISO14443A Tag!
    if (pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, uid, &uidLength, 0))
    {
#ifdef DISPLAY_TARGET_ID_EN
        // Display some basic information about the card
        ESP_LOGI(CARD_READER_TAG, "Found an ISO14443A card");
        ESP_LOGI(CARD_READER_TAG, "  UID Length: %d bytes, UID Value:", uidLength);
        esp_log_buffer_hexdump_internal(CARD_READER_TAG, uid, uidLength, ESP_LOG_INFO);
#endif

        if (uidLength != 7)
        {
#ifdef NTAG_DEBUG_EN
            ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
#endif
            return false;
        }
        else
        {
            uint8_t data[32];
#ifdef NTAG_DEBUG_EN
            ESP_LOGI(CARD_READER_TAG, "Seems to be an NTAG2xx tag (7 byte UID)");
#endif

            // 3.) Check if the NDEF Capability Container (CC) bits are already set
            // in OTP memory (page 3)
            memset(data, 0, 4);
            if (!pn532_ntag2xx_ReadPage(&nfc, 3, data))
            {
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "Unable to read the Capability Container (page 3)");
#endif
                return false;
            }
            else
            {
                // If the tag has already been formatted as NDEF, byte 0 should be:
                // Byte 0 = Magic Number (0xE1)
                // Byte 1 = NDEF Version (Should be 0x10)
                // Byte 2 = Data Area Size (value * 8 bytes)
                // Byte 3 = Read/Write Access (0x00 for full read and write)
                if (!((data[0] == 0xE1) && (data[1] == 0x10)))
                {
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NDEF formatted tag.");
                    ESP_LOGI(CARD_READER_TAG, "Page 3 should start with 0xE1 0x10.");
#endif
                    return false;
                }
                else
                {
                    // 4.) Determine and display the data area size
                    dataLength = data[2] * 8;
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "Tag is NDEF formatted. Data area size = %d bytes", dataLength);
                    // 5.) Erase the old data area
                    ESP_LOGI(CARD_READER_TAG, "Erasing previous data area ");
#endif
                    for (uint8_t i = 4; i < (dataLength / 4) + 4; i++)
                    {
                        memset(data, 0, 4);
                        if (!pn532_ntag2xx_WritePage(&nfc, i, data))
                        {
#ifdef NTAG_DEBUG_EN
                            ESP_LOGI(CARD_READER_TAG, " ERROR!");
#endif
                            return false;
                        }
                    }
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, " DONE!");
#endif

// 6.) Try to add a new NDEF URI record
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "Writing Text as NDEF Record ... ");
#endif
                    if (pn532_ntag2xx_WriteNDEF_TEXT(&nfc, userID, dataLength))
                    {
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "DONE!");
#endif
                    }
                    else
                    {
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "ERROR! (Text length?)");
#endif
                        return false;
                    }

                } // CC contents NDEF record check
            } // CC page read check
        } // UUID length check

        // Wait a bit before trying again
    } // Start waiting for a new ISO14443A tag
    return true;
}

bool read_user_id(char *userID, uint16_t readerTimeout)
{
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
    uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    uint8_t dataLength;
    uint8_t pageBuffer[4];
    uint8_t pageHeader[12];

    // int messageLength, ndefStartIndex, bufferSize;
    // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
    // the UID, and uidLength will indicate the size of the UUID (normally 7)

    if (pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, uid, &uidLength, readerTimeout))
    {
#ifdef DISPLAY_TARGET_ID_EN
        // Display some basic information about the card
        ESP_LOGI(CARD_READER_TAG, "Found an ISO14443A card");
        ESP_LOGI(CARD_READER_TAG, "  UID Length: %d bytes, UID Value:", uidLength);
        esp_log_buffer_hexdump_internal(CARD_READER_TAG, uid, uidLength, ESP_LOG_INFO);
#endif

        if (uidLength == 7)
        {
            uint8_t data[32];
#ifdef NTAG_DEBUG_EN
            ESP_LOGI(CARD_READER_TAG, "Seems to be an NTAG2xx tag (7 byte UID)");
#endif
            // 3.) Check if the NDEF Capability Container (CC) bits are already set
            // in OTP memory (page 3)

            memset(data, 0, 4);
            memset(pageBuffer, 0, 4);
            memset(pageHeader, 0, 12);

            if (!pn532_ntag2xx_ReadPage(&nfc, 3, data))
            {
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "Unable to read the Capability Container (page 3)");
#endif
                return false;
            }
            else
            {
                // If the tag has already been formatted as NDEF, byte 0 should be:
                // Byte 0 = Magic Number (0xE1)
                // Byte 1 = NDEF Version (Should be 0x10)
                // Byte 2 = Data Area Size (value * 8 bytes)
                // Byte 3 = Read/Write Access (0x00 for full read and write)
                if (!((data[0] == 0xE1) && (data[1] == 0x10)))
                {
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NDEF formatted tag.");
                    ESP_LOGI(CARD_READER_TAG, "Page 3 should start with 0xE1 0x10.");
#endif
                    return false;
                }
                else
                {
                    // 4.) Determine and display the data area size
                    dataLength = data[2] * 8;
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "Tag is NDEF formatted. Data area size = %d bytes", dataLength);
#endif
                }
            } // End of CC check

            for (uint8_t i = 0; i < 3; i++)
            {
                if (!pn532_ntag2xx_ReadPage(&nfc, i + 4, pageBuffer))
                {
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "Unable to read page %d", i);
#endif
                    return false;
                }
                memcpy(pageHeader + (i * 4), pageBuffer, 4);
            }

            if (pageHeader[5] != 0x03)
            {
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be a valid NDEF Message. Tag field is %x, should be 0x03", pageHeader[5]);
#endif
                return false;
            }

            uint8_t payloadLength = pageHeader[6] - 5;

            if (pageHeader[8] != 0x01 || pageHeader[10] != NDEF_TEXT_RECORD_TYPE)
            {
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be a valid TEXT record");
                ESP_LOGI(CARD_READER_TAG, "Record Length: %x, Record Type: %x", pageHeader[8], pageHeader[10]);
#endif
                return false;
            }

            uint8_t langCodeLen = pageHeader[11];                // 0x02 for language encoding
            uint8_t textLen = payloadLength - (1 + langCodeLen); // Subtract encoding info byte
            if (textLen >= MAX_ID_LEN)
            {
                textLen = MAX_ID_LEN - 1; // Prevent buffer overflow
            }

            uint8_t currentPage = 7;
            uint8_t bytesRead = 0;
            uint8_t textPayload[MAX_ID_LEN];
            memset(textPayload, 0, MAX_ID_LEN);

            while (bytesRead < textLen + langCodeLen + 1)
            {
                if (!pn532_ntag2xx_ReadPage(&nfc, currentPage, pageBuffer))
                {
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "Unable to read page %d", currentPage);
#endif
                }
                uint8_t bytesToCopy = ((textLen + langCodeLen + 1) - bytesRead < 4) ? ((textLen + langCodeLen + 1) - bytesRead) : 4;
                memcpy(&textPayload[bytesRead], pageBuffer, bytesToCopy);

                bytesRead += bytesToCopy;
                currentPage++;
            }

            // Copy only the text portion (skip the lang code)
            // char userID[textLen];
            memcpy(userID, textPayload + langCodeLen, textLen);
            userID[textLen] = '\0'; // Null-terminate the string
                                    // #ifdef NTAG_DEBUG_EN
            ESP_LOGI(CARD_READER_TAG, "Received NDEF Message %s and transmitting", userID);
            esp_log_buffer_hexdump_internal(CARD_READER_TAG, userID, textLen, ESP_LOG_INFO);
            return true;
        }
        else
        {

#ifdef NTAG_DEBUG_EN
            ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
#endif
            return false;
        }
    }
    return false;
}

void ntag2xx_read_user_id_task(void *pvParameters)
{
    while (1)
    {
        uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
        uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
        uint8_t dataLength;
        uint8_t pageBuffer[4];
        uint8_t pageHeader[12];

        // int messageLength, ndefStartIndex, bufferSize;
        // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
        // the UID, and uidLength will indicate the size of the UUID (normally 7)

        if (pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, uid, &uidLength, 0))
        {
#ifdef DISPLAY_TARGET_ID_EN
            // Display some basic information about the card
            ESP_LOGI(CARD_READER_TAG, "Found an ISO14443A card");
            ESP_LOGI(CARD_READER_TAG, "  UID Length: %d bytes, UID Value:", uidLength);
            esp_log_buffer_hexdump_internal(CARD_READER_TAG, uid, uidLength, ESP_LOG_INFO);
#endif

            if (uidLength == 7)
            {
                uint8_t data[32];
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "Seems to be an NTAG2xx tag (7 byte UID)");
#endif
                // 3.) Check if the NDEF Capability Container (CC) bits are already set
                // in OTP memory (page 3)

                memset(data, 0, 4);
                memset(pageBuffer, 0, 4);
                memset(pageHeader, 0, 12);

                if (!pn532_ntag2xx_ReadPage(&nfc, 3, data))
                {
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "Unable to read the Capability Container (page 3)");
#endif
                    vTaskDelete(NULL);
                }
                else
                {
                    // If the tag has already been formatted as NDEF, byte 0 should be:
                    // Byte 0 = Magic Number (0xE1)
                    // Byte 1 = NDEF Version (Should be 0x10)
                    // Byte 2 = Data Area Size (value * 8 bytes)
                    // Byte 3 = Read/Write Access (0x00 for full read and write)
                    if (!((data[0] == 0xE1) && (data[1] == 0x10)))
                    {
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NDEF formatted tag.");
                        ESP_LOGI(CARD_READER_TAG, "Page 3 should start with 0xE1 0x10.");
#endif
                        continue;
                    }
                    else
                    {
                        // 4.) Determine and display the data area size
                        dataLength = data[2] * 8;
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "Tag is NDEF formatted. Data area size = %d bytes", dataLength);
#endif
                    }
                } // End of CC check

                for (uint8_t i = 0; i < 3; i++)
                {
                    if (!pn532_ntag2xx_ReadPage(&nfc, i + 4, pageBuffer))
                    {
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "Unable to read page %d", i);
#endif
                        continue;
                    }
                    memcpy(pageHeader + (i * 4), pageBuffer, 4);
                }

                if (pageHeader[5] != 0x03)
                {
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be a valid NDEF Message. Tag field is %x, should be 0x03", pageHeader[5]);
#endif
                    continue;
                }

                uint8_t payloadLength = pageHeader[6] - 5;

                if (pageHeader[8] != 0x01 || pageHeader[10] != NDEF_TEXT_RECORD_TYPE)
                {
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be a valid TEXT record");
                    ESP_LOGI(CARD_READER_TAG, "Record Length: %x, Record Type: %x", pageHeader[8], pageHeader[10]);
#endif
                    continue;
                }

                uint8_t langCodeLen = pageHeader[11];                // 0x02 for language encoding
                uint8_t textLen = payloadLength - (1 + langCodeLen); // Subtract encoding info byte
                if (textLen >= MAX_ID_LEN)
                {
                    textLen = MAX_ID_LEN - 1; // Prevent buffer overflow
                    continue;                 // Prevent buffer overflow
                }

                uint8_t currentPage = 7;
                uint8_t bytesRead = 0;
                uint8_t textPayload[MAX_ID_LEN];
                memset(textPayload, 0, MAX_ID_LEN);

                while (bytesRead < textLen + langCodeLen + 1)
                {
                    if (!pn532_ntag2xx_ReadPage(&nfc, currentPage, pageBuffer))
                    {
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "Unable to read page %d", currentPage);
#endif
                        continue;
                    }
                    uint8_t bytesToCopy = ((textLen + langCodeLen + 1) - bytesRead < 4) ? ((textLen + langCodeLen + 1) - bytesRead) : 4;
                    memcpy(&textPayload[bytesRead], pageBuffer, bytesToCopy);

                    bytesRead += bytesToCopy;
                    currentPage++;
                }

                // Copy only the text portion (skip the lang code)
                char userID[textLen];
                memcpy(userID, textPayload + langCodeLen, textLen);
                userID[textLen] = '\0'; // Null-terminate the string
                                        // #ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "Received NDEF Message %s and transmitting", userID);
                esp_log_buffer_hexdump_internal(CARD_READER_TAG, userID, textLen, ESP_LOG_INFO);
                // #endif
                // xTaskNotify(id_receiver_task_handle, userID, eSetValueWithOverwrite); // Transmit ID to receiver task
                // vTaskDelete(NULL);
            }
            else
            {
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
#endif
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void ntag_erase_task(void *pvParameters)
{
    while (1)
    {
        uint8_t success;
        uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
        uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

        // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
        // the UID, and uidLength will indicate the size of the UUID (normally 7)

        if (pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, uid, &uidLength, 0))
        {
#ifdef DISPLAY_TARGET_ID_EN
            // Display some basic information about the card
            ESP_LOGI(CARD_READER_TAG, "Found an ISO14443A card");
            ESP_LOGI(CARD_READER_TAG, "  UID Length: %d bytes, UID Value:", uidLength);
            esp_log_buffer_hexdump_internal(CARD_READER_TAG, uid, uidLength, ESP_LOG_INFO);
#endif

            if (uidLength == 7)
            {
                uint8_t data[32];

#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "Seems to be an NTAG2xx tag (7 byte UID)");
                ESP_LOGI(CARD_READER_TAG, "Writing 0x00 0x00 0x00 0x00 to pages 4..39");
#endif
                for (uint8_t i = 4; i < NTAG_213_USER_STOP; i++)
                {
                    memset(data, 0, 4);
                    success = pn532_ntag2xx_WritePage(&nfc, i, data);

                    // Display the current page number
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "Page %c%d: %s", (i < 10) ? '0' : ' ', i, success ? "Erased" : "Unable to write to the requested page!");
#endif
                }
            }
            else
            {
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
#endif
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void ntag_write_uri_task(void *pvParameters)
{
    while (1)
    {
        uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
        uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
        uint8_t dataLength;
        char *url = "adafruit.com/blog/";
        uint8_t ndefprefix = NDEF_URIPREFIX_HTTP_WWWDOT;
#ifdef NTAG_DEBUG_EN
        ESP_LOGI(CARD_READER_TAG, "\r\nPlace your NDEF formatted NTAG2xx tag on the reader to update the NDEF record");
#endif

        // It seems we found a valid ISO14443A Tag!
        if (pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, uid, &uidLength, 0))
        {
#ifdef DISPLAY_TARGET_ID_EN
            // Display some basic information about the card
            ESP_LOGI(CARD_READER_TAG, "Found an ISO14443A card");
            ESP_LOGI(CARD_READER_TAG, "  UID Length: %d bytes, UID Value:", uidLength);
            esp_log_buffer_hexdump_internal(CARD_READER_TAG, uid, uidLength, ESP_LOG_INFO);
#endif

            if (uidLength != 7)
            {
                ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
            }
            else
            {
                uint8_t data[32];
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "Seems to be an NTAG2xx tag (7 byte UID)");
#endif

                // 3.) Check if the NDEF Capability Container (CC) bits are already set
                // in OTP memory (page 3)
                memset(data, 0, 4);
                if (!pn532_ntag2xx_ReadPage(&nfc, 3, data))
                {
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "Unable to read the Capability Container (page 3)");
#endif
                    continue;
                }
                else
                {
                    // If the tag has already been formatted as NDEF, byte 0 should be:
                    // Byte 0 = Magic Number (0xE1)
                    // Byte 1 = NDEF Version (Should be 0x10)
                    // Byte 2 = Data Area Size (value * 8 bytes)
                    // Byte 3 = Read/Write Access (0x00 for full read and write)
                    if (!((data[0] == 0xE1) && (data[1] == 0x10)))
                    {
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NDEF formatted tag.");
                        ESP_LOGI(CARD_READER_TAG, "Page 3 should start with 0xE1 0x10.");
#endif
                    }
                    else
                    {
                        // 4.) Determine and display the data area size
                        dataLength = data[2] * 8;
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "Tag is NDEF formatted. Data area size = %d bytes", dataLength);

                        // 5.) Erase the old data area
                        ESP_LOGI(CARD_READER_TAG, "Erasing previous data area ");
#endif
                        for (uint8_t i = 4; i < (dataLength / 4) + 4; i++)
                        {
                            memset(data, 0, 4);
                            if (!pn532_ntag2xx_WritePage(&nfc, i, data))
                            {
#ifdef NTAG_DEBUG_EN
                                ESP_LOGI(CARD_READER_TAG, " ERROR!");
#endif
                                continue;
                            }
                        }
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, " DONE!");
#endif

// 6.) Try to add a new NDEF URI record
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "Writing URI as NDEF Record ... ");
#endif
                        if (pn532_ntag2xx_WriteNDEFURI(&nfc, ndefprefix, url, dataLength))
                        {
#ifdef NTAG_DEBUG_EN
                            ESP_LOGI(CARD_READER_TAG, "DONE!");
#endif
                        }
                        else
                        {
#ifdef NTAG_DEBUG_EN
                            ESP_LOGI(CARD_READER_TAG, "ERROR! (URI length?)");
#endif
                        }

                    } // CC contents NDEF record check
                } // CC page read check
            } // UUID length check

            // Wait a bit before trying again
        } // Start waiting for a new ISO14443A tag
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void ntag_write_text_task(void *pvParameters)
{
    while (1)
    {
        uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
        uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
        uint8_t dataLength;
        char *userName = "Cory Brynds";

        // Require some user feedback before running this example!
#ifdef NTAG_DEBUG_EN
        ESP_LOGI(CARD_READER_TAG, "\r\nPlace your NDEF formatted NTAG2xx tag on the reader to update the NDEF record");
#endif

        // 1.) Wait for an NTAG203 card.  When one is found 'uid' will be populated with
        // the UID, and uidLength will indicate the size of the UID (normally 7)

        // It seems we found a valid ISO14443A Tag!
        if (pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, uid, &uidLength, 0))
        {
#ifdef DISPLAY_TARGET_ID_EN
            // Display some basic information about the card
            ESP_LOGI(CARD_READER_TAG, "Found an ISO14443A card");
            ESP_LOGI(CARD_READER_TAG, "  UID Length: %d bytes, UID Value:", uidLength);
            esp_log_buffer_hexdump_internal(CARD_READER_TAG, uid, uidLength, ESP_LOG_INFO);
#endif

            if (uidLength != 7)
            {
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
#endif
            }
            else
            {
                uint8_t data[32];
#ifdef NTAG_DEBUG_EN
                ESP_LOGI(CARD_READER_TAG, "Seems to be an NTAG2xx tag (7 byte UID)");
#endif

                // 3.) Check if the NDEF Capability Container (CC) bits are already set
                // in OTP memory (page 3)
                memset(data, 0, 4);
                if (!pn532_ntag2xx_ReadPage(&nfc, 3, data))
                {
#ifdef NTAG_DEBUG_EN
                    ESP_LOGI(CARD_READER_TAG, "Unable to read the Capability Container (page 3)");
#endif
                    continue;
                }
                else
                {
                    // If the tag has already been formatted as NDEF, byte 0 should be:
                    // Byte 0 = Magic Number (0xE1)
                    // Byte 1 = NDEF Version (Should be 0x10)
                    // Byte 2 = Data Area Size (value * 8 bytes)
                    // Byte 3 = Read/Write Access (0x00 for full read and write)
                    if (!((data[0] == 0xE1) && (data[1] == 0x10)))
                    {
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "This doesn't seem to be an NDEF formatted tag.");
                        ESP_LOGI(CARD_READER_TAG, "Page 3 should start with 0xE1 0x10.");
#endif
                    }
                    else
                    {
                        // 4.) Determine and display the data area size
                        dataLength = data[2] * 8;
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "Tag is NDEF formatted. Data area size = %d bytes", dataLength);
                        // 5.) Erase the old data area
                        ESP_LOGI(CARD_READER_TAG, "Erasing previous data area ");
#endif
                        for (uint8_t i = 4; i < (dataLength / 4) + 4; i++)
                        {
                            memset(data, 0, 4);
                            if (!pn532_ntag2xx_WritePage(&nfc, i, data))
                            {
#ifdef NTAG_DEBUG_EN
                                ESP_LOGI(CARD_READER_TAG, " ERROR!");
#endif
                                continue;
                            }
                        }
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, " DONE!");
#endif

// 6.) Try to add a new NDEF URI record
#ifdef NTAG_DEBUG_EN
                        ESP_LOGI(CARD_READER_TAG, "Writing Text as NDEF Record ... ");
#endif
                        if (pn532_ntag2xx_WriteNDEF_TEXT(&nfc, userName, dataLength))
                        {
#ifdef NTAG_DEBUG_EN
                            ESP_LOGI(CARD_READER_TAG, "DONE!");
#endif
                        }
                        else
                        {
#ifdef NTAG_DEBUG_EN
                            ESP_LOGI(CARD_READER_TAG, "ERROR! (Text length?)");
#endif
                        }

                    } // CC contents NDEF record check
                } // CC page read check
            } // UUID length check

            // Wait a bit before trying again
        } // Start waiting for a new ISO14443A tag
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

uint8_t pn532_ntag2xx_WriteNDEF_TEXT(pn532_t *obj, char *text, uint8_t dataLen)
{
    uint8_t pageBuffer[4] = {0, 0, 0, 0};

    // Remove NDEF record overhead from the URI data (pageHeader below)
    uint8_t wrapperSize = 12;

    // Figure out how long the string is
    uint8_t payloadLen = strlen(text) + strlen("en") + 1;
    // Total field length is: payloadLength + Record Type + Payload Length + Type Length + Record Header

    // Make sure the text payload will fit in dataLen (include 0xFE trailer)
    if ((payloadLen < 1) || (payloadLen + 1 > (dataLen - wrapperSize)))
        return 0;

    // Setup the record header
    // See NFCForum-TS-Type-2-Tag_1.1.pdf for details
    uint8_t pageHeader[12] =
        {
            /* NDEF Lock Control TLV (must be first and always present) */
            0x01, /* Tag Field (0x01 = Lock Control TLV) */
            0x03, /* Payload Length (always 3) */
            0xA0, /* The position inside the tag of the lock bytes (upper 4 = page address, lower 4 = uint8_t offset) */
            0x10, /* Size in bits of the lock area */
            0x44, /* Size in bytes of a page and the number of bytes each lock bit can lock (4 bit + 4 bits) */
            /* NDEF Message TLV - URI Record */
            0x03,                  /* Tag Field (0x03 = NDEF Message) */
            payloadLen + 5,        /* Payload Length (not including 0xFE trailer) */
            0xD1,                  /* NDEF Record Header (TNF=0x1:Well known record + SR + ME + MB) */
            0x01,                  /* Type Length for the record type indicator */
            payloadLen + 1,        /* Payload len */
            NDEF_TEXT_RECORD_TYPE, /* Record Type Indicator (0x54 or 'T' = Text Record) */
            0x02,                  /* Status Byte (UTF-8, encoding length = 1) */
        };

    // Write 12 uint8_t header (three pages of data starting at page 4)
    memcpy(pageBuffer, pageHeader, 4);
    if (!(pn532_ntag2xx_WritePage(obj, 4, pageBuffer)))
        return 0;
    memcpy(pageBuffer, pageHeader + 4, 4);
    if (!(pn532_ntag2xx_WritePage(obj, 5, pageBuffer)))
        return 0;
    memcpy(pageBuffer, pageHeader + 8, 4);
    if (!(pn532_ntag2xx_WritePage(obj, 6, pageBuffer)))
        return 0;

    // Write URI (starting at page 7)
    uint8_t currentPage = 7;
    char textPayload[payloadLen];
    sprintf(textPayload, "en%s", text);
    char *textCpy = textPayload;

    while (payloadLen)
    {
        if (payloadLen < 4)
        {
            memset(pageBuffer, 0, 4);
            memcpy(pageBuffer, textCpy, payloadLen);
            pageBuffer[payloadLen] = 0xFE; // NDEF record footer
            if (!(pn532_ntag2xx_WritePage(obj, currentPage, pageBuffer)))
                return 0;
            // DONE!
            return 1;
        }
        else if (payloadLen == 4)
        {
            memcpy(pageBuffer, textCpy, payloadLen);
            if (!(pn532_ntag2xx_WritePage(obj, currentPage, pageBuffer)))
                return 0;
            memset(pageBuffer, 0, 4);
            pageBuffer[0] = 0xFE; // NDEF record footer
            currentPage++;
            if (!(pn532_ntag2xx_WritePage(obj, currentPage, pageBuffer)))
                return 0;
            // DONE!
            return 1;
        }
        else
        {
            // More than one page of data left
            memcpy(pageBuffer, textCpy, 4);
            if (!(pn532_ntag2xx_WritePage(obj, currentPage, pageBuffer)))
                return 0;
            currentPage++;
            textCpy += 4;
            payloadLen -= 4;
        }
    }

    // Seems that everything was OK (?!)
    return 1;
}