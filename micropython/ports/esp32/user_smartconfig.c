
/*

#include "user_smartconfig.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "freertos/event_groups.h"
#include <string.h>

static EventGroupHandle_t wifi_event_group;

wifi_config_t wifi_sta_config;

enum SmartConfig
{
    ScModeFlag = BIT0,
    ScConnected = BIT1,
    ScEspTouchDone = BIT2,
    ScFinlishDone = BIT3,
};

static const char *TAG = "smartconfig";

void smartconfig_task(void *parm);

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START: // wifi STA 开始
            printf("[WIFI AP+STA] STA start event!\r\n");
            if (ScModeFlag & xEventGroupGetBits(wifi_event_group))
            {
                xTaskCreate(smartconfig_task, "smartconfig_task", 2048, NULL, 3, NULL);
            }
            else
            {
                ESP_ERROR_CHECK(esp_wifi_connect()); // 连接wifi
            }
            break;
        case SYSTEM_EVENT_STA_GOT_IP: // 分配到了IP
            printf("[WIFI AP+STA] Get IP event!\r\n");
            printf("[WIFI AP+STA] ESP32 IP: %s !\r\n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            break;
        case SYSTEM_EVENT_STA_CONNECTED: // 连接上了wifi
        {
            wifi_ap_record_t my_wifi_info;
            printf("[WIFI AP+STA] Wifi STA connect event!\r\n");
            esp_wifi_sta_get_ap_info(&my_wifi_info); // STA模式下，获取模块连接上的wifi热点的信息
            printf("[WIFI AP+STA] Connect to : %s!\r\n", my_wifi_info.ssid);
            if (ScModeFlag & xEventGroupGetBits(wifi_event_group))
            {
                xEventGroupClearBits(wifi_event_group, ScConnected);
            }
            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED: // 断开了与wifi的连接
            printf("[WIFI AP+STA] Wifi STA disconnect event, reconnect!\r\n");
            ESP_ERROR_CHECK(esp_wifi_connect()); // 重新连接wifi
            if (ScModeFlag & xEventGroupGetBits(wifi_event_group))
            {
                xEventGroupClearBits(wifi_event_group, ScConnected);
            }
            break;
        case SYSTEM_EVENT_AP_START: // wifi AP 开始
            printf("[WIFI AP+STA] AP start event!\r\n");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED: // 有站点（STA）连接上ESP32的AP
            printf("[WIFI AP+STA] A station connected to ESP32 soft-AP!\r\n");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED: // 有站点（STA）与ESP32的AP断开连接
            printf("[WIFI AP+STA] A station disconnected from ESP32 soft-AP!\r\n");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void smartconfig_callback(smartconfig_status_t status, void *pdata)
{
    switch (status)
    {
        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
            ESP_ERROR_CHECK(esp_wifi_disconnect());
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config));
            ESP_ERROR_CHECK(esp_wifi_connect());
            wifi_sta_config = *wifi_config;
            break;
        case SC_STATUS_LINK_OVER:
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL)
            {
                uint8_t phone_ip[4] = {0};
                memcpy(phone_ip, (uint8_t *)pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(wifi_event_group, ScEspTouchDone);
            break;
        default:
            break;
    }
}

void smartconfig_task(void *parm)
{
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    ESP_ERROR_CHECK(esp_smartconfig_start(smartconfig_callback));
    while (1)
    {
        EventBits_t uxBits = xEventGroupWaitBits(wifi_event_group, ScConnected | ScEspTouchDone, true, false, portMAX_DELAY);
        if (uxBits & ScConnected)
        {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ScEspTouchDone)
        {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            xEventGroupClearBits(wifi_event_group, ScModeFlag | ScEspTouchDone);
            xEventGroupSetBits(wifi_event_group, ScFinlishDone);
            vTaskDelete(NULL);
        }
    }
}

void config_smartconfig_wifi(void)
{
    wifi_event_group = xEventGroupCreate();

    xEventGroupSetBits(wifi_event_group, ScModeFlag);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wait ScFinlishDone");
    EventBits_t uxBits = xEventGroupWaitBits(wifi_event_group, ScFinlishDone, true, false, portMAX_DELAY);
    if(uxBits & ScFinlishDone)
    {
        ESP_LOGI(TAG, "finlish");
        // ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_deinit());
        xEventGroupClearBits(wifi_event_group, ScModeFlag);
    }
    
    vEventGroupDelete(wifi_event_group);
}

void config_ap_sta_wifi(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config)); // STA配置
    wifi_config_t wifi_ap_config = {0};
    wifi_ap_config.ap.ssid_len = strlen(WIFI_AP_SSID);
    memcpy(wifi_ap_config.ap.ssid, WIFI_AP_SSID, strlen(WIFI_AP_SSID));
    wifi_ap_config.ap.max_connection = 4;                                  // 能连接的设备数
    wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;                           //WIFI_AUTH_WPA_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config)); // AP配置
    ESP_ERROR_CHECK(esp_wifi_start());
    puts("config_ap_sta_wifi start");
}

#include "esp_spiffs.h"
#include "esp_wifi_types.h"

bool wifi_config_init()
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 3,
      .format_if_mount_failed = true
    };
    // esp_spiffs_format(NULL);
    return (ESP_OK == esp_vfs_spiffs_register(&conf));
}

void wifi_config_exit()
{
    esp_vfs_spiffs_unregister(NULL);
}

bool wifi_config_read(wifi_config_t *config)
{
    bool res = false;
    puts("wifi_config_read");
    FILE* f = fopen(SMART_CONFIG_FILE, "rb");
    if(f)
    {
        puts("fopen");
        if(1 == fread(config, sizeof(*config), 1, f))
        {
            puts("fread");
            res = true;
        }
        fclose(f);
    }
    puts("wifi_config_read_fail");
    return res;
}

bool wifi_config_write(wifi_config_t *config)
{
    bool res = false;
    puts("wifi_config_write");
    FILE* f = fopen(SMART_CONFIG_FILE, "wb");
    if(f)
    {
        puts("fopen");
        if(1 == fwrite(config, sizeof(*config), 1, f))
        {
            puts("fwrite");
            res = true;
        }
        fclose(f);
    }
    puts("wifi_config_write_fail");
    return res;
}

void config_default_wifi()
{
    tcpip_adapter_init();
    
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

    bool spiffs = wifi_config_init(), config = false;
    
    printf("spiffs wifi_config_init result:%d\n", spiffs);
    
    gpio_set_direction(SMART_CONFIG_KEY, GPIO_MODE_INPUT);
    if ((spiffs && false == (config = wifi_config_read(&wifi_sta_config))) || 0 == gpio_get_level(SMART_CONFIG_KEY))
    {
        config_smartconfig_wifi();
        config = true;
        if(spiffs && false == wifi_config_write(&wifi_sta_config))
        {
            puts("error config save fail");
            remove(SMART_CONFIG_FILE);
            config = false;
        }
        // esp_restart();
    }
    
    if(spiffs)
    {
        puts("wifi_config_exit");
        wifi_config_exit(); 
    }
    else
    {
        puts("spiffs error! need to erase");
    }
    
    if(config)
    {
        // wifi_sta_config
        if(1)
        {        
            config_ap_sta_wifi();
        }
    }
    else
    {
        printf("press SMART_CONFIG_KEY : %d and reset to SmartConfig please.\n", SMART_CONFIG_KEY);
    }    
}
*/

