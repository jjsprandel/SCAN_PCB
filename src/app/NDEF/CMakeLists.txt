idf_component_register(SRCS "ntag_reader.c" "pn532.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_gpio)