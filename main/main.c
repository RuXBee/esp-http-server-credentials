/**
 * @file main.c
 * 
 * @author Ruben (rubenartero86@gmail.com)
 * 
 * @brief This example will cover all the phases to connect
 *        ESP32 device to Internet Network.
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
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "lwip/ip4.h"


#define EXAMPLE_ESP_WIFI_SSID "esp_device"
#define EXAMPLE_ESP_WIFI_PASS "free"
#define EXAMPLE_MAX_STA_CONN 1

static const char *TAG = "wifi softAP";


static httpd_handle_t http_server_instance = NULL;

/**
 * @brief Binary files from html templates
 * 
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
    char device_name[10];
    char wifi_ssid[20];
    char wifi_password[20];
} wifi_credentials_t;
static wifi_credentials_t wifi_device;


/**
 * @brief Handler html for WiFi Credeentials Form
 * 
 * @param req 
 * @return esp_err_t 
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    size_t index_len = index_end - index_start;
    const char *index_html = (const char*)malloc(index_len);
    esp_err_t err;

    memcpy(index_html, index_start, index_len);

    ESP_LOGI("HTTP_GET", "This is the handler for the <%s> URI", req->uri);

    err = httpd_resp_send(req, index_html, index_len);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "Error %d while sending response", err);
    }
    free(index_html);
    return ESP_OK;
}

/**
 * @brief Index URI
 * 
 */
static httpd_uri_t index_uri = {
    .uri = "/", 
    .method = HTTP_GET,
    .handler = index_handler,
};

/**
 * @brief Handler html for handler WiFi Form Credentials
 * 
 * @param req 
 * @return esp_err_t 
 */
static esp_err_t credentials_handler(httpd_req_t *req)
{
    int i = 0;
    char *ch;
    size_t alert_len = alert_end - alert_start;
    const char *alert_html = (const char*)malloc(alert_len);
    esp_err_t err;
    
    // Copy in memory the view in order to respond HTTP_GET form
    memcpy(alert_html, alert_start, alert_len);

    // Log URI
    ESP_LOGI("HTTP_POST", "This is the handler for the <%s> URI", req->uri);
    
    // Found credentials of the form
    ch = strchr(req->uri, '=');
    if (ch != NULL) {
        ESP_LOGI("HTTP_POST", "WiFi Credentials received correctly");
        // Fill device_name
        while (*ch != '&') {
            ch++; 
            wifi_device.device_name[i++] = *ch;
        }
        i = 0;
        // Fill WiFi SSID
        while (*ch != '=') 
            ch++;
        while (*ch != '&') {
            ch++; 
            wifi_device.wifi_ssid[i++] = *ch;
        }
        i = 0;
        // Fill WiFi password
        while (*ch != '=') 
            ch++;
        while (*ch != '\0') {
            ch++; 
            wifi_device.wifi_password[i++] = *ch;
        }

        // Log credentials
        ESP_LOGI("WiFi Credentials", "Device name = %s\nWiFi SSID = %s\nWiFi Password = %s",
            wifi_device.device_name,
            wifi_device.wifi_ssid,
            wifi_device.wifi_password);
        
        // Send view to user
        err = httpd_resp_send(req, alert_html, alert_len);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Error %d while sending response", err);
        }
        // Reset module in order to configure WiFi as STA and try connection
        esp_restart();
    }
    free(alert_html);
    return ESP_OK;
}
/**
 * @brief Credentials URI
 * 
 */
static httpd_uri_t credentials_uri = {
    .uri = "/wifi/cred", 
    .method = HTTP_GET,
    .handler = credentials_handler,
};

/**
 * @brief Start http server function 
 * 
 */
static void start_http_server(void)
{
    httpd_config_t http_server_conf = HTTPD_DEFAULT_CONFIG();
    ESP_LOGI(TAG, "Start web server on port: %d", http_server_conf.server_port);
    if (httpd_start(&http_server_instance, &http_server_conf) == ESP_OK) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(http_server_instance, &index_uri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(http_server_instance, &credentials_uri));
    }
}

/**
 * @brief Stop http server function
 * 
 */
static void stop_http_server(void)
{
    if (http_server_instance != NULL) {
        ESP_ERROR_CHECK(httpd_stop(http_server_instance));
    }
}

/**
 * @brief WiFi Event handler
 * 
 * @param arg 
 * @param event_base 
 * @param event_id 
 * @param event_data 
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
        start_http_server();
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
        stop_http_server();
    } else if(event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA start");
    }
}

/**
 * @brief WiFi manager
 * 
 */
static void wifi_manager_init(void)
{
    esp_netif_ip_info_t ip_info;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *p_netif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    IP4_ADDR(&ip_info.ip, 192, 168, 1, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(p_netif);
    esp_netif_set_ip_info(p_netif, &ip_info);
    esp_netif_dhcps_start(p_netif);

    ESP_ERROR_CHECK(esp_netif_get_ip_info(p_netif, &ip_info));

    ESP_LOGI(TAG, "ESP32 AP configured.\r\n- IP = "IPSTR"\r\n- SSID = %s\r\n- PASS = %s",
                IP2STR(&ip_info.ip),
                EXAMPLE_ESP_WIFI_SSID,
                EXAMPLE_ESP_WIFI_PASS);
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_manager_init();
}