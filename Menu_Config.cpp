#include "Menu_Config.h"
#include "Battery.h"
#include "Config.h"
#include "Globals.h"
#include "HWProbe.h"
#include "Menu_Main.h"
#include "Menu_NRF24.h"
#include "Menu_RF.h"
#include "UI.h"
#include "Language.h"

#include <SPI.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_chip_info.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <math.h>

#define SCR_W 128
#define SCR_H 160

static const char *configItems_raw[] = {"Voltar",      "Sobre",     "Mudar MAC",
                                    "Brilho",      "Modo Menu", "Armazenamento",
                                    "Descanso Tela", "Idioma", "Hard Reset", "Desligar"};

static const char* getConfigItem(int i) {
    switch(i) {
        case 0: return lang->cfg_itm_voltar;
        case 1: return lang->cfg_itm_sobre;
        case 2: return lang->cfg_itm_mac;
        case 3: return lang->cfg_itm_brilho;
        case 4: return lang->cfg_itm_modomenu;
        case 5: return lang->cfg_itm_storage;
        case 6: return lang->cfg_itm_saver;
        case 7: return lang->cfg_itm_idioma;
        case 8: return lang->cfg_itm_hardreset;
        case 9: return lang->cfg_itm_desligar;
        default: return configItems_raw[i];
    }
}
static const int NUM_CONFIG_ITEMS = 10;
static int opcaoConfig = 0;

// Forward declarations para hard reset (usadas em handleConfiguracoes)
static int hrStep = 0;
static int hrSel  = 0;

static void initScreensaverMenu();

// ═══════════════════════════════════════════════
//  CONFIGURAÇÕES
// ═══════════════════════════════════════════════
#define CONFIG_PER_PAGE 6
static int configScroll = 0;

void displayConfiguracoes() {
  tft.fillScreen(C_BG);
  drawHeader(lang->cfg_hdr_settings, true);

  if (opcaoConfig < configScroll)
    configScroll = opcaoConfig;
  if (opcaoConfig >= configScroll + CONFIG_PER_PAGE)
    configScroll = opcaoConfig - CONFIG_PER_PAGE + 1;

  for (int i = 0; i < CONFIG_PER_PAGE; i++) {
    int idx = configScroll + i;
    if (idx >= NUM_CONFIG_ITEMS)
      break;
    drawMenuItem(0, 16 + i * 20, 128, 19, getConfigItem(idx), idx == opcaoConfig);
  }

  if (configScroll > 0) {
    tft.setTextColor(C_GREY);
    tft.setCursor(120, 20);
    tft.print("^");
  }
  if (configScroll + CONFIG_PER_PAGE < NUM_CONFIG_ITEMS) {
    tft.setTextColor(C_GREY);
    tft.setCursor(120, 16 + (CONFIG_PER_PAGE - 1) * 20 + 6);
    tft.print("v");
  }

  drawFooter();
  batteryDraw();
}

void handleConfiguracoes() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int old = opcaoConfig;
      opcaoConfig = (opcaoConfig - 1 + NUM_CONFIG_ITEMS) % NUM_CONFIG_ITEMS;
      lastDebounceTime = millis();
      if (opcaoConfig < configScroll ||
          opcaoConfig >= configScroll + CONFIG_PER_PAGE || old < configScroll ||
          old >= configScroll + CONFIG_PER_PAGE) {
        displayConfiguracoes();
      } else {
        drawMenuItem(0, 16 + (old - configScroll) * 20, 128, 19,
                     getConfigItem(old), false);
        drawMenuItem(0, 16 + (opcaoConfig - configScroll) * 20, 128, 19,
                     getConfigItem(opcaoConfig), true);
      }
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = opcaoConfig;
      opcaoConfig = (opcaoConfig + 1) % NUM_CONFIG_ITEMS;
      lastDebounceTime = millis();
      if (opcaoConfig < configScroll ||
          opcaoConfig >= configScroll + CONFIG_PER_PAGE || old < configScroll ||
          old >= configScroll + CONFIG_PER_PAGE) {
        displayConfiguracoes();
      } else {
        drawMenuItem(0, 16 + (old - configScroll) * 20, 128, 19,
                     getConfigItem(old), false);
        drawMenuItem(0, 16 + (opcaoConfig - configScroll) * 20, 128, 19,
                     getConfigItem(opcaoConfig), true);
      }
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      if (opcaoConfig == 0) {
        opcaoConfig = 0;
        estadoAtual = MENU_INICIAL;
        displayMenuInicial();
      } else if (opcaoConfig == 1) {
        estadoAtual = TELA_SOBRE;
        displaySobre();
      } else if (opcaoConfig == 2) {
        estadoAtual = TELA_MAC_CHANGER;
        displayMudarMAC();
      } else if (opcaoConfig == 3) {
        estadoAtual = TELA_BRILHO;
        displayBrilho();
      } else if (opcaoConfig == 4) {
        estadoAtual = TELA_MODO_MENU;
        displayModoMenu();
      } else if (opcaoConfig == 5) {
        estadoAtual = TELA_ARMAZENAMENTO;
        displayArmazenamento();
      } else if (opcaoConfig == 6) {
        estadoAtual = TELA_SCREENSAVER_TEST;
        initScreensaverMenu();
        displayScreensaverTest();
      } else if (opcaoConfig == 7) {
        estadoAtual = TELA_IDIOMA;
        displayIdioma();
      } else if (opcaoConfig == 8) {
        hrStep = 0;
        hrSel  = 0;
        estadoAtual = TELA_HARDRESET;
        displayHardReset();
      } else if (opcaoConfig == 9) {
        estadoAtual = TELA_DESLIGAR;
        displayDesligar();
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  MODO MENU (Grade vs Lista)
// ═══════════════════════════════════════════════
static int modoMenuTemp = 0; // Para navegação temporária na tela

static void drawModoMenuBlock(int i, bool hover, bool active) {
  const int bw = 116;
  const int bh = 48;
  const int bx = (SCR_W - bw) / 2;
  const int by1 = 30;
  const int by2 = 86;
  int y = (i == 0) ? by1 : by2;

  tft.fillRect(bx, y, bw, bh, hover ? C_GOLD_SEL : C_BG);
  tft.drawRoundRect(bx, y, bw, bh, 4, hover ? C_GOLD : C_GREY);

  uint16_t iconColor = hover ? C_GOLD : C_WHITE;
  if (i == 0) {
    tft.drawRect(bx + 12, y + 12, 10, 10, iconColor);
    tft.drawRect(bx + 24, y + 12, 10, 10, iconColor);
    tft.drawRect(bx + 12, y + 26, 10, 10, iconColor);
    tft.drawRect(bx + 24, y + 26, 10, 10, iconColor);
  } else {
    tft.drawRoundRect(bx + 12, y + 14, 22, 6, 2, iconColor);
    tft.drawRoundRect(bx + 12, y + 22, 22, 6, 2, iconColor);
    tft.drawRoundRect(bx + 12, y + 30, 22, 6, 2, iconColor);
  }

  tft.setTextSize(1);
  tft.setTextColor(hover ? C_GOLD : C_WHITE);
  tft.setCursor(bx + 46, y + 20);
  tft.print(i == 0 ? lang->cfg_mm_bloco : lang->cfg_mm_lista);

  int rx = bx + bw - 16;
  int ry = y + bh / 2;
  tft.drawCircle(rx, ry, 6, hover ? C_GOLD : C_GREY);
  if (active) {
    tft.fillCircle(rx, ry, 3, hover ? C_GOLD : C_WHITE);
  } else {
    tft.fillCircle(rx, ry, 3, hover ? C_GOLD_SEL : C_BG);
  }
}

void displayModoMenu() {
  tft.fillScreen(C_BG);
  drawHeader(lang->cfg_hdr_modomenu, true);

  for (int i = 0; i < 2; i++) {
    drawModoMenuBlock(i, modoMenuTemp == i, menuStyle == i);
  }

  drawFooter();
  batteryDraw();
}

void handleModoMenu() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW) {
      modoMenuTemp = (modoMenuTemp == 0) ? 1 : 0;
      lastDebounceTime = millis();
      drawModoMenuBlock(0, modoMenuTemp == 0, menuStyle == 0);
      drawModoMenuBlock(1, modoMenuTemp == 1, menuStyle == 1);
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      menuStyle = modoMenuTemp;
      prefs.putInt("menuStyle", menuStyle); // SALVA NA MEMÓRIA FIXA
      lastDebounceTime = millis();
      estadoAtual = MENU_CONFIGURACOES;
      displayConfiguracoes();
    }
  }
}

// ═══════════════════════════════════════════════
//  INICIALIZAÇÃO DAS CONFIGURAÇÕES SALVAS (PREFS)
// ═══════════════════════════════════════════════

// Precisa ser declarado antes de initConfig pois ela usa macBuf
static uint8_t macState = 0;
static uint8_t macBuf[6] = {0};

// Forward declaration de blInit para aplicar o brilho salvo
static void blInit();

void initConfig() {
  prefs.begin("r4bb1t", false);

  // Carrega Modo Menu (0=Grade, 1=Lista)
  menuStyle = prefs.getInt("menuStyle", 0);

  // Carrega MAC customizado se houver
  if (prefs.getBytesLength("mac") == 6) {
    prefs.getBytes("mac", macBuf, 6);
    esp_wifi_set_mac(WIFI_IF_STA, macBuf); // Aplica MAC salvo
  }
  // (se não houver, macBuf fica {0} e o MAC nativo do chip é usado)

  // Carrega Idioma
  int langId = prefs.getInt("idioma", 0);
  setLanguage(langId);

  // Aplica o brilho salvo
  blInit();
}

// ═══════════════════════════════════════════════
//  IDIOMA
// ═══════════════════════════════════════════════
static int idiomaTemp = 0; // 0 = PT, 1 = EN

void displayIdioma() {
  tft.fillScreen(C_BG);
  drawHeader(lang->cfg_hdr_idioma, true);

  // Inicializa o cursor temporário com o valor atual salvo na EEPROM
  idiomaTemp = prefs.getInt("idioma", 0);

  drawMenuItem(0, 40, SCR_W, 19, lang->cfg_lang_pt, idiomaTemp == 0);
  drawMenuItem(0, 60, SCR_W, 19, lang->cfg_lang_en, idiomaTemp == 1);

  drawSeparator(SCR_H - 24, C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, SCR_H - 18);
  tft.print(lang->cfg_lang_hint);

  batteryDraw();
}

void handleIdioma() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      idiomaTemp = (idiomaTemp == 0) ? 1 : 0;
      drawMenuItem(0, 40, SCR_W, 19, lang->cfg_lang_pt, idiomaTemp == 0);
      drawMenuItem(0, 60, SCR_W, 19, lang->cfg_lang_en, idiomaTemp == 1);
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      // Salva e aplica a alteração
      setLanguage(idiomaTemp);
      prefs.putInt("idioma", idiomaTemp);
      
      // Feedback rápido
      tft.fillRect(0, 85, SCR_W, 12, C_BG);
      tft.setTextColor(C_GREEN);
      tft.setCursor(30, 85);
      tft.print(lang->cfg_lang_salvo);
      delay(600);

      estadoAtual = MENU_CONFIGURACOES;
      displayConfiguracoes();
    }
  }
}

