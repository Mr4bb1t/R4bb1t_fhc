#include "Menu_Attacks.h"
#include "Attacks.h"
#include "Battery.h"
#include "Captive.h"
#include "Config.h"
#include "Globals.h"
#include "Language.h"
#include "Menu_Networks.h"
#include "Radio.h"
#include "UI.h"
#include <SPIFFS.h>

// ── Helper: trunca SSID para caber na tela ─────
static String truncSSID(const String &s, int maxLen = 18) {
  if ((int)s.length() > maxLen)
    return s.substring(0, maxLen - 1) + ".";
  return s;
}

// Seleção de item da lista do menu Deauther (0=VOLTAR, 1=Broadcast, 2=Targeted)
static int deauthMenuSel = 1;

// Forward declaration
static void IRAM_ATTR clientSnifferCb(void *buf, wifi_promiscuous_pkt_type_t type);
// ═══════════════════════════════════════════════
//  MENU ATAQUES
// ═══════════════════════════════════════════════
void displayMenuAtaques() {
  tft.fillScreen(C_BG);
  tft.setTextSize(1);

  drawHeader(lang->atk_hdr_wifi, true);

  tft.setTextColor(C_GOLD_DIM);
  String ssid = truncSSID(ssidSelecionado, 21);
  tft.setCursor(4, 17);
  tft.print(ssid);
  tft.drawFastHLine(0, 25, 128, C_GREY);

  bool isPT = (String(lang->cfg_itm_voltar) == "Voltar");
  String infoLbl = isPT ? "Informacoes" : "Net Info";
  const char *items[] = {lang->atk_itm_back, infoLbl.c_str(), lang->atk_itm_captive, lang->atk_itm_deauther, lang->atk_itm_navjammer,
                         lang->atk_itm_beaconspam};
  for (int i = 0; i < 6; i++) {
    drawMenuItem(0, 27 + i * 20, 128, 19, items[i],
                 opcaoAtaqueSelecionada == i);
  }

  batteryDraw();
}

void handleMenuAtaques() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = opcaoAtaqueSelecionada;
      opcaoAtaqueSelecionada = (opcaoAtaqueSelecionada > 0) ? opcaoAtaqueSelecionada - 1 : 5;
      lastDebounceTime = millis();
      bool isPT = (String(lang->cfg_itm_voltar) == "Voltar");
      String infoLbl = isPT ? "Informacoes" : "Net Info";
      const char *items[] = {lang->atk_itm_back, infoLbl.c_str(), lang->atk_itm_captive, lang->atk_itm_deauther, lang->atk_itm_navjammer, lang->atk_itm_beaconspam};
      drawMenuItem(0, 27 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 27 + opcaoAtaqueSelecionada * 20, 128, 19, items[opcaoAtaqueSelecionada], true);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = opcaoAtaqueSelecionada;
      opcaoAtaqueSelecionada = (opcaoAtaqueSelecionada < 5) ? opcaoAtaqueSelecionada + 1 : 0;
      lastDebounceTime = millis();
      bool isPT = (String(lang->cfg_itm_voltar) == "Voltar");
      String infoLbl = isPT ? "Informacoes" : "Net Info";
      const char *items[] = {lang->atk_itm_back, infoLbl.c_str(), lang->atk_itm_captive, lang->atk_itm_deauther, lang->atk_itm_navjammer, lang->atk_itm_beaconspam};
      drawMenuItem(0, 27 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 27 + opcaoAtaqueSelecionada * 20, 128, 19, items[opcaoAtaqueSelecionada], true);
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      switch (opcaoAtaqueSelecionada) {
      case 0:
        estadoAtual = SELECAO_REDES;
        displayNetworks();
        break;
      case 1:
        estadoAtual = ATAQUE_INFO_REDE;
        displayInfoRede();
        break;
      case 2:
        estadoAtual = ATAQUE_CAPTIVE_PORTAL;
        createCaptivePortal(ssidSelecionado);
        displayAtaqueCaptivePortal();
        break;
      case 3:
        estadoAtual = ATAQUE_DEAUTHER;
        displayAtaqueDeauther();
        break;
      case 4:
        estadoAtual = ATAQUE_CTS_JAMMER;
        displayAtaqueCtsJammer();
        break;
      case 5:
        estadoAtual = ATAQUE_BEACON_MODO;
        displayAtaqueBeaconModo();
        break;
      }
      lastDebounceTime = millis();
    }
  }
}

// ═══════════════════════════════════════════════
//  INFO REDE — layout otimizado 128px
// ═══════════════════════════════════════════════
static String infoRedeSecLabel(wifi_auth_mode_t auth, bool isPT) {
  switch (auth) {
    case WIFI_AUTH_OPEN:            return isPT ? "Aberta"       : "Open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    default:                        return "???";
  }
}

static int infoRedeRSSILast = -999;

static void drawInfoRedeRSSI(int yPos) {
  tft.fillRect(8, yPos, 60, 9, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, yPos);
  tft.printf("%d dBm", apRecordSelecionado.rssi);
}

static void drawInfoRedeSignalBar(int x, int y, int rssi) {
  tft.fillRect(x, y - 6, 20, 10, C_BG); // limpa área anterior
  int bars;
  if      (rssi >= -50) bars = 4;
  else if (rssi >= -65) bars = 3;
  else if (rssi >= -75) bars = 2;
  else if (rssi >= -85) bars = 1;
  else                  bars = 0;

  uint16_t col = (bars >= 3) ? C_GREEN : (bars >= 2) ? C_YELLOW : C_RED;
  for (int i = 0; i < 4; i++) {
    int bh = 2 + i * 2;
    int bx = x + i * 5;
    tft.fillRect(bx, y + (2 - bh), 3, bh, i < bars ? col : C_GREY);
  }
}

void displayInfoRede() {
  tft.fillScreen(C_BG);
  bool isPT = (String(lang->cfg_itm_voltar) == "Voltar");
  drawHeader(isPT ? "INFORMACOES" : "NETWORK INFO", true);

  tft.setTextSize(1);

  // ── BOX 1: SSID ──
  tft.drawRoundRect(4, 18, 120, 28, 4, C_GOLD);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(8, 22);
  tft.print("SSID");
  tft.setTextColor(C_WHITE);
  String ssidDisp = ssidSelecionado;
  if ((int)ssidDisp.length() > 18)
    ssidDisp = ssidDisp.substring(0, 17) + ".";
  tft.setCursor(8, 33);
  tft.print(ssidDisp);

  // ── BOX 2: MAC ──
  tft.drawRoundRect(4, 49, 120, 28, 4, C_GOLD);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(8, 53);
  tft.print("MAC");
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 64);
  tft.print(macSelecionado);

  // ── BOX 3: CANAL (Left) ──
  tft.drawRoundRect(4, 80, 54, 28, 4, C_GOLD);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(8, 84);
  tft.print(isPT ? "CANAL" : "CH");
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 95);
  tft.print(apRecordSelecionado.primary);

  // ── BOX 4: SEGURANÇA (Right) ──
  tft.drawRoundRect(62, 80, 62, 28, 4, C_GOLD);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(66, 84);
  tft.print(isPT ? "SEG" : "SEC");
  tft.setTextColor(C_WHITE);
  tft.setCursor(66, 95);
  String sec = infoRedeSecLabel(apRecordSelecionado.authmode, isPT);
  tft.print(sec);

  // ── BOX 5: RSSI ──
  tft.drawRoundRect(4, 111, 120, 24, 4, C_GOLD);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(8, 115);
  tft.print("RSSI");

  infoRedeRSSILast = apRecordSelecionado.rssi;
  drawInfoRedeRSSI(124);
  drawInfoRedeSignalBar(98, 124, apRecordSelecionado.rssi);

  // ── Botao Voltar ──
  int btnY = 139;
  tft.fillRect(32, btnY, 64, 18, C_GOLD_SEL);
  tft.drawRect(32, btnY, 64, 18, C_GOLD);
  tft.setTextColor(C_GOLD);
  String back = isPT ? "VOLTAR" : "BACK";
  tft.setCursor(32 + (64 - back.length() * 6) / 2, btnY + 5);
  tft.print(back);

  batteryDraw();
}

