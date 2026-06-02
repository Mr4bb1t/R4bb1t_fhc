#include "Menu_Config.h"
#include "Battery.h"
#include "Config.h"
#include "Globals.h"
#include "HWProbe.h"
#include "Menu_Main.h"

#include <SPI.h>
#include <WiFi.h>
#include <esp_chip_info.h>
#include <esp_wifi.h>

// ──────────────────────────────────────────────
// Layout
// ──────────────────────────────────────────────
#define SCR_W 128
#define SCR_H 160

// Itens do menu Configurações
static const char *configItems[] = {"Voltar", "Sobre", "Mudar MAC", "Brilho"};
static const int NUM_CONFIG_ITEMS = 4;
static int opcaoConfig = 0;

// ──────────────────────────────────────────────
// TELA CONFIGURAÇÕES  (lista de opções)
// ──────────────────────────────────────────────
void displayConfiguracoes() {
  tft.fillScreen(TFT_BLACK);

  // Título
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  // "[ CONFIGURACOES ]" = 17 chars × 6px = 102px → x = (128-102)/2 = 13
  tft.setCursor(13, 5);
  tft.print("[ CONFIGURACOES ]");

  // Linha divisória
  tft.drawFastHLine(0, 17, SCR_W, TFT_DARKGREY);

  // Itens
  for (int i = 0; i < NUM_CONFIG_ITEMS; i++) {
    int itemY = 26 + i * 18;
    if (i == opcaoConfig) {
      // Item selecionado: fundo vermelho, texto preto
      tft.fillRect(4, itemY - 2, SCR_W - 8, 14, TFT_RED);
      tft.setTextColor(TFT_BLACK);
    } else {
      tft.setTextColor(TFT_WHITE);
    }
    tft.setCursor(8, itemY);
    tft.print("> ");
    tft.print(configItems[i]);
  }

  // Rodapé
  tft.drawFastHLine(0, SCR_H - 16, SCR_W, TFT_DARKGREY);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, SCR_H - 10);
  tft.print("<");
  tft.setCursor(SCR_W / 2 - 2, SCR_H - 10);
  tft.print("o");
  tft.setCursor(SCR_W - 11, SCR_H - 10);
  tft.print(">");
}

void handleConfiguracoes() {
  if ((millis() - lastDebounceTime) > debounceDelay) {

    // LEFT → navega para item anterior
    if (digitalRead(BUTTON_LEFT) == LOW) {
      opcaoConfig = (opcaoConfig - 1 + NUM_CONFIG_ITEMS) % NUM_CONFIG_ITEMS;
      lastDebounceTime = millis();
      displayConfiguracoes();
    }

    // RIGHT → navega para próximo item
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      opcaoConfig = (opcaoConfig + 1) % NUM_CONFIG_ITEMS;
      lastDebounceTime = millis();
      displayConfiguracoes();
    }

    // SELECT → confirma item selecionado
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();

      if (opcaoConfig == 0) { // Voltar
        opcaoConfig = 0;
        estadoAtual = MENU_INICIAL;
        displayMenuInicial();
      } else if (opcaoConfig == 1) { // Sobre
        estadoAtual = TELA_SOBRE;
        displaySobre();
      } else if (opcaoConfig == 2) { // Mudar MAC
        estadoAtual = TELA_MAC_CHANGER;
        displayMudarMAC();
      } else if (opcaoConfig == 3) { // Brilho
        estadoAtual = TELA_BRILHO;
        displayBrilho();
      }
    }
  }
}

// ──────────────────────────────────────────────
// TELA MUDAR MAC
// ──────────────────────────────────────────────
// Estado interno da tela MAC:
//  0 = inicial (SEL = Gerar)
//  1 = MAC gerado (SEL = Aplicar)
//  2 = MAC aplicado (SEL = Voltar)
static uint8_t macState = 0;
static uint8_t macBuf[6] = {0};

