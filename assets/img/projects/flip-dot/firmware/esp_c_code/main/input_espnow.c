/**
 * @file input_espnow.c
 * @brief ESP-NOW input implementation for the input system
 *
 * @author Mikael Bengtsson
 * @date 2025-06-11
 */

#include "input.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "input_espnow";

// Global reference to input system
static input_system_t *g_input_sys = NULL;

// ESP-NOW data structure for controller input
typedef struct {
    uint8_t buttons;  // Bitmap of button states
    uint8_t reserved; // Reserved for future use
} espnow_input_data_t;

// Button bit positions
#define BTN_UP_BIT     0
#define BTN_DOWN_BIT   1
#define BTN_LEFT_BIT   2
#define BTN_RIGHT_BIT  3
#define BTN_SELECT_BIT 4
#define BTN_START_BIT  5
#define BTN_RESET_BIT  6
#define BTN_BACK_BIT   7

// ESP-NOW callback function
static void espnow_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int len) {
    if (!data || len != sizeof(espnow_input_data_t) || !g_input_sys) {
        ESP_LOGW(TAG, "Invalid data received: len=%d, input_sys=%p", len, g_input_sys);
        return;
    }

    espnow_input_data_t *input_data = (espnow_input_data_t *)data;
    
    // Process each button state
    if (input_data->buttons & (1 << BTN_UP_BIT)) {
        ESP_LOGI(TAG, "Button UP pressed");
        send_input_event(g_input_sys, INPUT_CMD_UP, INPUT_TYPE_ESPNOW);
    }
    if (input_data->buttons & (1 << BTN_DOWN_BIT)) {
        ESP_LOGI(TAG, "Button DOWN pressed");
        send_input_event(g_input_sys, INPUT_CMD_DOWN, INPUT_TYPE_ESPNOW);
    }
    if (input_data->buttons & (1 << BTN_LEFT_BIT)) {
        ESP_LOGI(TAG, "Button LEFT pressed");
        send_input_event(g_input_sys, INPUT_CMD_LEFT, INPUT_TYPE_ESPNOW);
    }
    if (input_data->buttons & (1 << BTN_RIGHT_BIT)) {
        ESP_LOGI(TAG, "Button RIGHT pressed");
        send_input_event(g_input_sys, INPUT_CMD_RIGHT, INPUT_TYPE_ESPNOW);
    }
    if (input_data->buttons & (1 << BTN_SELECT_BIT)) {
        ESP_LOGI(TAG, "Button SELECT pressed");
        send_input_event(g_input_sys, INPUT_CMD_SELECT, INPUT_TYPE_ESPNOW);
    }
    if (input_data->buttons & (1 << BTN_START_BIT)) {
        ESP_LOGI(TAG, "Button START pressed");
        send_input_event(g_input_sys, INPUT_CMD_START, INPUT_TYPE_ESPNOW);
    }
    if (input_data->buttons & (1 << BTN_RESET_BIT)) {
        ESP_LOGI(TAG, "Button RESET pressed");
        send_input_event(g_input_sys, INPUT_CMD_RESET, INPUT_TYPE_ESPNOW);
    }
    if (input_data->buttons & (1 << BTN_BACK_BIT)) {
        ESP_LOGI(TAG, "Button BACK pressed");
        send_input_event(g_input_sys, INPUT_CMD_BACK, INPUT_TYPE_ESPNOW);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA disconnected, reconnecting");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t input_espnow_init(input_system_t *input_sys) {
    ESP_LOGI(TAG, "Initializing ESP-NOW input");
    
    // Store reference to input system
    g_input_sys = input_sys;
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    // Initialize WiFi in APSTA mode for better ESP-NOW reliability
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t sta_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
        },
    };
    if (strlen((const char *)sta_config.sta.password) == 0) {
        sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));

    // Configure AP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "Flip-Dot-Display",
            .password = "",
            .ssid_len = 0,
            .channel = 1,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 1,
            .max_connection = 0,
            .beacon_interval = 100
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    
    // Register callback
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    
    // Add peer if MAC address is provided
    if (input_sys->config.espnow_config.peer_mac[0] != 0) {
        esp_now_peer_info_t peer_info = {
            .channel = input_sys->config.espnow_config.channel,
            .ifidx = ESP_IF_WIFI_STA,
            .encrypt = input_sys->config.espnow_config.enable_encryption
        };
        memcpy(peer_info.peer_addr, input_sys->config.espnow_config.peer_mac, 6);
        
        // Remove existing peer if any
        esp_now_del_peer(peer_info.peer_addr);
        
        esp_err_t result = esp_now_add_peer(&peer_info);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Failed to add peer (error %d)", result);
            return result;
        }
        ESP_LOGI(TAG, "Successfully added peer");
    } else {
        ESP_LOGW(TAG, "No peer MAC address provided");
    }
    
    input_sys->espnow_enabled = true;
    ESP_LOGI(TAG, "ESP-NOW input initialized");
    
    return ESP_OK;
}

void input_espnow_process(input_system_t *input_sys) {
    // ESP-NOW processing is handled in the callback
    // This function is kept for consistency with other input types
}

espnow_input_config_t input_get_default_espnow_config(void) {
    espnow_input_config_t config = {
        .channel = 1,
        .enable_encryption = false,
        .peer_mac = {0x98, 0xa3, 0x16, 0x8e, 0x8b, 0x04}  // Controller's MAC address 98:a3:16:8e:8b:04
        //.peer_mac = {0x98, 0xa3, 0x16, 0x8f, 0x0b, 0xf0}  // Controller's MAC address 98:a3:16:8f:0b:f0
    };
    return config;
} 