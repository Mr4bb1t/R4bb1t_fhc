#include "Menu_Attacks.h"
#include "Attacks.h"
#include "Battery.h"
#include "Captive.h"
#include "Config.h"
#include "Globals.h"
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

  drawHeader("WIFI ATTACKS", true);

  tft.setTextColor(C_GOLD_DIM);
  String ssid = truncSSID(ssidSelecionado, 21);
  tft.setCursor(4, 17);
  tft.print(ssid);
  tft.drawFastHLine(0, 25, 128, C_GREY);

  const char *items[] = {"< VOLTAR", "Captive Portal", "Deauther", "NAV Jammer",
                         "Beacon Spam"};
  for (int i = 0; i < 5; i++) {
    drawMenuItem(0, 27 + i * 20, 128, 19, items[i],
                 opcaoAtaqueSelecionada == i);
  }

  batteryDraw();
}

void handleMenuAtaques() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = opcaoAtaqueSelecionada;
      opcaoAtaqueSelecionada = (opcaoAtaqueSelecionada > 0) ? opcaoAtaqueSelecionada - 1 : 4;
      lastDebounceTime = millis();
      const char *items[] = {"< VOLTAR", "Captive Portal", "Deauther", "NAV Jammer", "Beacon Spam"};
      drawMenuItem(0, 27 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 27 + opcaoAtaqueSelecionada * 20, 128, 19, items[opcaoAtaqueSelecionada], true);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = opcaoAtaqueSelecionada;
      opcaoAtaqueSelecionada = (opcaoAtaqueSelecionada < 4) ? opcaoAtaqueSelecionada + 1 : 0;
      lastDebounceTime = millis();
      const char *items[] = {"< VOLTAR", "Captive Portal", "Deauther", "NAV Jammer", "Beacon Spam"};
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
        estadoAtual = ATAQUE_CAPTIVE_PORTAL;
        createCaptivePortal(ssidSelecionado);
        displayAtaqueCaptivePortal();
        break;
      case 2:
        estadoAtual = ATAQUE_DEAUTHER;
        displayAtaqueDeauther();
        break;
      case 3:
        estadoAtual = ATAQUE_CTS_JAMMER;
        displayAtaqueCtsJammer();
        break;
      case 4:
        estadoAtual = ATAQUE_BEACON_MODO;
        displayAtaqueBeaconModo();
        break;
      }
      lastDebounceTime = millis();
    }
  }
}

// ═══════════════════════════════════════════════
//  CAPTIVE PORTAL
// ═══════════════════════════════════════════════
void displayAtaqueCaptivePortal() {
  tft.fillScreen(C_BG);
  drawHeader("CAPTIVE PORTAL", true);

  tft.setTextSize(1);
  tft.setTextColor(C_RED);
  tft.setCursor(40, 20);
  tft.print("[ ATIVO ]");

  tft.setTextColor(C_GOLD);
  tft.setCursor(4, 33);
  tft.print("Portal: 192.168.4.1");
  drawSeparator(43, C_GREY);

  tft.setTextColor(C_GOLD);
  tft.setCursor(4, 50);
  tft.print(truncSSID(ssidSelecionado, 21));

  tft.setTextColor(C_RED);
  tft.setCursor(4, 64);
  tft.print("+ Deauther Ativo");

  drawSeparator(78, C_GREY);

  drawMenuItem(0, 80, 128, 19, "< VOLTAR", opcaoSubMenuAtaque == 0);
  drawMenuItem(0, 100, 128, 19, "Apagar dados", opcaoSubMenuAtaque == 1);
  drawMenuItem(0, 120, 128, 19, "Credenciais", opcaoSubMenuAtaque == 2);

  batteryDraw();
}

