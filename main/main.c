/**
 * @file main.c
 * 
 * @author Ruben (rubenartero86@gmail.com)
 * 
 * @brief This example will cover all the phases to connect
 *        ESP32 device to Internet Network.
 * 
 * @note If you found this stuff interesting for your self../\/\
 * 
 * @version 0.1
 * @date 2023-02-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_manager.h"


static const char *TAG = "main";


void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    //  Create default event loop that running in background
	ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    //  Initialize WiFi manager in order to configure WiFi stack
    wifi_manager_init();
}