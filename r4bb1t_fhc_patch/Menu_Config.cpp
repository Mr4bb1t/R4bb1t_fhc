#include "Menu_Config.h"
#include "Battery.h"
#include "Config.h"
#include "Globals.h"
#include "HWProbe.h"
#include "Menu_Main.h"
#include "UI.h"

#include <SPI.h>
#include <WiFi.h>
#include <esp_chip_info.h>
#include <esp_wifi.h>

#define SCR_W 128
#define SCR_H 160

static const char *configItems[] = {"Voltar", "Sobre", "Mudar MAC", "Brilho", "Modo Menu"};
static const int NUM_CONFIG_ITEMS = 5;
static int opcaoConfig = 0;


// ═══════════════════════════════════════════════
//  CONFIGURAÇÕES
// ═══════════════════════════════════════════════
void displayConfiguracoes() {
  tft.fillScreen(C_BG);
  drawHeader("SETTINGS", true);

  for (int i = 0; i < NUM_CONFIG_ITEMS; i++) {
    drawMenuItem(0, 16 + i * 20, 128, 19, configItems[i],
                 i == opcaoConfig);
  }

  drawFooter();
  batteryDraw();
}

void handleConfiguracoes() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      opcaoConfig = (opcaoConfig - 1 + NUM_CONFIG_ITEMS) % NUM_CONFIG_ITEMS;
      lastDebounceTime = millis(); displayConfiguracoes();
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      opcaoConfig = (opcaoConfig + 1) % NUM_CONFIG_ITEMS;
      lastDebounceTime = millis(); displayConfiguracoes();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      if      (opcaoConfig == 0) { opcaoConfig = 0; estadoAtual = MENU_INICIAL;       displayMenuInicial();   }
      else if (opcaoConfig == 1) { estadoAtual = TELA_SOBRE;          displaySobre();          }
      else if (opcaoConfig == 2) { estadoAtual = TELA_MAC_CHANGER;    displayMudarMAC();       }
      else if (opcaoConfig == 3) { estadoAtual = TELA_BRILHO;         displayBrilho();         }
      else if (opcaoConfig == 4) { estadoAtual = TELA_MODO_MENU;      displayModoMenu();       }
    }
  }
}

// ═══════════════════════════════════════════════
//  MODO MENU (Grade vs Lista)
// ═══════════════════════════════════════════════
static int modoMenuTemp = 0; // Para navegação temporária na tela

void displayModoMenu() {
  tft.fillScreen(C_BG);
  drawHeader("MODO MENU", true);

  // Layout dos dois blocos
  const int bw = 116;
  const int bh = 48;
  const int bx = (SCR_W - bw) / 2;
  const int by1 = 30;
  const int by2 = 86;

  for (int i = 0; i < 2; i++) {
    int y = (i == 0) ? by1 : by2;
    bool hover = (modoMenuTemp == i);
    bool active = (menuStyle == i);

    // Fundo do bloco
    tft.fillRect(bx, y, bw, bh, hover ? C_GOLD_SEL : C_BG);
    // Borda (dourada se sob o cursor, senão cinza)
    tft.drawRoundRect(bx, y, bw, bh, 4, hover ? C_GOLD : C_GREY);

    // Ícone da esquerda
    uint16_t iconColor = hover ? C_GOLD : C_WHITE;
    if (i == 0) {
      // Ícone Grade (4 quadradinhos)
      tft.drawRect(bx + 12, y + 12, 10, 10, iconColor);
      tft.drawRect(bx + 24, y + 12, 10, 10, iconColor);
      tft.drawRect(bx + 12, y + 26, 10, 10, iconColor);
      tft.drawRect(bx + 24, y + 26, 10, 10, iconColor);
    } else {
      // Ícone Lista (3 linhas com rounded corners)
      tft.drawRoundRect(bx + 12, y + 14, 22, 6, 2, iconColor);
      tft.drawRoundRect(bx + 12, y + 22, 22, 6, 2, iconColor);
      tft.drawRoundRect(bx + 12, y + 30, 22, 6, 2, iconColor);
    }

    // Texto Central
    tft.setTextSize(1);
    tft.setTextColor(hover ? C_GOLD : C_WHITE);
    tft.setCursor(bx + 46, y + 20);
    tft.print(i == 0 ? "BLOCO" : "LISTA");

    // Radio button na direita
    int rx = bx + bw - 16;
    int ry = y + bh / 2;
    tft.drawCircle(rx, ry, 6, hover ? C_GOLD : C_GREY);
    if (active) {
      tft.fillCircle(rx, ry, 3, hover ? C_GOLD : C_WHITE);
    }
  }

  drawFooter();
  batteryDraw();
}