void handleAtaqueCaptivePortal() {
  dnsServer.processNextRequest();
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = opcaoSubMenuAtaque;
      opcaoSubMenuAtaque = (opcaoSubMenuAtaque > 0) ? opcaoSubMenuAtaque - 1 : 2;
      lastDebounceTime = millis();
      const char *items[] = {"< VOLTAR", "Apagar dados", "Credenciais"};
      drawMenuItem(0, 80 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 80 + opcaoSubMenuAtaque * 20, 128, 19, items[opcaoSubMenuAtaque], true);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = opcaoSubMenuAtaque;
      opcaoSubMenuAtaque = (opcaoSubMenuAtaque < 2) ? opcaoSubMenuAtaque + 1 : 0;
      lastDebounceTime = millis();
      const char *items[] = {"< VOLTAR", "Apagar dados", "Credenciais"};
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
  drawHeader("APAGAR DADOS", true);

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 30);
  tft.print("Deseja mesmo apagar?");

  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 45);
  tft.print("Isso e irreversivel.");

  drawSeparator(60, C_GREY);

  drawMenuItem(0, 70, 128, 19, "< CANCELAR", confirmaApagarSel == 0);
  drawMenuItem(0, 90, 128, 19, "[ APAGAR ]", confirmaApagarSel == 1);

  batteryDraw();
}

void handleConfirmaApagar() {
  dnsServer.processNextRequest();

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = confirmaApagarSel;
      confirmaApagarSel = (confirmaApagarSel > 0) ? confirmaApagarSel - 1 : 1;
      lastDebounceTime = millis();
      const char *items[] = {"< CANCELAR", "[ APAGAR ]"};
      drawMenuItem(0, 70 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 70 + confirmaApagarSel * 20, 128, 19, items[confirmaApagarSel], true);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = confirmaApagarSel;
      confirmaApagarSel = (confirmaApagarSel < 1) ? confirmaApagarSel + 1 : 0;
      lastDebounceTime = millis();
      const char *items[] = {"< CANCELAR", "[ APAGAR ]"};
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
        drawHeader("APAGAR DADOS", true);
        tft.setTextColor(C_GREEN);
        tft.setCursor(5, 80);
        tft.print("Credenciais apagadas!");
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
  tft.print("PKT:");
  tft.setTextColor(C_WHITE);
  tft.fillRect(30, 104, 60, 8, C_BG);
  tft.setCursor(30, 104);
  tft.printf("%lu", deauthCounter);
}

