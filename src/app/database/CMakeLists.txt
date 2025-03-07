# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "firebase_http_client.c" "firebase_utils.c" "cJSON_Utils.c" "cJSON.c"
                    INCLUDE_DIRS "." "./include"
                    EMBED_TXTFILES ${project_dir}/server_certs/firebase_cert.pem
                    PRIV_REQUIRES esp_event esp_http_client efuse mbedtls esp-tls)