// ═══════════════════════════════════════════════
//  MUDAR MAC
// ═══════════════════════════════════════════════

void displayMudarMAC() {
  tft.fillScreen(C_BG);
  drawHeader(lang->cfg_hdr_mac, true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 18);
  tft.print(lang->cfg_mac_atual);
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 28);
  tft.print(WiFi.macAddress());
  drawSeparator(38, C_GREY);

  if (macState == 0) {
    tft.setTextColor(C_GOLD);
    tft.setCursor(8, 60);
    tft.print(lang->cfg_mac_gerar);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(32, 76);
    tft.print(lang->cfg_mac_sel_gerar);
  } else if (macState == 1) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", macBuf[0],
             macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 48);
    tft.print(lang->cfg_mac_novo);
    tft.setTextColor(C_GOLD);
    tft.setCursor(4, 60);
    tft.print(buf);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(28, 80);
    tft.print(lang->cfg_mac_sel_aplicar);
  } else if (macState == 2) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", macBuf[0],
             macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 48);
    tft.print(lang->cfg_mac_novo);
    tft.setTextColor(C_GREEN);
    tft.setCursor(4, 60);
    tft.print(buf);
    tft.setCursor(20, 76);
    tft.print(lang->cfg_mac_aplicado);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(32, 96);
    tft.print(lang->cfg_mac_sel_voltar);
  }

  batteryDraw();
}

void handleMudarMAC() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      if (macState == 0) {
        for (int i = 0; i < 6; i++)
          macBuf[i] = (uint8_t)(esp_random() & 0xFF);
        macBuf[0] = (macBuf[0] & 0xFE) | 0x02;
        macState = 1;
        displayMudarMAC();
      } else if (macState == 1) {
        WiFi.mode(WIFI_STA);
        esp_wifi_set_mac(WIFI_IF_STA, macBuf);
        prefs.putBytes("mac", macBuf, 6); // SALVA NA MEMÓRIA FIXA
        macState = 2;
        displayMudarMAC();
      } else if (macState == 2) {
        macState = 0;
        estadoAtual = MENU_CONFIGURACOES;
        displayConfiguracoes();
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  SOBRE (carrossel de 4 páginas)
// ═══════════════════════════════════════════════
#define SOBRE_PAGES 5
static int sobrePage = 0;

// Indicadores de página (bolinhas) em dourado
static void sobreDots(int cur) {
  const int DOT_Y = SCR_H - 28;
  const int DOT_R = 3;
  const int GAP = 10;
  int totalW =
      SOBRE_PAGES * (DOT_R * 2) + (SOBRE_PAGES - 1) * (GAP - DOT_R * 2);
  int startX = (SCR_W - totalW) / 2 + DOT_R;
  for (int i = 0; i < SOBRE_PAGES; i++) {
    int cx = startX + i * GAP;
    if (i == cur)
      tft.fillCircle(cx, DOT_Y, DOT_R, C_GOLD);
    else
      tft.drawCircle(cx, DOT_Y, DOT_R, C_GREY);
  }
}

static void sobreHeader(const char *titulo) {
  tft.fillScreen(C_BG);
  drawHeader(titulo, true);
}

static void sobreFooter(int cur) {
  sobreDots(cur);
  drawSeparator(SCR_H - 18, C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(5, SCR_H - 12);
  tft.print(cur > 0 ? "<" : "x");
  tft.setCursor(61, SCR_H - 12);
  tft.print("o");
  tft.setCursor(SCR_W - 11, SCR_H - 12);
  tft.print(cur < SOBRE_PAGES - 1 ? ">" : " ");
}

// Página 0: Sistema
static void displaySobre_p0() {
  sobreHeader("R4BB1T FHC");

  esp_chip_info_t chip;
  esp_chip_info(&chip);
  const char *model = "ESP32";
  if (chip.model == CHIP_ESP32S2)
    model = "ESP32-S2";
  else if (chip.model == CHIP_ESP32S3)
    model = "ESP32-S3";
  else if (chip.model == CHIP_ESP32C3)
    model = "ESP32-C3";
  else if (chip.model == CHIP_ESP32H2)
    model = "ESP32-H2";

  int y = 18;
  const int LH = 12;
  auto row = [&](const char *lbl, String val, uint16_t c = C_WHITE) {
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, y);
    tft.print(lbl);
    tft.setTextColor(c);
    tft.setCursor(46, y);
    tft.print(val);
    y += LH;
  };

  row(lang->cfg_lbl_chip, model);
  row(lang->cfg_lbl_cores, String(chip.cores));
  row(lang->cfg_lbl_rev, String(chip.revision));
  row(lang->cfg_lbl_flash, String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB");
  row(lang->cfg_lbl_heap, String(ESP.getFreeHeap() / 1024) + " KB", C_GREEN);
  row(lang->cfg_lbl_sdk, String(ESP.getSdkVersion()).substring(0, 10));
  row(lang->cfg_lbl_fw, "v1.0.0", C_GOLD);

  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, y);
  tft.print(lang->cfg_lbl_mac);
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, y + 10);
  tft.print(WiFi.macAddress());

  sobreFooter(0);
}

// Página 1: NRF24L01
static void displaySobre_p1() {
  nrfProbe();
  sobreHeader("NRF24L01");

  int y = 18;
  const int LH = 14;
  tft.setTextSize(1);
  auto row = [&](const char *lbl, const char *val, uint16_t c = C_WHITE) {
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, y);
    tft.print(lbl);
    tft.setTextColor(c);
    tft.setCursor(52, y);
    tft.print(val);
    y += LH;
  };

  row("Status:", hwNRF24_ok ? lang->cfg_st_conectado : lang->cfg_st_nao_detect,
      hwNRF24_ok ? C_GREEN : C_RED);
  row(lang->cfg_lbl_bus, lang->cfg_val_hspi, C_GOLD_DIM);
  row(lang->cfg_lbl_ce, "GPIO 22");
  row(lang->cfg_lbl_csn, "GPIO 4");
  row(lang->cfg_lbl_sck, "GPIO 33");
  row(lang->cfg_lbl_miso, "GPIO 19");
  row(lang->cfg_lbl_mosi, "GPIO 13");

  sobreFooter(1);
}

// Página 2: NRF24L01 (Módulo 2)
static void displaySobre_p2() {
  nrfProbe2();
  sobreHeader("NRF24L01 #2");

  int y = 18;
  const int LH = 14;
  tft.setTextSize(1);
  auto row = [&](const char *lbl, const char *val, uint16_t c = C_WHITE) {
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, y);
    tft.print(lbl);
    tft.setTextColor(c);
    tft.setCursor(52, y);
    tft.print(val);
    y += LH;
  };

  row("Status:", hwNRF24_2_ok ? lang->cfg_st_conectado : lang->cfg_st_nao_detect,
      hwNRF24_2_ok ? C_GREEN : C_RED);
  row(lang->cfg_lbl_bus, lang->cfg_val_hspi, C_GOLD_DIM);
  row(lang->cfg_lbl_ce, "GPIO 12");
  row(lang->cfg_lbl_csn, "GPIO 15");
  row(lang->cfg_lbl_sck, "GPIO 33");
  row(lang->cfg_lbl_miso, "GPIO 19");
  row(lang->cfg_lbl_mosi, "GPIO 13");

  sobreFooter(2);
}

// Página 3: CC1101
static void displaySobre_p3() {
  hwCC1101_ok = rfInit();
  sobreHeader("CC1101");

  int y = 18;
  const int LH = 14;
  bool ok = hwCC1101_ok;
  auto row = [&](const char *lbl, const char *val, uint16_t c = C_WHITE) {
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, y);
    tft.print(lbl);
    tft.setTextColor(c);
    tft.setCursor(52, y);
    tft.print(val);
    y += LH;
  };

  row("Status:", ok ? lang->cfg_st_conectado : lang->cfg_st_nao_detect, ok ? C_GREEN : C_RED);
  row(lang->cfg_lbl_freq, "433.92 MHz", C_GOLD);
  row(lang->cfg_lbl_cs, "GPIO 25");
  row(lang->cfg_lbl_gdo0, "GPIO 2");
  row(lang->cfg_lbl_gdo2, "GPIO 32");
  row(lang->cfg_lbl_bus, lang->cfg_val_hspi, C_GOLD_DIM);

  sobreFooter(3);
}

// Página 4: Bateria
static void displaySobre_p4() {
  sobreHeader("BATERIA");

  int pct = batteryPercent();
  float vbat = 3.0f + (pct / 100.0f) * 1.2f;
  uint16_t col = pct > 50 ? C_GREEN : (pct > 20 ? C_YELLOW : C_RED);

  // Percentual grande
  tft.setTextSize(4);
  char buf[6];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  int bx = (SCR_W - (int)strlen(buf) * 24) / 2;
  tft.setTextColor(col);
  tft.setCursor(bx < 2 ? 2 : bx, 18);
  tft.print(buf);

  // Barra de carga
  const int BW = 114;
  const int BH = 20;
  const int BX = (SCR_W - BW) / 2;
  const int BY = 72;
  tft.drawRect(BX - 1, BY - 1, BW + 2, BH + 2, C_GOLD);
  tft.fillRect(BX, BY, BW, BH, C_GREY_DARK);
  int fill = (int)((long)BW * pct / 100);
  if (fill > 0)
    tft.fillRect(BX, BY, fill, BH, col);
  tft.fillRect(BX + BW + 1, BY + 6, 4, BH - 12, col);

  // Label dentro da barra
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK);
  if (fill > 20) {
    int li = pct < 10 ? 0 : pct < 25 ? 1 : pct < 50 ? 2 : pct < 80 ? 3 : 4;
    const char* lvlStr;
    switch(li) {
      case 0: lvlStr = lang->cfg_bat_critico; break;
      case 1: lvlStr = lang->cfg_bat_baixo; break;
      case 2: lvlStr = lang->cfg_bat_medio; break;
      case 3: lvlStr = lang->cfg_bat_bom; break;
      default: lvlStr = lang->cfg_bat_cheio; break;
    }
    int lw = (int)strlen(lvlStr) * 6;
    tft.setCursor(BX + (fill - lw) / 2, BY + 7);
    tft.print(lvlStr);
  }

  // Detalhes
  int y = 102;
  const int LH = 13;
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, y);
  tft.print(lang->cfg_lbl_tensao);
  char vbuf[12];
  dtostrf(vbat, 4, 2, vbuf);
  tft.setTextColor(col);
  tft.setCursor(54, y);
  tft.print(vbuf);
  tft.print(" V");
  y += LH;
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, y);
  tft.print(lang->cfg_lbl_adcpin);
  tft.setTextColor(C_WHITE);
  tft.setCursor(54, y);
  tft.print("GPIO 36");

  sobreFooter(4);
}

