// Menu_RF.cpp — módulo RF 433 MHz para r4bb1t
// Usa CC1101 via HSPI (pinos livres) + RCSwitch
// Display: TFT_eSPI 128×160

#include "Menu_RF.h"
#include "Globals.h"
#include "Menu_Main.h"

#include "Battery.h"
#include "Config.h"
#include "HWProbe.h"
#include "UI.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>
#include <SPI.h>
#include <SPIFFS.h>

// ── Objetos ──────────────────────────────────────
static RCSwitch rcSwitch;
static bool rfReady = false;

// ── Layout ───────────────────────────────────────
#define SCR_W 128
#define SCR_H 160

// ── Arquivo de sinais salvos ──────────────────────
#define RF_SIGNALS_FILE "/rf_signals.txt"
#define MAX_RF_SIGNALS 20

struct RFSignal {
  unsigned long value;
  int bits;
  int protocol;
  float freq;
};

// ── Sub-menu RF ──────────────────────────────────
// Índices: 0=Voltar 1=Replay 2=Raw 3=Analyser 4=Random 5=Saved
static const int RF_ITEMS = 6;
static int rfOpcao = 0;

// Cor e label de cada item
static const char *rfLabels[] = {"< Voltar", "Capturar", "Sniffer",
                                 "Grafico",  "Jammer",   "Salvos"};
static const uint16_t rfColors[] = {TFT_WHITE,  TFT_GREEN, TFT_CYAN,
                                    TFT_YELLOW, TFT_RED,   TFT_MAGENTA};

// ── Helpers de display ───────────────────────────
static void rfHeader(const char *titulo) {
  tft.fillScreen(C_BG);
  drawHeader(titulo, true);
}

static void rfFooter(const char *hint = nullptr) {
  drawSeparator(SCR_H - 18, C_GREY);
  tft.setTextSize(1);
  if (hint) {
    tft.setTextColor(C_GREY);
    tft.setCursor(2, SCR_H - 12);
    tft.print(hint);
  }
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(5, SCR_H - 12);
  tft.print("<");
  tft.setCursor(61, SCR_H - 12);
  tft.print("o");
  tft.setCursor(SCR_W - 11, SCR_H - 12);
  tft.print(">");
}

// Forward declaration — definida na seção do scanner
static void drawDetectedFreq(int y);

// ── SPIFFS: salvar sinal ─────────────────────────
static bool saveRFSignal(unsigned long val, int bits, int proto, float freq) {
  File f = SPIFFS.open(RF_SIGNALS_FILE, FILE_APPEND);
  if (!f)
    return false;
  f.printf("%lu,%d,%d,%.2f\n", val, bits, proto, freq);
  f.close();
  return true;
}

// ── SPIFFS: carregar sinais em array ─────────────
static int loadRFSignals(RFSignal *out, int maxCount) {
  File f = SPIFFS.open(RF_SIGNALS_FILE, FILE_READ);
  if (!f)
    return 0;
  int count = 0;
  while (f.available() && count < maxCount) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;
    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0)
      continue;
    out[count].value = strtoul(line.substring(0, c1).c_str(), nullptr, 10);
    out[count].bits = line.substring(c1 + 1, c2).toInt();

    int c3 = line.indexOf(',', c2 + 1);
    if (c3 > 0) {
      out[count].protocol = line.substring(c2 + 1, c3).toInt();
      out[count].freq = line.substring(c3 + 1).toFloat();
    } else {
      out[count].protocol = line.substring(c2 + 1).toInt();
      out[count].freq = 433.92f;
    }
    count++;
  }
  f.close();
  return count;
}

// ── SPIFFS: deletar sinal pelo índice ────────────
static bool deleteRFSignal(int index) {
  RFSignal buf[MAX_RF_SIGNALS];
  int count = loadRFSignals(buf, MAX_RF_SIGNALS);
  if (index < 0 || index >= count)
    return false;

  // Reescreve o arquivo sem o sinal deletado
  File f = SPIFFS.open(RF_SIGNALS_FILE, FILE_WRITE);
  if (!f)
    return false;
  for (int i = 0; i < count; i++) {
    if (i == index)
      continue;
    f.printf("%lu,%d,%d,%.2f\n", buf[i].value, buf[i].bits, buf[i].protocol,
             buf[i].freq);
  }
  f.close();
  return true;
}

// ── Inicialização CC1101 ─────────────────────────
bool rfInit() {
  Serial.println("[RF] Iniciando CC1101...");
  Serial.printf("[RF] Pinos: SCK=%d MISO=%d MOSI=%d CS=%d GDO0=%d GDO2=%d\n",
                RF_SCK, RF_MISO, RF_MOSI, RF_CS, RF_GDO0, RF_GDO2);

  ELECHOUSE_cc1101.setSpiPin(RF_SCK, RF_MISO, RF_MOSI, RF_CS);
  ELECHOUSE_cc1101.setGDO(RF_GDO0, RF_GDO2);

  if (!ELECHOUSE_cc1101.getCC1101()) {
    Serial.println("[RF] ERRO: CC1101 nao encontrado! Verifique a fiacao SPI.");
    rfReady = false;
    return false;
  }

  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(433.92);
  ELECHOUSE_cc1101.SetRx();

  rcSwitch.enableReceive(RF_GDO2);
  rcSwitch.enableTransmit(RF_GDO0);
  rcSwitch.setRepeatTransmit(10);

  rfReady = true;

  // Lê RSSI inicial para confirmar comunicação SPI
  float rssiInit = ELECHOUSE_cc1101.getRssi();
  float lqiInit = ELECHOUSE_cc1101.getLqi();
  Serial.println("[RF] CC1101 OK @ 433.92 MHz");
  Serial.printf("[RF] RSSI inicial: %.1f dBm  |  LQI: %.0f\n", rssiInit,
                lqiInit);
  return true;
}

