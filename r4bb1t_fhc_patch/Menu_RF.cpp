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
static SPIClass spiCC(HSPI);
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

// ── SPIFFS: salvar sinal ─────────────────────────
static bool saveRFSignal(unsigned long val, int bits, int proto) {
  File f = SPIFFS.open(RF_SIGNALS_FILE, FILE_APPEND);
  if (!f)
    return false;
  f.printf("%lu,%d,%d\n", val, bits, proto);
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
    out[count].protocol = line.substring(c2 + 1).toInt();
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
    f.printf("%lu,%d,%d\n", buf[i].value, buf[i].bits, buf[i].protocol);
  }
  f.close();
  return true;
}

// ── Inicialização CC1101 ─────────────────────────
bool rfInit() {
  ELECHOUSE_cc1101.setSpiPin(RF_SCK, RF_MISO, RF_MOSI, RF_CS);
  ELECHOUSE_cc1101.setGDO(RF_GDO0, RF_GDO2);

  if (!ELECHOUSE_cc1101.getCC1101()) {
    Serial.println("[RF] CC1101 nao encontrado!");
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
  Serial.println("[RF] CC1101 OK @ 433.92 MHz");
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
    hwCC1101_ok = rfInit();
  }

  tft.fillScreen(C_BG);
  tft.setTextSize(1);
  drawHeader("RF 433", true);

  // Status CC1101 logo abaixo do header
  tft.setTextColor(rfReady ? C_GREEN : C_RED);
  tft.setCursor(4, 17);
  // tft.print(rfReady ? "CC1101 433.92MHz OK" : "CC1101: ERRO!");
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
      rfOpcao = (rfOpcao + 1) % RF_ITEMS;
      lastDebounceTime = millis();
      displayRF();
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
        rfOpcao = (rfOpcao - 1 + RF_ITEMS) % RF_ITEMS;
        displayRF();
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

  tft.setTextSize(1);
  if (!replayHasSig) {
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(4, 30);
    tft.print("Aguardando sinal...");
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 46);
    tft.print("Aponte o controle e");
    tft.setCursor(4, 58);
    tft.print("pressione o botao.");
  } else {
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(4, 30);
    tft.print("Capturado:");
    tft.setTextColor(TFT_GREEN);
    char buf[32];
    snprintf(buf, sizeof(buf), "Val: %lu", replayVal);
    tft.setCursor(4, 44);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "Bits:%d  Proto:%d", replayBits, replayProtocol);
    tft.setCursor(4, 56);
    tft.print(buf);

    tft.setTextColor(TFT_CYAN);
    tft.setCursor(4, 74);
    tft.print("o=TX  v=Salvar  ^=Vol");
  }

  if (!replayHasSig) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, SCR_H - 28);
    tft.print("^ = Voltar");
  }
  rfFooter();
  batteryDraw();
}