void displaySobre() {
  switch (sobrePage) {
  case 0:
    displaySobre_p0();
    break;
  case 1:
    displaySobre_p1();
    break;
  case 2:
    displaySobre_p2();
    break;
  case 3:
    displaySobre_p3();
    break;
  case 4:
    displaySobre_p4();
    break;
  default:
    sobrePage = 0;
    displaySobre_p0();
    break;
  }
}

void handleSobre() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      if (sobrePage < SOBRE_PAGES - 1) {
        sobrePage++;
        displaySobre();
      }
    }
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      if (sobrePage > 0) {
        sobrePage--;
        displaySobre();
      } else {
        sobrePage = 0;
        estadoAtual = MENU_CONFIGURACOES;
        displayConfiguracoes();
      }
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      sobrePage = 0;
      estadoAtual = MENU_CONFIGURACOES;
      displayConfiguracoes();
    }
  }
}

// ═══════════════════════════════════════════════
//  BRILHO
// ═══════════════════════════════════════════════
#define BL_CHANNEL 0
#define BL_FREQ 5000
#define BL_RES 8
#define BL_STEPS 10
#define BL_MIN 15
#define BL_MAX 255

static int brilhoAtual = BL_MAX;
static bool blIniciado = false;

static void blInit() {
  if (!blIniciado) {
    ledcSetup(BL_CHANNEL, BL_FREQ, BL_RES);
    ledcAttachPin(TFT_BL, BL_CHANNEL);
    brilhoAtual = prefs.getInt("brilho", BL_MAX);
    if (brilhoAtual < BL_MIN)
      brilhoAtual = BL_MIN;
    ledcWrite(BL_CHANNEL, brilhoAtual);
    blIniciado = true;
  }
}
static void blSet(int v) {
  blInit();
  brilhoAtual = v;
  ledcWrite(BL_CHANNEL, v);
  prefs.putInt("brilho", v); // SALVA NA MEMÓRIA FIXA
}

static void blDim() {
  blInit();
  ledcWrite(BL_CHANNEL, BL_MIN);
}

static void blRestore() {
  blInit();
  ledcWrite(BL_CHANNEL, brilhoAtual);
}

void blOff() {
  blInit();
  ledcWrite(BL_CHANNEL, 0);
  ledcDetachPin(TFT_BL);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);
  gpio_hold_en((gpio_num_t)TFT_BL);
  gpio_deep_sleep_hold_en();
}

static void drawBrilhoSlider() {
  int pct = (int)((long)(brilhoAtual - BL_MIN) * 100 / (BL_MAX - BL_MIN));
  if (pct < 0)
    pct = 0;
  if (pct > 100)
    pct = 100;

  tft.fillRect(0, 40, SCR_W, 24, C_BG);

  tft.setTextSize(3);
  char pctBuf[6];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
  int pctX = (SCR_W - (int)strlen(pctBuf) * 18) / 2;
  tft.setTextColor(C_GOLD);
  tft.setCursor(pctX, 40);
  tft.print(pctBuf);

  const int slX0 = 10;
  const int slX1 = SCR_W - 10;
  const int slY = 88;
  const int slW = slX1 - slX0;

  tft.fillRect(slX0 - 6, slY - 6, slW + 12, 12, C_BG);

  tft.drawFastHLine(slX0, slY, slW, C_GREY);
  int fillW = (int)((long)slW * (brilhoAtual - BL_MIN) / (BL_MAX - BL_MIN));
  if (fillW > 0)
    tft.drawFastHLine(slX0, slY, fillW, C_GOLD);

  int dotX = slX0 + fillW;
  tft.fillCircle(dotX, slY, 5, C_GOLD);
  tft.drawCircle(dotX, slY, 5, C_WHITE);
}

void displayBrilho() {
  tft.fillScreen(C_BG);
  drawHeader(lang->cfg_hdr_brilho, true);

  drawBrilhoSlider();

  const int slX0 = 10;
  const int slX1 = SCR_W - 10;
  const int slY = 88;
  tft.setTextSize(1);
  tft.setTextColor(C_GREY);
  tft.setCursor(slX0, slY + 10);
  tft.print(lang->cfg_bl_min);
  tft.setCursor(slX1 - 12, slY + 10);
  tft.print(lang->cfg_bl_max);

  drawSeparator(110, C_GREY);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(10, 116);
  tft.print(lang->cfg_bl_hint1);
  tft.setCursor(10, 128);
  tft.print(lang->cfg_bl_hint2);

  drawFooter();
  batteryDraw();
}

void handleBrilho() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    int step = (BL_MAX - BL_MIN) / BL_STEPS;
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      brilhoAtual += step;
      if (brilhoAtual > BL_MAX)
        brilhoAtual = BL_MAX;
      blSet(brilhoAtual);
      drawBrilhoSlider();
    }
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      brilhoAtual -= step;
      if (brilhoAtual < BL_MIN)
        brilhoAtual = BL_MIN;
      blSet(brilhoAtual);
      drawBrilhoSlider();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      estadoAtual = MENU_CONFIGURACOES;
      displayConfiguracoes();
    }
  }
}

// ═══════════════════════════════════════════════
//  ARMAZENAMENTO — Donut Chart + File Browser
// ═══════════════════════════════════════════════

// Cor laranja para segmento de firmware (RGB565)
#ifndef C_ORANGE
#define C_ORANGE 0xFB60
#endif

// Estados internos da tela de armazenamento
static int storageState = 0; // 0=donut, 1=lista, 2=viewer txt, 3=viewer bmp
static int storageOpcao = 1; // 0=VOLTAR, 1=ARQUIVOS

// ── Estado: lista de arquivos ──
#define FILES_MAX 24
#define FILES_PER_PAGE 8
static int fileCursor = 0;
static int fileCount = 0;
static char fileNames[FILES_MAX][26];
static size_t fileSizes[FILES_MAX];

// ── Estado: visualizador de texto ──
#define VIEWER_LINE_W 21
#define VIEWER_LINES 9
static char viewBuf[4096];
static int viewLen = 0;
#define VIEW_LINES_MAX 256
static int viewLineOff[VIEW_LINES_MAX];
static int viewLineLen[VIEW_LINES_MAX];
static int viewTotalLines = 0;
static int viewScroll = 0;

// Constantes do donut
#define DONUT_CX 36   // centro X (metade esquerda da tela)
#define DONUT_CY 72   // centro Y
#define DONUT_ROUT 34 // raio externo
#define DONUT_RIN 21  // raio interno

// ── Desenha um arco de a0_deg até a1_deg (graus, 0=topo, sentido horário) ──
static void drawArcSeg(int cx, int cy, int r_out, int r_in, float a0, float a1,
                       uint16_t color) {
  int r_in2 = r_in * r_in;
  int r_out2 = r_out * r_out;
  for (int y = cy - r_out; y <= cy + r_out; y++) {
    for (int x = cx - r_out; x <= cx + r_out; x++) {
      int dx = x - cx;
      int dy = y - cy;
      int r2 = dx * dx + dy * dy;
      if (r2 >= r_in2 && r2 <= r_out2) {
        float angle =
            atan2f((float)dy, (float)dx) * 180.0f / (float)M_PI + 90.0f;
        if (angle < 0.0f)
          angle += 360.0f;
        if (angle >= a0 && angle <= a1) {
          tft.drawPixel(x, y, color);
        }
      }
    }
  }
}

// ── Label central do donut ──
static void drawDonutCenter(int cx, int cy, int pct) {
  tft.setTextSize(1);
  char buf[5];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  int tx = cx - ((int)strlen(buf) * 6) / 2;
  tft.setTextColor(C_WHITE);
  tft.setCursor(tx, cy - 4);
  tft.print(buf);
  tft.setTextColor(C_GOLD_DIM);
  int ux = cx - 9;
  tft.setCursor(ux, cy + 5);
  tft.print(lang->cfg_st_used);
}

// ── Item de legenda: quadrado colorido + nome + valor ──
static void drawStorLegend(int x, int y, uint16_t color, const char *label,
                           const char *value) {
  tft.fillRect(x, y, 7, 7, color);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(x + 10, y);
  tft.print(label);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(x + 10, y + 9);
  tft.print(value);
}