// ════════════════════════════════════════════════
//  TELA: Sub-menu RF
//  Padrão visual do projeto (estilo Menu_Attacks):
//   • barra colorida 5px à esquerda
//   • fundo escuro no item selecionado
//   • primeiro item = Voltar
// ════════════════════════════════════════════════
void displayRF() {
  static bool rfInitDone = false;
  if (!rfInitDone) {
    rfInitDone = true;
    if (!hwCC1101_ok) {
      hwCC1101_ok = rfInit();
    }
  }

  tft.fillScreen(C_BG);
  tft.setTextSize(1);
  drawHeader("SUB GHZ", true);

  tft.drawFastHLine(0, 26, SCR_W, C_GREY);

  // Lista de itens com drawMenuItem
  for (int i = 0; i < RF_ITEMS; i++) {
    drawMenuItem(0, 28 + i * 19, 128, 18, rfLabels[i], i == rfOpcao);
  }

  drawFooter();
  batteryDraw();
}

void handleRF() {
  if ((millis() - lastDebounceTime) > debounceDelay) {

    // RIGHT → próximo item
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = rfOpcao;
      rfOpcao = (rfOpcao + 1) % RF_ITEMS;
      lastDebounceTime = millis();
      drawMenuItem(0, 28 + old * 19, 128, 18, rfLabels[old], false);
      drawMenuItem(0, 28 + rfOpcao * 19, 128, 18, rfLabels[rfOpcao], true);
    }

    // LEFT → item anterior (ou atalho direto para voltar ao menu principal
    //         se já estiver no item 0)
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      if (rfOpcao == 0) {
        // Atalho: voltar sem precisar apertar SELECT
        rfOpcao = 0;
        estadoAtual = MENU_INICIAL;
        displayMenuInicial();
      } else {
        int old = rfOpcao;
        rfOpcao = (rfOpcao - 1 + RF_ITEMS) % RF_ITEMS;
        drawMenuItem(0, 28 + old * 19, 128, 18, rfLabels[old], false);
        drawMenuItem(0, 28 + rfOpcao * 19, 128, 18, rfLabels[rfOpcao], true);
      }
    }

    // SELECT → abre o mode selecionado
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      switch (rfOpcao) {
      case 0:
        rfOpcao = 0;
        estadoAtual = MENU_INICIAL;
        displayMenuInicial();
        break;
      case 1:
        estadoAtual = TELA_RF_REPLAY;
        displayRF_Replay();
        break;
      case 2:
        estadoAtual = TELA_RF_RAW;
        displayRF_Raw();
        break;
      case 3:
        estadoAtual = TELA_RF_ANALYSER;
        displayRF_Analyser();
        break;
      case 4:
        estadoAtual = TELA_RF_RANDOM;
        displayRF_Random();
        break;
      case 5:
        estadoAtual = TELA_RF_SAVED;
        displayRF_Saved();
        break;
      }
    }
  }
}

// ════════════════════════════════════════════════
//  MODO: REPLAY — captura e retransmite sinal
//  LEFT=Voltar  SELECT=Transmitir  RIGHT=Salvar
// ════════════════════════════════════════════════
static unsigned long replayVal = 0;
static int replayBits = 0;
static int replayProtocol = 0;
static bool replayHasSig = false;

void displayRF_Replay() {
  rfHeader("REPLAY");

  // Frequência detectada no topo
  drawDetectedFreq(17);

  tft.setTextSize(1);
  if (!replayHasSig) {
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(4, 32);
    tft.print("Aguardando sinal...");
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 48);
    tft.print("Aponte o controle e");
    tft.setCursor(4, 60);
    tft.print("pressione o botao.");
  } else {
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(4, 32);
    tft.print("Capturado:");
    tft.setTextColor(TFT_GREEN);
    char buf[32];
    snprintf(buf, sizeof(buf), "Val: %lX", replayVal);
    tft.setCursor(4, 46);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "Bits:%d  Proto:%d", replayBits, replayProtocol);
    tft.setCursor(4, 58);
    tft.print(buf);

    tft.setTextColor(TFT_CYAN);
    tft.setCursor(4, 76);
    tft.print("o=TX  v=Salvar  ^=Vol");
  }

  if (!replayHasSig) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, SCR_H - 28);
    tft.print("^ = Voltar");
  }
  rfFooter();
  batteryDraw();

  // Sintoniza na frequência detectada
  if (rfReady && rfDetectedMHz > 0) {
    ELECHOUSE_cc1101.setMHZ(rfDetectedMHz);
    ELECHOUSE_cc1101.SetRx();
    rcSwitch.enableReceive(RF_GDO2);
  }
}