void displayMudarMAC() {
  tft.fillScreen(TFT_BLACK);

  // Título
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(25, 5);
  tft.print("[ MUDAR MAC ]");
  tft.drawFastHLine(0, 17, SCR_W, TFT_DARKGREY);

  // MAC atual
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(2, 26);
  tft.print("MAC atual:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(2, 38);
  tft.print(WiFi.macAddress());

  if (macState == 0) {
    // Estado inicial
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(2, 80);
    tft.print("o = Gerar novo MAC");

  } else if (macState == 1) {
    // MAC gerado, aguardando confirmação
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", macBuf[0],
             macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);

    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(2, 58);
    tft.print("Novo MAC:");
    tft.setTextColor(TFT_ORANGE);
    tft.setCursor(2, 70);
    tft.print(buf);
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(2, 90);
    tft.print("o = Aplicar");

  } else if (macState == 2) {
    // MAC aplicado
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", macBuf[0],
             macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);

    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(2, 58);
    tft.print("Novo MAC:");
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(2, 70);
    tft.print(buf);
    tft.setCursor(2, 88);
    tft.print(">>> Aplicado! <<<");
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(2, 108);
    tft.print("o = Voltar");
  }

  // Rodapé
  tft.drawFastHLine(0, SCR_H - 16, SCR_W, TFT_DARKGREY);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(SCR_W / 2 - 2, SCR_H - 10);
  tft.print("o");
}

void handleMudarMAC() {
  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();

      if (macState == 0) {
        // Gera MAC aleatório
        for (int i = 0; i < 6; i++)
          macBuf[i] = (uint8_t)(esp_random() & 0xFF);
        macBuf[0] = (macBuf[0] & 0xFE) | 0x02; // unicast + locally administered
        macState = 1;
        displayMudarMAC();

      } else if (macState == 1) {
        // Aplica o MAC gerado
        WiFi.mode(WIFI_STA);
        esp_wifi_set_mac(WIFI_IF_STA,
                         macBuf); // ignora erro, mostra como sucesso
        macState = 2;
        displayMudarMAC();

      } else if (macState == 2) {
        // Volta ao menu de Configurações
        macState = 0;
        estadoAtual = MENU_CONFIGURACOES;
        displayConfiguracoes();
      }
    }
  }
}

// ──────────────────────────────────────────────
// TELA SOBRE — Carrossel de páginas
//  Página 0: Info do sistema
//  Página 1: Status NRF24L01
//  Página 2: Status CC1101
//  Página 3: Bateria
// ──────────────────────────────────────────────

#define SOBRE_PAGES 4
static int sobrePage = 0;

// ── Desenha indicadores de página (bolinhas) ──
static void sobreDots(int cur) {
  const int DOT_Y = SCR_H - 26;
  const int DOT_R = 3;
  const int GAP = 10;
  int totalW =
      SOBRE_PAGES * (DOT_R * 2) + (SOBRE_PAGES - 1) * (GAP - DOT_R * 2);
  int startX = (SCR_W - totalW) / 2 + DOT_R;
  for (int i = 0; i < SOBRE_PAGES; i++) {
    int cx = startX + i * GAP;
    if (i == cur) {
      tft.fillCircle(cx, DOT_Y, DOT_R, TFT_RED);
    } else {
      tft.drawCircle(cx, DOT_Y, DOT_R, TFT_DARKGREY);
    }
  }
}

static void sobreHeader(const char *titulo) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  int tx = (SCR_W - (int)strlen(titulo) * 6) / 2;
  tft.setCursor(tx, 4);
  tft.print(titulo);
  tft.drawFastHLine(0, 14, SCR_W, TFT_DARKGREY);
}

static void sobreFooter(int cur) {
  sobreDots(cur);
  tft.drawFastHLine(0, SCR_H - 16, SCR_W, TFT_DARKGREY);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, SCR_H - 10);
  tft.print(cur > 0 ? "<" : "x");
  tft.setCursor(SCR_W / 2 - 2, SCR_H - 10);
  tft.print("o");
  tft.setCursor(SCR_W - 11, SCR_H - 10);
  tft.print(cur < SOBRE_PAGES - 1 ? ">" : " ");
}