void handleInfoRede() {
  static unsigned long lastRSSIUpdate = 0;
  static int scanState = 0; // 0=idle, 1=scan started

  if (scanState == 1) {
    int16_t res = WiFi.scanComplete();
    if (res >= 0) {
      for (int i = 0; i < res; i++) {
        if (WiFi.BSSIDstr(i) == macSelecionado) {
          int newRSSI = WiFi.RSSI(i);
          if (newRSSI != infoRedeRSSILast) {
            apRecordSelecionado.rssi = newRSSI;
            infoRedeRSSILast = newRSSI;
            drawInfoRedeRSSI(124);
            drawInfoRedeSignalBar(98, 124, newRSSI);
          }
          break;
        }
      }
      WiFi.scanDelete();
      scanState = 0;
    }
  }

  if (scanState == 0 && (millis() - lastRSSIUpdate > 400)) {
    lastRSSIUpdate = millis();
    WiFi.scanNetworks(true);
    scanState = 1;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_SELECT) == LOW || digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW) {
      if (scanState == 1) {
        WiFi.scanDelete();
      }
      estadoAtual = MENU_ATAQUES;
      displayMenuAtaques();
      lastDebounceTime = millis();
    }
  }
}

// ═══════════════════════════════════════════════
//  CAPTIVE PORTAL
// ═══════════════════════════════════════════════
void displayAtaqueCaptivePortal() {
  tft.fillScreen(C_BG);
  drawHeader(lang->atk_hdr_captive, true);

  tft.setTextSize(1);
  tft.setTextColor(C_RED);
  tft.setCursor(40, 20);
  tft.print(lang->atk_cp_ativo);

  tft.setTextColor(C_GOLD);
  tft.setCursor(4, 33);
  tft.print(lang->atk_cp_portal);
  drawSeparator(43, C_GREY);

  tft.setTextColor(C_GOLD);
  tft.setCursor(4, 50);
  tft.print(truncSSID(ssidSelecionado, 21));

  tft.setTextColor(C_RED);
  tft.setCursor(4, 64);
  tft.print(lang->atk_cp_deauth);

  drawSeparator(78, C_GREY);

  drawMenuItem(0, 80, 128, 19, lang->atk_itm_back, opcaoSubMenuAtaque == 0);
  drawMenuItem(0, 100, 128, 19, lang->atk_cp_apagar, opcaoSubMenuAtaque == 1);
  drawMenuItem(0, 120, 128, 19, lang->atk_cp_credenciais, opcaoSubMenuAtaque == 2);

  batteryDraw();
}

void handleAtaqueCaptivePortal() {
  dnsServer.processNextRequest();
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = opcaoSubMenuAtaque;
      opcaoSubMenuAtaque = (opcaoSubMenuAtaque > 0) ? opcaoSubMenuAtaque - 1 : 2;
      lastDebounceTime = millis();
      const char *items[] = {lang->atk_itm_back, lang->atk_cp_apagar, lang->atk_cp_credenciais};
      drawMenuItem(0, 80 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 80 + opcaoSubMenuAtaque * 20, 128, 19, items[opcaoSubMenuAtaque], true);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = opcaoSubMenuAtaque;
      opcaoSubMenuAtaque = (opcaoSubMenuAtaque < 2) ? opcaoSubMenuAtaque + 1 : 0;
      lastDebounceTime = millis();
      const char *items[] = {lang->atk_itm_back, lang->atk_cp_apagar, lang->atk_cp_credenciais};
      drawMenuItem(0, 80 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 80 + opcaoSubMenuAtaque * 20, 128, 19, items[opcaoSubMenuAtaque], true);
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      switch (opcaoSubMenuAtaque) {
      case 0:
        stopCaptivePortal();
        estadoAtual = MENU_ATAQUES;
        displayMenuAtaques();
        break;
      case 1:
        estadoAtual = CONFIRMA_APAGAR_CREDENCIAIS;
        displayConfirmaApagar();
        break;
      case 2:
        estadoAtual = VISUALIZAR_CREDENCIAIS;
        contarCredenciais();
        displayCredenciais();
        break;
      }
      lastDebounceTime = millis();
    }
  }
}

// ═══════════════════════════════════════════════
//  CONFIRMAÇÃO APAGAR
// ═══════════════════════════════════════════════
static int confirmaApagarSel = 0;

void displayConfirmaApagar() {
  tft.fillScreen(C_BG);
  drawHeader(lang->atk_hdr_apagar, true);

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 30);
  tft.print(lang->atk_ap_msg);

  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 45);
  tft.print(lang->atk_ap_irreversivel);

  drawSeparator(60, C_GREY);

  drawMenuItem(0, 70, 128, 19, lang->atk_ap_cancelar, confirmaApagarSel == 0);
  drawMenuItem(0, 90, 128, 19, lang->atk_ap_confirmar, confirmaApagarSel == 1);

  batteryDraw();
}

void handleConfirmaApagar() {
  dnsServer.processNextRequest();

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = confirmaApagarSel;
      confirmaApagarSel = (confirmaApagarSel > 0) ? confirmaApagarSel - 1 : 1;
      lastDebounceTime = millis();
      const char *items[] = {lang->atk_ap_cancelar, lang->atk_ap_confirmar};
      drawMenuItem(0, 70 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 70 + confirmaApagarSel * 20, 128, 19, items[confirmaApagarSel], true);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = confirmaApagarSel;
      confirmaApagarSel = (confirmaApagarSel < 1) ? confirmaApagarSel + 1 : 0;
      lastDebounceTime = millis();
      const char *items[] = {lang->atk_ap_cancelar, lang->atk_ap_confirmar};
      drawMenuItem(0, 70 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 70 + confirmaApagarSel * 20, 128, 19, items[confirmaApagarSel], true);
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      if (confirmaApagarSel == 0) {
        estadoAtual = ATAQUE_CAPTIVE_PORTAL;
        displayAtaqueCaptivePortal();
      } else {
        EraseData();
        tft.fillScreen(C_BG);
        drawHeader(lang->atk_hdr_apagar, true);
        tft.setTextColor(C_GREEN);
        tft.setCursor(5, 80);
        tft.print(lang->atk_ap_cred_apagadas);
        delay(1500);
        confirmaApagarSel = 0;
        estadoAtual = ATAQUE_CAPTIVE_PORTAL;
        displayAtaqueCaptivePortal();
      }
      lastDebounceTime = millis();
    }
  }
}