void displayAtaqueDeauther() {
  tft.fillScreen(C_BG);
  drawHeader("DEAUTHER", true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD);
  tft.setCursor(4, 17);
  tft.print(truncSSID(ssidSelecionado, 21));
  drawSeparator(26, C_GREY);

  if (!deautherAtivo) {
    const char *ditems[] = {"< VOLTAR", "Broadcast", "Targeted"};
    for (int i = 0; i < 3; i++) {
      drawMenuItem(0, 29 + i * 20, 128, 19, ditems[i], deauthMenuSel == i);
    }
    drawSeparator(89, C_GREY);

    if (deauthMenuSel > 0) {
      tft.fillRect(14, 118, 100, 22, C_GOLD_SEL);
      tft.drawRect(14, 118, 100, 22, C_GOLD);
      tft.drawRect(15, 119, 98, 20, C_GOLD_DIM);
      tft.setTextColor(C_GOLD);
      tft.setCursor(29, 125);
      tft.print("[  INICIAR  ]");
    } else {
      tft.fillRect(14, 118, 100, 22, C_BG);
    }

    drawSeparator(145, C_GREY);
    tft.setTextColor(C_GREY);
    tft.setCursor(3, 150);
    tft.print("<         o         >");

  } else {
    tft.setTextColor(C_RED);
    tft.setCursor(41, 33);
    tft.print("[ ATIVO ]");

    drawDeautherPulse();

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(93, 104);
    tft.print(deauthTipo == 0 ? "BCAST" : "TRGD");

    drawSeparator(116, C_GREY);
    tft.fillRect(14, 120, 100, 20, C_GOLD_SEL);
    tft.drawRect(14, 120, 100, 20, C_RED);
    tft.setTextColor(C_RED);
    tft.setCursor(34, 127);
    tft.print("[  PARAR  ]");
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
        const char *items[] = {"< VOLTAR", "Broadcast", "Targeted"};
        drawMenuItem(0, 29 + old * 20, 128, 19, items[old], false);
        drawMenuItem(0, 29 + deauthMenuSel * 20, 128, 19, items[deauthMenuSel], true);
        return;
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        int old = deauthMenuSel;
        deauthMenuSel = (deauthMenuSel < 2) ? deauthMenuSel + 1 : 0;
        lastDebounceTime = millis();
        const char *items[] = {"< VOLTAR", "Broadcast", "Targeted"};
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
          drawHeader("DEAUTHER", true);
          tft.setTextColor(C_RED);
          tft.setCursor(20, 80);
          tft.println("ERRO: Radio!");
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
        drawHeader("DEAUTHER", true);
        tft.setTextColor(C_GOLD);
        tft.setCursor(34, 75);
        tft.print("ATAQUE PARADO");
        tft.setTextColor(C_GOLD_DIM);
        tft.setCursor(16, 90);
        tft.printf("Enviados: %lu", deauthCounter);
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
    drawHeader("CLIENTES", true);

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
    tft.print("[ PARAR SCAN ]");
  } else {
    tft.fillRect(4, 29, 120, 16, scanBtnActive ? C_GOLD_SEL : C_BG);
    tft.drawRect(4, 29, 120, 16, C_GOLD);
    tft.setTextColor(scanBtnActive ? C_GOLD : C_GOLD_DIM);
    tft.setCursor(22, 34);
    tft.print("[ ESCANEAR ]");
  }

  // Botão 2: VOLTAR (selecionável com scanBtnSel == 1)
  bool voltarBtnActive = (clientScanBtnSel == 1);
  tft.fillRect(4, 47, 120, 16, voltarBtnActive ? C_GOLD_SEL : C_BG);
  tft.drawRect(4, 47, 120, 16, C_GREY);
  tft.setTextColor(voltarBtnActive ? C_GOLD : C_GREY);
  tft.setCursor(34, 52);
  tft.print("< VOLTAR");

  drawSeparator(65, C_GREY);

  // ── Lista de clientes ───────────────────────────────────
  if (clientCount == 0) {
    tft.fillRect(0, 68, 128, 70, C_BG);
    if (clientScanRunning) {
      int dotPos = (millis() / 400) % 4;
      tft.setTextColor(C_GREEN);
      tft.setCursor(4, 68);
      tft.print("Scan");
      for (int i = 0; i < dotPos; i++)
        tft.print(".");
    } else {
      tft.setTextColor(C_GREY);
      tft.setCursor(16, 80);
      tft.print("Nenhum cliente");
      tft.setCursor(24, 92);
      tft.print("encontrado...");
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
      tft.printf("MAC %d/%d  SEL=Atacar", listSel + 1, clientCount);
    } else {
      tft.printf("%d cliente(s)", clientCount);
    }
  } else {
    tft.print("<> navegar  SEL=acao");
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
            drawHeader("DEAUTHER", true);
            tft.setTextColor(C_RED);
            tft.setCursor(20, 80);
            tft.println("ERRO: Radio!");
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
  drawHeader("NAV JAMMER", true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 17);
  tft.print(truncSSID(ssidSelecionado, 21));
  drawSeparator(27, C_GREY);

  if (!ctsAtivo) {
    tft.setTextColor(C_WHITE);
    tft.setCursor(4, 35);
    tft.print("Eficaz contra WPA3");

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 50);
    tft.print("Envia 'QoS Null Data'");
    tft.setCursor(4, 62);
    tft.print("Congela o canal (NAV)");
    tft.setCursor(4, 74);
    tft.print("Ignora 802.11w / WPA3");

    drawSeparator(95, C_GREY);
    
    const char *items[] = {"< VOLTAR", "Iniciar Ataque"};
    for (int i = 0; i < 2; i++) {
      drawMenuItem(0, 102 + i * 20, 128, 19, items[i], ctsMenuSel == i);
    }
  } else {
    tft.setTextColor(C_RED);
    tft.setCursor(38, 40);
    tft.print("[ ATIVO ]");

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 60);
    tft.print("Canal travado:");
    tft.setTextColor(C_WHITE);
    tft.setCursor(90, 60);
    tft.print(apRecordSelecionado.primary);

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 75);
    tft.print("Frames QoS:");
    tft.setTextColor(C_WHITE);
    tft.setCursor(76, 75);
    tft.printf("%lu", ctsCounter);

    drawSeparator(116, C_GREY);
    tft.fillRect(14, 120, 100, 20, C_GOLD_SEL);
    tft.drawRect(14, 120, 100, 20, C_RED);
    tft.setTextColor(C_RED);
    tft.setCursor(34, 127);
    tft.print("[  PARAR  ]");
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
        const char *items[] = {"< VOLTAR", "Iniciar Ataque"};
        drawMenuItem(0, 102 + old * 20, 128, 19, items[old], false);
        drawMenuItem(0, 102 + ctsMenuSel * 20, 128, 19, items[ctsMenuSel], true);
        return;
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        int old = ctsMenuSel;
        ctsMenuSel = (ctsMenuSel < 1) ? ctsMenuSel + 1 : 0;
        lastDebounceTime = millis();
        const char *items[] = {"< VOLTAR", "Iniciar Ataque"};
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
            drawHeader("CTS JAMMER", true);
            tft.setTextColor(C_RED);
            tft.setCursor(20, 80);
            tft.println("ERRO: Radio!");
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
        drawHeader("CTS JAMMER", true);
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
      tft.fillRect(76, 75, 50, 8, C_BG);
      tft.setTextColor(C_WHITE);
      tft.setCursor(76, 75);
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
  drawHeader("BEACON MODO", true);

  tft.setTextSize(1);
  const char *itens[] = {"< VOLTAR", "Copia", "Aleatorio", "Personalizado"};

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
      const char *items[] = {"< VOLTAR", "Copia", "Aleatorio", "Personalizado"};
      drawMenuItem(0, 27 + old * 20, 128, 19, items[old], false);
      drawMenuItem(0, 27 + beaconModoSel * 20, 128, 19, items[beaconModoSel], true);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = beaconModoSel;
      beaconModoSel = (beaconModoSel < 3) ? beaconModoSel + 1 : 0;
      lastDebounceTime = millis();
      const char *items[] = {"< VOLTAR", "Copia", "Aleatorio", "Personalizado"};
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