// ── Atualiza apenas os botões da tela de armazenamento ──
static void updateArmazenamentoBotoes() {
  const int BTN_Y = 119;
  const int BTN_H = 16;
  const int BTN_W = 55;

  // [VOLTAR]
  bool selV = (storageOpcao == 0);
  tft.fillRoundRect(3, BTN_Y, BTN_W, BTN_H, 3, selV ? C_GOLD_SEL : C_BG);
  tft.drawRoundRect(3, BTN_Y, BTN_W, BTN_H, 3, selV ? C_GOLD : C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(selV ? C_GOLD : C_WHITE);
  tft.setCursor(15, BTN_Y + 5);
  tft.print(lang->cfg_st_voltar);

  // [ARQUIVOS]
  bool selA = (storageOpcao == 1);
  tft.fillRoundRect(70, BTN_Y, BTN_W, BTN_H, 3, selA ? C_GOLD_SEL : C_BG);
  tft.drawRoundRect(70, BTN_Y, BTN_W, BTN_H, 3, selA ? C_GOLD : C_GREY);
  tft.setTextColor(selA ? C_GOLD : C_WHITE);
  tft.setCursor(76, BTN_Y + 5);
  tft.print(lang->cfg_st_arquivos);
}

// ── Tela principal: donut + legenda + botões ──
void displayArmazenamento() {
  tft.fillScreen(C_BG);
  drawHeader(lang->cfg_hdr_storage, true);

  // Coleta dados SPIFFS
  size_t sp_total = SPIFFS.totalBytes();
  size_t sp_used = SPIFFS.usedBytes();
  size_t sp_free = sp_total - sp_used;

  // Firmware na flash
  size_t fw_size = ESP.getSketchSize();
  size_t flash_tot = ESP.getFlashChipSize();
  if (flash_tot == 0)
    flash_tot = 4 * 1024 * 1024; // fallback 4MB

  // Livre na flash total (o que resta na pizza)
  size_t flash_free =
      flash_tot > (fw_size + sp_used) ? flash_tot - fw_size - sp_used : 0;

  // Percentuais sobre a flash total
  float fw_pct = (fw_size * 100.0f) / (float)flash_tot;
  float sp_pct = (sp_used * 100.0f) / (float)flash_tot;

  // Ângulos dos arcos
  float a_fw = fw_pct * 3.6f;
  float a_sp = a_fw + sp_pct * 3.6f;

  // ── Desenha arcos ──
  // Livre (cinza) — fundo completo primeiro
  drawArcSeg(DONUT_CX, DONUT_CY, DONUT_ROUT, DONUT_RIN, 0.0f, 360.0f, C_GREY);
  // Firmware (laranja)
  if (a_fw > 0.5f)
    drawArcSeg(DONUT_CX, DONUT_CY, DONUT_ROUT, DONUT_RIN, 0.0f, a_fw, C_ORANGE);
  // SPIFFS usado (dourado)
  if (sp_pct > 0.3f)
    drawArcSeg(DONUT_CX, DONUT_CY, DONUT_ROUT, DONUT_RIN, a_fw, a_sp, C_GOLD);
  // Buraco central
  tft.fillCircle(DONUT_CX, DONUT_CY, DONUT_RIN - 1, C_BG);

  // Label central: % total usado
  int used_pct = (int)(fw_pct + sp_pct);
  if (used_pct > 100)
    used_pct = 100;
  drawDonutCenter(DONUT_CX, DONUT_CY, used_pct);

  // ── Legenda (lado direito) ──
  char buf_fw[10], buf_sp[10], buf_fr[10], buf_ft[12];
  snprintf(buf_fw, sizeof(buf_fw), "%uKB", (unsigned)(fw_size / 1024));
  snprintf(buf_sp, sizeof(buf_sp), "%uKB", (unsigned)(sp_used / 1024));
  snprintf(buf_fr, sizeof(buf_fr), "%uKB", (unsigned)(flash_free / 1024));
  snprintf(buf_ft, sizeof(buf_ft), "%uMB tot",
           (unsigned)(flash_tot / (1024 * 1024)));

  int lx = DONUT_CX + DONUT_ROUT + 8;
  drawStorLegend(lx, 22, C_ORANGE, "FW", buf_fw);
  drawStorLegend(lx, 44, C_GOLD, "SPIF", buf_sp);
  drawStorLegend(lx, 66, C_GREY, "FREE", buf_fr);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(lx, 88);
  tft.print(buf_ft);

  // ── Botões de ação ──
  drawSeparator(114, C_GREY);

  updateArmazenamentoBotoes();

  // Dica de controles
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(14, 140);
  tft.print(lang->cfg_st_hint);

  batteryDraw();
}

// ── Coleta arquivos do SPIFFS ──
static void spiffsCollect() {
  fileCount = 0;
  File root = SPIFFS.open("/");
  if (!root || !root.isDirectory())
    return;
  File f = root.openNextFile();
  while (f && fileCount < FILES_MAX) {
    if (!f.isDirectory()) {
      strncpy(fileNames[fileCount], f.name(), 25);
      fileNames[fileCount][25] = '\0';
      fileSizes[fileCount] = f.size();
      fileCount++;
    }
    f = root.openNextFile();
  }
}

// ── Tela 1: Lista de Arquivos com Cursor ──
static void drawArquivosRow(int i, bool sel) {
  int pageStart = (fileCursor / FILES_PER_PAGE) * FILES_PER_PAGE;
  int y = 17 + (i - pageStart) * 15;
  const int ROW_H = 14;

  if (sel) {
    tft.fillRect(0, y - 1, SCR_W, ROW_H + 1, C_GOLD_SEL);
    tft.drawFastHLine(0, y - 1, SCR_W, C_GOLD);
    tft.drawFastHLine(0, y + ROW_H, SCR_W, C_GOLD);
  } else {
    tft.fillRect(0, y - 1, SCR_W, ROW_H + 2, C_BG);
    tft.drawFastHLine(4, y + ROW_H, SCR_W - 8, C_GREY_DARK);
  }

  if (i == 0) {
    tft.setTextSize(1);
    tft.setTextColor(sel ? C_GOLD : C_GREY);
    tft.setCursor(4, y + 3);
    tft.print(lang->cfg_st_back);
  } else {
    int fi = i - 1;
    char sname[17];
    strncpy(sname, fileNames[fi], 16);
    sname[16] = '\0';
    tft.setTextSize(1);
    tft.setTextColor(sel ? C_GOLD : C_WHITE);
    tft.setCursor(4, y + 3);
    tft.print(sname);

    char sbuf[9];
    if (fileSizes[fi] < 1024)
      snprintf(sbuf, sizeof(sbuf), "%uB", (unsigned)fileSizes[fi]);
    else
      snprintf(sbuf, sizeof(sbuf), "%uKB", (unsigned)(fileSizes[fi] / 1024));
    int sw = (int)strlen(sbuf) * 6;
    tft.setTextColor(sel ? C_GOLD : C_GOLD_DIM);
    tft.setCursor(SCR_W - sw - 3, y + 3);
    tft.print(sbuf);
  }
}

static void displayArquivosSPIFFS() {
  tft.fillScreen(C_BG);
  char htitle[18];
  snprintf(htitle, sizeof(htitle), "ARQUIVOS (%d)", fileCount);
  drawHeader(htitle, true);

  int totalItems = 1 + fileCount;
  int pageStart = (fileCursor / FILES_PER_PAGE) * FILES_PER_PAGE;

  for (int i = pageStart; i < totalItems && (i - pageStart) < FILES_PER_PAGE;
       i++) {
    drawArquivosRow(i, i == fileCursor);
  }

  if (pageStart > 0) {
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(SCR_W / 2 - 3, 17);
    tft.print("^");
  }
  if (pageStart + FILES_PER_PAGE < totalItems) {
    int arrowY = 17 + FILES_PER_PAGE * 15;
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(SCR_W / 2 - 3, arrowY);
    tft.print("v");
  }

  drawSeparator(SCR_H - 14, C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, SCR_H - 10);
  tft.print(lang->cfg_st_hint2);
  batteryDraw();
}

// ── Tela 2: Viewer Text e Tela 3: Viewer BMP ──
static uint16_t bmp_r16(File &f) {
  return (uint16_t)f.read() | ((uint16_t)f.read() << 8);
}
static uint32_t bmp_r32(File &f) {
  uint32_t lo = bmp_r16(f);
  return lo | ((uint32_t)bmp_r16(f) << 16);
}

static void displayFileViewerBMP() {
  tft.fillScreen(C_BG);
  int fi = fileCursor - 1;
  String path = fileNames[fi];
  if (!path.startsWith("/"))
    path = "/" + path;
  File f = SPIFFS.open(path.c_str(), FILE_READ);
  if (!f)
    return;
  if (bmp_r16(f) != 0x4D42) {
    f.close();
    return;
  }
  bmp_r32(f);
  bmp_r32(f);
  uint32_t dataOffset = bmp_r32(f);
  bmp_r32(f);
  int32_t bmpW = (int32_t)bmp_r32(f);
  int32_t bmpH = (int32_t)bmp_r32(f);
  bmp_r16(f);
  uint16_t bpp = bmp_r16(f);
  uint32_t comp = bmp_r32(f);
  if (bpp != 24 || comp != 0) {
    f.close();
    return;
  }

  bool flipY = (bmpH > 0);
  if (bmpH < 0)
    bmpH = -bmpH;
  int16_t scrW = 128;
  int16_t scrH = 140;

  uint32_t sx = (uint32_t)scrW * 256 / (uint32_t)bmpW;
  uint32_t sy = (uint32_t)scrH * 256 / (uint32_t)bmpH;
  uint32_t sc = (sx < sy) ? sx : sy;
  if (sc > 256)
    sc = 256;

  int16_t dW = (int16_t)((uint32_t)bmpW * sc / 256);
  int16_t dH = (int16_t)((uint32_t)bmpH * sc / 256);
  if (dW < 1)
    dW = 1;
  if (dH < 1)
    dH = 1;
  int16_t ox = (scrW - dW) / 2;
  int16_t oy = (scrH - dH) / 2;

  size_t fbSize = (size_t)dW * (size_t)dH;
  uint16_t *fb = (uint16_t *)malloc(fbSize * sizeof(uint16_t));
  uint32_t rowBytes = ((uint32_t)(bmpW * 3 + 3) / 4) * 4;
  uint8_t *row = (uint8_t *)malloc(rowBytes);
  if (!fb || !row) {
    if (fb)
      free(fb);
    if (row)
      free(row);
    f.close();
    return;
  }

  f.seek(dataOffset);
  for (int32_t fileRow = 0; fileRow < bmpH; fileRow++) {
    if (f.read(row, rowBytes) != (int)rowBytes)
      break;
    int32_t imgY = flipY ? (bmpH - 1 - fileRow) : fileRow;
    int16_t sY = (int16_t)((int32_t)imgY * dH / bmpH);
    if (sY < 0 || sY >= dH)
      continue;
    uint16_t *fbRow = &fb[(size_t)sY * (size_t)dW];
    for (int16_t x = 0; x < dW; x++) {
      int32_t sX = (int32_t)x * bmpW / dW;
      fbRow[x] = ((uint16_t)(row[sX * 3 + 2] & 0xF8) << 8) |
                 ((uint16_t)(row[sX * 3 + 1] & 0xFC) << 3) |
                 (row[sX * 3 + 0] >> 3);
    }
  }
  free(row);
  f.close();
  tft.setSwapBytes(true);
  tft.pushImage(ox, oy, dW, dH, fb);
  tft.setSwapBytes(false);
  free(fb);

  drawSeparator(SCR_H - 14, C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, SCR_H - 10);
  tft.print(lang->cfg_st_galeria);
  tft.setCursor(80, SCR_H - 10);
  tft.print(lang->cfg_st_sair);
  batteryDraw();
}

static void viewerBuildLines() {
  viewTotalLines = 0;
  int i = 0;
  while (i < viewLen && viewTotalLines < VIEW_LINES_MAX) {
    viewLineOff[viewTotalLines] = i;
    int end = i;
    int col = 0;
    while (end < viewLen && viewBuf[end] != '\n' && col < VIEWER_LINE_W) {
      end++;
      col++;
    }
    viewLineLen[viewTotalLines] = end - i;
    viewTotalLines++;
    if (end < viewLen && viewBuf[end] == '\n')
      end++;
    i = end;
  }
}

static void displayFileViewerText() {
  tft.fillScreen(C_BG);
  int fi = fileCursor - 1;
  char htitle[14];
  const char *fname = fileNames[fi];
  if (fname[0] == '/')
    fname++;
  strncpy(htitle, fname, 13);
  htitle[13] = '\0';
  drawHeader(htitle, true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 17);
  char info[28];
  snprintf(info, sizeof(info), "%uB  %d linhas", (unsigned)fileSizes[fi],
           viewTotalLines);
  tft.print(info);
  drawSeparator(26, C_GREY_DARK);

  int y = 29;
  const int LH = 12;
  for (int l = viewScroll;
       l < viewTotalLines && (l - viewScroll) < VIEWER_LINES; l++) {
    char line[VIEWER_LINE_W + 1];
    int ll = viewLineLen[l];
    if (ll > VIEWER_LINE_W)
      ll = VIEWER_LINE_W;
    memcpy(line, viewBuf + viewLineOff[l], ll);
    line[ll] = '\0';
    tft.setTextColor(C_WHITE);
    tft.setCursor(2, y);
    tft.print(line);
    y += LH;
  }

  if (viewTotalLines > VIEWER_LINES) {
    const int SBX = SCR_W - 3;
    const int SBY0 = 29;
    const int SBH = VIEWER_LINES * LH;
    tft.drawFastVLine(SBX, SBY0, SBH, C_GREY_DARK);
    int thumbH = SBH * VIEWER_LINES / viewTotalLines;
    if (thumbH < 4)
      thumbH = 4;
    int thumbY =
        SBY0 + (SBH - thumbH) * viewScroll / (viewTotalLines - VIEWER_LINES);
    tft.drawFastVLine(SBX, thumbY, thumbH, C_GOLD);
  }

  drawSeparator(SCR_H - 14, C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  char pg[14];
  snprintf(pg, sizeof(pg), "L%d/%d", viewScroll + 1, viewTotalLines);
  tft.setCursor(4, SCR_H - 10);
  tft.print(pg);
  tft.setCursor(55, SCR_H - 10);
  tft.print(lang->cfg_st_hint3);
  batteryDraw();
}

static void openFileForView(int fi) {
  const char *fname = fileNames[fi];
  int len = strlen(fname);
  if (len > 4 && strcasecmp(fname + len - 4, ".bmp") == 0) {
    storageState = 3;
    displayFileViewerBMP();
    return;
  }

  viewLen = 0;
  viewScroll = 0;
  String path = fname;
  if (!path.startsWith("/"))
    path = "/" + path;
  File f = SPIFFS.open(path.c_str(), FILE_READ);
  if (!f)
    return;
  viewLen = f.read((uint8_t *)viewBuf, sizeof(viewBuf) - 1);
  if (viewLen < 0)
    viewLen = 0;
  viewBuf[viewLen] = '\0';
  f.close();
  for (int i = 0; i < viewLen; i++) {
    if (viewBuf[i] != '\n' && (viewBuf[i] < 0x20 || viewBuf[i] > 0x7E))
      viewBuf[i] = '.';
  }
  viewerBuildLines();
  storageState = 2;
  displayFileViewerText();
}

static void switchViewerFile(int dir) {
  if (fileCount <= 1) return;
  
  int current = fileCursor - 1;
  current += dir;
  if (current < 0) current = fileCount - 1;
  if (current >= fileCount) current = 0;
  
  fileCursor = current + 1;
  openFileForView(current);
}

// ── Handlers ──
void handleArmazenamento() {
  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (storageState == 0) {
      if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW) {
        storageOpcao = (storageOpcao == 0) ? 1 : 0;
        lastDebounceTime = millis();
        updateArmazenamentoBotoes();
      }
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        if (storageOpcao == 1) { // 1 = ARQUIVOS
          spiffsCollect();
          fileCursor = 0;
          storageState = 1;
          displayArquivosSPIFFS();
        } else { // 0 = VOLTAR
          storageState = 0;
          storageOpcao = 1; // Reseta o estado para ARQUIVOS quando voltar a entrar
          estadoAtual = MENU_CONFIGURACOES;
          displayConfiguracoes();
        }
      }
    } else if (storageState == 1) {
      int totalItems = 1 + fileCount;
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        int oldCursor = fileCursor;
        fileCursor = (fileCursor + 1) % totalItems;
        lastDebounceTime = millis();
        if ((oldCursor / FILES_PER_PAGE) != (fileCursor / FILES_PER_PAGE)) {
          displayArquivosSPIFFS();
        } else {
          drawArquivosRow(oldCursor, false);
          drawArquivosRow(fileCursor, true);
        }
      }
      if (digitalRead(BUTTON_LEFT) == LOW) {
        int oldCursor = fileCursor;
        fileCursor = (fileCursor - 1 + totalItems) % totalItems;
        lastDebounceTime = millis();
        if ((oldCursor / FILES_PER_PAGE) != (fileCursor / FILES_PER_PAGE)) {
          displayArquivosSPIFFS();
        } else {
          drawArquivosRow(oldCursor, false);
          drawArquivosRow(fileCursor, true);
        }
      }
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        if (fileCursor == 0) {
          storageState = 0;
          displayArmazenamento();
        } else {
          openFileForView(fileCursor - 1);
        }
      }
    } else if (storageState == 2) {
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        unsigned long start = millis();
        while(digitalRead(BUTTON_RIGHT) == LOW) {
           if(millis() - start > 400) break;
           delay(10);
        }
        if (millis() - start > 400) {
           switchViewerFile(1);
        } else {
           if (viewScroll < viewTotalLines - VIEWER_LINES) viewScroll++;
           displayFileViewerText();
        }
        lastDebounceTime = millis();
      }
      if (digitalRead(BUTTON_LEFT) == LOW) {
        unsigned long start = millis();
        while(digitalRead(BUTTON_LEFT) == LOW) {
           if(millis() - start > 400) break;
           delay(10);
        }
        if (millis() - start > 400) {
           switchViewerFile(-1);
        } else {
           if (viewScroll > 0) viewScroll--;
           displayFileViewerText();
        }
        lastDebounceTime = millis();
      }
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        storageState = 1;
        displayArquivosSPIFFS();
      }
    } else if (storageState == 3) {
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        lastDebounceTime = millis();
        switchViewerFile(1);
      }
      if (digitalRead(BUTTON_LEFT) == LOW) {
        lastDebounceTime = millis();
        switchViewerFile(-1);
      }
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        storageState = 1;
        displayArquivosSPIFFS();
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  DESLIGAR
// ═══════════════════════════════════════════════

