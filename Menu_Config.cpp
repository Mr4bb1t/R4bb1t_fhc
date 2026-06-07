#include "Menu_Config.h"
#include "Battery.h"
#include "Config.h"
#include "Globals.h"
#include "HWProbe.h"
#include "Menu_Main.h"
#include "UI.h"

#include <SPI.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_chip_info.h>
#include <esp_wifi.h>
#include <math.h>

#define SCR_W 128
#define SCR_H 160

static const char *configItems[] = {"Voltar", "Sobre",     "Mudar MAC",
                                    "Brilho", "Modo Menu", "Armazenamento"};
static const int NUM_CONFIG_ITEMS = 6;
static int opcaoConfig = 0;

// ═══════════════════════════════════════════════
//  CONFIGURAÇÕES
// ═══════════════════════════════════════════════
void displayConfiguracoes() {
  tft.fillScreen(C_BG);
  drawHeader("SETTINGS", true);

  for (int i = 0; i < NUM_CONFIG_ITEMS; i++) {
    drawMenuItem(0, 16 + i * 20, 128, 19, configItems[i], i == opcaoConfig);
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
      drawMenuItem(0, 16 + old * 20, 128, 19, configItems[old], false);
      drawMenuItem(0, 16 + opcaoConfig * 20, 128, 19, configItems[opcaoConfig], true);
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = opcaoConfig;
      opcaoConfig = (opcaoConfig + 1) % NUM_CONFIG_ITEMS;
      lastDebounceTime = millis();
      drawMenuItem(0, 16 + old * 20, 128, 19, configItems[old], false);
      drawMenuItem(0, 16 + opcaoConfig * 20, 128, 19, configItems[opcaoConfig], true);
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
  tft.print(i == 0 ? "BLOCO" : "LISTA");

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
  drawHeader("MODO MENU", true);

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
  tft.setCursor(4, 18);
  tft.print("MAC atual:");
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 28);
  tft.print(WiFi.macAddress());
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
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", macBuf[0],
             macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 48);
    tft.print("Novo MAC:");
    tft.setTextColor(C_GOLD);
    tft.setCursor(4, 60);
    tft.print(buf);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(28, 80);
    tft.print("SEL = Aplicar");
  } else if (macState == 2) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", macBuf[0],
             macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 48);
    tft.print("Novo MAC:");
    tft.setTextColor(C_GREEN);
    tft.setCursor(4, 60);
    tft.print(buf);
    tft.setCursor(20, 76);
    tft.print(">>> Aplicado! <<<");
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(32, 96);
    tft.print("SEL = Voltar");
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
#define SOBRE_PAGES 4
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

  row("Chip:", model);
  row("Cores:", String(chip.cores));
  row("Rev:", String(chip.revision));
  row("Flash:", String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB");
  row("Heap:", String(ESP.getFreeHeap() / 1024) + " KB", C_GREEN);
  row("SDK:", String(ESP.getSdkVersion()).substring(0, 10));
  row("FW:", "v1.0.0", C_GOLD);

  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, y);
  tft.print("MAC:");
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, y + 10);
  tft.print(WiFi.macAddress());

  sobreFooter(0);
}

// Página 1: NRF24L01
static void displaySobre_p1() {
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

  row("Status:", hwNRF24_ok ? "Conectado" : "Nao detectado",
      hwNRF24_ok ? C_GREEN : C_RED);
  row("Bus:", "HSPI", C_GOLD_DIM);
  row("CE:", "GPIO 22");
  row("CSN:", "GPIO 4");
  row("SCK:", "GPIO 33");
  row("MISO:", "GPIO 19");
  row("MOSI:", "GPIO 13");

  tft.setTextColor(C_GREY);
  tft.setCursor(4, y);
  tft.print("VCC: 3.3V");
  sobreFooter(1);
}