void displayAtaqueBeaconCustom() {
  tft.fillScreen(C_BG);
  drawHeader("NOME CUSTOM", true);

  tft.drawRect(4, 18, 120, 18, C_GOLD);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 23);
  tft.print(beaconCustomSSID);

  tft.setTextSize(1);
  const char *chars = kbdCaps ? kbdUpper : kbdLower;
  for (int i = 0; i < 48; i++) {
    int r = i / 8;
    int c = i % 8;
    int x = 6 + c * 15;
    int y = 40 + r * 14;

    if (i == kbdSel) {
      tft.fillRect(x - 2, y - 1, 11, 11, C_GOLD_SEL);
      tft.setTextColor(C_GOLD);
    } else {
      tft.setTextColor(C_WHITE);
    }
    tft.setCursor(x, y + 1);
    tft.print(chars[i]);
  }

  int yBtn = 138;

  if (kbdSel == 48) {
    tft.fillRect(4, yBtn - 2, 35, 12, C_GOLD_SEL);
    tft.setTextColor(C_GOLD);
  } else {
    tft.setTextColor(kbdCaps ? C_GREEN : C_WHITE);
  }
  tft.setCursor(6, yBtn);
  tft.print("SHIFT");

  if (kbdSel == 49) {
    tft.fillRect(45, yBtn - 2, 25, 12, C_GOLD_SEL);
    tft.setTextColor(C_GOLD);
  } else {
    tft.setTextColor(C_RED);
  }
  tft.setCursor(48, yBtn);
  tft.print("DEL");

  if (kbdSel == 50) {
    tft.fillRect(75, yBtn - 2, 45, 12, C_GOLD_SEL);
    tft.setTextColor(C_GOLD);
  } else {
    tft.setTextColor(C_GREEN);
  }
  tft.setCursor(82, yBtn);
  tft.print("ENTER");

  batteryDraw();
}