#include "user_smartconfig.h"

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"

static wifi_config_t wifi_sta_config;

static bool smartconfig_mode = false;
static EventGroupHandle_t wifi_event_group;

static const int ESPTOUCH_DONE_BIT = BIT0;
static const int SMARTCONFIG_DONE_BIT = BIT1;
static const char *TAG = "sc";

void smartconfig_task(void * parm);

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    
        case SYSTEM_EVENT_STA_START: // wifi STA 开始
            printf("[WIFI AP+STA] STA start event!\r\n");
            if (smartconfig_mode)
            {
                xTaskCreate(smartconfig_task, "smartconfig_task", 2048, NULL, 3, NULL);
            }
            else
            {
                ESP_ERROR_CHECK(esp_wifi_connect()); // 连接wifi
            }
            break;
        case SYSTEM_EVENT_STA_GOT_IP: // 分配到了IP
            printf("[WIFI AP+STA] Get IP event!\r\n");
            printf("[WIFI AP+STA] ESP32 IP: %s !\r\n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            break;
        case SYSTEM_EVENT_STA_CONNECTED: // 连接上了wifi
        {
            wifi_ap_record_t my_wifi_info;
            printf("[WIFI AP+STA] Wifi STA connect event!\r\n");
            esp_wifi_sta_get_ap_info(&my_wifi_info); // STA模式下，获取模块连接上的wifi热点的信息
            printf("[WIFI AP+STA] Connect to : %s!\r\n", my_wifi_info.ssid);
            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED: // 断开了与wifi的连接
            printf("[WIFI AP+STA] Wifi STA disconnect event, reconnect!\r\n");
            ESP_ERROR_CHECK(esp_wifi_connect()); // 重新连接wifi
            break;
        case SYSTEM_EVENT_AP_START: // wifi AP 开始
            printf("[WIFI AP+STA] AP start event!\r\n");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED: // 有站点（STA）连接上ESP32的AP
            printf("[WIFI AP+STA] A station connected to ESP32 soft-AP!\r\n");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED: // 有站点（STA）与ESP32的AP断开连接
            printf("[WIFI AP+STA] A station disconnected from ESP32 soft-AP!\r\n");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            wifi_sta_config = *wifi_config;
            break;
        case SC_STATUS_LINK_OVER:
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

void smartconfig_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    while (true) {
        uxBits = xEventGroupWaitBits(wifi_event_group, ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 

        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            xEventGroupClearBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            xEventGroupSetBits(wifi_event_group, SMARTCONFIG_DONE_BIT);
            vTaskDelete(NULL);
        }
    }
}

#include "esp_spiffs.h"
#include "esp_wifi_types.h"

bool wifi_config_init()
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 3,
      .format_if_mount_failed = true
    };
    // esp_spiffs_format(NULL);
    return (ESP_OK == esp_vfs_spiffs_register(&conf));
}