void handleRF_Replay() {
  // Recepção contínua
  if (rfReady && rcSwitch.available()) {
    replayVal = rcSwitch.getReceivedValue();
    replayBits = rcSwitch.getReceivedBitlength();
    replayProtocol = rcSwitch.getReceivedProtocol();
    rcSwitch.resetAvailable();
    replayHasSig = true;

    float rssi = ELECHOUSE_cc1101.getRssi();
    float lqi = ELECHOUSE_cc1101.getLqi();
    Serial.println("[RF][REPLAY] Sinal capturado!");
    Serial.printf("[RF][REPLAY]   Valor   : %lu (0x%lX)\n", replayVal,
                  replayVal);
    Serial.printf("[RF][REPLAY]   Bits    : %d  |  Protocolo: %d\n", replayBits,
                  replayProtocol);
    Serial.printf("[RF][REPLAY]   RSSI    : %.1f dBm  |  LQI: %.0f\n", rssi,
                  lqi);

    displayRF_Replay();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    // SELECT → retransmite
    if (digitalRead(BUTTON_SELECT) == LOW && replayHasSig && rfReady) {
      lastDebounceTime = millis();
      float txFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
      Serial.printf("[RF][REPLAY] Retransmitindo na %.2fMHz -> Val:%lu Bits:%d "
                    "Proto:%d\n",
                    txFreq, replayVal, replayBits, replayProtocol);
      ELECHOUSE_cc1101.setMHZ(txFreq);
      ELECHOUSE_cc1101.SetTx();
      delay(5);
      rcSwitch.setProtocol(replayProtocol);
      rcSwitch.send(replayVal, replayBits);
      delay(5);
      ELECHOUSE_cc1101.SetRx();
      Serial.println("[RF][REPLAY] TX concluido, voltando para RX.");
      tft.fillRect(4, 92, SCR_W - 8, 12, TFT_BLACK);
      tft.setTextColor(TFT_ORANGE);
      tft.setCursor(4, 92);
      tft.print(">>> ENVIADO! <<<");
      delay(700);
      displayRF_Replay();
    }

    // RIGHT → salva no SPIFFS
    if (digitalRead(BUTTON_RIGHT) == LOW && replayHasSig) {
      lastDebounceTime = millis();
      float saveFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
      bool ok = saveRFSignal(replayVal, replayBits, replayProtocol, saveFreq);
      Serial.printf("[RF][REPLAY] Salvar no SPIFFS: %s\n", ok ? "OK" : "ERRO");
      tft.fillRect(4, 92, SCR_W - 8, 12, TFT_BLACK);
      tft.setTextColor(ok ? TFT_GREEN : TFT_RED);
      tft.setCursor(4, 92);
      tft.print(ok ? "Salvo no SPIFFS!" : "Erro ao salvar!");
      delay(1000);
      displayRF_Replay();
    }

    // LEFT → volta ao sub-menu RF
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      replayHasSig = false;
      estadoAtual = MENU_RF;
      displayRF();
    }
  }
}

// ════════════════════════════════════════════════
//  MODO: RAW — exibe pulsos recebidos em tempo real
// ════════════════════════════════════════════════
static bool rawListening = true;

void displayRF_Raw() {
  rfHeader("RAW  RX");

  // Frequência detectada
  drawDetectedFreq(17);

  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(4, 30);
  tft.print("Escutando pulsos...");
  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(4, SCR_H - 28);
  tft.print("^ = Voltar");
  rfFooter();
  rawListening = true;
  batteryDraw();

  // Sintoniza na frequência detectada
  if (rfReady && rfDetectedMHz > 0) {
    ELECHOUSE_cc1101.setMHZ(rfDetectedMHz);
    ELECHOUSE_cc1101.SetRx();
    rcSwitch.enableReceive(RF_GDO2);
  }
}

void handleRF_Raw() {
  static int peakRSSI = -100;
  static unsigned long lastRssiPoll = 0;

  // Realiza polling do RSSI a cada 5ms para não travar o barramento SPI
  // mas rápido o suficiente para não cair nos "gaps" de silêncio do RF
  if (rfReady && (millis() - lastRssiPoll > 5)) {
    lastRssiPoll = millis();
    int currentRssi = (int)ELECHOUSE_cc1101.getRssi();
    if (currentRssi > peakRSSI) {
      peakRSSI = currentRssi;
    }
  }

  if (rfReady && rcSwitch.available()) {
    unsigned long val = rcSwitch.getReceivedValue();
    int bits = rcSwitch.getReceivedBitlength();
    int proto = rcSwitch.getReceivedProtocol();
    rcSwitch.resetAvailable();

    // Pega o maior sinal detectado
    int dbm = peakRSSI;
    if (dbm == -100)
      dbm = (int)ELECHOUSE_cc1101.getRssi(); // Fallback
    float lqi = ELECHOUSE_cc1101.getLqi();
    peakRSSI = -100; // Reseta para a próxima captura

    Serial.println("[RF][RAW] Sinal recebido:");
    Serial.printf("[RF][RAW]   Dec     : %lu\n", val);
    Serial.printf("[RF][RAW]   Hex     : 0x%lX\n", val);
    Serial.printf("[RF][RAW]   Bits    : %d  |  Protocolo: %d\n", bits, proto);
    Serial.printf("[RF][RAW]   RSSI    : %d dBm  |  LQI: %.0f\n", dbm, lqi);
    Serial.printf("[RF][RAW]   Barras  : %d/10\n",
                  constrain(map(dbm, -45, -20, 1, 10), 1, 10));

    // Partial redraw: limpa as 3 linhas de texto (Y=40 até 74) e a linha do
    // Sinal/Barras (Y=82 até 90)
    tft.fillRect(0, 40, SCR_W, 34, TFT_BLACK);
    tft.fillRect(0, 82, SCR_W, 8, TFT_BLACK);

    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN);
    char buf[32];
    snprintf(buf, sizeof(buf), "Dec: %lu", val);
    tft.setCursor(4, 40);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "Hex: 0x%lX", val);
    tft.setCursor(4, 52);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "Bits: %d   Proto: %d", bits, proto);
    tft.setCursor(4, 64);
    tft.print(buf);

    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 82);
    // Agora mostramos também o valor real do RSSI
    char sbuf[16];
    snprintf(sbuf, sizeof(sbuf), "dBm:%d", dbm);
    tft.print(sbuf);

    // Mapeia o RSSI real para 1 a 10 barras usando a exata mesma métrica do
    // Analyser (-45 a -20)
    int bars = constrain(map(dbm, -45, -20, 1, 10), 1, 10);
    for (int b = 0; b < 10; b++) {
      if (b < bars) {
        tft.fillRect(60 + b * 6, 82, 5, 8, TFT_CYAN); // Quadrado preenchido
      } else {
        tft.drawRect(60 + b * 6, 82, 5, 8, TFT_DARKGREY); // Quadrado vazio
      }
    }
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      estadoAtual = MENU_RF;
      displayRF();
    }
  }
}