void displayDesligar() {
  tft.fillScreen(C_BG);
  drawHeader(lang->cfg_hdr_desligar, true);

  // ── Símbolo Power (Círculo aberto com traço) ──
  int px = 64;
  int py = 54;
  int size = 16;
  uint16_t color = C_GOLD;

  for (int r = size - 2; r <= size; r++) {
    for (float a = 30.0f; a <= 330.0f; a += 1.0f) {
      float rad = a * PI / 180.0f;
      int dx = px + (int)(r * sin(rad));
      int dy = py - (int)(r * cos(rad));
      tft.drawPixel(dx, dy, color);
    }
  }
  tft.fillRect(px - 1, py - size, 3, size + 2, color);

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  const char* t = lang->cfg_dl_msg;
  tft.setCursor((128 - strlen(t) * 6) / 2, 82);
  tft.print(t);

  drawSeparator(95, C_GREY);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(14, 106);
  tft.print(lang->cfg_dl_cancelar);
  tft.setTextColor(C_GOLD);
  tft.setCursor(14, 120);
  tft.print(lang->cfg_dl_confirmar);

  drawFooter();
  batteryDraw();
}

void handleDesligar() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      estadoAtual = MENU_CONFIGURACOES;
      displayConfiguracoes();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();

      tft.fillScreen(TFT_BLACK);

      // ── Símbolo Power vermelho e maior ──
      int px = 64;
      int py = 54;
      int size = 22;
      uint16_t color = TFT_RED;

      for (int r = size - 3; r <= size; r++) {
        for (float a = 30.0f; a <= 330.0f; a += 0.5f) {
          float rad = a * PI / 180.0f;
          int dx = px + (int)(r * sin(rad));
          int dy = py - (int)(r * cos(rad));
          tft.drawPixel(dx, dy, color);
        }
      }
      tft.fillRect(px - 1, py - size - 2, 4, size + 4, color);

      tft.setTextSize(1);
      tft.setTextColor(TFT_RED);
      const char* tMsg = lang->cfg_dl_sistema;
      tft.setCursor((128 - strlen(tMsg) * 6) / 2, 95);
      tft.print(tMsg);

      for (int i = 3; i >= 1; i--) {
        tft.fillRect(0, 110, 128, 16, TFT_BLACK);
        char cbuf[16];
        snprintf(cbuf, sizeof(cbuf), lang->cfg_dl_aguarde, i);
        tft.setTextColor(TFT_DARKGREY);
        tft.setCursor((128 - strlen(cbuf) * 6) / 2, 114);
        tft.print(cbuf);
        delay(1000);
      }

      tft.fillScreen(TFT_BLACK);
      blOff();
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
      esp_deep_sleep_start();
    }
  }
}

// ═══════════════════════════════════════════════
//  TESTAR TELA — Animações do ESP32-third-eye
//  Adaptado de 240x240 round (GFX canvas) para
//  128x160 retangular (TFT_eSPI direto)
// ═══════════════════════════════════════════════

#define ANIM_W 128
#define ANIM_H 160
#define ANIM_CX 64
#define ANIM_CY 80

// LUT de seno (256 entradas, amplitude ±127)
static int16_t animSinLUT[256];
static bool animLUTReady = false;

static void animInitLUT() {
  if (animLUTReady)
    return;
  for (int i = 0; i < 256; i++)
    animSinLUT[i] = (int16_t)(sin(i * 2.0 * PI / 256.0) * 127);
  animLUTReady = true;
}
static inline int16_t aSin(float a) { return animSinLUT[((int)a) & 0xFF]; }
static inline int16_t aCos(float a) { return animSinLUT[((int)a + 64) & 0xFF]; }

// Cor ciclante baseada em tempo
static uint16_t pCol(uint8_t speed, int offset) {
  uint32_t t = (speed == 0) ? 0 : (millis() / speed);
  uint8_t r = aSin(t + offset) + 128;
  uint8_t g = aSin(t + offset + 85) + 128;
  uint8_t b = aSin(t + offset + 170) + 128;
  return tft.color565(r, g, b);
}

// Número de animações (9 normais + 1 Splash + 1 botão voltar)
#define ANIM_COUNT 11

static int animIndex = 0;
static const char *animNames[] = {
    "<- VOLTAR", "Logo R4BB1T", "Matrix Rain", "Cubo 3D", "Plasma", "Tesseract 4D", "Corredor",
    "Onda Grade",  "Aneis",   "Quadrados", "Olho Magenta"};

static const char* getAnimName(int i) {
    switch(i) {
        case 0: return lang->cfg_sv_voltar;
        case 1: return lang->sv_name_logo;
        case 2: return lang->sv_name_matrix;
        case 3: return lang->sv_name_cubo;
        case 4: return lang->sv_name_plasma;
        case 5: return lang->sv_name_tesseract;
        case 6: return lang->sv_name_corredor;
        case 7: return lang->sv_name_ondagrade;
        case 8: return lang->sv_name_aneis;
        case 9: return lang->sv_name_quadrados;
        case 10: return lang->sv_name_olho;
        default: return "";
    }
}

static TFT_eSprite *animSpr = nullptr;

static void initScreensaverMenu() {
  animIndex = prefs.getInt("screensaver", 1);
  if (animIndex == 0 || animIndex >= ANIM_COUNT) animIndex = 1;
}

static int scrScroll = 0;