// Página 2: CC1101
static void displaySobre_p2() {
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

  row("Status:", ok ? "Conectado" : "Nao encontrado", ok ? C_GREEN : C_RED);
  row("Freq:", "433.92 MHz", C_GOLD);
  row("CS:", "GPIO 25");
  row("GDO0:", "GPIO 2");
  row("GDO2:", "GPIO 32");
  row("Bus:", "HSPI", C_GOLD_DIM);

  tft.setTextColor(C_GREY);
  tft.setCursor(4, y);
  tft.print("SCK:33 MISO:19 MOSI:13");
  sobreFooter(2);
}

// Página 3: Bateria
static void displaySobre_p3() {
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
    static const char *lvl[] = {"CRITICO", "BAIXO", "MEDIO", "BOM", "CHEIO"};
    int li = pct < 10 ? 0 : pct < 25 ? 1 : pct < 50 ? 2 : pct < 80 ? 3 : 4;
    int lw = (int)strlen(lvl[li]) * 6;
    tft.setCursor(BX + (fill - lw) / 2, BY + 7);
    tft.print(lvl[li]);
  }

  // Detalhes
  int y = 102;
  const int LH = 13;
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, y);
  tft.print("Tensao:");
  char vbuf[12];
  dtostrf(vbat, 4, 2, vbuf);
  tft.setTextColor(col);
  tft.setCursor(54, y);
  tft.print(vbuf);
  tft.print(" V");
  y += LH;
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, y);
  tft.print("ADC PIN:");
  tft.setTextColor(C_WHITE);
  tft.setCursor(54, y);
  tft.print("GPIO 36");

  sobreFooter(3);
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
    ledcAttach(TFT_BL, BL_FREQ, BL_RES);
    brilhoAtual = prefs.getInt("brilho", BL_MAX);
    if (brilhoAtual < BL_MIN)
      brilhoAtual = BL_MIN;
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

void blOff() {
  blInit();
  ledcWrite(TFT_BL, 0);
}

static void drawBrilhoSlider() {
  int pct = (int)((long)(brilhoAtual - BL_MIN) * 100 / (BL_MAX - BL_MIN));
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

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
  if (fillW > 0) tft.drawFastHLine(slX0, slY, fillW, C_GOLD);
  
  int dotX = slX0 + fillW;
  tft.fillCircle(dotX, slY, 5, C_GOLD);
  tft.drawCircle(dotX, slY, 5, C_WHITE);
}

void displayBrilho() {
  tft.fillScreen(C_BG);
  drawHeader("BRILHO", true);

  drawBrilhoSlider();

  const int slX0 = 10;
  const int slX1 = SCR_W - 10;
  const int slY = 88;
  tft.setTextSize(1);
  tft.setTextColor(C_GREY);
  tft.setCursor(slX0, slY + 10);
  tft.print("min");
  tft.setCursor(slX1 - 12, slY + 10);
  tft.print("max");

  drawSeparator(110, C_GREY);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(10, 116);
  tft.print("< / >  ajusta brilho");
  tft.setCursor(10, 128);
  tft.print("  o    salva e volta");

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
      blSet(brilhoAtual);
      drawBrilhoSlider();
    }
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      brilhoAtual -= step;
      if (brilhoAtual < BL_MIN) brilhoAtual = BL_MIN;
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
static int storageOpcao = 0; // 0=ARQUIVOS, 1=VOLTAR

