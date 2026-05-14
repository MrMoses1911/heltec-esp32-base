idf_component_register(
                      SRCS "src/RadioLib_convenience.cpp"
                      INCLUDE_DIRS "src"
                      REQUIRES arduino RadioLib esp8266-oled-ssd1306 HotButton
                      )