// ════════════════════════════════════════════════
//  AUTO-DETECÇÃO DE FREQUÊNCIA
//  Varre o espectro do CC1101 e encontra o pico
// ════════════════════════════════════════════════

// Frequências comuns a varrer (MHz) — faixas do CC1101
static const float SCAN_FREQS[] = {
    300.00, 303.87, 310.00, 315.00, 318.00, 330.00, 390.00, 418.00, 430.00, 
    431.00, 432.00, 433.00, 433.42, 433.92, 434.42, 435.00, 436.00, 438.00, 
    440.00, 450.00, 868.00, 868.35, 868.95, 869.50, 915.00, 916.00, 920.00, 
    925.00};
#define SCAN_FREQ_COUNT (sizeof(SCAN_FREQS) / sizeof(SCAN_FREQS[0]))

static int scanIdx = 0;          // índice atual na varredura
static bool scanRunning = false; // varredura ativa?
static int scanBestRSSI = -120;  // melhor RSSI encontrado na varredura
static float scanBestMHz = 0;    // frequência com melhor RSSI
static unsigned long scanLastStep = 0;

// Executa um passo da varredura (chamado no loop do Analyser)
// Retorna true quando completou um ciclo completo
static bool rfScanStep() {
  if (!rfReady)
    return false;

  // Sintoniza na frequência atual
  ELECHOUSE_cc1101.setMHZ(SCAN_FREQS[scanIdx]);
  ELECHOUSE_cc1101.SetRx();
  delayMicroseconds(800); // tempo de settling do PLL

  // Lê RSSI (média de 3 leituras para estabilidade)
  int rssiSum = 0;
  for (int i = 0; i < 3; i++) {
    rssiSum += ELECHOUSE_cc1101.getRssi();
    delayMicroseconds(200);
  }
  int rssi = rssiSum / 3;

  if (rssi > scanBestRSSI) {
    scanBestRSSI = rssi;
    scanBestMHz = SCAN_FREQS[scanIdx];
  }

  scanIdx++;
  if (scanIdx >= (int)SCAN_FREQ_COUNT) {
    scanIdx = 0;

    // Atualiza frequência detectada se o pico é acima do ruído
    if (scanBestRSSI > -75) {
      rfDetectedMHz = scanBestMHz;
      rfDetectedRSSI = scanBestRSSI;
      Serial.printf("[RF][SCAN] Freq detectada: %.2f MHz  RSSI: %d dBm\n",
                    rfDetectedMHz, rfDetectedRSSI);
    }

    // Reseta para próximo ciclo
    scanBestRSSI = -120;
    scanBestMHz = 0;

    // Volta para a frequência detectada (ou 433.92 padrão) para o waterfall
    float tuneFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
    ELECHOUSE_cc1101.setMHZ(tuneFreq);
    ELECHOUSE_cc1101.SetRx();

    return true; // ciclo completo
  }
  return false;
}

// Helper: desenha a frequência detectada em qualquer tela
// y = posição Y para desenhar, clearW = largura para limpar
static void drawDetectedFreq(int y) {
  tft.fillRect(0, y, SCR_W, 10, TFT_BLACK);
  tft.setTextSize(1);
  if (rfDetectedMHz > 0) {
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(2, y);
    char buf[24];
    snprintf(buf, sizeof(buf), "F:%.2fMHz %ddBm", rfDetectedMHz,
             rfDetectedRSSI);
    tft.print(buf);
  } else {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(2, y);
    tft.print("Freq: buscando...");
  }
}

// ════════════════════════════════════════════════
//  MODO: ANALISADOR — Waterfall Spectrum
//  Com detecção automática de frequência
//
//  −45 dBm = azul fino central
//  −20 dBm = vermelho largo quase toda a tela
//
//  Sinal com envelope suave:
//    Attack  : sobe gradualmente do RSSI real até 255
//    Sustain : mantém 255 enquanto rcSwitch recebe
//    Decay   : desce gradualmente de volta ao RSSI real
//
//  Sem branco. Cor + largura sempre pela paleta.
// ════════════════════════════════════════════════

#define WF_Y 20
#define WF_H 120
#define WF_CX (SCR_W / 2)

#define WF_DBM_MIN -45
#define WF_DBM_MAX -20

#define WF_HALF_MIN 1
#define WF_HALF_MAX (SCR_W / 2 - 2)

// Linhas de rampa de subida (attack)
#define WF_ATTACK_LINES 8
// Linhas de rampa de descida (decay)
#define WF_DECAY_LINES 20

