idf_component_register(SRCS "main.cpp"
                "platform/platform_bootstrap.cpp"
                "sensors/q5x_pfm.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES arduino heltec_esp32_lora_v3)