void handleAtaqueBeaconCustom() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      kbdSel--;
      if (kbdSel < 0)
        kbdSel = 50;
      lastDebounceTime = millis();
      displayAtaqueBeaconCustom();
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      kbdSel++;
      if (kbdSel > 50)
        kbdSel = 0;
      lastDebounceTime = millis();
      displayAtaqueBeaconCustom();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      if (kbdSel < 48) {
        if (beaconCustomSSID.length() < 32) {
          const char *chars = kbdCaps ? kbdUpper : kbdLower;
          beaconCustomSSID += chars[kbdSel];
        }
      } else if (kbdSel == 48) {
        kbdCaps = !kbdCaps;
      } else if (kbdSel == 49) {
        if (beaconCustomSSID.length() > 0) {
          beaconCustomSSID.remove(beaconCustomSSID.length() - 1);
        }
      } else if (kbdSel == 50) {
        estadoAtual = ATAQUE_BEACON;
        displayAtaqueBeacon();
        return;
      }
      displayAtaqueBeaconCustom();
    }
  }
}

// ═══════════════════════════════════════════════
//  BEACON SPAM
// ═══════════════════════════════════════════════
void displayAtaqueBeacon() {
  tft.fillScreen(C_BG);
  drawHeader("BEACON SPAM", true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 17);

  if (beaconModo == 0) {
    tft.print(truncSSID(ssidSelecionado, 21));
  } else if (beaconModo == 1) {
    tft.print("[ Aleatorio ]");
  } else {
    tft.print(truncSSID(beaconCustomSSID, 21));
  }

  drawSeparator(26, C_GREY);

  if (!beaconAtivo) {
    tft.setTextColor(C_WHITE);
    tft.setCursor(8, 34);
    tft.print("Pool de redes:");

    tft.drawRect(38, 46, 52, 26, C_GOLD);
    tft.drawRect(39, 47, 50, 24, C_GOLD_SEL);
    tft.setTextSize(2);
    int numX = 38 + (52 - (int)(String(beaconQuantidade).length()) * 12) / 2;
    tft.setTextColor(C_GOLD);
    tft.setCursor(numX, 53);
    tft.print(beaconQuantidade);
    tft.setTextSize(1);

    drawSeparator(78, C_GREY);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(8, 84);
    tft.print("< > Tamanho pool");
    tft.setCursor(8, 96);
    tft.print("SEL = Iniciar");
    tft.setCursor(8, 108);
    tft.print("HOLD SEL = Voltar");
  } else {
    tft.setTextColor(C_RED);
    tft.setCursor(38, 32);
    tft.print("[ ATIVO ]");

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 50);
    tft.print("Pool:");
    tft.setTextColor(C_GOLD);
    tft.setCursor(50, 50);
    tft.print(beaconQuantidade);

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 65);
    tft.print("Beacons:");
    tft.setTextColor(C_WHITE);
    tft.setCursor(56, 65);
    tft.printf("%lu", beaconCounter);

    int dotPos = (millis() / 300) % 4;
    tft.setTextColor(C_GREEN);
    tft.setCursor(4, 80);
    for (int i = 0; i < 4; i++) {
      if (i <= dotPos)
        tft.print(".");
    }

    drawSeparator(92, C_GREY);
    tft.setTextColor(C_RED);
    tft.setCursor(34, 96);
    tft.print("SEL = PARAR");
  }

  batteryDraw();
}