void displayScreensaverTest() {
  tft.fillScreen(C_BG);
  drawHeader(lang->cfg_hdr_saver, true);

  if (animIndex < scrScroll)
    scrScroll = animIndex;
  if (animIndex >= scrScroll + 6)
    scrScroll = animIndex - 6 + 1;

  int savedIdx = prefs.getInt("screensaver", 1);

  for (int i = 0; i < 6; i++) {
    int idx = scrScroll + i;
    if (idx >= ANIM_COUNT) break;

    String label = String(getAnimName(idx));
    if (idx == savedIdx && idx != 0) label += lang->cfg_sv_ativo;

    drawMenuItem(0, 16 + i * 19, 125, 19, label.c_str(), idx == animIndex);
  }

  // Barra de rolagem
  if (ANIM_COUNT > 6) {
    int barH = (6 * 114) / ANIM_COUNT; 
    int barY = 16 + (scrScroll * 114) / ANIM_COUNT;
    tft.drawFastVLine(126, 16, 114, C_GREY);
    tft.drawFastVLine(127, 16, 114, C_GREY);
    tft.drawFastVLine(126, barY, barH, C_GOLD);
    tft.drawFastVLine(127, barY, barH, C_GOLD);
  }

  drawSeparator(132, C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 136);
  tft.print(lang->cfg_sv_hint1);
  tft.setCursor(70, 136);
  tft.print(lang->cfg_sv_hint2);
  
  batteryDraw();
}

// ── Estado de execução da animação ──────────────────────────
static bool animRunning = false;

// Matriz rain columns
struct AnimCol {
  int y, speed;
  uint32_t lastUpdate;
};
static AnimCol matCols[13];
static void matInitCol(int i) {
  matCols[i].y = random(-100, 0);
  matCols[i].speed = random(30, 120);
  matCols[i].lastUpdate = millis();
}

// Olho: física de movimento
static float eyeX = ANIM_CX, eyeY = ANIM_CY;
static float eyeTX = ANIM_CX, eyeTY = ANIM_CY;
static float eyeSpd = 2.0f;
static uint32_t eyeNextMove = 0;
static bool eyeMoving = false;

static void eyePhysics(uint32_t now) {
  if (eyeMoving) {
    float dx = eyeTX - eyeX, dy = eyeTY - eyeY;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 1.0f || now > eyeNextMove) {
      eyeX = eyeTX;
      eyeY = eyeTY;
      eyeMoving = false;
      eyeNextMove = now + random(400, 1400);
    } else {
      if (dist > eyeSpd) {
        eyeX += (dx / dist) * eyeSpd;
        eyeY += (dy / dist) * eyeSpd;
      } else {
        eyeX = eyeTX;
        eyeY = eyeTY;
      }
    }
  } else {
    if (now > eyeNextMove) {
      eyeTX = random(30, ANIM_W - 30);
      eyeTY = random(40, ANIM_H - 40);
      eyeSpd = (float)random(20, 60) / 10.0f;
      eyeMoving = true;
      eyeNextMove = now + 1500;
    }
  }
}

