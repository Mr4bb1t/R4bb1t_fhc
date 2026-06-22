#include "Radio.h"
#include "Globals.h"
#include "esp_mac.h"

extern "C" {
  #include "wsl_bypasser.h"
}



bool initRadioForAttack(uint8_t channel) {
  Serial.printf("\n=== INICIANDO RÁDIO NO CANAL %d ===\n", channel);
  
  if (wifiMutex == NULL) {
    Serial.println("ERRO: Mutex não inicializado!");
    return false;
  }
  
  if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(5000))) {
    
    // Parar qualquer operação anterior
    if (attackTaskRunning) {
      attackTaskRunning = false;
      vTaskDelay(pdMS_TO_TICKS(300));
    }
    
    // Limpar estado WiFi
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    esp_err_t err;
    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
      Serial.printf("Aviso: esp_wifi_stop retornou %d\n", err);
    }
    
    err = esp_wifi_deinit();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
      Serial.printf("Aviso: esp_wifi_deinit retornou %d\n", err);
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Inicializar WiFi no modo STA (modo para injeção)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
      Serial.printf("ERRO: esp_wifi_init falhou: %d\n", err);
      xSemaphoreGive(wifiMutex);
      return false;
    }
    
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
      Serial.printf("ERRO: set_mode falhou: %d\n", err);
      xSemaphoreGive(wifiMutex);
      return false;
    }
    
    // ── AP FANTASMA ────────────────────────────────────────────────────────
    // O IDF 5.x exige que a interface AP tenha um wifi_config_t válido antes
    // do esp_wifi_start(). Sem isso, WIFI_IF_AP fica não-configurada e o
    // driver descarta esp_wifi_80211_tx mesmo em modo APSTA.
    // O AP nunca será encontrado por scan externo enquanto não houver
    // beacon — mas a interface fica ativa para TX de frames raw.
    {
      wifi_config_t ap_cfg = {};
      // SSID aleatório a partir dos 3 últimos bytes do MAC base
      uint8_t mac[6];
      esp_wifi_get_mac(WIFI_IF_AP, mac);
      snprintf((char*)ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid),
               "r4b_%02x%02x%02x", mac[3], mac[4], mac[5]);
      ap_cfg.ap.ssid_len        = 0;          // 0 = usar strlen
      strlcpy((char*)ap_cfg.ap.password, "r4bb1tsec!", sizeof(ap_cfg.ap.password));
      ap_cfg.ap.channel         = channel;
      ap_cfg.ap.authmode        = WIFI_AUTH_WPA2_PSK;
      ap_cfg.ap.max_connection  = 0;          // Nenhuma conexão real aceita
      ap_cfg.ap.beacon_interval = 60000;      // Intervalo enorme (~60s) — quasi-invisível
      ap_cfg.ap.ssid_hidden     = 1;          // SSID oculto
      err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
      if (err != ESP_OK) {
        Serial.printf("AVISO: set_config AP fantasma falhou: %d\n", err);
        // Não aborta — tenta continuar assim mesmo
      }
    }
    // ───────────────────────────────────────────────────────────────────────
    
    err = esp_wifi_start();
    if (err != ESP_OK) {
      Serial.printf("ERRO: esp_wifi_start falhou: %d\n", err);
      xSemaphoreGive(wifiMutex);
      return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Configurar modo promíscuo (opcional, mas ajuda a manter o rádio ativo)
    // esp_wifi_set_promiscuous(true);
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Definir canal
    err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
      Serial.printf("ERRO: set_channel falhou: %d\n", err);
      xSemaphoreGive(wifiMutex);
      return false;
    }
    
    // Desativar power save
    esp_wifi_set_ps(WIFI_PS_NONE);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Inicializar wsl_bypasser
    Serial.println("Inicializando wsl_bypasser...");
    wsl_bypasser_init();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    radioLocked = true;
    canalTravado = channel;
    
    Serial.println("✓ Rádio inicializado para ataques");
    xSemaphoreGive(wifiMutex);
    return true;
  }
  
  Serial.println("ERRO: Timeout ao tentar adquirir mutex!");
  return false;
}

void deinitRadio() {
  Serial.println("\n=== DESINICIALIZANDO RÁDIO ===");
  
  if (wifiMutex == NULL) {
    Serial.println("AVISO: Mutex não existe");
    return;
  }
  
  if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(5000))) {
    
    attackTaskRunning = false;
    vTaskDelay(pdMS_TO_TICKS(300));
    
    Serial.println("Desligando wsl_bypasser...");
    wsl_bypasser_deinit();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    esp_wifi_stop();
    esp_wifi_deinit();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(150));
    
    radioLocked = false;
    canalTravado = 0;
    
    deautherAtivo = false;
    beaconAtivo = false;
    ctsAtivo = false;
    
    xSemaphoreGive(wifiMutex);
    
    Serial.println("✓ Rádio desinicializado");
  } else {
    Serial.println("ERRO: Timeout ao desinicializar rádio");
  }
}