void handleAtaqueBeacon() {
  static unsigned long holdStart = 0;
  static bool holdingSelect = false;
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (!beaconAtivo) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        if (beaconQuantidade > 100)
          beaconQuantidade -= 25;
        else if (beaconQuantidade > 10)
          beaconQuantidade -= 5;
        else if (beaconQuantidade > 1)
          beaconQuantidade -= 1;
        lastDebounceTime = millis();
        displayAtaqueBeacon();
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        if (beaconQuantidade < 10)
          beaconQuantidade += 1;
        else if (beaconQuantidade < 100)
          beaconQuantidade += 5;
        else
          beaconQuantidade += 25;
        if (beaconQuantidade > 600)
          beaconQuantidade = 600;
        lastDebounceTime = millis();
        displayAtaqueBeacon();
      }
      if (selectPressed && !holdingSelect) {
        holdStart = millis();
        holdingSelect = true;
      }
      if (!selectPressed && holdingSelect) {
        unsigned long holdTime = millis() - holdStart;
        if (holdTime > 1000) {
          estadoAtual = ATAQUE_BEACON_MODO;
          beaconAtivo = false;
          beaconCounter = 0;
          displayAtaqueBeaconModo();
        } else {
          uint8_t canal = (apRecordSelecionado.primary >= 1)
                              ? apRecordSelecionado.primary
                              : 1;
          if (initRadioForAttack(canal)) {
            beaconAtivo = true;
            beaconCounter = 0;
            if (attackTaskHandle == NULL) {
              attackTaskRunning = true;
              xTaskCreatePinnedToCore(attackTask, "AttackTask", 4096, NULL, 1,
                                      &attackTaskHandle, 1);
            }
            displayAtaqueBeacon();
          } else {
            tft.fillScreen(C_BG);
            drawHeader("BEACON SPAM", true);
            tft.setTextColor(C_RED);
            tft.setCursor(20, 80);
            tft.println("ERRO: Radio!");
            delay(1500);
            displayAtaqueBeacon();
          }
        }
        holdingSelect = false;
        lastDebounceTime = millis();
      }
    } else {
      if (selectPressed && !holdingSelect) {
        beaconAtivo = false;
        beaconCounter = 0;
        deinitRadio();
        if (attackTaskHandle != NULL) {
          attackTaskRunning = false;
          vTaskDelay(pdMS_TO_TICKS(300));
          attackTaskHandle = NULL;
        }
        tft.fillScreen(C_BG);
        drawHeader("BEACON SPAM", true);
        tft.setTextColor(C_GOLD);
        tft.setCursor(28, 80);
        tft.println("ATAQUE PARADO");
        delay(1500);
        displayAtaqueBeacon();
        lastDebounceTime = millis();
        holdingSelect = true;
      }
      if (!selectPressed)
        holdingSelect = false;
    }
  }

  if (beaconAtivo) {
    static unsigned long lastBeaconUpdate = 0;
    if (millis() - lastBeaconUpdate > 200) {
      lastBeaconUpdate = millis();
      tft.setTextSize(1);
      tft.fillRect(56, 65, 70, 8, C_BG);
      tft.setTextColor(C_WHITE);
      tft.setCursor(56, 65);
      tft.printf("%lu", beaconCounter);
      int dotPos = (millis() / 300) % 4;
      tft.fillRect(4, 80, 30, 8, C_BG);
      tft.setTextColor(C_GREEN);
      tft.setCursor(4, 80);
      for (int i = 0; i < 4; i++) {
        if (i <= dotPos)
          tft.print(".");
      }
      batteryDraw();
    }
  } else if (!beaconAtivo && attackTaskHandle != NULL && attackTaskRunning) {
    attackTaskRunning = false;
    deinitRadio();
    attackTaskHandle = NULL;
    beaconCounter = 0;
    displayAtaqueBeacon();
    lastDebounceTime = millis();
  }
}

// ═══════════════════════════════════════════════
//  CREDENCIAIS
// ═══════════════════════════════════════════════
void displayCredenciais() {
  tft.fillScreen(C_BG);
  drawHeader("CREDENCIAIS", true);
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
    tft.println("Nenhuma credencial");
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