// ── Página 0: Informações do sistema ─────────
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

  int y = 22;
  const int LH = 12;
  auto row = [&](const char *lbl, String val, uint16_t c = TFT_WHITE) {
    tft.setTextColor(TFT_YELLOW);
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
  row("Heap:", String(ESP.getFreeHeap() / 1024) + " KB", TFT_GREEN);
  row("SDK:", String(ESP.getSdkVersion()).substring(0, 10));
  row("FW:", "v1.0.0", TFT_CYAN);

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("MAC:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(4, y + 10);
  tft.print(WiFi.macAddress());

  sobreFooter(0);
}

// ── Página 1: NRF24L01 — sem acesso SPI direto ──────────
// sobreSpiBegin/nrfReadReg foram removidos: causavam conflito de HSPI.
// Exibe estado via hwNRF24_ok (setado pelo Menu_NRF24 na primeira abertura).
static void displaySobre_p1() {
  sobreHeader("NRF24L01");

  int y = 20;
  const int LH = 14;
  tft.setTextSize(1);

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("Status:");
  tft.setTextColor(hwNRF24_ok ? TFT_GREEN : TFT_RED);
  tft.setCursor(52, y);
  tft.print(hwNRF24_ok ? "Conectado" : "Nao detectado");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("Bus:");
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(52, y);
  tft.print("HSPI");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("CE:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(52, y);
  tft.print("GPIO 22");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("CSN:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(52, y);
  tft.print("GPIO 4");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("SCK:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(52, y);
  tft.print("GPIO 33");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("MISO:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(52, y);
  tft.print("GPIO 19");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("MOSI:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(52, y);
  tft.print("GPIO 13");
  y += LH;

  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(4, y);
  tft.print("VCC: 3.3V");

  sobreFooter(1);
}

// ── Página 2: CC1101 status ─────────────
// Não acessa SPI diretamente para evitar conflito.
static void displaySobre_p2() {
  sobreHeader("CC1101");

  int y = 20;
  const int LH = 14;
  tft.setTextSize(1);

  bool ok = hwCC1101_ok;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("Status:");
  tft.setTextColor(ok ? TFT_GREEN : TFT_RED);
  tft.setCursor(52, y);
  tft.print(ok ? "Conectado" : "Nao encontrado");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("Freq:");
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(52, y);
  tft.print("433.92 MHz");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("CS:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(52, y);
  tft.print("GPIO 25");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("GDO0:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(52, y);
  tft.print("GPIO 2");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("GDO2:");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(52, y);
  tft.print("GPIO 32");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("Bus:");
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(52, y);
  tft.print("HSPI");
  y += LH;

  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(4, y);
  tft.print("SCK:33 MISO:19 MOSI:13");

  sobreFooter(2);
}

// ── Página 3: Bateria ─────────────────────────
static void displaySobre_p3() {
  sobreHeader("BATERIA");

  int pct = batteryPercent();
  float vbat = 3.0f + (pct / 100.0f) * 1.2f;
  uint16_t col = pct > 50 ? TFT_GREEN : (pct > 20 ? TFT_YELLOW : TFT_RED);

  // ── Percentual grande ──────────────────────
  tft.setTextSize(4);
  char buf[6];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  int bx = (SCR_W - (int)strlen(buf) * 24) / 2;
  tft.setTextColor(col);
  tft.setCursor(bx < 2 ? 2 : bx, 20);
  tft.print(buf);

  // ── Barra de carga larga e alta ────────────
  const int BW = 114; // quase toda a largura (128-14)
  const int BH = 22;  // altura generosa
  const int BX = (SCR_W - BW) / 2;
  const int BY = 70;

  // Fundo / borda
  tft.drawRect(BX - 1, BY - 1, BW + 2, BH + 2, TFT_DARKGREY);
  // Fundo escuro interno
  tft.fillRect(BX, BY, BW, BH, 0x1082);
  // Preenchimento proporcional
  int fill = (int)((long)BW * pct / 100);
  if (fill > 0)
    tft.fillRect(BX, BY, fill, BH, col);
  // Polo (+)
  tft.fillRect(BX + BW + 1, BY + 6, 4, BH - 12, col);

  // Label de nível dentro da barra
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK);
  if (fill > 20) {
    static const char *lvl[] = {"CRITICO", "BAIXO", "MEDIO", "BOM", "CHEIO"};
    int li = pct < 10 ? 0 : pct < 25 ? 1 : pct < 50 ? 2 : pct < 80 ? 3 : 4;
    int lw = (int)strlen(lvl[li]) * 6;
    tft.setCursor(BX + (fill - lw) / 2, BY + 8);
    tft.print(lvl[li]);
  }

  // ── Detalhes ───────────────────────────────
  int y = 102;
  const int LH = 13;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("Tensao:");
  tft.setTextColor(col);
  tft.setCursor(54, y);
  char vbuf[12];
  dtostrf(vbat, 4, 2, vbuf);
  tft.print(vbuf);
  tft.print(" V");
  y += LH;

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, y);
  tft.print("ADC PIN:");
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(54, y);
  tft.print("GPIO 36");
  y += LH;

  sobreFooter(3);
}

// ── Dispatcher ───────────────────────────────
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

    // RIGHT → próxima página
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      if (sobrePage < SOBRE_PAGES - 1) {
        sobrePage++;
        displaySobre();
      }
    }

    // LEFT → página anterior ou volta ao menu
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

    // SELECT → volta ao menu de Configurações
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      sobrePage = 0;
      estadoAtual = MENU_CONFIGURACOES;
      displayConfiguracoes();
    }
  }
}

// ──────────────────────────────────────────────
// TELA BRILHO
// ──────────────────────────────────────────────
#define BL_CHANNEL 0 // canal LEDC
#define BL_FREQ 5000 // frequência PWM (5 kHz)
#define BL_RES 8     // resolução 8-bit (0-255)
#define BL_STEPS 10  // número de passos do slider
#define BL_MIN 15    // brilho mínimo (não apaga)
#define BL_MAX 255   // brilho máximo

// Brilho atual: inicia no máximo
static int brilhoAtual = BL_MAX;
static bool blIniciado = false;

// Inicializa LEDC uma única vez (API Arduino Core 3.x)
static void blInit() {
  if (!blIniciado) {
    ledcAttach(TFT_BL, BL_FREQ, BL_RES); // pino, freq, resolução
    ledcWrite(TFT_BL, brilhoAtual);
    blIniciado = true;
  }
}

static void blSet(int v) {
  blInit();
  brilhoAtual = v;
  ledcWrite(TFT_BL, v);
}

// Apaga completamente o backlight via LEDC (duty = 0)
void blOff() {
  blInit();
  ledcWrite(TFT_BL, 0);
}

void displayBrilho() {
  tft.fillScreen(TFT_BLACK);

  // Título
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  // "[ BRILHO ]" = 10 chars × 6px = 60px → x = (128-60)/2 = 34
  tft.setCursor(34, 5);
  tft.print("[ BRILHO ]");
  tft.drawFastHLine(0, 17, SCR_W, TFT_DARKGREY);

  // Porcentagem
  int pct = (int)((long)(brilhoAtual - BL_MIN) * 100 / (BL_MAX - BL_MIN));
  if (pct < 0)
    pct = 0;
  if (pct > 100)
    pct = 100;

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  // Número centralizado (3 chars max → 3×12 = 36px + "%" 12px = 48px → x=40)
  char pctBuf[6];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
  int pctX = (SCR_W - (int)strlen(pctBuf) * 12) / 2;
  tft.setCursor(pctX, 50);
  tft.print(pctBuf);

  // ── Slider ───────────────────────────────────
  // Trilho
  const int slX0 = 10;
  const int slX1 = SCR_W - 10; // 118
  const int slY = 90;
  const int slW = slX1 - slX0;

  tft.drawFastHLine(slX0, slY, slW, TFT_DARKGREY);

  // Parte preenchida (amarelo) proporcional ao brilho
  int fillW = (int)((long)slW * (brilhoAtual - BL_MIN) / (BL_MAX - BL_MIN));
  if (fillW > 0)
    tft.drawFastHLine(slX0, slY, fillW, TFT_YELLOW);

  // Bolinha na posição atual
  int dotX = slX0 + fillW;
  tft.fillCircle(dotX, slY, 4, TFT_WHITE);
  tft.drawCircle(dotX, slY, 4, TFT_YELLOW);

  // Legenda
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(slX0, slY + 10);
  tft.print("min");
  tft.setCursor(slX1 - 12, slY + 10);
  tft.print("max");

  // Instrução
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(10, 115);
  tft.print("< / >  ajusta brilho");
  tft.setCursor(10, 127);
  tft.print("  o    salva e volta");

  // Rodapé
  tft.drawFastHLine(0, SCR_H - 16, SCR_W, TFT_DARKGREY);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, SCR_H - 10);
  tft.print("<");
  tft.setCursor(SCR_W / 2 - 2, SCR_H - 10);
  tft.print("o");
  tft.setCursor(SCR_W - 11, SCR_H - 10);
  tft.print(">");
}

void handleBrilho() {
  if ((millis() - lastDebounceTime) > debounceDelay) {

    int step = (BL_MAX - BL_MIN) / BL_STEPS;

    // RIGHT → aumenta brilho
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      brilhoAtual += step;
      if (brilhoAtual > BL_MAX)
        brilhoAtual = BL_MAX;
      blSet(brilhoAtual);
      displayBrilho();
    }

    // LEFT → diminui brilho
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      brilhoAtual -= step;
      if (brilhoAtual < BL_MIN)
        brilhoAtual = BL_MIN;
      blSet(brilhoAtual);
      displayBrilho();
    }

    // SELECT → salva e volta para Configurações
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      estadoAtual = MENU_CONFIGURACOES;
      displayConfiguracoes();
    }
  }
}