// ═══════════════════════════════════════════════
//  DEAUTHER
// ═══════════════════════════════════════════════

static void drawDeautherPulse() {
  int cx = 64;
  int cy = 70;

  uint8_t phase = (millis() / 120) & 0x1F;

  int outerR = 22 + (phase < 16 ? phase / 3 : (31 - phase) / 3);
  uint16_t ringColor = (phase < 16) ? C_RED : C_GOLD;

  static int lastOuterR = 0;
  if (lastOuterR > 0 && lastOuterR != outerR) {
    tft.drawCircle(cx, cy, lastOuterR, C_BG);
    tft.drawCircle(cx, cy, lastOuterR - 1, C_BG);
  }
  lastOuterR = outerR;

  tft.drawCircle(cx, cy, outerR, ringColor);
  tft.drawCircle(cx, cy, outerR - 1, ringColor);

  tft.fillCircle(cx, cy, 14, C_GOLD_SEL);
  tft.drawCircle(cx, cy, 14, C_GOLD);
  tft.drawCircle(cx, cy, 15, C_GOLD);

  tft.setTextSize(1);
  tft.setTextColor((phase & 0x08) ? C_RED : C_GOLD);
  tft.setCursor(cx - 2, cy - 4);
  tft.print("X");

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 104);
  tft.print(lang->atk_lbl_pkt);
  tft.setTextColor(C_WHITE);
  tft.fillRect(30, 104, 60, 8, C_BG);
  tft.setCursor(30, 104);
  tft.printf("%lu", deauthCounter);
}

void displayAtaqueDeauther() {
  tft.fillScreen(C_BG);
  drawHeader(lang->atk_hdr_deauther, true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD);
  tft.setCursor(4, 17);
  tft.print(truncSSID(ssidSelecionado, 21));
  drawSeparator(26, C_GREY);

  if (!deautherAtivo) {
    const char *ditems[] = {lang->atk_da_back, lang->atk_da_broadcast, lang->atk_da_targeted};
    for (int i = 0; i < 3; i++) {
      drawMenuItem(0, 29 + i * 20, 128, 19, ditems[i], deauthMenuSel == i);
    }
    drawSeparator(89, C_GREY);

    if (deauthMenuSel > 0) {
      tft.fillRect(14, 125, 100, 22, C_GOLD_SEL);
      tft.drawRect(14, 125, 100, 22, C_GOLD);
      tft.drawRect(15, 126, 98, 20, C_GOLD_DIM);
      tft.setTextColor(C_GOLD);
      tft.setCursor(29, 132);
      tft.print(lang->atk_da_iniciar);
    } else {
      tft.fillRect(14, 125, 100, 22, C_BG);
    }

  } else {
    tft.setTextColor(C_RED);
    tft.setCursor(41, 33);
    tft.print(lang->atk_cp_ativo);

    drawDeautherPulse();

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(93, 104);
    tft.print(deauthTipo == 0 ? lang->atk_da_bcast : lang->atk_da_trgd);

    drawSeparator(116, C_GREY);
    tft.fillRect(14, 120, 100, 20, C_GOLD_SEL);
    tft.drawRect(14, 120, 100, 20, C_RED);
    tft.setTextColor(C_RED);
    tft.setCursor(34, 127);
    tft.print(lang->atk_da_parar);
  }

  batteryDraw();
}

void handleAtaqueDeauther() {
  static bool holdingSelect = false;
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (!deautherAtivo) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        int old = deauthMenuSel;
        deauthMenuSel = (deauthMenuSel > 0) ? deauthMenuSel - 1 : 2;
        lastDebounceTime = millis();
        const char *items[] = {lang->atk_da_back, lang->atk_da_broadcast, lang->atk_da_targeted};
        drawMenuItem(0, 29 + old * 20, 128, 19, items[old], false);
        drawMenuItem(0, 29 + deauthMenuSel * 20, 128, 19, items[deauthMenuSel], true);
        return;
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        int old = deauthMenuSel;
        deauthMenuSel = (deauthMenuSel < 2) ? deauthMenuSel + 1 : 0;
        lastDebounceTime = millis();
        const char *items[] = {lang->atk_da_back, lang->atk_da_broadcast, lang->atk_da_targeted};
        drawMenuItem(0, 29 + old * 20, 128, 19, items[old], false);
        drawMenuItem(0, 29 + deauthMenuSel * 20, 128, 19, items[deauthMenuSel], true);
        return;
      }
      if (selectPressed && !holdingSelect) {
        holdingSelect = true;
      }
      if (!selectPressed && holdingSelect) {
        holdingSelect = false;
        lastDebounceTime = millis();
        if (deauthMenuSel == 0) {
          estadoAtual = MENU_ATAQUES;
          displayMenuAtaques();
          return;
        }
        deauthTipo = deauthMenuSel - 1;
        if (deauthTipo == 1) {
          // Targeted: inicia scan automaticamente
          clientCount = 0;
          clientSelected = 0;
          clientScanRunning = false;
          estadoAtual = ATAQUE_DEAUTHER_SCAN;
          // Inicia o scan imediatamente
          uint8_t ch = (apRecordSelecionado.primary >= 1)
                           ? apRecordSelecionado.primary
                           : 1;
          WiFi.mode(WIFI_MODE_NULL);
          delay(100);
          wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
          esp_wifi_init(&cfg);
          esp_wifi_set_mode(WIFI_MODE_STA);
          esp_wifi_start();
          esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
          esp_wifi_set_promiscuous(true);
          esp_wifi_set_promiscuous_rx_cb(clientSnifferCb);
          clientScanRunning = true;
          displayAtaqueDeautherScan(true);
          return;
        }
        if (initRadioForAttack(apRecordSelecionado.primary)) {
          deautherAtivo = true;
          deauthCounter = 0;
          if (attackTaskHandle == NULL) {
            attackTaskRunning = true;
            xTaskCreatePinnedToCore(attackTask, "AttackTask", 4096, NULL, 1,
                                    &attackTaskHandle, 1);
          }
          displayAtaqueDeauther();
        } else {
          tft.fillScreen(C_BG);
          drawHeader(lang->atk_hdr_deauther, true);
          tft.setTextColor(C_RED);
          tft.setCursor(20, 80);
          tft.println(lang->atk_da_erro_radio);
          delay(1500);
          displayAtaqueDeauther();
        }
      }
    } else {
      if (selectPressed && !holdingSelect) {
        holdingSelect = true;
      }
      if (!selectPressed && holdingSelect) {
        holdingSelect = false;
        deautherAtivo = false;
        deinitRadio();
        if (attackTaskHandle != NULL) {
          attackTaskRunning = false;
          vTaskDelay(pdMS_TO_TICKS(300));
          attackTaskHandle = NULL;
        }
        tft.fillScreen(C_BG);
        drawHeader(lang->atk_hdr_deauther, true);
        tft.setTextColor(C_GOLD);
        tft.setCursor(34, 75);
        tft.print(lang->atk_da_parado);
        tft.setTextColor(C_GOLD_DIM);
        tft.setCursor(16, 90);
        tft.printf(lang->atk_da_enviados, deauthCounter);
        delay(1200);
        displayAtaqueDeauther();
        lastDebounceTime = millis();
        return;
      }
      if (!selectPressed)
        holdingSelect = false;
    }
  }

  if (deautherAtivo) {
    static unsigned long lastAnim = 0;
    if (millis() - lastAnim > 80) {
      lastAnim = millis();
      drawDeautherPulse();
      batteryDraw();
    }
  }
}