void handleModoMenu() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      modoMenuTemp = (modoMenuTemp == 0) ? 1 : 0;
      lastDebounceTime = millis();
      displayModoMenu();
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      modoMenuTemp = (modoMenuTemp == 0) ? 1 : 0;
      lastDebounceTime = millis();
      displayModoMenu();
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

  // Aplica o brilho salvo
  blInit();
}

// ═══════════════════════════════════════════════
//  MUDAR MAC
// ═══════════════════════════════════════════════


void displayMudarMAC() {
  tft.fillScreen(C_BG);
  drawHeader("MAC CHANGER", true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 18); tft.print("MAC atual:");
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 28); tft.print(WiFi.macAddress());
  drawSeparator(38, C_GREY);

  if (macState == 0) {
    tft.setTextColor(C_GOLD);
    tft.setCursor(8, 60);
    tft.print("Gerar MAC aleatorio");
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(32, 76);
    tft.print("SEL = Gerar");
  } else if (macState == 1) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             macBuf[0], macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 48); tft.print("Novo MAC:");
    tft.setTextColor(C_GOLD);
    tft.setCursor(4, 60); tft.print(buf);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(28, 80); tft.print("SEL = Aplicar");
  } else if (macState == 2) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             macBuf[0], macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 48); tft.print("Novo MAC:");
    tft.setTextColor(C_GREEN);
    tft.setCursor(4, 60); tft.print(buf);
    tft.setCursor(20, 76); tft.print(">>> Aplicado! <<<");
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(32, 96); tft.print("SEL = Voltar");
  }

  batteryDraw();
}

void handleMudarMAC() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      if (macState == 0) {
        for (int i = 0; i < 6; i++) macBuf[i] = (uint8_t)(esp_random() & 0xFF);
        macBuf[0] = (macBuf[0] & 0xFE) | 0x02;
        macState = 1; displayMudarMAC();
      } else if (macState == 1) {
        WiFi.mode(WIFI_STA);
        esp_wifi_set_mac(WIFI_IF_STA, macBuf);
        prefs.putBytes("mac", macBuf, 6); // SALVA NA MEMÓRIA FIXA
        macState = 2; displayMudarMAC();
      } else if (macState == 2) {
        macState = 0; estadoAtual = MENU_CONFIGURACOES; displayConfiguracoes();
      }
    }
  }
}

// ═══════════════════════════════════════════════
//  SOBRE (carrossel de 4 páginas)
// ═══════════════════════════════════════════════
#define SOBRE_PAGES 4
static int sobrePage = 0;

