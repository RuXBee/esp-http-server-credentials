#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "lwip/ip4.h"
#include "wifi_manager.h"
#include "cJSON.h"

static const char *TAG = "wifi_manager";
static httpd_handle_t http_server_instance = NULL;
static wifi_manager_t wifi_mgr_device;

static esp_err_t 

/**
 * @brief Handler html for WiFi Credeentials Form
 * 
 * @param req 
 * @return esp_err_t 
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    size_t index_len = index_end - index_start;
    char *index_html = (char*)malloc(index_len);
    esp_err_t err;

    memcpy(index_html, index_start, index_len);

    ESP_LOGI(TAG, "This is the handler for the <%s> URI", req->uri);

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
 *          1) Check integrity data received from WebServer
 *          2) Create cJSON object in order to manage buffer
 *          3) Save struc in nvm (non volatile memory)
 *          4) Restart ESP in order to configure as STA in the next power up  
 * @param req 
 * @return esp_err_t 
 */
static esp_err_t credentials_handler(httpd_req_t *req)
{
    char buff[100];
    size_t buff_size = MIN(req->content_len, sizeof(buff));
    size_t alert_len = alert_end - alert_start;
    char alert_html[alert_len];
    int ret = httpd_req_recv(req, buff, buff_size);
    esp_err_t err;
    wifi_config_t wifi_config;
    cJSON *json;

    ESP_LOGI(TAG, "Buffer received: %s", buff);

    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    
    // Parse buffer data in cJSON object
    json = cJSON_Parse(buff);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGI(TAG, "Error %s parsing cJSON", error_ptr);
            httpd_resp_send_404(req);
        }
        return ESP_FAIL;
    }
    // Parse device_name
    memcpy(&wifi_mgr_device.device_name, 
        cJSON_GetObjectItem(json, "device_name")->valuestring,
        MIN(strlen(cJSON_GetObjectItem(json, "device_name")->valuestring), sizeof(wifi_mgr_device.device_name)));
    // Parse WiFi SSID
    memcpy(&wifi_mgr_device.wifi_ssid, 
        cJSON_GetObjectItem(json, "ssid")->valuestring,
        MIN(strlen(cJSON_GetObjectItem(json, "ssid")->valuestring), sizeof(wifi_mgr_device.wifi_ssid)));
    // Parse WiFi password
    memcpy(&wifi_mgr_device.wifi_password, 
        cJSON_GetObjectItem(json, "password")->valuestring,
        MIN(strlen(cJSON_GetObjectItem(json, "password")->valuestring), sizeof(wifi_mgr_device.wifi_password)));
    // Deallocate cJSON struct memory 
    cJSON_Delete(json);

    printf("\n================================");
    ESP_LOGI(TAG, "\n\tDevice name: %s\n\tWiFi SSID: %s\n\tWiFi pwd: %s", 
        wifi_mgr_device.device_name,
        wifi_mgr_device.wifi_ssid,
        wifi_mgr_device.wifi_password);
    printf("================================\n");
    
    // Send view to user
    memcpy(alert_html, alert_start, alert_len);
    err = httpd_resp_send(req, alert_html, alert_len);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error %d while sending response", err);
    }
    // // Reset module in order to configure WiFi as STA and try connection
    // esp_restart();
    return ESP_OK;
}
/**
 * @brief Credentials URI
 * 
 */
static httpd_uri_t credentials_uri = {
    .uri = "/wifi/cred", 
    .method = HTTP_POST,
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
esp_err_t wifi_manager_init(void)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif_ap;
    esp_netif_t *netif_sta;

	//  Initialize TCP/IP network interface (should be called only once in application)
	ESP_ERROR_CHECK(esp_netif_init());
	
    //  Create default AP and STA WiFi mode
    netif_ap = esp_netif_create_default_wifi_ap();
    netif_sta = esp_netif_create_default_wifi_sta();

    // Init WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //  Get old WiFi mode
    ESP_ERROR_CHECK(esp_wifi_get_mode(&wifi_mgr_device.old_wifi_mode));

    // Storage WiFi configuration in flash
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    
    // Event handler configured in order to attend Wi
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Configure WiFi as AP
    if (wifi_mgr_device.old_wifi_mode == WIFI_MODE_AP || wifi_mgr_device.old_wifi_mode == WIFI_MODE_APSTA) {
        NULL;
        }
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_WIFI_SSID,
            .ssid_len = strlen(AP_WIFI_SSID),
            .password = AP_WIFI_PASSWORD,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(AP_WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_dhcps_stop(netif_ap);
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_set_ip_info(netif_ap, &ip_info);
    esp_netif_dhcps_start(netif_ap);

    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif_ap, &ip_info));

    ESP_LOGI(TAG, "ESP32 AP configured.\r\n- IP = "IPSTR"\r\n- SSID = %s\r\n- PASS = %s",
                IP2STR(&ip_info.ip),
                AP_WIFI_SSID,
                AP_WIFI_PASSWORD);
    return ESP_OK;
}