// ── Estado: lista de arquivos ──
#define FILES_MAX 24
#define FILES_PER_PAGE 6
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
#define DONUT_CX 36      // centro X (metade esquerda da tela)
#define DONUT_CY 72      // centro Y
#define DONUT_ROUT 34    // raio externo
#define DONUT_RIN 21     // raio interno
#define FILES_PER_PAGE 6 // arquivos visíveis por vez na lista

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
        float angle = atan2f((float)dy, (float)dx) * 180.0f / (float)M_PI + 90.0f;
        if (angle < 0.0f) angle += 360.0f;
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
  tft.print("used");
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

  // [ARQUIVOS]
  bool selA = (storageOpcao == 0);
  tft.fillRoundRect(3, BTN_Y, BTN_W, BTN_H, 3, selA ? C_GOLD_SEL : C_BG);
  tft.drawRoundRect(3, BTN_Y, BTN_W, BTN_H, 3, selA ? C_GOLD : C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(selA ? C_GOLD : C_WHITE);
  tft.setCursor(8, BTN_Y + 5);
  tft.print("ARQUIVOS");

  // [VOLTAR]
  bool selV = (storageOpcao == 1);
  tft.fillRoundRect(70, BTN_Y, BTN_W, BTN_H, 3, selV ? C_GOLD_SEL : C_BG);
  tft.drawRoundRect(70, BTN_Y, BTN_W, BTN_H, 3, selV ? C_GOLD : C_GREY);
  tft.setTextColor(selV ? C_GOLD : C_WHITE);
  tft.setCursor(82, BTN_Y + 5);
  tft.print("VOLTAR");
}

// ── Tela principal: donut + legenda + botões ──
void displayArmazenamento() {
  tft.fillScreen(C_BG);
  drawHeader("STORAGE", true);

  // Coleta dados SPIFFS
  size_t sp_total = SPIFFS.totalBytes();
  size_t sp_used = SPIFFS.usedBytes();
  size_t sp_free = sp_total - sp_used;

  // Firmware na flash
  size_t fw_size = ESP.getSketchSize();
  size_t flash_tot = ESP.getFlashChipSize();
  if (flash_tot == 0)
    flash_tot = 4 * 1024 * 1024; // fallback 4MB

  // Percentuais sobre a flash total
  float fw_pct = (fw_size * 100.0f) / (float)flash_tot;
  float sp_pct = (sp_used * 100.0f) / (float)flash_tot;
  float fr_pct = 100.0f - fw_pct - sp_pct;
  if (fr_pct < 0.0f)
    fr_pct = 0.0f;

  // Ângulos dos arcos
  float a_fw = fw_pct * 3.6f;
  float a_sp = a_fw + sp_pct * 3.6f;
  // Restante (livre) vai de a_sp até 360°

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
  // Label central: % total usado (fw + spiffs)
  int used_pct = (int)(fw_pct + sp_pct);
  if (used_pct > 100)
    used_pct = 100;
  drawDonutCenter(DONUT_CX, DONUT_CY, used_pct);

  // ── Legenda (lado direito) ──
  char buf_fw[10], buf_sp[10], buf_fr[10], buf_ft[12];
  snprintf(buf_fw, sizeof(buf_fw), "%uKB", (unsigned)(fw_size / 1024));
  snprintf(buf_sp, sizeof(buf_sp), "%uKB", (unsigned)(sp_used / 1024));
  snprintf(buf_fr, sizeof(buf_fr), "%uKB", (unsigned)(sp_free / 1024));
  snprintf(buf_ft, sizeof(buf_ft), "%uMB tot",
           (unsigned)(flash_tot / (1024 * 1024)));

  int lx = DONUT_CX + DONUT_ROUT + 8;
  drawStorLegend(lx, 28, C_ORANGE, "FW", buf_fw);
  drawStorLegend(lx, 50, C_GOLD, "SPIF", buf_sp);
  drawStorLegend(lx, 72, C_GREY, "FREE", buf_fr);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(lx, 94);
  tft.print(buf_ft);

  // ── Botões de ação ──
  drawSeparator(114, C_GREY);

  updateArmazenamentoBotoes();

  // Dica de controles
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(14, 140);
  tft.print("< >  sel    o  ok");

  batteryDraw();
}

