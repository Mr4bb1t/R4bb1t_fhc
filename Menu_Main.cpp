#include "Menu_Main.h"
#include "Globals.h"
#include "Config.h"
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

// Each cell: 54×60 px, starting at y=18 (below header)
struct Cell { int x, y, w, h; };

static const Cell cells[4] = {
    {4,  18, 58, 62},  // 0 – WiFi
    {66, 18, 58, 62},  // 1 – Bluetooth
    {4,  84, 58, 62},  // 2 – RF
    {66, 84, 58, 62},  // 3 – Config
};

static const int iconX[4] = {
  cells[0].x + 29, cells[1].x + 24,
  cells[2].x + 18, cells[3].x + 19,
};
static const int iconY[4] = {
  cells[0].y + 10, cells[1].y + 7,
  cells[2].y + 9,  cells[3].y + 7,
};

static const int labelY[4] = {
  cells[0].y + 46, cells[1].y + 46,
  cells[2].y + 46, cells[3].y + 46,
};

static const char *labels[4] = {"WiFi", "BT", "RF 433", "Config"};

// ── Timer de inatividade (screensaver) ─────────
#define IDLE_TIMEOUT_MS 30000UL
static unsigned long lastActivityTime = 0;

// ──────────────────────────────────────────────
static void updateMenuInicialItems() {
  if (menuStyle == 1) {
    // Modo Lista
    for (int i = 0; i < 4; i++) {
      int y = 22 + i * 28;
      int h = 26;
      bool sel = (opcaoMenuInicial == i);
      uint16_t iconColor = sel ? C_GOLD : C_GOLD_DIM;

      if (sel) {
        tft.fillRect(0, y, 128, h, C_GOLD_SEL);
        tft.fillRect(0, y, 3, h, C_GOLD);
        tft.setTextColor(C_GOLD);
      } else {
        tft.fillRect(0, y, 128, h, C_BG);
        tft.fillRect(0, y, 1, h, C_GREY);
        tft.setTextColor(C_WHITE);
      }

      // Alinhamento centralizado para todos os ícones pequenos (X base = 4)
      if (i == 0)      drawWiFiIconSmall(4, y, iconColor);
      else if (i == 1) drawBluetoothIconSmall(4, y, iconColor);
      else if (i == 2) drawRFIconSmall(4, y, iconColor);
      else             drawSettingsIconSmall(4, y, iconColor, sel ? C_GOLD_SEL : C_BG);

      tft.setCursor(34, y + (h - 8) / 2 + 1);
      tft.print(labels[i]);

      tft.setTextColor(sel ? C_GOLD : C_GREY);
      tft.setCursor(128 - 9, y + (h - 8) / 2 + 1);
      tft.print(">");

      tft.drawFastHLine(0, y + h - 1, 128, C_GREY);
    }
  } else {
    // Modo Grade (Quadradinho)
    for (int i = 0; i < 4; i++) {
      const Cell &c = cells[i];
      bool sel = (opcaoMenuInicial == i);

      uint16_t borderColor = sel ? C_GOLD : C_GREY;
      uint16_t iconColor   = sel ? C_GOLD : C_GOLD_DIM;
      uint16_t textColor   = sel ? C_GOLD : C_WHITE;

      // Fundo da célula
      tft.fillRect(c.x, c.y, c.w, c.h, C_BG);

      // Borda da célula
      tft.drawRect(c.x, c.y, c.w, c.h, borderColor);

      // Borda interna extra para item selecionado
      if (sel) {
        tft.drawRect(c.x + 1, c.y + 1, c.w - 2, c.h - 2, C_GOLD_SEL);
      }

      // Ícones
      if (i == 0) drawWiFiIcon(iconX[i], iconY[i], iconColor);
      else if (i == 1) drawBluetoothIcon(iconX[i], iconY[i], iconColor);
      else if (i == 2) drawRFIcon(iconX[i], iconY[i], iconColor);
      else drawSettingsIcon(iconX[i], iconY[i], iconColor, C_BG);

      // Label
      int lx = c.x + (c.w - (int)strlen(labels[i]) * 6) / 2;
      tft.setTextSize(1);
      tft.setTextColor(textColor);
      tft.setCursor(lx, labelY[i]);
      tft.print(labels[i]);
    }
  }
}

void displayMenuInicial() {
  lastActivityTime = millis();
  tft.fillScreen(C_BG);

  // ── Header ─────────────────────────────────
  drawHeader("R4BB1T");

  updateMenuInicialItems();

  // ── Rodapé ─────────────────────────────────
  drawFooter();

  // Restaura ícone de bateria
  batteryDraw();
}

// ──────────────────────────────────────────────
void handleMenuInicial() {
  if ((millis() - lastActivityTime) >= IDLE_TIMEOUT_MS) {
    lastActivityTime = 0;
    estadoAtual = TELA_SCREENSAVER;
    displaySplash(0);
    return;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (digitalRead(BUTTON_LEFT) == LOW) {
      opcaoMenuInicial = (opcaoMenuInicial + 3) % 4;
      lastDebounceTime = millis();
      updateMenuInicialItems();
    }

    if (digitalRead(BUTTON_RIGHT) == LOW) {
      opcaoMenuInicial = (opcaoMenuInicial + 1) % 4;
      lastDebounceTime = millis();
      updateMenuInicialItems();
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();

      if (opcaoMenuInicial == 0) {
        tft.fillScreen(C_BG);
        drawHeader("WIFI", true);
        tft.setTextSize(1);
        tft.setTextColor(C_GOLD);
        tft.setCursor(30, 80);
        tft.println("Escaneando...");
        estadoAtual = SELECAO_REDES;
        scanNetworks();
        displayNetworks();

      } else if (opcaoMenuInicial == 1) {
        estadoAtual = MODO_BLUETOOTH;
        displayModoBluetooth();

      } else if (opcaoMenuInicial == 2) {
        estadoAtual = MENU_RF;
        displayRF();

      } else if (opcaoMenuInicial == 3) {
        estadoAtual = MENU_CONFIGURACOES;
        displayConfiguracoes();
      }
    }

    if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW ||
        digitalRead(BUTTON_SELECT) == LOW) {
      lastActivityTime = millis();
    }
  }
}

// ──────────────────────────────────────────────
void displayModoBluetooth() { displayMenuBT(); }
void handleModoBluetooth()  { handleMenuBT();  }

// ──────────────────────────────────────────────
void handleScreensaver() {
  if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW ||
      digitalRead(BUTTON_SELECT) == LOW) {
    lastActivityTime = millis();
    lastDebounceTime = millis();
    estadoAtual = MENU_INICIAL;
    displayMenuInicial();
  }
}