void wifi_config_exit()
{
    esp_vfs_spiffs_unregister(NULL);
}

bool wifi_config_read(wifi_config_t *config)
{
    bool res = false;
    FILE* f = fopen(SMART_CONFIG_FILE, "rb");
    if(f)
    {
        puts("fopen");
        if(1 == fread(config, sizeof(*config), 1, f))
        {
            puts("fread");
            res = true;
        }
        fclose(f);
    }
    return res;
}

bool wifi_config_write(wifi_config_t *config)
{
    bool res = false;
    FILE* f = fopen(SMART_CONFIG_FILE, "wb");
    if(f)
    {
        puts("fopen");
        if(1 == fwrite(config, sizeof(*config), 1, f))
        {
            puts("fwrite");
            res = true;
        }
        fclose(f);
    }
    return res;
}

bool config_default_wifi(void)
{
    bool result = false;
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
    wifi_config_t wifi_ap_config = {0};
    wifi_ap_config.ap.ssid_len = strlen(WIFI_AP_SSID);
    memcpy(wifi_ap_config.ap.ssid, WIFI_AP_SSID, strlen(WIFI_AP_SSID));
    wifi_ap_config.ap.max_connection = 4;                                  // 能连接的设备数
    wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;                           //WIFI_AUTH_WPA_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config)); // AP配置
    
    bool spiffs = wifi_config_init(), config = false;
    printf("spiffs wifi_config_init result:%d\n", spiffs);
    
    gpio_set_direction(SMART_CONFIG_KEY, GPIO_MODE_INPUT);
    if ((spiffs && false == (config = wifi_config_read(&wifi_sta_config))) || 0 == gpio_get_level(SMART_CONFIG_KEY))
    {
        smartconfig_mode = true;
    }
    else if(config)
    {
        wifi_config_t *wifi_config = &wifi_sta_config;
        ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
        ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
    }
    else
    {
        printf("press SMART_CONFIG_KEY : %d and reset to SmartConfig please.\n", SMART_CONFIG_KEY);
    }
    
    ESP_ERROR_CHECK( esp_wifi_start() );
    if(smartconfig_mode)
    {
        smartconfig_mode = false;
        ESP_LOGI(TAG, "wait SMARTCONFIG_DONE_BIT");
        
        EventBits_t uxBits = xEventGroupWaitBits(wifi_event_group, SMARTCONFIG_DONE_BIT, true, false, portMAX_DELAY);
        
        if(uxBits & SMARTCONFIG_DONE_BIT)
        {
            result = true;
            ESP_LOGI(TAG, "finlish");
            if(false == wifi_config_write(&wifi_sta_config))
            {
                puts("error config save fail");
                remove(SMART_CONFIG_FILE);
                result = false;
            }
            // esp_restart();
        }
    }
    
    (spiffs) ? wifi_config_exit(), puts("wifi_config_exit") : puts("spiffs error! need to erase");
    
    vEventGroupDelete(wifi_event_group);
    return result;
}