// ═══════════════════════════════════════════════
//  DEAUTHER - SCAN DE CLIENTES
// ═══════════════════════════════════════════════

static void IRAM_ATTR clientSnifferCb(void *buf,
                                      wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT)
    return;

  auto *pkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *payload = pkt->payload;
  int len = pkt->rx_ctrl.sig_len;
  if (len < 24)
    return;

  const uint8_t *addr2 = payload + 10;
  const uint8_t *addr3 = payload + 16;

  bool toBSSID = memcmp(addr3, apRecordSelecionado.bssid, 6) == 0;
  if (!toBSSID)
    return;

  if (addr2[0] & 0x01)
    return;
  if (memcmp(addr2, apRecordSelecionado.bssid, 6) == 0)
    return;

  for (int i = 0; i < clientCount; i++) {
    if (memcmp(clientList[i].mac, addr2, 6) == 0)
      return;
  }

  if (clientCount < MAX_CLIENTS) {
    memcpy(clientList[clientCount].mac, addr2, 6);
    clientList[clientCount].rssi = pkt->rx_ctrl.rssi;
    clientCount++;
  }
}

static String macShort(const uint8_t *m) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2],
           m[3], m[4], m[5]);
  return String(buf);
}

// ─────────────────────────────────────────────────────────
//  SCAN DE CLIENTES — layout redesenhado
//
//  Topo (fixo):
//    [ PARAR SCAN ]   ou   [ ESCANEAR ]     — botão topo
//    [ VOLTAR ]                              — botão abaixo
//    ─────────────────────────────────────
//  Meio: lista de MACs encontrados (scroll)
//  Rodapé: contador e dica de seleção
// ─────────────────────────────────────────────────────────
void displayAtaqueDeautherScan(bool init) {
  if (init) {
    tft.fillScreen(C_BG);
    drawHeader(lang->atk_hdr_clientes, true);

    tft.setTextSize(1);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 17);
    tft.print(truncSSID(ssidSelecionado, 21));
    drawSeparator(26, C_GREY);
    drawSeparator(65, C_GREY);
    drawSeparator(140, C_GREY);
  }

  // ── Botões no topo ──────────────────────────────────────
  // Botão 1: PARAR SCAN / ESCANEAR (selecionável com scanBtnSel == 0)
  bool scanBtnActive = (clientScanBtnSel == 0);
  if (clientScanRunning) {
    tft.fillRect(4, 29, 120, 16, scanBtnActive ? C_RED : C_BG);
    tft.drawRect(4, 29, 120, 16, C_RED);
    tft.setTextColor(scanBtnActive ? C_WHITE : C_RED);
    tft.setCursor(22, 34);
    tft.print(lang->atk_ds_parar_scan);
  } else {
    tft.fillRect(4, 29, 120, 16, scanBtnActive ? C_GOLD_SEL : C_BG);
    tft.drawRect(4, 29, 120, 16, C_GOLD);
    tft.setTextColor(scanBtnActive ? C_GOLD : C_GOLD_DIM);
    tft.setCursor(22, 34);
    tft.print(lang->atk_ds_escanear);
  }

  // Botão 2: VOLTAR (selecionável com scanBtnSel == 1)
  bool voltarBtnActive = (clientScanBtnSel == 1);
  tft.fillRect(4, 47, 120, 16, voltarBtnActive ? C_GOLD_SEL : C_BG);
  tft.drawRect(4, 47, 120, 16, C_GREY);
  tft.setTextColor(voltarBtnActive ? C_GOLD : C_GREY);
  tft.setCursor(34, 52);
  tft.print(lang->atk_ds_voltar);

  drawSeparator(65, C_GREY);

  // ── Lista de clientes ───────────────────────────────────
  if (clientCount == 0) {
    tft.fillRect(0, 68, 128, 70, C_BG);
    if (clientScanRunning) {
      int dotPos = (millis() / 400) % 4;
      tft.setTextColor(C_GREEN);
      tft.setCursor(4, 68);
      tft.print(lang->atk_ds_scan);
      for (int i = 0; i < dotPos; i++)
        tft.print(".");
    } else {
      tft.setTextColor(C_GREY);
      tft.setCursor(16, 80);
      tft.print(lang->atk_ds_nenhum);
      tft.setCursor(24, 92);
      tft.print(lang->atk_ds_encontrado);
    }
  } else {
    const int MAX_VIS = 4;
    int start = 0;
    int listSel = clientScanBtnSel - 2;
    if (listSel >= MAX_VIS)
      start = listSel - MAX_VIS + 1;
    if (start < 0)
      start = 0;

    for (int i = 0; i < MAX_VIS; i++) {
      int idx = start + i;
      if (idx >= clientCount) {
        tft.fillRect(0, 68 + i * 18, 128, 17, C_BG);
        continue;
      }

      String mac = macShort(clientList[idx].mac);

      bool selected = (idx == listSel);
      drawMenuItem(0, 68 + i * 18, 128, 17, mac.c_str(), selected);
    }
  }

  // ── Rodapé ─────────────────────────────────────────────
  tft.fillRect(0, 145, 128, 10, C_BG);
  drawSeparator(140, C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 145);
  if (clientCount > 0) {
    int listSel = clientScanBtnSel - 2;
    if (listSel >= 0) {
      tft.printf(lang->atk_ds_mac_fmt, listSel + 1, clientCount);
    } else {
      tft.printf(lang->atk_ds_clientes_fmt, clientCount);
    }
  } else {
    tft.print(lang->atk_ds_hint);
  }

  batteryDraw();
}