// ── Renderiza um frame da animação atual ─────────────────────
static void animFrame(uint32_t now) {
  if (!animSpr)
    return;

  animSpr->fillSprite(TFT_BLACK);

  switch (animIndex) {

  // 1: LOGO R4BB1T (Splash) - Não é animado via sprite
  case 1: break;

  // 2: MATRIX RAIN
  case 2: {
    for (int i = 0; i < 13; i++) {
      if (now - matCols[i].lastUpdate > matCols[i].speed) {
        matCols[i].y += 10;
        matCols[i].lastUpdate = now;
      }
      int x = i * 10;
      int y = matCols[i].y;
      if (y >= -10 && y < ANIM_H) {
        animSpr->setCursor(x, y);
        animSpr->setTextColor(0xBE76);
        animSpr->print((char)random(33, 126));
      }
      for (int j = 1; j < 10; j++) {
        int tY = y - j * 10;
        if (tY >= -10 && tY < ANIM_H) {
          int gv = 255 - j * 25;
          if (gv < 40)
            gv = 40;
          animSpr->setCursor(x, tY);
          animSpr->setTextColor(animSpr->color565(0, gv, 0));
          animSpr->print((char)random(33, 126));
        }
      }
      if (matCols[i].y > ANIM_H + 60)
        matInitCol(i);
    }
    break;
  }

  // 3: CUBO 3D ROTATIVO
  case 3: {
    float cube[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
    int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                        {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    float rX = now / 1000.0f, rY = now / 1300.0f, rZ = now / 1700.0f;
    int px[8], py[8];
    for (int i = 0; i < 8; i++) {
      float x = cube[i][0], y = cube[i][1], z = cube[i][2];
      float ty = y * cos(rX) - z * sin(rX), tz = y * sin(rX) + z * cos(rX);
      y = ty;
      z = tz;
      float tx = x * cos(rY) + z * sin(rY);
      tz = -x * sin(rY) + z * cos(rY);
      x = tx;
      z = tz;
      tx = x * cos(rZ) - y * sin(rZ);
      ty = x * sin(rZ) + y * cos(rZ);
      x = tx;
      y = ty;
      float p = 3.0f / (3.0f - z);
      px[i] = ANIM_CX + (int)(x * p * 32);
      py[i] = ANIM_CY + (int)(y * p * 32);
    }
    for (int i = 0; i < 12; i++)
      animSpr->drawLine(px[edges[i][0]], py[edges[i][0]], px[edges[i][1]],
                        py[edges[i][1]], pCol(5, i * 15));
    break;
  }

  // 4: PLASMA FLUINDO
  case 4: {
    uint32_t t = now + 50000;
    for (int x = 0; x < ANIM_W; x += 10) {
      for (int y = 0; y < ANIM_H; y += 10) {
        float v = aSin(x / 16.0f + t / 800.0f) + aSin((y + t / 10.0f) / 20.0f) +
                  aSin((x + y + t / 15.0f) / 30.0f);
        uint8_t spd = (uint8_t)(v * 4 + 10);
        animSpr->fillRect(x, y, 10, 10, pCol(spd, x / 2 + y / 2));
      }
    }
    break;
  }

  // 5: TESSERACT 4D
  case 5: {
    float nd[16][4] = {
        {-1, -1, -1, -1}, {1, -1, -1, -1}, {1, 1, -1, -1}, {-1, 1, -1, -1},
        {-1, -1, 1, -1},  {1, -1, 1, -1},  {1, 1, 1, -1},  {-1, 1, 1, -1},
        {-1, -1, -1, 1},  {1, -1, -1, 1},  {1, 1, -1, 1},  {-1, 1, -1, 1},
        {-1, -1, 1, 1},   {1, -1, 1, 1},   {1, 1, 1, 1},   {-1, 1, 1, 1}};
    int ed[32][2] = {{0, 1},   {1, 2},   {2, 3},   {3, 0},  {4, 5},   {5, 6},
                     {6, 7},   {7, 4},   {0, 4},   {1, 5},  {2, 6},   {3, 7},
                     {8, 9},   {9, 10},  {10, 11}, {11, 8}, {12, 13}, {13, 14},
                     {14, 15}, {15, 12}, {8, 12},  {9, 13}, {10, 14}, {11, 15},
                     {0, 8},   {1, 9},   {2, 10},  {3, 11}, {4, 12},  {5, 13},
                     {6, 14},  {7, 15}};
    float r = now / 1000.0f;
    int px[16], py[16];
    for (int i = 0; i < 16; i++) {
      float x = nd[i][0], y = nd[i][1], z = nd[i][2], w = nd[i][3];
      float tw = w * cos(r) - x * sin(r);
      float tx = w * sin(r) + x * cos(r);
      w = tw;
      x = tx;
      float ty = y * cos(r * 0.8f) - z * sin(r * 0.8f);
      float tz = y * sin(r * 0.8f) + z * cos(r * 0.8f);
      y = ty;
      z = tz;
      float p = 4.0f / (4.0f - w), p2 = 3.0f / (3.0f - z);
      px[i] = ANIM_CX + (int)(x * p * p2 * 28);
      py[i] = ANIM_CY + (int)(y * p * p2 * 28);
    }
    for (int i = 0; i < 32; i++)
      animSpr->drawLine(px[ed[i][0]], py[ed[i][0]], px[ed[i][1]], py[ed[i][1]],
                        pCol(8, i * 4));
    break;
  }

  // 6: CORREDOR (tuneel infinito)
  case 6: {
    float spd = now / 250.0f;
    const int NS = 12;
    float spacing = 1.0f;
    float maxZ = NS * spacing;
    struct Seg {
      int x1, y1, x2, y2, sz;
      uint16_t col, drk;
    } s[NS];
    float off = fmod(spd, spacing);
    for (int i = 0; i < NS; i++) {
      float z = maxZ - i * spacing - off;
      if (z <= 0.1f)
        z = 0.1f;
      s[i].sz = (int)(140.0f / z);
      int cx2 = ANIM_CX + (int)(0.6f * (z * z) * 1.0f);
      s[i].x1 = cx2 - s[i].sz / 2;
      s[i].y1 = ANIM_CY - s[i].sz / 2;
      s[i].x2 = cx2 + s[i].sz / 2;
      s[i].y2 = ANIM_CY + s[i].sz / 2;
      uint16_t base = pCol(15, (int)(spd * 10 + i * 20));
      uint8_t r2 = (base >> 11) << 3, g2 = ((base >> 5) & 0x3F) << 2,
              b2 = (base & 0x1F) << 3;
      s[i].col = animSpr->color565(r2, g2, b2);
      s[i].drk = animSpr->color565(r2 / 5, g2 / 4, b2 / 3);
    }
    for (int i = 0; i < NS - 1; i++) {
      if (s[i].sz < 2 || s[i + 1].sz > 500)
        continue;
      animSpr->fillTriangle(s[i + 1].x1, s[i + 1].y1, s[i + 1].x1, s[i + 1].y2,
                            s[i].x1, s[i].y2, s[i].drk);
      animSpr->fillTriangle(s[i + 1].x1, s[i + 1].y1, s[i + 1].x1, s[i + 1].y2,
                            s[i].x1, s[i].y1, s[i].drk);
      animSpr->fillTriangle(s[i + 1].x2, s[i + 1].y1, s[i + 1].x2, s[i + 1].y2,
                            s[i].x2, s[i].y2, s[i].drk);
      animSpr->fillTriangle(s[i + 1].x2, s[i + 1].y1, s[i + 1].x2, s[i + 1].y2,
                            s[i].x2, s[i].y1, s[i].drk);
      animSpr->fillTriangle(s[i + 1].x1, s[i + 1].y1, s[i + 1].x2, s[i + 1].y1,
                            s[i].x2, s[i].y1, s[i].drk);
      animSpr->fillTriangle(s[i + 1].x1, s[i + 1].y1, s[i + 1].x2, s[i + 1].y1,
                            s[i].x1, s[i].y1, s[i].drk);
      animSpr->fillTriangle(s[i + 1].x1, s[i + 1].y2, s[i + 1].x2, s[i + 1].y2,
                            s[i].x2, s[i].y2, s[i].drk);
      animSpr->fillTriangle(s[i + 1].x1, s[i + 1].y2, s[i + 1].x2, s[i + 1].y2,
                            s[i].x1, s[i].y2, s[i].drk);
      animSpr->drawLine(s[i + 1].x1, s[i + 1].y1, s[i].x1, s[i].y1, s[i].col);
      animSpr->drawLine(s[i + 1].x1, s[i + 1].y2, s[i].x1, s[i].y2, s[i].col);
      animSpr->drawLine(s[i + 1].x2, s[i + 1].y1, s[i].x2, s[i].y1, s[i].col);
      animSpr->drawLine(s[i + 1].x2, s[i + 1].y2, s[i].x2, s[i].y2, s[i].col);
      animSpr->drawRect(s[i].x1, s[i].y1, s[i].sz, s[i].sz, s[i].col);
    }
    animSpr->drawRect(s[NS - 1].x1, s[NS - 1].y1, s[NS - 1].sz, s[NS - 1].sz,
                      s[NS - 1].col);
    break;
  }

  // 7: ONDA GRADE
  case 7: {
    for (int x = 0; x < ANIM_W; x += 12) {
      for (int y = 0; y < ANIM_H; y += 12) {
        float v = aSin(x + now / 16.0f) + aSin(y + now / 12.0f);
        uint8_t spd = (uint8_t)(v / 8 + 5);
        animSpr->fillRect(x, y, 12, 12, pCol(spd, x + y));
      }
    }
    break;
  }

  // 8: ANEIS PULSANTES
  case 8: {
    for (int i = 0; i < 8; i++) {
      int br = (aSin(now / 8.0f) + 150) * 30 / 127;
      animSpr->drawCircle(ANIM_CX, ANIM_CY, br + i * 8, pCol(10, i * 15));
    }
    break;
  }

  // 9: QUADRADOS EXPANSIVOS
  case 9: {
    for (int i = 0; i < 10; i++) {
      int sz = (now / 10 + i * 20) % 160;
      animSpr->drawRect(ANIM_CX - sz / 2, ANIM_CY - sz / 2, sz, sz,
                        pCol(5, i * 20));
    }
    break;
  }

  // 10: OLHO MAGENTA
  case 10: {
    int irX = ANIM_CX + (int)((eyeX - ANIM_CX) * 0.35f);
    int irY = ANIM_CY + (int)((eyeY - ANIM_CY) * 0.35f);
    int puX = ANIM_CX + (int)((eyeX - ANIM_CX) * 0.6f);
    int puY = ANIM_CY + (int)((eyeY - ANIM_CY) * 0.6f);

    animSpr->fillCircle(ANIM_CX, ANIM_CY, 55, 0xF81F); // magenta
    animSpr->fillCircle(irX, irY, 38, 0xFFFF);
    animSpr->fillCircle(puX, puY, 28, 0xF81F);
    animSpr->fillCircle(puX, puY, 14, TFT_BLACK);
    animSpr->fillCircle(puX - 6, puY - 6, 4, 0xFFFF); // brilho
    eyePhysics(now);
    break;
  }
  }

  animSpr->pushSprite(0, 0);
}

// ── Inicializa ou desaloca buffers dependendo da animação ───────────
static void initCurrentAnim() {
  if (animIndex == 1) {
    if (animSpr) {
      animSpr->deleteSprite();
      delete animSpr;
      animSpr = nullptr;
    }
    tft.fillScreen(TFT_BLACK);
    extern void displaySplash(unsigned long delayMs);
    displaySplash(0);
  } else {
    animInitLUT();
    if (!animSpr) {
      animSpr = new TFT_eSprite(&tft);
      animSpr->createSprite(ANIM_W, ANIM_H);
    }
    if (animIndex == 2) {
      for (int i = 0; i < 13; i++) matInitCol(i);
    }
    eyeX = ANIM_CX;
    eyeY = ANIM_CY;
    eyeTX = ANIM_CX;
    eyeTY = ANIM_CY;
    eyeNextMove = millis() + 800;
    eyeMoving = false;
  }
}

// ── Iniciar o Screensaver de fora (idle na tela inicial) ───────────
static bool animStartedFromIdle = false;

void startScreensaver(bool fromIdle) {
  animStartedFromIdle = fromIdle;
  estadoAtual = TELA_SCREENSAVER_TEST; // reaproveita o estado
  
  if (fromIdle) {
    animIndex = prefs.getInt("screensaver", 1);
    if (animIndex == 0 || animIndex >= ANIM_COUNT) animIndex = 1;
  }

  animRunning = true;
  initCurrentAnim();
  
  // Escurece a tela apenas se for a partir do repouso
  if (fromIdle) {
    blDim();
  }
}

// ── Handler da tela de teste ─────────────────────────────────
void handleScreensaverTest() {
  if (!animRunning) {
    // Modo seleção
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        int old = animIndex;
        animIndex = (animIndex - 1 + ANIM_COUNT) % ANIM_COUNT;
        lastDebounceTime = millis();
        if (animIndex >= scrScroll && animIndex < scrScroll + 6 && old >= scrScroll && old < scrScroll + 6) {
          int savedIdx = prefs.getInt("screensaver", 1);
          String labelOld = String(getAnimName(old));
          if (old == savedIdx && old != 0) labelOld += lang->cfg_sv_ativo;
          drawMenuItem(0, 16 + (old - scrScroll) * 19, 125, 19, labelOld.c_str(), false);
          
          String labelNew = String(getAnimName(animIndex));
          if (animIndex == savedIdx && animIndex != 0) labelNew += lang->cfg_sv_ativo;
          drawMenuItem(0, 16 + (animIndex - scrScroll) * 19, 125, 19, labelNew.c_str(), true);
        } else {
          displayScreensaverTest();
        }
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        int old = animIndex;
        animIndex = (animIndex + 1) % ANIM_COUNT;
        lastDebounceTime = millis();
        if (animIndex >= scrScroll && animIndex < scrScroll + 6 && old >= scrScroll && old < scrScroll + 6) {
          int savedIdx = prefs.getInt("screensaver", 1);
          String labelOld = String(getAnimName(old));
          if (old == savedIdx && old != 0) labelOld += lang->cfg_sv_ativo;
          drawMenuItem(0, 16 + (old - scrScroll) * 19, 125, 19, labelOld.c_str(), false);
          
          String labelNew = String(getAnimName(animIndex));
          if (animIndex == savedIdx && animIndex != 0) labelNew += lang->cfg_sv_ativo;
          drawMenuItem(0, 16 + (animIndex - scrScroll) * 19, 125, 19, labelNew.c_str(), true);
        } else {
          displayScreensaverTest();
        }
      }
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();

        // Se for a primeira opção ("<- VOLTAR")
        if (animIndex == 0) {
          estadoAtual = MENU_CONFIGURACOES;
          displayConfiguracoes();
          return;
        }

        // Salva a escolha do screensaver
        prefs.putInt("screensaver", animIndex);
        startScreensaver(false);
      }
    }
  } else {
    // Modo animação rodando — sem debounce rígido para fluidez dos botões
    uint32_t now = millis();
    animFrame(now);

    // Verifica botões para trocar/sair
    if ((now - lastDebounceTime) > 150) {
      if (digitalRead(BUTTON_SELECT) == LOW || 
         (animStartedFromIdle && (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW))) {
        
        lastDebounceTime = now;
        animRunning = false;

        if (animSpr) {
          animSpr->deleteSprite();
          delete animSpr;
          animSpr = nullptr;
        }

        // Restaura o brilho original apenas se estava escurecido
        if (animStartedFromIdle) {
          blRestore();
        }

        if (animStartedFromIdle) {
          extern void displayMenuInicial();
          estadoAtual = MENU_INICIAL;
          displayMenuInicial();
        } else {
          displayScreensaverTest();
        }
      } else if (!animStartedFromIdle) {
        if (digitalRead(BUTTON_LEFT) == LOW) {
          lastDebounceTime = now;
          // Pula a opção "VOLTAR" (index 0) durante a execução
          animIndex = animIndex - 1;
          if (animIndex <= 0) animIndex = ANIM_COUNT - 1;
          
          initCurrentAnim();
        }
        if (digitalRead(BUTTON_RIGHT) == LOW) {
          lastDebounceTime = now;
          // Pula a opção "VOLTAR" (index 0) durante a execução
          animIndex = animIndex + 1;
          if (animIndex >= ANIM_COUNT) animIndex = 1;
          
          initCurrentAnim();
        }
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  HARD RESET — redesign Cyber Edition
// ═══════════════════════════════════════════════
// (hrStep e hrSel declarados no topo do arquivo)
// ── Botão estilizado para hard reset ──
static void drawHRButton(int x, int y, int w, int h, const char* lbl, bool danger, bool active) {
  uint16_t borderCol = active ? (danger ? C_RED : C_GOLD) : C_GREY;
  uint16_t bgCol     = active ? (danger ? 0x2000 : C_GOLD_SEL) : C_BG;
  uint16_t txtCol    = active ? (danger ? C_RED : C_GOLD) : C_GREY;
  tft.fillRoundRect(x, y, w, h, 3, bgCol);
  tft.drawRoundRect(x, y, w, h, 3, borderCol);
  tft.setTextSize(1);
  tft.setTextColor(txtCol);
  int lw = (int)strlen(lbl) * 6;
  tft.setCursor(x + (w - lw) / 2, y + (h - 8) / 2);
  tft.print(lbl);
}

void displayHardReset() {
  tft.fillScreen(C_BG);
  drawHeader(lang->hr_hdr_reset, true);

  if (hrStep == 0) {
    // ── Passo 1: Aviso inicial ──
    // Ícone de aviso (triângulo com !) em vermelho
    const int cx = 64, ty = 26;
    int tri[3][2] = {{cx, ty}, {cx - 12, ty + 20}, {cx + 12, ty + 20}};
    tft.fillTriangle(tri[0][0], tri[0][1], tri[1][0], tri[1][1], tri[2][0], tri[2][1], 0x2000);
    tft.drawTriangle(tri[0][0], tri[0][1], tri[1][0], tri[1][1], tri[2][0], tri[2][1], C_RED);
    tft.setTextColor(C_RED);
    tft.setTextSize(2);
    tft.setCursor(cx - 4, ty + 6);
    tft.print("!");

    // Linha separadora
    drawSeparator(51, C_GREY);

    // Textos das linhas (curtos para não quebrar)
    tft.setTextSize(1);
    tft.setTextColor(C_WHITE);
    tft.setCursor(4, 56);
    tft.print(lang->hr_conf1_msg1);
    tft.setCursor(4, 70);
    tft.print(lang->hr_conf1_msg2);
    tft.setCursor(4, 82);
    tft.print(lang->hr_conf1_msg3);

    // Separador rodapé
    drawSeparator(102, C_GREY);

    // Botões — hrSel determina qual está ativo
    drawHRButton(3,  108, 55, 18, lang->hr_btn_cancelar, false, hrSel == 0);
    drawHRButton(66, 108, 58, 18, lang->hr_btn_proximo,  false, hrSel == 1);

    // Dica de navegação — extremidades da tela
    tft.setTextSize(1);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(5, 140);
    tft.print("<");
    tft.setCursor(116, 140);
    tft.print(">");

  } else if (hrStep == 1) {
    // ── Passo 2: Confirmação final ──
    // Ícone DANGER: banner vermelho com texto do idioma
    tft.fillRoundRect(4, 22, 120, 28, 4, 0x2000);
    tft.drawRoundRect(4, 22, 120, 28, 4, C_RED);
    tft.setTextSize(1);
    tft.setTextColor(C_RED);
    // Linha 1 do banner: conf2_msg1 (ex: "Tem certeza?" / "Are you sure?")
    int dlw = (int)strlen(lang->hr_conf2_msg1) * 6;
    tft.setCursor((128 - dlw) / 2, 28);
    tft.print(lang->hr_conf2_msg1);
    // Linha 2 do banner: conf2_msg2 (ex: "Acao IRREVERSIVEL." / "NO going back.")
    int dlw2 = (int)strlen(lang->hr_conf2_msg2) * 6;
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor((128 - dlw2) / 2, 38);
    tft.print(lang->hr_conf2_msg2);

    // Linha separadora na mesma altura que o passo 1
    drawSeparator(51, C_GREY);

    // Textos das linhas idênticos ao passo 1
    tft.setTextSize(1);
    tft.setTextColor(C_WHITE);
    tft.setCursor(4, 56);
    tft.print(lang->hr_conf1_msg1);
    tft.setCursor(4, 70);
    tft.print(lang->hr_conf1_msg2);
    tft.setCursor(4, 82);
    tft.print(lang->hr_conf1_msg3);

    // Separador rodapé na mesma altura que o passo 1
    drawSeparator(102, C_GREY);

    // Botões na mesma altura que o passo 1
    drawHRButton(3,  108, 55, 18, lang->hr_btn_cancelar, false, hrSel == 0);
    drawHRButton(66, 108, 58, 18, lang->hr_btn_confirmar, true,  hrSel == 1);

    // Dica de navegação — extremidades da tela
    tft.setTextSize(1);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(5, 140);
    tft.print("<");
    tft.setCursor(116, 140);
    tft.print(">");

  } else if (hrStep == 2) {
    // ── Passo 3: Executando ──
    // Spinner / barra de progresso
    const int BX = 14, BY = 80, BW = 100, BH = 8;
    tft.drawRect(BX - 1, BY - 1, BW + 2, BH + 2, C_GOLD);
    tft.fillRect(BX, BY, BW, BH, C_GREY_DARK);
    // Animacao simples — barra cheia
    tft.fillRect(BX, BY, BW, BH, C_RED);

    tft.setTextSize(1);
    tft.setTextColor(C_RED);
    int lw = (int)strlen(lang->hr_msg_apagando) * 6;
    tft.setCursor((128 - lw) / 2, 60);
    tft.print(lang->hr_msg_apagando);

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(24, 98);
    tft.print("Aguarde...\0");
  }
}

void handleHardReset() {
  if (hrStep == 2) return;

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (hrStep == 0) {
      // ─ Navegar entre os botões ─
      if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW) {
        lastDebounceTime = millis();
        hrSel = (hrSel == 0) ? 1 : 0;
        drawHRButton(3,  108, 55, 18, lang->hr_btn_cancelar, false, hrSel == 0);
        drawHRButton(66, 108, 58, 18, lang->hr_btn_proximo,  false, hrSel == 1);
      }
      // ─ Confirmar seleção ─
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        if (hrSel == 0) {
          hrStep = 0;
          hrSel  = 0;
          estadoAtual = MENU_CONFIGURACOES;
          displayConfiguracoes();
        } else {
          hrStep = 1;
          hrSel  = 0; // volta ao cancelar na tela de confirmação
          displayHardReset();
        }
      }
    } else if (hrStep == 1) {
      // ─ Navegar entre os botões ─
      if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW) {
        lastDebounceTime = millis();
        hrSel = (hrSel == 0) ? 1 : 0;
        drawHRButton(3,  108, 55, 18, lang->hr_btn_cancelar,  false, hrSel == 0);
        drawHRButton(66, 108, 58, 18, lang->hr_btn_confirmar, true,  hrSel == 1);
      }
      // ─ Confirmar seleção ─
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        if (hrSel == 0) {
          hrStep = 0;
          hrSel  = 0;
          displayHardReset();
        } else {
          hrStep = 2;
          displayHardReset();

          // Limpa arquivos
          File f = SPIFFS.open("/rf_signals.txt", FILE_WRITE);
          if (f) f.close();
          f = SPIFFS.open("/credenciais.txt", FILE_WRITE);
          if (f) f.close();

          // Limpa EEPROM/NVRAM
          prefs.clear();

          delay(1000);
          ESP.restart();
        }
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  PRIMEIRO BOOT (SELEÇÃO DE IDIOMA)
// ═══════════════════════════════════════════════
static int fbLangId = 0;

void displayPrimeiroBoot() {
  tft.fillScreen(C_BG);
  drawHeader("R4BB1T FHC", false);

  setLanguage(fbLangId);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(5, 25);
  tft.print(lang->fb_sel_idioma);

  drawMenuItem(0, 50, SCR_W, 20, "Portugues (PT-BR)", fbLangId == 0);
  drawMenuItem(0, 75, SCR_W, 20, "English (EN)", fbLangId == 1);

  drawFooter();
}

void handlePrimeiroBoot() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      fbLangId = !fbLangId;
      displayPrimeiroBoot();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      setLanguage(fbLangId);
      prefs.putInt("idioma", fbLangId);
      
      estadoAtual = TELA_BEM_VINDO;
      displayBemVindo();
    }
  }
}

