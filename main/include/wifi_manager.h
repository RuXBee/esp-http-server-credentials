#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H
#include "esp_wifi_types.h"

/**
 * @brief MACRO defined by user
 * 
 */
#define AP_WIFI_SSID            "esp_device"
#define AP_WIFI_PASSWORD        "12345678"
#define AP_MAX_CONN             1

/**
 * @brief Binary files from html templates
 */
extern unsigned char index_start[] asm("_binary_index_html_start");
extern unsigned char index_end[] asm("_binary_index_html_end");
extern unsigned char alert_start[] asm("_binary_alert_html_start");
extern unsigned char alert_end[] asm("_binary_alert_html_end");

/**
 * @brief WiFi form data structure
 * 
 */
typedef struct {
    uint8_t retries_connection_sta;
    wifi_mode_t wifi_mode;
    wifi_mode_t old_wifi_mode;
    char device_name[10];
    char wifi_ssid[20];
    char wifi_password[20];
} wifi_manager_t;

/**
 * @brief Prototype functions declaration
 * 
 */
esp_err_t wifi_manager_init(void);

#endif /* WIFI_MANAGER_H */