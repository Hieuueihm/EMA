idf_component_register(
  SRCS
    "main.c"
    "wifi_sta.c"
    "wifi_ap.c"
    "mqtt.c"
  INCLUDE_DIRS "."
  REQUIRES driver esp_wifi esp_netif esp_event nvs_flash esp_http_server mqtt
  EMBED_TXTFILES
    "index.html"           
)