set(MAIN_SRCS "main.cpp"
    "comms/lora_manager.cpp"
    "comms/lora_protocol.cpp"
    "platform/platform_bootstrap.cpp"
    "sensors/q5x_pfm.cpp"
    "storage/config_repository.cpp"
    "storage/nvs_store.cpp"
    "system/network_manager.cpp"
    "ui/menu_manager.cpp"
    "ui/status_view.cpp")

set(MAIN_REQUIRES app_update arduino esp_driver_pcnt esp_event esp_http_server esp_netif esp_wifi heltec_esp32_lora_v3 nvs_flash)

idf_component_register(SRCS ${MAIN_SRCS}
                    INCLUDE_DIRS "."
                    REQUIRES ${MAIN_REQUIRES})