void handleAtaqueDeautherScan() {
  static unsigned long lastScanDraw = 0;
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);

  // Redesenha periodicamente enquanto escaneia (animação + novos MACs)
  if (clientScanRunning && millis() - lastScanDraw > 500) {
    lastScanDraw = millis();
    displayAtaqueDeautherScan();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    // ── Navegação esquerda ────────────────────────────────
    if (digitalRead(BUTTON_LEFT) == LOW) {
      // Sobe na lista: botões (0,1) depois itens da lista
      int total = 2 + clientCount; // 2 botões + N clientes
      clientScanBtnSel =
          (clientScanBtnSel > 0) ? clientScanBtnSel - 1 : total - 1;
      lastDebounceTime = millis();
      displayAtaqueDeautherScan();
      return;
    }

    // ── Navegação direita ─────────────────────────────────
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int total = 2 + clientCount;
      clientScanBtnSel =
          (clientScanBtnSel < total - 1) ? clientScanBtnSel + 1 : 0;
      lastDebounceTime = millis();
      displayAtaqueDeautherScan();
      return;
    }

    // ── Seleção ───────────────────────────────────────────
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();

      if (clientScanBtnSel == 0) {
        // Botão PARAR SCAN / ESCANEAR
        if (clientScanRunning) {
          // Para o scan
          clientScanRunning = false;
          esp_wifi_set_promiscuous(false);
          esp_wifi_set_promiscuous_rx_cb(nullptr);
          wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
          esp_wifi_set_mode(WIFI_MODE_APSTA);
        } else {
          // Inicia novo scan
          clientCount = 0;
          clientScanBtnSel = 0;
          uint8_t ch = (apRecordSelecionado.primary >= 1)
                           ? apRecordSelecionado.primary
                           : 1;
          WiFi.mode(WIFI_MODE_NULL);
          delay(100);
          wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
          esp_wifi_init(&cfg);
          esp_wifi_set_mode(WIFI_MODE_STA);
          esp_wifi_start();
          esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
          esp_wifi_set_promiscuous(true);
          esp_wifi_set_promiscuous_rx_cb(clientSnifferCb);
          clientScanRunning = true;
        }
        displayAtaqueDeautherScan();

      } else if (clientScanBtnSel == 1) {
        // Botão VOLTAR
        if (clientScanRunning) {
          clientScanRunning = false;
          esp_wifi_set_promiscuous(false);
          esp_wifi_set_promiscuous_rx_cb(nullptr);
          esp_wifi_set_mode(WIFI_MODE_APSTA);
        }
        clientScanBtnSel = 0;
        estadoAtual = ATAQUE_DEAUTHER;
        displayAtaqueDeauther();

      } else {
        // Selecionou um cliente da lista → atacar
        int listSel = clientScanBtnSel - 2;
        if (listSel >= 0 && listSel < clientCount) {
          if (clientScanRunning) {
            clientScanRunning = false;
            esp_wifi_set_promiscuous(false);
            esp_wifi_set_promiscuous_rx_cb(nullptr);
          }
          if (initRadioForAttack(apRecordSelecionado.primary)) {
            deautherAtivo = true;
            deauthTipo = 1; // Targeted
            deauthCounter = 0;
            memcpy(targetClientMac, clientList[listSel].mac, 6);
            if (attackTaskHandle == NULL) {
              attackTaskRunning = true;
              xTaskCreatePinnedToCore(attackTask, "AttackTask", 4096, NULL, 1,
                                      &attackTaskHandle, 1);
            }
            estadoAtual = ATAQUE_DEAUTHER;
            displayAtaqueDeauther();
          } else {
            tft.fillScreen(C_BG);
            drawHeader(lang->atk_hdr_deauther, true);
            tft.setTextColor(C_RED);
            tft.setCursor(20, 80);
            tft.println(lang->atk_da_erro_radio);
            delay(1500);
            displayAtaqueDeautherScan();
          }
        }
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  CTS JAMMER
// ═══════════════════════════════════════════════
static int ctsMenuSel = 1;

void displayAtaqueCtsJammer() {
  tft.fillScreen(C_BG);
  drawHeader(lang->atk_hdr_navjammer, true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 17);
  tft.print(truncSSID(ssidSelecionado, 21));
  drawSeparator(27, C_GREY);

  if (!ctsAtivo) {
    tft.setTextColor(C_WHITE);
    tft.setCursor(4, 35);
    tft.print(lang->atk_cts_desc1);

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 50);
    tft.print(lang->atk_cts_desc2);
    tft.setCursor(4, 62);
    tft.print(lang->atk_cts_desc3);
    tft.setCursor(4, 74);
    tft.print(lang->atk_cts_desc4);

    drawSeparator(95, C_GREY);
    
    const char *items[] = {lang->atk_da_back, lang->atk_cts_iniciar};
    for (int i = 0; i < 2; i++) {
      drawMenuItem(0, 102 + i * 20, 128, 19, items[i], ctsMenuSel == i);
    }
  } else {
    tft.setTextColor(C_RED);
    tft.setCursor(38, 40);
    tft.print(lang->atk_cp_ativo);

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 60);
    tft.print(lang->atk_cts_canal);
    tft.setTextColor(C_WHITE);
    tft.setCursor(90, 60);
    tft.print(apRecordSelecionado.primary);

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 75);
    tft.print(lang->atk_cts_navflood);
    tft.setTextColor(C_WHITE);
    tft.setCursor(68, 75);
    tft.printf("%lu", ctsCounter);

    drawSeparator(116, C_GREY);
    tft.fillRect(14, 120, 100, 20, C_GOLD_SEL);
    tft.drawRect(14, 120, 100, 20, C_RED);
    tft.setTextColor(C_RED);
    tft.setCursor(34, 127);
    tft.print(lang->atk_da_parar);
  }

  batteryDraw();
}

void handleAtaqueCtsJammer() {
  static bool holdingSelect = false;
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (!ctsAtivo) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        int old = ctsMenuSel;
        ctsMenuSel = (ctsMenuSel > 0) ? ctsMenuSel - 1 : 1;
        lastDebounceTime = millis();
        const char *items[] = {lang->atk_da_back, lang->atk_cts_iniciar};
        drawMenuItem(0, 102 + old * 20, 128, 19, items[old], false);
        drawMenuItem(0, 102 + ctsMenuSel * 20, 128, 19, items[ctsMenuSel], true);
        return;
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        int old = ctsMenuSel;
        ctsMenuSel = (ctsMenuSel < 1) ? ctsMenuSel + 1 : 0;
        lastDebounceTime = millis();
        const char *items[] = {lang->atk_da_back, lang->atk_cts_iniciar};
        drawMenuItem(0, 102 + old * 20, 128, 19, items[old], false);
        drawMenuItem(0, 102 + ctsMenuSel * 20, 128, 19, items[ctsMenuSel], true);
        return;
      }

      if (selectPressed && !holdingSelect) {
        holdingSelect = true;
      }
      if (!selectPressed && holdingSelect) {
        holdingSelect = false;
        lastDebounceTime = millis();
        
        if (ctsMenuSel == 0) {
          estadoAtual = MENU_ATAQUES;
          displayMenuAtaques();
        } else {
          uint8_t canal = (apRecordSelecionado.primary >= 1) ? apRecordSelecionado.primary : 1;
          if (initRadioForAttack(canal)) {
            ctsAtivo = true;
            ctsCounter = 0;
            if (attackTaskHandle == NULL) {
              attackTaskRunning = true;
              xTaskCreatePinnedToCore(attackTask, "AttackTask", 4096, NULL, 1,
                                      &attackTaskHandle, 1);
            }
            displayAtaqueCtsJammer();
          } else {
            tft.fillScreen(C_BG);
            drawHeader(lang->atk_hdr_navjammer, true);
            tft.setTextColor(C_RED);
            tft.setCursor(20, 80);
            tft.println(lang->atk_da_erro_radio);
            delay(1500);
            displayAtaqueCtsJammer();
          }
        }
      }
    } else {
      if (selectPressed && !holdingSelect) {
        holdingSelect = true;
      }
      if (!selectPressed && holdingSelect) {
        holdingSelect = false;
        lastDebounceTime = millis();
        
        ctsAtivo = false;
        deinitRadio();
        if (attackTaskHandle != NULL) {
          attackTaskRunning = false;
          vTaskDelay(pdMS_TO_TICKS(300));
          attackTaskHandle = NULL;
        }
        tft.fillScreen(C_BG);
        drawHeader(lang->atk_hdr_navjammer, true);
        tft.setTextColor(C_GOLD);
        tft.setCursor(28, 80);
        tft.println("ATAQUE PARADO");
        delay(1500);
        displayAtaqueCtsJammer();
      }
    }
  }

  if (ctsAtivo) {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 200) {
      lastUpdate = millis();
      tft.setTextSize(1);
      tft.fillRect(68, 75, 58, 8, C_BG);
      tft.setTextColor(C_WHITE);
      tft.setCursor(68, 75);
      tft.printf("%lu", ctsCounter);
      batteryDraw();
    }
  }
}