static uint8_t wfBuf[WF_H];
static int wfHead = 0;
static unsigned long wfLastUpdate = 0;
static unsigned long wfSigCount = 0;

// Máquina de estados do envelope
typedef enum { ENV_IDLE, ENV_ATTACK, ENV_SUSTAIN, ENV_DECAY } EnvState;
static EnvState wfEnv = ENV_IDLE;
static uint8_t wfEnvLevel = 0;   // nível atual do envelope 0-255
static int wfEnvStep = 0;        // contador de linhas na fase atual
static bool wfSigActive = false; // rcSwitch ainda recebendo?

// ── Normaliza dBm → 0-255 ────────────────────────
static uint8_t rssiNorm(int dbm) {
  return (uint8_t)constrain(map(dbm, WF_DBM_MIN, WF_DBM_MAX, 0, 255), 0, 255);
}

// ── Paleta 9 pontos RGB565 ────────────────────────
static const uint16_t WF_PALETTE[9] = {
    0x000F, // 0  Azul muito escuro
    0x001F, // 1  Azul puro
    0x043F, // 2  Azul-índigo
    0x07FF, // 3  Ciano
    0x07E0, // 4  Verde puro
    0x8FE0, // 5  Verde-amarelo
    0xFFE0, // 6  Amarelo
    0xFD00, // 7  Laranja
    0xF800, // 8  Vermelho puro
};

static uint16_t lerpColor(uint16_t a, uint16_t b, float t) {
  int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  return (uint16_t)((int)(ar + (br - ar) * t)) << 11 |
         (uint16_t)((int)(ag + (bg - ag) * t)) << 5 |
         (uint16_t)((int)(ab + (bb - ab) * t));
}

static uint16_t wfColor(uint8_t v) {
  float pos = (v / 255.0f) * 8.0f;
  int idx = (int)pos;
  if (idx >= 8)
    return WF_PALETTE[8];
  return lerpColor(WF_PALETTE[idx], WF_PALETTE[idx + 1], pos - idx);
}

static int wfHalfWidth(uint8_t v) {
  return constrain((int)map(v, 0, 255, WF_HALF_MIN, WF_HALF_MAX), WF_HALF_MIN,
                   WF_HALF_MAX);
}

static void wfDrawLine(int screenY, uint8_t norm) {
  tft.drawFastHLine(0, screenY, SCR_W, TFT_BLACK);
  if (norm == 0)
    return;
  uint16_t col = wfColor(norm);
  int halfW = wfHalfWidth(norm);
  tft.drawFastHLine(WF_CX - halfW, screenY, halfW * 2, col);
}

static void wfRedraw() {
  for (int i = 0; i < WF_H; i++) {
    int idx = (wfHead + i) % WF_H;
    wfDrawLine(WF_Y + i, wfBuf[idx]);
  }
}

// ── Inicializa ────────────────────────────────────
void displayRF_Analyser() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(2, 4);
  tft.print("ANALISADOR");

  // Mostra frequência no header (detectada ou padrão)
  tft.setTextColor(rfDetectedMHz > 0 ? TFT_GREEN : TFT_DARKGREY);
  tft.setCursor(68, 4);
  char hdrBuf[16];
  if (rfDetectedMHz > 0)
    snprintf(hdrBuf, sizeof(hdrBuf), "%.2f", rfDetectedMHz);
  else
    snprintf(hdrBuf, sizeof(hdrBuf), "scan...");
  tft.print(hdrBuf);
  tft.drawFastHLine(0, 14, SCR_W, TFT_DARKGREY);

  // Barra de frequência no rodapé — mostra frequência real detectada
  int axisY = WF_Y + WF_H + 1;
  drawDetectedFreq(axisY);
  tft.drawFastHLine(0, axisY + 10, SCR_W, 0x2945);

  tft.drawFastHLine(0, SCR_H - 16, SCR_W, TFT_DARKGREY);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, SCR_H - 10);
  tft.print("<");

  memset(wfBuf, 0, sizeof(wfBuf));
  wfHead = 0;
  wfSigCount = 0;
  wfLastUpdate = 0;
  wfEnv = ENV_IDLE;
  wfEnvLevel = 0;
  wfEnvStep = 0;
  wfSigActive = false;

  // Inicializa scanner
  scanRunning = true;
  scanIdx = 0;
  scanBestRSSI = -120;
  scanBestMHz = 0;
  scanLastStep = 0;

  wfRedraw();
  batteryDraw();
}