// Indicadores de página (bolinhas) em dourado
static void sobreDots(int cur) {
  const int DOT_Y = SCR_H - 28;
  const int DOT_R = 3;
  const int GAP   = 10;
  int totalW  = SOBRE_PAGES * (DOT_R * 2) + (SOBRE_PAGES - 1) * (GAP - DOT_R * 2);
  int startX  = (SCR_W - totalW) / 2 + DOT_R;
  for (int i = 0; i < SOBRE_PAGES; i++) {
    int cx = startX + i * GAP;
    if (i == cur) tft.fillCircle(cx, DOT_Y, DOT_R, C_GOLD);
    else          tft.drawCircle(cx, DOT_Y, DOT_R, C_GREY);
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
  tft.setCursor(5,  SCR_H - 12); tft.print(cur > 0 ? "<" : "x");
  tft.setCursor(61, SCR_H - 12); tft.print("o");
  tft.setCursor(SCR_W - 11, SCR_H - 12); tft.print(cur < SOBRE_PAGES - 1 ? ">" : " ");
}

// Página 0: Sistema
static void displaySobre_p0() {
  sobreHeader("R4BB1T FHC");

  esp_chip_info_t chip;
  esp_chip_info(&chip);
  const char *model = "ESP32";
  if (chip.model == CHIP_ESP32S2) model = "ESP32-S2";
  else if (chip.model == CHIP_ESP32S3) model = "ESP32-S3";
  else if (chip.model == CHIP_ESP32C3) model = "ESP32-C3";
  else if (chip.model == CHIP_ESP32H2) model = "ESP32-H2";

  int y = 18;
  const int LH = 12;
  auto row = [&](const char *lbl, String val, uint16_t c = C_WHITE) {
    tft.setTextColor(C_GOLD_DIM); tft.setCursor(4, y);  tft.print(lbl);
    tft.setTextColor(c);          tft.setCursor(46, y); tft.print(val);
    y += LH;
  };

  row("Chip:",  model);
  row("Cores:", String(chip.cores));
  row("Rev:",   String(chip.revision));
  row("Flash:", String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB");
  row("Heap:",  String(ESP.getFreeHeap() / 1024) + " KB", C_GREEN);
  row("SDK:",   String(ESP.getSdkVersion()).substring(0, 10));
  row("FW:",    "v1.0.0", C_GOLD);

  tft.setTextColor(C_GOLD_DIM); tft.setCursor(4, y);     tft.print("MAC:");
  tft.setTextColor(C_WHITE);    tft.setCursor(4, y + 10); tft.print(WiFi.macAddress());

  sobreFooter(0);
}

// Página 1: NRF24L01
static void displaySobre_p1() {
  sobreHeader("NRF24L01");

  int y = 18;
  const int LH = 14;
  tft.setTextSize(1);
  auto row = [&](const char *lbl, const char *val, uint16_t c = C_WHITE) {
    tft.setTextColor(C_GOLD_DIM); tft.setCursor(4,  y); tft.print(lbl);
    tft.setTextColor(c);          tft.setCursor(52, y); tft.print(val);
    y += LH;
  };

  row("Status:", hwNRF24_ok ? "Conectado" : "Nao detectado",
                 hwNRF24_ok ? C_GREEN : C_RED);
  row("Bus:",   "HSPI", C_GOLD_DIM);
  row("CE:",    "GPIO 22");
  row("CSN:",   "GPIO 4");
  row("SCK:",   "GPIO 33");
  row("MISO:",  "GPIO 19");
  row("MOSI:",  "GPIO 13");

  tft.setTextColor(C_GREY); tft.setCursor(4, y); tft.print("VCC: 3.3V");
  sobreFooter(1);
}

// Página 2: CC1101
static void displaySobre_p2() {
  sobreHeader("CC1101");

  int y = 18;
  const int LH = 14;
  bool ok = hwCC1101_ok;
  auto row = [&](const char *lbl, const char *val, uint16_t c = C_WHITE) {
    tft.setTextColor(C_GOLD_DIM); tft.setCursor(4,  y); tft.print(lbl);
    tft.setTextColor(c);          tft.setCursor(52, y); tft.print(val);
    y += LH;
  };

  row("Status:", ok ? "Conectado" : "Nao encontrado", ok ? C_GREEN : C_RED);
  row("Freq:",   "433.92 MHz", C_GOLD);
  row("CS:",     "GPIO 25");
  row("GDO0:",   "GPIO 2");
  row("GDO2:",   "GPIO 32");
  row("Bus:",    "HSPI", C_GOLD_DIM);

  tft.setTextColor(C_GREY); tft.setCursor(4, y); tft.print("SCK:33 MISO:19 MOSI:13");
  sobreFooter(2);
}

// Página 3: Bateria
static void displaySobre_p3() {
  sobreHeader("BATERIA");

  int pct   = batteryPercent();
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
  if (fill > 0) tft.fillRect(BX, BY, fill, BH, col);
  tft.fillRect(BX + BW + 1, BY + 6, 4, BH - 12, col);

  // Label dentro da barra
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK);
  if (fill > 20) {
    static const char *lvl[] = {"CRITICO", "BAIXO", "MEDIO", "BOM", "CHEIO"};
    int li = pct < 10 ? 0 : pct < 25 ? 1 : pct < 50 ? 2 : pct < 80 ? 3 : 4;
    int lw = (int)strlen(lvl[li]) * 6;
    tft.setCursor(BX + (fill - lw) / 2, BY + 7); tft.print(lvl[li]);
  }

  // Detalhes
  int y = 102;
  const int LH = 13;
  tft.setTextColor(C_GOLD_DIM); tft.setCursor(4, y); tft.print("Tensao:");
  char vbuf[12]; dtostrf(vbat, 4, 2, vbuf);
  tft.setTextColor(col); tft.setCursor(54, y); tft.print(vbuf); tft.print(" V");
  y += LH;
  tft.setTextColor(C_GOLD_DIM); tft.setCursor(4, y); tft.print("ADC PIN:");
  tft.setTextColor(C_WHITE);    tft.setCursor(54, y); tft.print("GPIO 36");

  sobreFooter(3);
}

void displaySobre() {
  switch (sobrePage) {
    case 0: displaySobre_p0(); break;
    case 1: displaySobre_p1(); break;
    case 2: displaySobre_p2(); break;
    case 3: displaySobre_p3(); break;
    default: sobrePage = 0; displaySobre_p0(); break;
  }
}

void handleSobre() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      if (sobrePage < SOBRE_PAGES - 1) { sobrePage++; displaySobre(); }
    }
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      if (sobrePage > 0) { sobrePage--; displaySobre(); }
      else { sobrePage = 0; estadoAtual = MENU_CONFIGURACOES; displayConfiguracoes(); }
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      sobrePage = 0; estadoAtual = MENU_CONFIGURACOES; displayConfiguracoes();
    }
  }
}