// ═══════════════════════════════════════════════
//  BEACON SPAM - MODO
// ═══════════════════════════════════════════════
static int beaconModoSel = 1;

void displayAtaqueBeaconModo() {
  tft.fillScreen(C_BG);
  drawHeader(lang->atk_hdr_beaconmodo, true);

  tft.setTextSize(1);
  const char *itens[] = {lang->atk_bm_back, lang->atk_bm_copia, lang->atk_bm_aleatorio, lang->atk_bm_personalizado};

  for (int i = 0; i < 4; i++) {
    drawMenuItem(0, 27 + i * 20, 128, 19, itens[i], beaconModoSel == i);
  }

  batteryDraw();
}

void handleAtaqueBeaconModo() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = beaconModoSel;
      beaconModoSel = (beaconModoSel > 0) ? beaconModoSel - 1 : 3;
      lastDebounceTime = millis();
      const char *items[] = {lang->atk_bm_back, lang->atk_bm_copia, lang->atk_bm_aleatorio, lang->atk_bm_personalizado};
      drawMenuItem(0, 27 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 27 + beaconModoSel * 20, 128, 19, items[beaconModoSel], true);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = beaconModoSel;
      beaconModoSel = (beaconModoSel < 3) ? beaconModoSel + 1 : 0;
      lastDebounceTime = millis();
      const char *items[] = {lang->atk_bm_back, lang->atk_bm_copia, lang->atk_bm_aleatorio, lang->atk_bm_personalizado};
      drawMenuItem(0, 27 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 27 + beaconModoSel * 20, 128, 19, items[beaconModoSel], true);
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      if (beaconModoSel == 0) {
        estadoAtual = MENU_ATAQUES;
        displayMenuAtaques();
      } else {
        beaconModo = beaconModoSel - 1;
        if (beaconModo == 2) {
          estadoAtual = ATAQUE_BEACON_CUSTOM;
          displayAtaqueBeaconCustom();
        } else {
          estadoAtual = ATAQUE_BEACON;
          displayAtaqueBeacon();
        }
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  BEACON SPAM - CUSTOM (TECLADO)
// ═══════════════════════════════════════════════
static const char *kbdLower =
    "abcdefghijklmnopqrstuvwxyz0123456789_-@!?*.#%&+ ";
static const char *kbdUpper =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-@!?*.#%&+ ";
static int kbdSel = 0;
static bool kbdCaps = true;

static void drawKbdKey(int index, bool selected, bool isCaps) {
  const char *chars = isCaps ? kbdUpper : kbdLower;
  tft.setTextSize(1);
  if (index < 48) {
    int r = index / 8;
    int c = index % 8;
    int x = 6 + c * 15;
    int y = 40 + r * 14;

    if (selected) {
      tft.fillRect(x - 2, y - 1, 11, 11, C_GOLD_SEL);
      tft.setTextColor(C_GOLD);
    } else {
      tft.fillRect(x - 2, y - 1, 11, 11, C_BG);
      tft.setTextColor(C_WHITE);
    }
    tft.setCursor(x, y + 1);
    tft.print(chars[index]);
  } else {
    int yBtn = 138;
    if (index == 48) {
      if (selected) {
        tft.fillRect(4, yBtn - 2, 35, 12, C_GOLD_SEL);
        tft.setTextColor(C_GOLD);
      } else {
        tft.fillRect(4, yBtn - 2, 35, 12, C_BG);
        tft.setTextColor(isCaps ? C_GREEN : C_WHITE);
      }
      tft.setCursor(6, yBtn);
      tft.print(lang->atk_kbd_shift);
    } else if (index == 49) {
      if (selected) {
        tft.fillRect(45, yBtn - 2, 25, 12, C_GOLD_SEL);
        tft.setTextColor(C_GOLD);
      } else {
        tft.fillRect(45, yBtn - 2, 25, 12, C_BG);
        tft.setTextColor(C_RED);
      }
      tft.setCursor(48, yBtn);
      tft.print(lang->atk_kbd_del);
    } else if (index == 50) {
      if (selected) {
        tft.fillRect(75, yBtn - 2, 45, 12, C_GOLD_SEL);
        tft.setTextColor(C_GOLD);
      } else {
        tft.fillRect(75, yBtn - 2, 45, 12, C_BG);
        tft.setTextColor(C_GREEN);
      }
      tft.setCursor(82, yBtn);
      tft.print(lang->atk_kbd_enter);
    }
  }
}

void displayAtaqueBeaconCustom() {
  tft.fillScreen(C_BG);
  drawHeader(lang->atk_hdr_nomecustom, true);

  tft.drawRect(4, 18, 120, 18, C_GOLD);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 23);
  tft.print(beaconCustomSSID);

  for (int i = 0; i <= 50; i++) {
    drawKbdKey(i, i == kbdSel, kbdCaps);
  }

  batteryDraw();
}