// ── Loop principal ────────────────────────────────
void handleRF_Analyser() {
  // ── Varredura de frequência (intercalada com waterfall) ──
  if (scanRunning && (millis() - scanLastStep > 15)) {
    scanLastStep = millis();
    bool cycleComplete = rfScanStep();

    if (cycleComplete) {
      // Atualiza o header com a frequência detectada
      tft.fillRect(66, 2, 62, 12, TFT_BLACK);
      tft.setTextSize(1);
      tft.setTextColor(rfDetectedMHz > 0 ? TFT_GREEN : TFT_DARKGREY);
      tft.setCursor(68, 4);
      char hBuf[16];
      if (rfDetectedMHz > 0)
        snprintf(hBuf, sizeof(hBuf), "%.2f", rfDetectedMHz);
      else
        snprintf(hBuf, sizeof(hBuf), "scan...");
      tft.print(hBuf);

      // Atualiza barra de frequência no rodapé
      int axisY = WF_Y + WF_H + 1;
      drawDetectedFreq(axisY);

      // Re-habilita o rcSwitch na frequência detectada
      rcSwitch.enableReceive(RF_GDO2);
    }
  }

  // Detecta sinal novo do rcSwitch
  if (rfReady && rcSwitch.available()) {
    rcSwitch.resetAvailable();
    wfSigCount++;
    wfSigActive = true;

    float rssi = ELECHOUSE_cc1101.getRssi();
    float lqi = ELECHOUSE_cc1101.getLqi();
    Serial.printf("[RF][ANALYSER] Sinal #%lu  RSSI: %.1f dBm  LQI: %.0f\n",
                  wfSigCount, rssi, lqi);

    // Se estava em idle ou decay, reinicia attack
    if (wfEnv == ENV_IDLE || wfEnv == ENV_DECAY) {
      wfEnv = ENV_ATTACK;
      wfEnvStep = 0;
    }
    // Se já estava em attack ou sustain, apenas mantém
    // (wfEnvStep continua de onde estava)

    tft.fillRect(0, 4, 66, 10, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(2, 4);
    char buf[16];
    snprintf(buf, sizeof(buf), "#%lu", wfSigCount);
    tft.print(buf);
  }

  if (millis() - wfLastUpdate > 60) {
    wfLastUpdate = millis();

    int dbm = rfReady ? (int)ELECHOUSE_cc1101.getRssi() : WF_DBM_MIN;
    uint8_t rssiBase = rssiNorm(dbm);
    // Log periódico de RSSI a cada ~2s (cada tick = 60ms, 33 ticks ≈ 2s)
    static uint8_t _dbgTick = 0;
    if (++_dbgTick >= 33) {
      _dbgTick = 0;
      Serial.printf(
          "[RF][ANALYSER] RSSI: %d dBm  norm: %d  env: %d  freq: %.2f\n", dbm,
          rssiBase, wfEnvLevel, rfDetectedMHz);
    }

    // ── Máquina de estados do envelope ───────────
    switch (wfEnv) {

    case ENV_IDLE:
      wfEnvLevel = 0;
      break;

    case ENV_ATTACK: {
      // Sobe de rssiBase até 255 em WF_ATTACK_LINES linhas
      // Curva suave: seno da primeira metade (easing in)
      float t = (float)wfEnvStep / WF_ATTACK_LINES;
      // ease-in quadrático
      float ease = t * t;
      wfEnvLevel = (uint8_t)(rssiBase + (255 - rssiBase) * ease);
      wfEnvStep++;
      if (wfEnvStep >= WF_ATTACK_LINES) {
        wfEnvLevel = 255;
        wfEnv = ENV_SUSTAIN;
        wfEnvStep = 0;
        wfSigActive = false;
      }
      break;
    }

    case ENV_SUSTAIN:
      wfEnvLevel = 255;
      // Passa para decay assim que não há mais sinal ativo
      if (!wfSigActive) {
        wfEnv = ENV_DECAY;
        wfEnvStep = 0;
      }
      wfSigActive = false; // reseta; rcSwitch seta de novo se ainda receber
      break;

    case ENV_DECAY: {
      // Desce de 255 até rssiBase em WF_DECAY_LINES linhas
      // ease-out quadrático (rápido no início, lento no fim)
      float t = (float)wfEnvStep / WF_DECAY_LINES;
      float ease = 1.0f - (1.0f - t) * (1.0f - t);
      wfEnvLevel = (uint8_t)(255 - (255 - rssiBase) * ease);
      wfEnvStep++;
      if (wfEnvStep >= WF_DECAY_LINES) {
        wfEnvLevel = 0;
        wfEnv = ENV_IDLE;
        wfEnvStep = 0;
      }
      break;
    }
    }

    // Nível final: máximo entre RSSI real e envelope
    uint8_t norm = max(rssiBase, wfEnvLevel);

    wfBuf[wfHead] = norm;
    wfHead = (wfHead + 1) % WF_H;

    wfRedraw();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      scanRunning = false;
      // Restaura frequência para uso em outras telas
      float tuneFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
      ELECHOUSE_cc1101.setMHZ(tuneFreq);
      ELECHOUSE_cc1101.SetRx();
      rcSwitch.enableReceive(RF_GDO2);
      estadoAtual = MENU_RF;
      displayRF();
    }
  }
}

// ════════════════════════════════════════════════
//  MODO: JAMMER — inunda o canal 433 MHz com ruído RF
//  SELECT = iniciar / parar    LEFT = voltar (auto-para)
// ════════════════════════════════════════════════
static bool jammerAtivo = false;
static unsigned long jammerPackets = 0;
static unsigned long jammerLastDraw = 0;