void handleRF_Replay() {
  // Recepção contínua
  if (rfReady && rcSwitch.available()) {
    replayVal = rcSwitch.getReceivedValue();
    replayBits = rcSwitch.getReceivedBitlength();
    replayProtocol = rcSwitch.getReceivedProtocol();
    rcSwitch.resetAvailable();
    replayHasSig = true;
    displayRF_Replay();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    // SELECT → retransmite
    if (digitalRead(BUTTON_SELECT) == LOW && replayHasSig) {
      lastDebounceTime = millis();
      ELECHOUSE_cc1101.SetTx();
      delay(5);
      rcSwitch.setProtocol(replayProtocol);
      rcSwitch.send(replayVal, replayBits);
      delay(5);
      ELECHOUSE_cc1101.SetRx();
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
      bool ok = saveRFSignal(replayVal, replayBits, replayProtocol);
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
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(4, 26);
  tft.print("Escutando pulsos...");
  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(4, SCR_H - 28);
  tft.print("^ = Voltar");
  rfFooter();
  rawListening = true;
  batteryDraw();
}

void handleRF_Raw() {
  if (rfReady && rcSwitch.available()) {
    unsigned long val = rcSwitch.getReceivedValue();
    int bits = rcSwitch.getReceivedBitlength();
    int proto = rcSwitch.getReceivedProtocol();
    rcSwitch.resetAvailable();

    tft.fillRect(0, 38, SCR_W, SCR_H - 60, TFT_BLACK);
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
    tft.print("Sinal:");
    int bars = constrain(map(proto, 1, 6, 2, 10), 2, 10);
    for (int b = 0; b < bars; b++) {
      tft.fillRect(50 + b * 7, 78, 5, 8, TFT_CYAN);
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
//  MODO: ANALISADOR — Waterfall Spectrum 433 MHz
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
#define WF_H 130
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
  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(68, 4);
  tft.print("433.92 MHz");
  tft.drawFastHLine(0, 14, SCR_W, TFT_DARKGREY);

  int axisY = WF_Y + WF_H + 1;
  tft.setTextColor(0x528A);
  tft.setCursor(0, axisY);
  tft.print("431");
  tft.setCursor(50, axisY);
  tft.print("433.9");
  tft.setCursor(104, axisY);
  tft.print("436");
  tft.drawFastHLine(0, axisY + 8, SCR_W, 0x2945);
  tft.drawFastVLine(WF_CX, axisY + 2, 6, TFT_YELLOW);

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

  wfRedraw();
  batteryDraw();
}

// ── Loop principal ────────────────────────────────
void handleRF_Analyser() {
  // Detecta sinal novo do rcSwitch
  if (rfReady && rcSwitch.available()) {
    rcSwitch.resetAvailable();
    wfSigCount++;
    wfSigActive = true;

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
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(4, 30);
    tft.print("Canal: 433.92 MHz");

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
    tft.print("433.92 MHz bloqueado");

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
      jammerPackets = 0;
      displayRF_Random();
    }

  } else {
    // ─── MODO INATIVO: aguarda acão ───
    if ((millis() - lastDebounceTime) > debounceDelay) {

      // SELECT → iniciar jammer
      if (digitalRead(BUTTON_SELECT) == LOW && rfReady) {
        lastDebounceTime = millis();
        ELECHOUSE_cc1101.SetTx();
        jammerAtivo = true;
        jammerPackets = 0;
        jammerLastDraw = 0;
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

static void drawSavedList() {
  // Limpa área da lista
  tft.fillRect(0, 20, SCR_W, SCR_H - 38, TFT_BLACK);
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

    bool sel = (idx == savedIndex);
    tft.setTextColor(sel ? TFT_GREEN : TFT_WHITE);
    tft.setCursor(4, 22 + i * 16);

    char buf[28];
    // "#01 Val:12345 B:24"
    snprintf(buf, sizeof(buf), "#%02d %lu B:%d", idx + 1,
             savedSignals[idx].value, savedSignals[idx].bits);
    tft.print(sel ? ">" : " ");
    tft.print(buf);
  }

  // Indicador de scroll (se há mais itens)
  if (savedScroll > 0) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(SCR_W - 10, 22);
    tft.print("^");
  }
  if (savedScroll + SAVED_VISIBLE < savedCount) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(SCR_W - 10, 22 + (SAVED_VISIBLE - 1) * 16);
    tft.print("v");
  }
}

void displayRF_Saved() {
  rfHeader("SAVED RF");
  savedCount = loadRFSignals(savedSignals, MAX_RF_SIGNALS);
  savedIndex = 0;
  savedScroll = 0;

  drawSavedList();

  // Dica de ação
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(4, SCR_H - 28);
  if (savedCount > 0)
    tft.print("^x2=Del   o=TX   v=Prox");
  else
    tft.print("^ = Voltar");

  rfFooter();
  batteryDraw();
}

void handleRF_Saved() {
  static unsigned long leftPressStart = 0;
  static bool leftHeld = false;

  if ((millis() - lastDebounceTime) > debounceDelay) {

    // RIGHT → próximo sinal
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      if (savedCount > 0) {
        savedIndex = (savedIndex + 1) % savedCount;
        // Ajusta scroll
        if (savedIndex < savedScroll)
          savedScroll = savedIndex;
        if (savedIndex >= savedScroll + SAVED_VISIBLE)
          savedScroll = savedIndex - SAVED_VISIBLE + 1;
        drawSavedList();
        batteryDraw();
      }
    }

    // SELECT → transmite sinal selecionado
    if (digitalRead(BUTTON_SELECT) == LOW && savedCount > 0) {
      lastDebounceTime = millis();

      ELECHOUSE_cc1101.SetTx();
      rcSwitch.send(savedSignals[savedIndex].value,
                    savedSignals[savedIndex].bits);
      ELECHOUSE_cc1101.SetRx();

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