void handleAtaqueBeaconCustom() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = kbdSel;
      kbdSel--;
      if (kbdSel < 0)
        kbdSel = 50;
      lastDebounceTime = millis();
      drawKbdKey(old, false, kbdCaps);
      drawKbdKey(kbdSel, true, kbdCaps);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = kbdSel;
      kbdSel++;
      if (kbdSel > 50)
        kbdSel = 0;
      lastDebounceTime = millis();
      drawKbdKey(old, false, kbdCaps);
      drawKbdKey(kbdSel, true, kbdCaps);
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      if (kbdSel < 48) {
        if (beaconCustomSSID.length() < 32) {
          const char *chars = kbdCaps ? kbdUpper : kbdLower;
          beaconCustomSSID += chars[kbdSel];
          tft.fillRect(5, 19, 118, 16, C_BG);
          tft.setTextColor(C_WHITE);
          tft.setCursor(8, 23);
          tft.print(beaconCustomSSID);
        }
      } else if (kbdSel == 48) {
        kbdCaps = !kbdCaps;
        for (int i = 0; i <= 50; i++) {
          drawKbdKey(i, i == kbdSel, kbdCaps);
        }
      } else if (kbdSel == 49) {
        if (beaconCustomSSID.length() > 0) {
          beaconCustomSSID.remove(beaconCustomSSID.length() - 1);
          tft.fillRect(5, 19, 118, 16, C_BG);
          tft.setTextColor(C_WHITE);
          tft.setCursor(8, 23);
          tft.print(beaconCustomSSID);
        }
      } else if (kbdSel == 50) {
        estadoAtual = ATAQUE_BEACON;
        displayAtaqueBeacon();
        return;
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  BEACON SPAM
// ═══════════════════════════════════════════════
// beaconSel: 0=[-], 1=[+], 2=[VOLTAR], 3=[INICIAR]
static int beaconSel = 0;

static void drawBeaconBtn(int x, int y, int w, int h, const char* lbl, bool sel, uint16_t col = 0) {
  uint16_t border = sel ? C_GOLD : C_GREY;
  uint16_t bg     = sel ? C_GOLD_SEL : C_BG;
  uint16_t tc     = sel ? C_GOLD : (col ? col : C_WHITE);
  tft.fillRect(x, y, w, h, bg);
  tft.drawRect(x, y, w, h, border);
  tft.setTextColor(tc);
  int tx = x + (w - (int)strlen(lbl) * 6) / 2;
  int ty = y + (h - 8) / 2;
  tft.setTextSize(1);
  tft.setCursor(tx, ty);
  tft.print(lbl);
}

static void updateBeaconQuantity() {
  const int ROW_Y = 46;
  const int NUM_W = 64;
  const int NUM_H = 28;
  const int NUM_X = 32; // ROW_X(8) + BTN_W(20) + GAP(4)
  
  // Redesenha apenas o número central (tamanho 3)
  tft.fillRect(NUM_X, ROW_Y, NUM_W, NUM_H, C_BG);
  tft.drawRect(NUM_X, ROW_Y, NUM_W, NUM_H, C_GOLD_DIM);
  tft.setTextSize(3);
  tft.setTextColor(C_GOLD);
  tft.setTextDatum(MC_DATUM);
  tft.drawNumber(beaconQuantidade, NUM_X + NUM_W / 2, ROW_Y + NUM_H / 2 + 1);
  tft.setTextDatum(TL_DATUM); // restore top-left
  tft.setTextSize(1);
}

void displayAtaqueBeacon() {
  tft.fillScreen(C_BG);
  drawHeader(lang->atk_hdr_beaconspam, true);

  // SSID modo
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  int ssidX = 4;
  tft.setCursor(ssidX, 17);
  if (beaconModo == 0) {
    tft.print(truncSSID(ssidSelecionado, 21));
  } else if (beaconModo == 1) {
    tft.print(lang->atk_bs_aleatorio);
  } else {
    tft.print(truncSSID(beaconCustomSSID, 21));
  }
  drawSeparator(27, C_GREY);

  if (!beaconAtivo) {
    // ── Label pool ──
    tft.setTextColor(C_WHITE);
    tft.setCursor((128 - (int)strlen(lang->atk_bs_pool)*6)/2, 33);
    tft.print(lang->atk_bs_pool);

    // ── Linha de controle: [ - ] NÚMERO [ + ] ──
    const int ROW_Y  = 46;
    const int NUM_W  = 64;
    const int NUM_H  = 28;
    
    const int BTN_W  = 20;
    const int BTN_H  = 22;
    const int BTN_Y  = ROW_Y + (NUM_H - BTN_H) / 2;

    const int TOTAL  = BTN_W + 4 + NUM_W + 4 + BTN_W; // 112
    const int ROW_X  = (128 - TOTAL) / 2;             // 8
    const int NUM_X  = ROW_X + BTN_W + 4;             // 32

    // Botão [-]
    drawBeaconBtn(ROW_X, BTN_Y, BTN_W, BTN_H, "-", beaconSel == 0);

    // Display do número perfeitamente centralizado com MC_DATUM
    tft.fillRect(NUM_X, ROW_Y, NUM_W, NUM_H, C_BG);
    tft.drawRect(NUM_X, ROW_Y, NUM_W, NUM_H, C_GOLD_DIM);
    tft.setTextSize(3);
    tft.setTextColor(C_GOLD);
    tft.setTextDatum(MC_DATUM);
    tft.drawNumber(beaconQuantidade, NUM_X + NUM_W / 2, ROW_Y + NUM_H / 2 + 1);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);

    // Botão [+]
    drawBeaconBtn(NUM_X + NUM_W + 4, BTN_Y, BTN_W, BTN_H, "+", beaconSel == 1);

    // ── Linha de botões: [ VOLTAR ] [ INICIAR ] ──
    bool isPT = (String(lang->cfg_itm_voltar) == "Voltar");
    String backLbl  = isPT ? "VOLTAR"  : "BACK";
    String startLbl = isPT ? "INICIAR" : "START";
    const int BTN2_Y = ROW_Y + BTN_H + 16;
    const int BTN2_W = 56;
    const int BTN2_H = 24;
    const int GAP2   = 8;
    const int B2_X   = (128 - 2*BTN2_W - GAP2) / 2;
    drawBeaconBtn(B2_X,            BTN2_Y, BTN2_W, BTN2_H, backLbl.c_str(),  beaconSel == 2, C_RED);
    drawBeaconBtn(B2_X+BTN2_W+GAP2, BTN2_Y, BTN2_W, BTN2_H, startLbl.c_str(), beaconSel == 3, C_GREEN);

  } else {
    // ── Banner ATIVO ──
    tft.fillRect(0, 30, 128, 14, C_RED);
    tft.setTextColor(TFT_WHITE);
    int aw = strlen(lang->atk_cp_ativo)*6;
    tft.setCursor((128-aw)/2, 33);
    tft.print(lang->atk_cp_ativo);

    // Pool e Beacons
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(8, 52);
    tft.print(lang->atk_bs_pool_lbl);
    tft.setTextColor(C_GOLD);
    tft.setCursor(50, 52);
    tft.print(beaconQuantidade);

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(8, 66);
    tft.print(lang->atk_bs_beacons);
    tft.setTextColor(C_WHITE);
    tft.setCursor(56, 66);
    tft.printf("%lu", beaconCounter);

    // Dots animados
    int dotPos = (millis() / 300) % 4;
    tft.setTextColor(C_GREEN);
    tft.setCursor(8, 82);
    for (int i = 0; i < 4; i++) { if (i <= dotPos) tft.print("."); }

    // Botão PARAR centralizado
    bool isPT2 = (String(lang->cfg_itm_voltar) == "Voltar");
    String stopLbl = isPT2 ? "PARAR" : "STOP";
    int stopW = 80; int stopH = 22;
    int stopX = (128 - stopW) / 2;
    drawBeaconBtn(stopX, 96, stopW, stopH, stopLbl.c_str(), true, C_RED);
  }

  batteryDraw();
}