// ═══════════════════════════════════════════════
//  BRILHO
// ═══════════════════════════════════════════════
#define BL_CHANNEL 0
#define BL_FREQ    5000
#define BL_RES     8
#define BL_STEPS   10
#define BL_MIN     15
#define BL_MAX     255

static int  brilhoAtual = BL_MAX;
static bool blIniciado  = false;

static void blInit() {
  if (!blIniciado) {
    ledcAttach(TFT_BL, BL_FREQ, BL_RES);
    brilhoAtual = prefs.getInt("brilho", BL_MAX);
    if (brilhoAtual < BL_MIN) brilhoAtual = BL_MIN;
    ledcWrite(TFT_BL, brilhoAtual);
    blIniciado = true;
  }
}
static void blSet(int v) { 
  blInit(); 
  brilhoAtual = v; 
  ledcWrite(TFT_BL, v); 
  prefs.putInt("brilho", v); // SALVA NA MEMÓRIA FIXA
}

void blOff() { blInit(); ledcWrite(TFT_BL, 0); }

void displayBrilho() {
  tft.fillScreen(C_BG);
  drawHeader("BRILHO", true);

  int pct = (int)((long)(brilhoAtual - BL_MIN) * 100 / (BL_MAX - BL_MIN));
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;

  // Percentual em dourado grande
  tft.setTextSize(3);
  char pctBuf[6];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
  int pctX = (SCR_W - (int)strlen(pctBuf) * 18) / 2;
  tft.setTextColor(C_GOLD);
  tft.setCursor(pctX, 40);
  tft.print(pctBuf);

  // Slider
  const int slX0 = 10;
  const int slX1 = SCR_W - 10;
  const int slY  = 88;
  const int slW  = slX1 - slX0;

  tft.drawFastHLine(slX0, slY, slW, C_GREY);
  int fillW = (int)((long)slW * (brilhoAtual - BL_MIN) / (BL_MAX - BL_MIN));
  if (fillW > 0) tft.drawFastHLine(slX0, slY, fillW, C_GOLD);
  int dotX = slX0 + fillW;
  tft.fillCircle(dotX, slY, 5, C_GOLD);
  tft.drawCircle(dotX, slY, 5, C_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(C_GREY);
  tft.setCursor(slX0, slY + 10); tft.print("min");
  tft.setCursor(slX1 - 12, slY + 10); tft.print("max");

  drawSeparator(110, C_GREY);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(10, 116); tft.print("< / >  ajusta brilho");
  tft.setCursor(10, 128); tft.print("  o    salva e volta");

  drawFooter();
  batteryDraw();
}

void handleBrilho() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    int step = (BL_MAX - BL_MIN) / BL_STEPS;
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      brilhoAtual += step;
      if (brilhoAtual > BL_MAX) brilhoAtual = BL_MAX;
      blSet(brilhoAtual); displayBrilho();
    }
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      brilhoAtual -= step;
      if (brilhoAtual < BL_MIN) brilhoAtual = BL_MIN;
      blSet(brilhoAtual); displayBrilho();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      estadoAtual = MENU_CONFIGURACOES; displayConfiguracoes();
    }
  }
}
