idf_component_register(SRCS "main.c"
                            "wifi_manager.c" 
                        INCLUDE_DIRS "." 
                                    "include/"
                        REQUIRES esp_http_server json esp_event esp_netif esp_wifi nvs_flash
                        EMBED_TXTFILES "../html/index.html"
                                       "../html/alert.html")
                    