void handleAtaqueBeacon() {
  if (beaconAtivo) {
    // Update counters
    static unsigned long lastBeaconUpdate = 0;
    if (millis() - lastBeaconUpdate > 200) {
      lastBeaconUpdate = millis();
      tft.setTextSize(1);
      tft.fillRect(56, 66, 70, 8, C_BG);
      tft.setTextColor(C_WHITE);
      tft.setCursor(56, 66);
      tft.printf("%lu", beaconCounter);
      int dotPos = (millis() / 300) % 4;
      tft.fillRect(8, 82, 30, 8, C_BG);
      tft.setTextColor(C_GREEN);
      tft.setCursor(8, 82);
      for (int i = 0; i < 4; i++) { if (i <= dotPos) tft.print("."); }
      batteryDraw();
    }
    // Parar
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (digitalRead(BUTTON_SELECT) == LOW) {
        beaconAtivo = false;
        beaconCounter = 0;
        deinitRadio();
        if (attackTaskHandle != NULL) {
          attackTaskRunning = false;
          vTaskDelay(pdMS_TO_TICKS(300));
          attackTaskHandle = NULL;
        }
        beaconSel = 3;
        displayAtaqueBeacon();
        lastDebounceTime = millis();
      }
    }
    return;
  }

  // Cleanup se task parou por conta
  if (attackTaskHandle != NULL && !attackTaskRunning) {
    deinitRadio();
    attackTaskHandle = NULL;
    beaconCounter = 0;
    displayAtaqueBeacon();
    lastDebounceTime = millis();
    return;
  }

  if ((millis() - lastDebounceTime) <= debounceDelay) return;

  if (digitalRead(BUTTON_LEFT) == LOW) {
    // Na linha superior (0,1): navega -/+; na linha inferior (2,3): navega voltar/iniciar
    if (beaconSel == 0)      beaconSel = 2;  // pula pra VOLTAR
    else if (beaconSel == 1) beaconSel = 0;  // + -> -
    else if (beaconSel == 2) beaconSel = 0;  // VOLTAR -> -
    else                     beaconSel = 2;  // INICIAR -> VOLTAR
    lastDebounceTime = millis();
    displayAtaqueBeacon();
    return;
  }
  if (digitalRead(BUTTON_RIGHT) == LOW) {
    if (beaconSel == 0)      beaconSel = 1;  // - -> +
    else if (beaconSel == 1) beaconSel = 3;  // + -> INICIAR
    else if (beaconSel == 2) beaconSel = 3;  // VOLTAR -> INICIAR
    else                     beaconSel = 1;  // INICIAR -> +
    lastDebounceTime = millis();
    displayAtaqueBeacon();
    return;
  }
  if (digitalRead(BUTTON_SELECT) == LOW) {
    lastDebounceTime = millis();
    if (beaconSel == 0) {
      // Decrementar
      if (beaconQuantidade > 100)     beaconQuantidade -= 25;
      else if (beaconQuantidade > 10) beaconQuantidade -= 5;
      else if (beaconQuantidade > 1)  beaconQuantidade -= 1;
      updateBeaconQuantity();
    } else if (beaconSel == 1) {
      // Incrementar
      if (beaconQuantidade < 10)       beaconQuantidade += 1;
      else if (beaconQuantidade < 100) beaconQuantidade += 5;
      else                             beaconQuantidade += 25;
      if (beaconQuantidade > 600) beaconQuantidade = 600;
      updateBeaconQuantity();
    } else if (beaconSel == 2) {
      // Voltar
      beaconSel = 0;
      estadoAtual = ATAQUE_BEACON_MODO;
      beaconAtivo = false;
      beaconCounter = 0;
      displayAtaqueBeaconModo();
    } else {
      // Iniciar
      uint8_t canal = (apRecordSelecionado.primary >= 1) ? apRecordSelecionado.primary : 1;
      if (initRadioForAttack(canal)) {
        beaconAtivo = true;
        beaconCounter = 0;
        if (attackTaskHandle == NULL) {
          attackTaskRunning = true;
          xTaskCreatePinnedToCore(attackTask, "AttackTask", 4096, NULL, 1, &attackTaskHandle, 1);
        }
        displayAtaqueBeacon();
      } else {
        tft.fillScreen(C_BG);
        drawHeader(lang->atk_hdr_beaconspam, true);
        tft.setTextColor(C_RED);
        tft.setCursor(20, 80);
        tft.println(lang->atk_da_erro_radio);
        delay(1500);
        displayAtaqueBeacon();
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  CREDENCIAIS
// ═══════════════════════════════════════════════
void displayCredenciais() {
  tft.fillScreen(C_BG);
  drawHeader(lang->atk_hdr_credenciais, true);
  tft.setTextSize(1);

  int startY = 18;
  int lineHeight = 16;

  if (SPIFFS.exists("/credenciais.txt")) {
    File file = SPIFFS.open("/credenciais.txt", FILE_READ);
    if (file) {
      int linhaAtual = 0;
      int credencialInicio = paginaCredencialAtual * 5;
      String linha;
      while (file.available()) {
        linha = file.readStringUntil('\n');
        linha.trim();
        if (linha.length() > 0) {
          if (linhaAtual >= credencialInicio &&
              linhaAtual < credencialInicio + 5) {
            tft.setCursor(4, startY +
                                 (linhaAtual - credencialInicio) * lineHeight);
            tft.setTextColor(linhaAtual % 2 == 0 ? C_GOLD : C_WHITE);
            if (linha.length() > 20)
              linha = linha.substring(0, 20) + ".";
            tft.println(linha);
          }
          linhaAtual++;
        }
        if (linhaAtual >= credencialInicio + 5)
          break;
      }
      file.close();
    }
  } else {
    tft.setTextColor(C_GREY);
    tft.setCursor(20, 70);
    tft.println(lang->atk_cr_nenhuma);
  }

  drawSeparator(138, C_GREY);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(5, 142);
  tft.printf("%d/%d", paginaCredencialAtual + 1,
             totalCredenciais > 0 ? totalCredenciais : 1);
}

void handleVisualizarCredenciais() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      if (paginaCredencialAtual > 0)
        paginaCredencialAtual--;
      else
        paginaCredencialAtual = totalCredenciais > 0 ? totalCredenciais - 1 : 0;
      lastDebounceTime = millis();
      displayCredenciais();
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      if (paginaCredencialAtual < totalCredenciais - 1)
        paginaCredencialAtual++;
      else
        paginaCredencialAtual = 0;
      lastDebounceTime = millis();
      displayCredenciais();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      estadoAtual = ATAQUE_CAPTIVE_PORTAL;
      displayAtaqueCaptivePortal();
      lastDebounceTime = millis();
    }
  }
}