void displayRF_Random() {
  rfHeader("JAMMER 433");
  tft.setTextSize(1);

  if (!rfReady) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(4, 50);
    tft.print("CC1101 indisponivel!");
    rfFooter();
    batteryDraw();
    return;
  }

  if (!jammerAtivo) {
    // Frequência detectada
    drawDetectedFreq(17);

    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(4, 32);
    char freqBuf[28];
    snprintf(freqBuf, sizeof(freqBuf), "Canal: %.2f MHz",
             rfDetectedMHz > 0 ? rfDetectedMHz : 433.92f);
    tft.print(freqBuf);

    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 48);
    tft.print("Inunda o canal RF com");
    tft.setCursor(4, 60);
    tft.print("sinais aleatorios,");
    tft.setCursor(4, 72);
    tft.print("bloqueando recepcao.");

    tft.setTextColor(TFT_RED);
    tft.setCursor(4, 96);
    tft.print("o = INICIAR JAMMER");

    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, SCR_H - 28);
    tft.print("^ = Voltar");
  } else {
    // Status ATIVO
    tft.setTextColor(TFT_RED);
    tft.setCursor(4, 30);
    tft.print("\xB7\xB7 JAMMER ATIVO \xB7\xB7");

    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(4, 48);
    char jfBuf[24];
    snprintf(jfBuf, sizeof(jfBuf), "%.2f MHz bloqueado",
             rfDetectedMHz > 0 ? rfDetectedMHz : 433.92f);
    tft.print(jfBuf);

    // Contador de pacotes
    tft.fillRect(4, 66, SCR_W - 8, 12, TFT_BLACK);
    char buf[24];
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(4, 66);
    snprintf(buf, sizeof(buf), "Pacotes: %lu", jammerPackets);
    tft.print(buf);

    tft.setTextColor(TFT_RED);
    tft.setCursor(4, 96);
    tft.print("o = PARAR");
  }

  rfFooter();
  batteryDraw();
}

void handleRF_Random() {
  if (jammerAtivo) {
    // ─── MODO ATIVO: envia ruído continuamente (não-bloqueante) ───
    // Envia código aleatório de 32 bits sem repetições adicionais
    rcSwitch.setRepeatTransmit(1);
    rcSwitch.send(esp_random(), 32);
    rcSwitch.setRepeatTransmit(10); // restaura
    jammerPackets++;

    // Atualiza contador na tela a cada 150ms (sem redesenhar tudo)
    if (millis() - jammerLastDraw > 150) {
      jammerLastDraw = millis();
      // Log periódico do jammer a cada ~1.5s (10 ticks x 150ms)
      static uint8_t _jamTick = 0;
      if (++_jamTick >= 10) {
        _jamTick = 0;
        float rssiTx = ELECHOUSE_cc1101.getRssi();
        Serial.printf("[RF][JAMMER] Pacotes: %lu  RSSI TX: %.1f dBm\n",
                      jammerPackets, rssiTx);
      }
      tft.fillRect(4, 66, SCR_W - 8, 12, TFT_BLACK);
      tft.setTextSize(1);
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(4, 66);
      char buf[24];
      snprintf(buf, sizeof(buf), "Pacotes: %lu", jammerPackets);
      tft.print(buf);

      // Pulso visual animado (barra que cresce e volta)
      static int pulse = 0;
      pulse = (pulse + 4) % (SCR_W - 12);
      tft.fillRect(4, 80, SCR_W - 8, 8, TFT_BLACK);
      tft.fillRect(4, 80, pulse, 8, TFT_RED);
    }

    // SELECT → parar jammer
    if (digitalRead(BUTTON_SELECT) == LOW &&
        (millis() - lastDebounceTime) > debounceDelay) {
      lastDebounceTime = millis();
      jammerAtivo = false;
      ELECHOUSE_cc1101.SetRx();
      Serial.printf(
          "[RF][JAMMER] Jammer PARADO. Total de pacotes enviados: %lu\n",
          jammerPackets);
      jammerPackets = 0;
      displayRF_Random();
    }

  } else {
    // ─── MODO INATIVO: aguarda acão ───
    if ((millis() - lastDebounceTime) > debounceDelay) {

      // SELECT → iniciar jammer
      if (digitalRead(BUTTON_SELECT) == LOW && rfReady) {
        lastDebounceTime = millis();
        // Sintoniza na freq detectada antes de transmitir
        float jamFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
        ELECHOUSE_cc1101.setMHZ(jamFreq);
        ELECHOUSE_cc1101.SetTx();
        jammerAtivo = true;
        jammerPackets = 0;
        jammerLastDraw = 0;
        Serial.printf("[RF][JAMMER] Jammer INICIADO em %.2f MHz\n", jamFreq);
        displayRF_Random();
      }

      // LEFT → voltar ao sub-menu RF
      if (digitalRead(BUTTON_LEFT) == LOW) {
        lastDebounceTime = millis();
        estadoAtual = MENU_RF;
        displayRF();
      }
    }
  }
}

// ════════════════════════════════════════════════
//  MODO: SAVED — lista e reproduz sinais salvos
//  LEFT=Voltar  SELECT=Transmitir  RIGHT=Próximo
//  RIGHT (2s) = Deletar sinal atual
// ════════════════════════════════════════════════
static RFSignal savedSignals[MAX_RF_SIGNALS];
static int savedCount = 0;
static int savedIndex = 0; // sinal selecionado atualmente

// Linhas visíveis na tela (de y=22 a y=130, 15px por linha → 7 linhas)
#define SAVED_VISIBLE 6
static int savedScroll = 0; // primeiro índice visível

static void drawSavedItem(int idx, bool sel) {
  if (idx < savedScroll || idx >= savedScroll + SAVED_VISIBLE ||
      idx >= savedCount)
    return;

  int i = idx - savedScroll;
  int y = 30 + i * 16;

  tft.fillRect(0, y - 2, SCR_W, 16, TFT_BLACK);

  tft.setTextColor(sel ? TFT_GREEN : TFT_WHITE);
  tft.setCursor(4, y);

  char buf[28];
  float f = (savedSignals[idx].freq > 0) ? savedSignals[idx].freq : 433.92f;
  // Ex: "#01 14FD58 433.92"
  snprintf(buf, sizeof(buf), "#%02d %lX %.2f", idx + 1,
           savedSignals[idx].value, f);
  tft.print(sel ? ">" : " ");
  tft.print(buf);

  // Redesenha os indicadores de scroll (pois o fillRect apagou a linha inteira)
  if (i == 0 && savedScroll > 0) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(SCR_W - 10, y);
    tft.print("^");
  }
  if (i == SAVED_VISIBLE - 1 && savedScroll + SAVED_VISIBLE < savedCount) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(SCR_W - 10, y);
    tft.print("v");
  }
}

