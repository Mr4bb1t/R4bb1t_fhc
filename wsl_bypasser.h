/**
 * @file wsl_bypasser.h
 * @brief Wi-Fi Stack Libraries bypasser para arduino-esp32 3.x / IDF 5.x
 *
 * Usa dois mecanismos combinados:
 * 1. Override de ieee80211_raw_frame_sanity_check() — retorna 0 sempre
 * 2. Override de ieee80211_is_tx_allowed() — retorna 1 sempre
 * 3. objcopy --weaken-symbol aplicado na libnet80211.a pelo script de build
 *
 * Compatível com arduino-esp32 2.x e 3.x (IDF 4.x e 5.x)
 */
#ifndef WSL_BYPASSER_H
#define WSL_BYPASSER_H

#include "esp_wifi_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o WSL bypasser
 * Deve ser chamado após WiFi iniciado e configurado
 */
void wsl_bypasser_init(void);

/**
 * @brief Desinicializa o WSL bypasser
 */
void wsl_bypasser_deinit(void);

/**
 * @brief Envia frame raw 802.11 bypassando o filtro de management frames
 * @param frame_buffer  ponteiro para o frame completo
 * @param size          tamanho em bytes
 * @return ESP_OK em sucesso, código de erro caso contrário
 */
int wsl_bypasser_send_raw_frame(const uint8_t *frame_buffer, int size);

/**
 * @brief Envia frame de deautenticação broadcast como se fosse do AP
 * @param ap_record  AP alvo com BSSID válido
 */
void wsl_bypasser_send_deauth_frame(const wifi_ap_record_t *ap_record);

/**
 * @brief Envia frame de deautenticação unicast para cliente específico
 * @param ap_record   AP de onde o frame será forjado
 * @param client_mac  MAC do cliente alvo
 */
void wsl_bypasser_send_deauth_frame_unicast(const wifi_ap_record_t *ap_record,
                                            const uint8_t *client_mac);

/**
 * @brief Envia frame de disassociation broadcast
 * @param ap_record  AP alvo
 */
void wsl_bypasser_send_disassoc_frame(const wifi_ap_record_t *ap_record);

/**
 * @brief Envia frame CTS/NAV Jammer com duration máximo para travar o canal
 * @param target_mac  MAC de destino (ou Broadcast FF:FF:FF:FF:FF:FF)
 * @return ESP_OK em sucesso, código de erro caso contrário
 */
esp_err_t wsl_bypasser_send_cts_frame(const uint8_t *target_mac);

/**
 * @brief Verifica se o bypass está ativo e funcionando
 * @return true se inicializado
 */
bool wsl_bypasser_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* WSL_BYPASSER_H */