// ── Coleta arquivos do SPIFFS ──
static void spiffsCollect() {
  fileCount = 0;
  File root = SPIFFS.open("/");
  if (!root || !root.isDirectory()) return;
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
    tft.fillRect(0, y - 1, SCR_W, ROW_H + 1, C_BG);
    tft.drawFastHLine(4, y + ROW_H, SCR_W - 8, C_GREY_DARK);
  }

  if (i == 0) {
    tft.setTextSize(1);
    tft.setTextColor(sel ? C_GOLD : C_GREY);
    tft.setCursor(4, y + 3);
    tft.print("< VOLTAR");
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
    if (fileSizes[fi] < 1024) snprintf(sbuf, sizeof(sbuf), "%uB", (unsigned)fileSizes[fi]);
    else snprintf(sbuf, sizeof(sbuf), "%uKB", (unsigned)(fileSizes[fi] / 1024));
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

  for (int i = pageStart; i < totalItems && (i - pageStart) < FILES_PER_PAGE; i++) {
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
  tft.print("< > navegar   o abrir");
  batteryDraw();
}

// ── Tela 2: Viewer Text e Tela 3: Viewer BMP ──
static uint16_t bmp_r16(File &f) { return (uint16_t)f.read() | ((uint16_t)f.read() << 8); }
static uint32_t bmp_r32(File &f) { uint32_t lo = bmp_r16(f); return lo | ((uint32_t)bmp_r16(f) << 16); }

static void displayFileViewerBMP() {
  tft.fillScreen(C_BG);
  int fi = fileCursor - 1;
  String path = fileNames[fi];
  if (!path.startsWith("/")) path = "/" + path;
  File f = SPIFFS.open(path.c_str(), FILE_READ);
  if (!f) return;
  if (bmp_r16(f) != 0x4D42) { f.close(); return; }
  bmp_r32(f); bmp_r32(f);
  uint32_t dataOffset = bmp_r32(f);
  bmp_r32(f);
  int32_t bmpW = (int32_t)bmp_r32(f);
  int32_t bmpH = (int32_t)bmp_r32(f);
  bmp_r16(f);
  uint16_t bpp = bmp_r16(f);
  uint32_t comp = bmp_r32(f);
  if (bpp != 24 || comp != 0) { f.close(); return; }
  
  bool flipY = (bmpH > 0);
  if (bmpH < 0) bmpH = -bmpH;
  int16_t scrW = 128;
  int16_t scrH = 140;
  
  uint32_t sx = (uint32_t)scrW * 256 / (uint32_t)bmpW;
  uint32_t sy = (uint32_t)scrH * 256 / (uint32_t)bmpH;
  uint32_t sc = (sx < sy) ? sx : sy;
  if (sc > 256) sc = 256;
  
  int16_t dW = (int16_t)((uint32_t)bmpW * sc / 256);
  int16_t dH = (int16_t)((uint32_t)bmpH * sc / 256);
  if (dW < 1) dW = 1;
  if (dH < 1) dH = 1;
  int16_t ox = (scrW - dW) / 2;
  int16_t oy = (scrH - dH) / 2;
  
  size_t fbSize = (size_t)dW * (size_t)dH;
  uint16_t *fb = (uint16_t *)malloc(fbSize * sizeof(uint16_t));
  uint32_t rowBytes = ((uint32_t)(bmpW * 3 + 3) / 4) * 4;
  uint8_t  *row = (uint8_t *)malloc(rowBytes);
  if (!fb || !row) {
    if (fb) free(fb);
    if (row) free(row);
    f.close();
    return;
  }
  
  f.seek(dataOffset);
  for (int32_t fileRow = 0; fileRow < bmpH; fileRow++) {
    if (f.read(row, rowBytes) != (int)rowBytes) break;
    int32_t imgY = flipY ? (bmpH - 1 - fileRow) : fileRow;
    int16_t sY = (int16_t)((int32_t)imgY * dH / bmpH);
    if (sY < 0 || sY >= dH) continue;
    uint16_t *fbRow = &fb[(size_t)sY * (size_t)dW];
    for (int16_t x = 0; x < dW; x++) {
      int32_t sX = (int32_t)x * bmpW / dW;
      fbRow[x] = ((uint16_t)(row[sX * 3 + 2] & 0xF8) << 8) | ((uint16_t)(row[sX * 3 + 1] & 0xFC) << 3) | (row[sX * 3 + 0] >> 3);
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
  tft.setCursor(60, SCR_H - 10);
  tft.print("o=sair");
  batteryDraw();
}

static void viewerBuildLines() {
  viewTotalLines = 0;
  int i = 0;
  while (i < viewLen && viewTotalLines < VIEW_LINES_MAX) {
    viewLineOff[viewTotalLines] = i;
    int end = i;
    int col = 0;
    while (end < viewLen && viewBuf[end] != '\n' && col < VIEWER_LINE_W) { end++; col++; }
    viewLineLen[viewTotalLines] = end - i;
    viewTotalLines++;
    if (end < viewLen && viewBuf[end] == '\n') end++;
    i = end;
  }
}

static void displayFileViewerText() {
  tft.fillScreen(C_BG);
  int fi = fileCursor - 1;
  char htitle[14];
  const char *fname = fileNames[fi];
  if (fname[0] == '/') fname++;
  strncpy(htitle, fname, 13);
  htitle[13] = '\0';
  drawHeader(htitle, true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 17);
  char info[28];
  snprintf(info, sizeof(info), "%uB  %d linhas", (unsigned)fileSizes[fi], viewTotalLines);
  tft.print(info);
  drawSeparator(26, C_GREY_DARK);

  int y = 29;
  const int LH = 12;
  for (int l = viewScroll; l < viewTotalLines && (l - viewScroll) < VIEWER_LINES; l++) {
    char line[VIEWER_LINE_W + 1];
    int ll = viewLineLen[l];
    if (ll > VIEWER_LINE_W) ll = VIEWER_LINE_W;
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
    if (thumbH < 4) thumbH = 4;
    int thumbY = SBY0 + (SBH - thumbH) * viewScroll / (viewTotalLines - VIEWER_LINES);
    tft.drawFastVLine(SBX, thumbY, thumbH, C_GOLD);
  }

  drawSeparator(SCR_H - 14, C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  char pg[14];
  snprintf(pg, sizeof(pg), "L%d/%d", viewScroll + 1, viewTotalLines);
  tft.setCursor(4, SCR_H - 10);
  tft.print(pg);
  tft.setCursor(60, SCR_H - 10);
  tft.print("o=sair");
  batteryDraw();
}

static void openFileForView(int fi) {
  const char* fname = fileNames[fi];
  int len = strlen(fname);
  if (len > 4 && strcasecmp(fname + len - 4, ".bmp") == 0) {
    storageState = 3;
    displayFileViewerBMP();
    return;
  }
  
  viewLen = 0;
  viewScroll = 0;
  String path = fname;
  if (!path.startsWith("/")) path = "/" + path;
  File f = SPIFFS.open(path.c_str(), FILE_READ);
  if (!f) return;
  viewLen = f.read((uint8_t *)viewBuf, sizeof(viewBuf) - 1);
  if (viewLen < 0) viewLen = 0;
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
        if (storageOpcao == 0) {
          spiffsCollect();
          fileCursor = 0;
          storageState = 1;
          displayArquivosSPIFFS();
        } else {
          storageState = 0;
          storageOpcao = 0;
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
        lastDebounceTime = millis();
        if (viewScroll < viewTotalLines - VIEWER_LINES) viewScroll++;
        displayFileViewerText();
      }
      if (digitalRead(BUTTON_LEFT) == LOW) {
        lastDebounceTime = millis();
        if (viewScroll > 0) viewScroll--;
        displayFileViewerText();
      }
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        storageState = 1;
        displayArquivosSPIFFS();
      }
    } else if (storageState == 3) {
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        storageState = 1;
        displayArquivosSPIFFS();
      }
    }
  }
}
