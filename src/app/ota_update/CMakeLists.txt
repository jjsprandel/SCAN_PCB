# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "ota.c"
                    INCLUDE_DIRS "." "./include"
                    EMBED_TXTFILES ${project_dir}/server_certs/github_cert.pem
                    PRIV_REQUIRES esp_event app_update esp_http_client esp_https_ota efuse mbedtls)