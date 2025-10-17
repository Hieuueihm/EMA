idf_component_register(
  SRCS
    "main.c"
    "wifi_sta.c"
    "wifi_ap.c"
    "lora.c"
    "mqtt.c"
    "fake_node.c"
    "timesync.c"
  INCLUDE_DIRS "."
  REQUIRES driver esp_wifi esp_netif esp_event nvs_flash esp_http_server mqtt
  EMBED_TXTFILES
    "index.html"           
)

# idf_component_register(
#   SRCS
#     "my_first_app.c"
#     "lora.c"
#   INCLUDE_DIRS "."

# )