// ═══════════════════════════════════════════════
//  TELA BEM VINDO — animação via screensaver
// ═══════════════════════════════════════════════
static uint32_t startWelcome = 0;
static TFT_eSprite *wvSpr = nullptr; // sprite para a animacao
static uint32_t wvLastUpdate = 0;

// Splash screen do BEM VINDO:
// 1) Logo R4BB1T pulsante no centro
// 2) Texto WELCOME piscando
// 3) Aneis se expandindo (mesma técnica do screensaver)
static void wvDrawFrame(uint32_t now) {
  if (!wvSpr) return;
  wvSpr->fillSprite(TFT_BLACK);

  // ── Aneis pulsantes de fundo ──
  for (int i = 0; i < 6; i++) {
    int br = (int)((animSinLUT[((int)(now / 8.0f) + i * 42) & 0xFF] + 127)) * 25 / 255;
    int radius = br + i * 12 + 4;
    // cor ciclante dourada
    uint8_t rv = (uint8_t)((animSinLUT[((int)(now / 6) + i * 30) & 0xFF] + 127));
    uint8_t gv = (uint8_t)((animSinLUT[((int)(now / 6) + i * 30 + 85) & 0xFF] + 127) / 4);
    uint8_t bv = 0;
    uint16_t col = wvSpr->color565(rv, gv, bv);
    wvSpr->drawCircle(ANIM_CX, ANIM_CY, radius, col);
  }

  // ── Logo R4BB1T pixelart central ──
  const int SCALE = 2, COLS = 5, ROWS = 7, GAP = 2;
  const int letterW = COLS * SCALE;
  const int totalW  = 10 * letterW + 9 * GAP;
  int x0 = (ANIM_W - totalW) / 2;
  int yLogo = ANIM_CY - ROWS * SCALE;
  // pulso de brilho
  uint8_t pulse = (uint8_t)((animSinLUT[((int)(now / 4)) & 0xFF] + 127));
  uint16_t logoCol = wvSpr->color565(pulse, pulse / 4, 0);
  uint16_t shadowCol = wvSpr->color565(pulse / 4, 0, 0);

  static const uint8_t PROGMEM wv_glyphs[10][7] = {
    {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001}, // R
    {0b10001,0b10001,0b10001,0b11111,0b00001,0b00001,0b00001}, // 4
    {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}, // B
    {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}, // B
    {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110}, // 1
    {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100}, // T
    {0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b00000}, // space
    {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000}, // F
    {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}, // H
    {0b01111,0b10000,0b10000,0b10000,0b10000,0b10000,0b01111}, // C
  };

  for (int li = 0; li < 10; li++) {
    int lx = x0 + li * (letterW + GAP);
    for (int row = 0; row < ROWS; row++) {
      uint8_t bits = wv_glyphs[li][row];
      for (int col = 0; col < COLS; col++) {
        if (!((bits >> (4 - col)) & 1)) continue;
        int px = lx + col * SCALE;
        int py = yLogo + row * SCALE;
        wvSpr->fillRect(px + 1, py + 1, SCALE, SCALE, shadowCol);
        wvSpr->fillRect(px, py, SCALE, SCALE, logoCol);
      }
    }
  }

  // ── Texto BEM-VINDO piscante ──
  bool blink = ((now / 500) % 2) == 0;
  if (blink) {
    wvSpr->setTextSize(1);
    wvSpr->setTextColor(C_GREEN);
    const char* txt = lang->fb_bemvindo;
    int tw = (int)strlen(txt) * 6;
    wvSpr->setCursor((ANIM_W - tw) / 2, ANIM_CY + 18);
    wvSpr->print(txt);
  }

  // ── Barra de progresso ──
  uint32_t elapsed = now - startWelcome;
  if (elapsed > 3500) elapsed = 3500;
  const int BX = 14, BY = ANIM_CY + 36, BW = 100, BH = 4;
  wvSpr->fillRect(BX, BY, BW, BH, 0x18C3);
  int fill = (int)((long)BW * elapsed / 3500);
  if (fill > 0) wvSpr->fillRect(BX, BY, fill, BH, C_GOLD);
  wvSpr->drawRect(BX - 1, BY - 1, BW + 2, BH + 2, C_GOLD_DIM);

  wvSpr->pushSprite(0, 0);
}

void displayBemVindo() {
  animInitLUT(); // garante que a LUT está pronta

  if (!wvSpr) {
    wvSpr = new TFT_eSprite(&tft);
    wvSpr->createSprite(ANIM_W, ANIM_H);
  }
  startWelcome = millis();
  wvLastUpdate = 0;
}

void handleBemVindo() {
  uint32_t now = millis();

  // Renderiza a cada ~33ms (30fps)
  if (now - wvLastUpdate >= 33) {
    wvLastUpdate = now;
    wvDrawFrame(now);
  }

  // Dura 3.5 segundos
  if (now - startWelcome > 3500) {
    if (wvSpr) {
      wvSpr->deleteSprite();
      delete wvSpr;
      wvSpr = nullptr;
    }
    extern void displayMenuInicial();
    estadoAtual = MENU_INICIAL;
    displayMenuInicial();
  }
}
