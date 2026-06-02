/**
 * @file wsl_bypasser.h
 * @author risinek (risinek@gmail.com)
 * @date 2021-04-05
 * @copyright Copyright (c) 2021
 * 
 * @brief Provides interface for Wi-Fi Stack Libraries bypasser
 */
#ifndef WSL_BYPASSER_H
#define WSL_BYPASSER_H

#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WSL bypasser
 * 
 * Should be called after WiFi is started and configured
 */
void wsl_bypasser_init(void);

/**
 * @brief Deinitialize WSL bypasser
 * 
 * Should be called before stopping WiFi
 */
void wsl_bypasser_deinit(void);

/**
 * @brief Sends frame in frame_buffer using esp_wifi_80211_tx but bypasses blocking mechanism
 * 
 * @param frame_buffer 
 * @param size size of frame buffer
 * @return int Returns ESP_OK on success, error code otherwise
 */
int wsl_bypasser_send_raw_frame(const uint8_t *frame_buffer, int size);

/**
 * @brief Sends deauthentication frame with forged source AP from given ap_record
 *  
 * This will send deauthentication frame acting as frame from given AP, and destination will be broadcast
 * MAC address - \c ff:ff:ff:ff:ff:ff
 * 
 * @param ap_record AP record with valid AP information 
 */
void wsl_bypasser_send_deauth_frame(const wifi_ap_record_t *ap_record);

#ifdef __cplusplus
}
#endif

#endif