static void drawSavedList() {
  // Limpa área da lista (começa em y=28 pra não apagar a freq detectada)
  tft.fillRect(0, 28, SCR_W, SCR_H - 46, TFT_BLACK);
  tft.setTextSize(1);

  if (savedCount == 0) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(8, 50);
    tft.print("Nenhum sinal salvo.");
    tft.setCursor(8, 66);
    tft.print("Capture em Replay");
    tft.setCursor(8, 78);
    tft.print("e pressione v");
    return;
  }

  for (int i = 0; i < SAVED_VISIBLE; i++) {
    int idx = savedScroll + i;
    if (idx >= savedCount)
      break;

    drawSavedItem(idx, idx == savedIndex);
  }

  // (Os indicadores de scroll já são desenhados dentro do drawSavedItem)
}

void displayRF_Saved() {
  rfHeader("SAVED RF");

  savedCount = loadRFSignals(savedSignals, MAX_RF_SIGNALS);
  savedIndex = 0;
  savedScroll = 0;

  drawSavedList();
  rfFooter();
  batteryDraw();

  // Sintoniza na frequência detectada para TX
  if (rfReady && rfDetectedMHz > 0) {
    ELECHOUSE_cc1101.setMHZ(rfDetectedMHz);
    ELECHOUSE_cc1101.SetRx();
  }
}

void handleRF_Saved() {
  static unsigned long leftPressStart = 0;
  static bool leftHeld = false;

  if ((millis() - lastDebounceTime) > debounceDelay) {

    // RIGHT → próximo sinal
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      if (savedCount > 0) {
        int oldIndex = savedIndex;
        int oldScroll = savedScroll;
        savedIndex = (savedIndex + 1) % savedCount;

        // Ajusta scroll
        if (savedIndex < savedScroll)
          savedScroll = savedIndex;
        if (savedIndex >= savedScroll + SAVED_VISIBLE)
          savedScroll = savedIndex - SAVED_VISIBLE + 1;

        if (savedScroll != oldScroll) {
          drawSavedList();
        } else {
          drawSavedItem(oldIndex, false);
          drawSavedItem(savedIndex, true);
        }
      }
    }

    // SELECT → transmite sinal selecionado
    if (digitalRead(BUTTON_SELECT) == LOW && savedCount > 0 && rfReady) {
      lastDebounceTime = millis();

      float txFreq = (savedSignals[savedIndex].freq > 0)
                         ? savedSignals[savedIndex].freq
                         : 433.92f;
      Serial.printf("[RF][SAVED] Transmitindo sinal #%d na %.2fMHz -> Val:%lu "
                    "Bits:%d Proto:%d\n",
                    savedIndex + 1, txFreq, savedSignals[savedIndex].value,
                    savedSignals[savedIndex].bits,
                    savedSignals[savedIndex].protocol);
      ELECHOUSE_cc1101.setMHZ(txFreq);
      ELECHOUSE_cc1101.SetTx();
      rcSwitch.send(savedSignals[savedIndex].value,
                    savedSignals[savedIndex].bits);
      ELECHOUSE_cc1101.SetRx();
      Serial.println("[RF][SAVED] TX concluido.");

      // Feedback visual
      tft.fillRect(4, SCR_H - 42, SCR_W - 8, 12, TFT_BLACK);
      tft.setTextColor(TFT_ORANGE);
      tft.setCursor(4, SCR_H - 42);
      tft.print(">>> ENVIADO! <<<");
      delay(700);

      // Restaura hint
      tft.fillRect(4, SCR_H - 42, SCR_W - 8, 12, TFT_BLACK);
    }
  }

  // Botão LEFT: pressão curta = voltar, pressão longa (>1s) = deletar
  if (digitalRead(BUTTON_LEFT) == LOW) {
    if (!leftHeld) {
      leftHeld = true;
      leftPressStart = millis();
    } else if (millis() - leftPressStart > 1000 && savedCount > 0) {
      // Pressão longa → deletar
      lastDebounceTime = millis();
      leftHeld = false;

      bool ok = deleteRFSignal(savedIndex);
      tft.fillRect(4, SCR_H - 42, SCR_W - 8, 12, TFT_BLACK);
      tft.setTextColor(ok ? TFT_RED : TFT_DARKGREY);
      tft.setCursor(4, SCR_H - 42);
      tft.print(ok ? "Sinal deletado!" : "Erro ao deletar");
      delay(900);

      // Recarrega lista
      savedCount = loadRFSignals(savedSignals, MAX_RF_SIGNALS);
      if (savedIndex >= savedCount)
        savedIndex = max(0, savedCount - 1);
      savedScroll = 0;
      drawSavedList();
      batteryDraw();
    }
  } else {
    if (leftHeld && (millis() - leftPressStart) < 600) {
      // Pressão curta solta → Voltar
      leftHeld = false;
      lastDebounceTime = millis();
      estadoAtual = MENU_RF;
      displayRF();
    }
    leftHeld = false;
  }
}
