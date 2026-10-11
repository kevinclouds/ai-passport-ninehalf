idf_component_register(
    SRCS "main.c" "ninehalf_model.c" "ninehalf_stream.c"
         "ninehalf_store.c" "ninehalf_ui.c" "ninehalf_fonts.c"
         "ninehalf_wifi.c" "ninehalf_network.c" "ninehalf_blufi.c" "demo_blufi_security.c"
         "demo_radio.c"
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_event esp_hw_support esp_netif esp_timer esp_wifi
             nvs_flash esp_http_client esp-tls mbedtls json esp_rom
)
