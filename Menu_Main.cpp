#include "Menu_Main.h"
#include "Globals.h"
#include "Config.h"
#include "Menu_Config.h"
#include "Menu_Networks.h"
#include "Menu_RF.h"
#include "Menu_NRF24.h"

#include "Scanner.h"
#include "Splash.h"
#include "UI.h"
#include "Battery.h"
#include "Language.h"

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
  cells[0].x + 29, cells[1].x + 29,
  cells[2].x + 18, cells[3].x + 19,
};
static const int iconY[4] = {
  cells[0].y + 10, cells[1].y + 22,
  cells[2].y + 9,  cells[3].y + 7,
};

static const int labelY[4] = {
  cells[0].y + 46, cells[1].y + 46,
  cells[2].y + 46, cells[3].y + 46,
};

static const char *labels_raw[4] = {"WiFi", "2.4GHz", "Sub GHz", "Config"};

static const char* getMainMenuLabel(int i) {
    switch(i) {
        case 0: return lang->main_lbl_wifi;
        case 1: return lang->main_lbl_24ghz;
        case 2: return lang->main_lbl_subghz;
        case 3: return lang->main_lbl_config;
        default: return labels_raw[i];
    }
}

// ── Timer de inatividade (screensaver) ─────────
#define IDLE_TIMEOUT_MS 30000UL
static unsigned long lastActivityTime = 0;

// ──────────────────────────────────────────────
static void drawMenuInicialItem(int i, bool sel) {
  if (menuStyle == 1) {
    int y = 22 + i * 28;
    int h = 26;
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

    if (i == 0)      drawWiFiIconSmall(4, y, iconColor);
    else if (i == 1) drawNRF24IconSmall(4, y, iconColor);
    else if (i == 2) drawRFIconSmall(4, y, iconColor);
    else             drawSettingsIconSmall(4, y, iconColor, sel ? C_GOLD_SEL : C_BG);

    tft.setCursor(34, y + (h - 8) / 2 + 1);
    tft.print(getMainMenuLabel(i));

    tft.setTextColor(sel ? C_GOLD : C_GREY);
    tft.setCursor(128 - 9, y + (h - 8) / 2 + 1);
    tft.print(">");

    tft.drawFastHLine(0, y + h - 1, 128, C_GREY);
  } else {
    const Cell &c = cells[i];

    uint16_t borderColor = sel ? C_GOLD : C_GREY;
    uint16_t iconColor   = sel ? C_GOLD : C_GOLD_DIM;
    uint16_t textColor   = sel ? C_GOLD : C_WHITE;

    tft.fillRect(c.x, c.y, c.w, c.h, C_BG);
    tft.drawRect(c.x, c.y, c.w, c.h, borderColor);

    if (sel) {
      tft.drawRect(c.x + 1, c.y + 1, c.w - 2, c.h - 2, C_GOLD_SEL);
    }

    if (i == 0) drawWiFiIcon(iconX[i], iconY[i], iconColor);
    else if (i == 1) drawNRF24Icon(iconX[i], iconY[i], iconColor);
    else if (i == 2) drawRFIcon(iconX[i], iconY[i], iconColor);
    else drawSettingsIcon(iconX[i], iconY[i], iconColor, C_BG);

    int lx = c.x + (c.w - (int)strlen(getMainMenuLabel(i)) * 6) / 2;
    tft.setTextSize(1);
    tft.setTextColor(textColor);
    tft.setCursor(lx, labelY[i]);
    tft.print(getMainMenuLabel(i));
  }
}

static void updateMenuInicialItems() {
  for (int i = 0; i < 4; i++) {
    drawMenuInicialItem(i, opcaoMenuInicial == i);
  }
}

void displayMenuInicial() {
  lastActivityTime = millis();
  tft.fillScreen(C_BG);

  // ── Header ─────────────────────────────────
  drawHeader(lang->main_hdr_home);

  updateMenuInicialItems();

  // ── Rodapé ─────────────────────────────────
  drawFooter();

  // Restaura ícone de bateria
  batteryDraw();
}

// ──────────────────────────────────────────────
void handleMenuInicial() {
  if ((millis() - lastActivityTime) >= IDLE_TIMEOUT_MS) {
    lastActivityTime = millis();
    startScreensaver(true);
    return;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = opcaoMenuInicial;
      opcaoMenuInicial = (opcaoMenuInicial + 3) % 4;
      lastDebounceTime = millis();
      drawMenuInicialItem(old, false);
      drawMenuInicialItem(opcaoMenuInicial, true);
    }

    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = opcaoMenuInicial;
      opcaoMenuInicial = (opcaoMenuInicial + 1) % 4;
      lastDebounceTime = millis();
      drawMenuInicialItem(old, false);
      drawMenuInicialItem(opcaoMenuInicial, true);
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();

      if (opcaoMenuInicial == 0) {
        tft.fillScreen(C_BG);
        drawHeader(lang->main_hdr_wifi, true);
        tft.setTextSize(1);
        tft.setTextColor(C_GOLD);
        tft.setCursor(30, 80);
        tft.println(lang->main_scanning);
        estadoAtual = SELECAO_REDES;
        scanNetworks();
        displayNetworks();

      } else if (opcaoMenuInicial == 1) {
        estadoAtual = MENU_NRF24;
        displayModoNRF24();

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
void handleScreensaver() {
  if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW ||
      digitalRead(BUTTON_SELECT) == LOW) {
    lastActivityTime = millis();
    lastDebounceTime = millis();
    estadoAtual = MENU_INICIAL;
    displayMenuInicial();
  }
}
