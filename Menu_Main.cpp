#include "Menu_Main.h"
#include "Globals.h"
#include "Menu_Config.h"
#include "Menu_Networks.h"
#include "Menu_RF.h"
#include "Menu_NRF24.h"
#include "Menu_BT.h"
#include "Scanner.h"
#include "Splash.h"
#include "UI.h"
#include "Battery.h"

// ──────────────────────────────────────────────
// Layout constants  (display 128 × 160)
// ──────────────────────────────────────────────
#define SCR_W 128
#define SCR_H 160

// Each cell: 54×60 px, starting at y=18 (below title)
// Cell 0 → WiFi   (col 0, row 0)  → x=10,  y=22
// Cell 1 → BT     (col 1, row 0)  → x=70,  y=22
// Cell 2 → RF     (col 0, row 1)  → x=10,  y=90

struct Cell {
  int x;
  int y;
  int w;
  int h;
};

static const Cell cells[4] = {
    {8, 20, 52, 58},  // 0 – WiFi
    {68, 20, 52, 58}, // 1 – Bluetooth
    {8, 86, 52, 58},  // 2 – RF
    {68, 86, 52, 58}, // 3 – Configurações
};

// Settings: drawSettingsIcon cx=x+10, cy=y+10 → centro em (cx,cy)
//   cx=cells[3].x+26=94 → x=cells[3].x+16
//   cy=cells[3].y+19=105 → y=cells[3].y+9
static const int iconX[4] = {cells[0].x + 26, cells[1].x + 21, cells[2].x + 15,
                             cells[3].x + 16};
static const int iconY[4] = {cells[0].y + 12, cells[1].y + 9, cells[2].y + 11,
                             cells[3].y + 9};

// Label centrado no rodapé de cada célula
static const int labelY[4] = {cells[0].y + 44, cells[1].y + 44, cells[2].y + 44,
                              cells[3].y + 44};

static const char *labels[4] = {"WiFi", "BT", "RF 433", "Config"};

// ── Timer de inatividade (screensaver) ─────────
#define IDLE_TIMEOUT_MS 30000UL // 30 segundos
static unsigned long lastActivityTime = 0;

// ──────────────────────────────────────────────
void displayMenuInicial() {
  lastActivityTime = millis(); // sempre reinicia o timer ao entrar no menu
  tft.fillScreen(TFT_BLACK);




  // ── Icons + labels ─────────────────────────
  const uint16_t colors[4] = {
      (opcaoMenuInicial == 0) ? TFT_RED : TFT_WHITE,
      (opcaoMenuInicial == 1) ? TFT_RED : TFT_WHITE,
      (opcaoMenuInicial == 2) ? TFT_RED : TFT_WHITE,
      (opcaoMenuInicial == 3) ? TFT_RED : TFT_WHITE,
  };

  drawWiFiIcon(iconX[0], iconY[0], colors[0]);
  drawBluetoothIcon(iconX[1], iconY[1], colors[1]);
  drawRFIcon(iconX[2], iconY[2], colors[2]);
  drawSettingsIcon(iconX[3], iconY[3], colors[3], TFT_BLACK);

  for (int i = 0; i < 4; i++) {
    const Cell &c = cells[i];
    tft.setTextColor(colors[i]);

    // center label inside cell
    int lx = c.x + (c.w - (int)strlen(labels[i]) * 6) / 2;
    tft.setCursor(lx, labelY[i]);
    tft.print(labels[i]);
  }

  // ── Selection box ──────────────────────────
  const Cell &sel = cells[opcaoMenuInicial];
  tft.drawRect(sel.x, sel.y, sel.w, sel.h, TFT_RED);
  // inner highlight (double border)
  tft.drawRect(sel.x + 1, sel.y + 1, sel.w - 2, sel.h - 2, TFT_DARKGREY);

  // ── Footer ─────────────────────────────────
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1);

  // Left button symbol
  tft.setCursor(5, SCR_H - 10);
  tft.print("<");

  // Centre button symbol  (centred)
  tft.setCursor((SCR_W / 2) - 2, SCR_H - 10);
  tft.print("o");

  // Right button symbol
  tft.setCursor(SCR_W - 11, SCR_H - 10);
  tft.print(">");

  // Restaura ícone de bateria (apagado pelo fillScreen)
  batteryDraw();
}

// ──────────────────────────────────────────────
void handleMenuInicial() {
  // Verifica inatividade
  if ((millis() - lastActivityTime) >= IDLE_TIMEOUT_MS) {
    lastActivityTime = 0;
    estadoAtual = TELA_SCREENSAVER;
    displaySplash(0); // mostra imagem sem delay interno
    return;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    // LEFT button → previous option
    if (digitalRead(BUTTON_LEFT) == LOW) {
      opcaoMenuInicial = (opcaoMenuInicial + 3) % 4; // wrap backwards
      lastDebounceTime = millis();
      displayMenuInicial();
    }

    // RIGHT button → next option
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      opcaoMenuInicial = (opcaoMenuInicial + 1) % 4;
      lastDebounceTime = millis();
      displayMenuInicial();
    }

    // SELECT button → enter selected mode
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();

      if (opcaoMenuInicial == 0) {
        // WiFi → scan networks
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(10, 70);
        tft.setTextColor(TFT_YELLOW);
        tft.println("Escaneando...");
        estadoAtual = SELECAO_REDES;
        scanNetworks();
        displayNetworks();

      } else if (opcaoMenuInicial == 1) {
        // Bluetooth
        estadoAtual = MODO_BLUETOOTH;
        displayModoBluetooth();

      } else if (opcaoMenuInicial == 2) {
        // RF 433 MHz
        estadoAtual = MENU_RF;
        displayRF();

      } else if (opcaoMenuInicial == 3) {
        // Configurações — navega para o menu de config
        estadoAtual = MENU_CONFIGURACOES;
        displayConfiguracoes();
      }
    }

    // Qualquer botão pressionado reseta o timer de inatividade
    if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW ||
        digitalRead(BUTTON_SELECT) == LOW) {
      lastActivityTime = millis();
    }
  }
}

// ──────────────────────────────────────────────
// MODO_BLUETOOTH → sub-menu BT (Jammer / Spam)
void displayModoBluetooth() { displayMenuBT(); }
void handleModoBluetooth()  { handleMenuBT();  }

// ──────────────────────────────────────────────
void handleScreensaver() {
  // Qualquer botão volta ao menu inicial
  if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW ||
      digitalRead(BUTTON_SELECT) == LOW) {
    lastActivityTime = millis(); // reseta timer
    lastDebounceTime = millis(); // evita bounce imediato
    estadoAtual = MENU_INICIAL;
    displayMenuInicial();
  }
}
