// Menu_RF.cpp — módulo RF 433 MHz para r4bb1t
// Usa CC1101 via HSPI (pinos livres) + RCSwitch
// Display: TFT_eSPI 128×160

#include "Menu_RF.h"
#include "Globals.h"
#include "Menu_Main.h"

#include "Battery.h"
#include "Config.h"
#include "HWProbe.h"
#include "Language.h"
#include "UI.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>
#include <SPI.h>
#include <SPIFFS.h>

// ── Objetos ──────────────────────────────────────
static RCSwitch rcSwitch;
bool rfReady = false;

// ── Flags de ciclo de vida do CC1101 ──────────────────────────
// Escopo de arquivo (não static local) para que nrfDeinit() possa resetá-las.
static bool rfInitDone = false; // rfInit() já foi executado com sucesso
bool rfNeedsReinit = false;     // nrfDeinit() seta; displayRF() consome

// ── Layout ───────────────────────────────────────
#define SCR_W 128
#define SCR_H 160

// ── Arquivo de sinais salvos ──────────────────────
#define RF_SIGNALS_FILE "/rf_signals.txt"
#define MAX_RF_SIGNALS 20

// ── Raw pulse capture ────────────────────────────
#define RAW_BUF_SIZE 512        // max pulsos por pacote
#define RAW_MIN_PULSES 16       // minimo para ser sinal valido
#define RAW_GAP_MS 12           // silencio de 12ms = fim do pacote
#define RAW_PULSE_MIN_US 100    // menor pulso valido (filtra ruido)
#define RAW_PULSE_MAX_US 100000 // maior pulso valido (filtra silencio)
#define RAW_SIGNALS_FILE "/rf_raw.txt"

struct RFSignal {
  unsigned long value;
  int bits;
  int protocol;
  float freq;
  unsigned int delay; // pulseLength real capturado (us)
};

// ── Struct para Raw Signals ────────────────────────
#define MAX_RF_RAW_SIGNALS 10
#define RAW_MAX_PULSES 256

struct RFRawSignal {
  unsigned int pulses[RAW_MAX_PULSES];
  int count;
  float freq;
};

// ── Sub-menu RF ──────────────────────────────────
// Índices: 0=Voltar 1=Replay 2=Raw 3=Analyser 4=Random 5=Saved
static const int RF_ITEMS = 6;
static int rfOpcao = 0;

// Cor e label de cada item
static const char *rfLabels_raw[] = {"< Voltar", "Capturar", "Sniffer",
                                     "Grafico",  "Jammer",   "Salvos"};

static const char *getRFLabel(int i) {
  switch (i) {
  case 0:
    return lang->rf_itm_voltar;
  case 1:
    return lang->rf_itm_capturar;
  case 2:
    return lang->rf_itm_sniffer;
  case 3:
    return lang->rf_itm_grafico;
  case 4:
    return lang->rf_itm_jammer;
  case 5:
    return lang->rf_itm_salvos;
  default:
    return rfLabels_raw[i];
  }
}
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
static bool saveRFSignal(unsigned long val, int bits, int proto, float freq,
                         unsigned int delay = 0) {
  File f = SPIFFS.open(RF_SIGNALS_FILE, FILE_APPEND);
  if (!f)
    return false;
  f.printf("%lX,%d,%d,%.2f,%u\n", val, bits, proto, freq, delay);
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
    out[count].value = strtoul(line.substring(0, c1).c_str(), nullptr, 16);
    out[count].bits = line.substring(c1 + 1, c2).toInt();

    int c3 = line.indexOf(',', c2 + 1);
    if (c3 > 0) {
      int c4 = line.indexOf(',', c3 + 1);
      if (c4 > 0) {
        out[count].protocol = line.substring(c2 + 1, c3).toInt();
        out[count].freq = line.substring(c3 + 1, c4).toFloat();
        out[count].delay = (unsigned int)line.substring(c4 + 1).toInt();
      } else {
        out[count].protocol = line.substring(c2 + 1, c3).toInt();
        out[count].freq = line.substring(c3 + 1).toFloat();
        out[count].delay = 0; // arquivo antigo sem delay
      }
    } else {
      out[count].protocol = line.substring(c2 + 1).toInt();
      out[count].freq = 433.92f;
      out[count].delay = 0;
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
    f.printf("%lX,%d,%d,%.2f,%u\n", buf[i].value, buf[i].bits, buf[i].protocol,
             buf[i].freq, buf[i].delay);
  }
  f.close();
  return true;
}

// ── SPIFFS: RAW Signals ──────────────────────────
static bool saveRFRawSignal(const unsigned int *pulses, int count, float freq) {
  File f = SPIFFS.open(RAW_SIGNALS_FILE, FILE_APPEND);
  if (!f)
    return false;
  f.printf("%.2f,%d", freq, count);
  for (int i = 0; i < count; i++) {
    f.printf(",%u", pulses[i]);
  }
  f.print("\n");
  f.close();
  return true;
}

static int loadRFRawSignals(RFRawSignal *out, int maxCount) {
  File f = SPIFFS.open(RAW_SIGNALS_FILE, FILE_READ);
  if (!f)
    return 0;
  int count = 0;
  while (f.available() && count < maxCount) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;
    int c1 = line.indexOf(',');
    if (c1 < 0)
      continue;
    out[count].freq = line.substring(0, c1).toFloat();
    int c2 = line.indexOf(',', c1 + 1);
    if (c2 < 0)
      continue;
    out[count].count = line.substring(c1 + 1, c2).toInt();
    if (out[count].count > RAW_MAX_PULSES)
      out[count].count = RAW_MAX_PULSES;

    int lastC = c2;
    for (int i = 0; i < out[count].count; i++) {
      int nextC = line.indexOf(',', lastC + 1);
      if (nextC < 0) {
        out[count].pulses[i] = (unsigned int)line.substring(lastC + 1).toInt();
        break;
      } else {
        out[count].pulses[i] =
            (unsigned int)line.substring(lastC + 1, nextC).toInt();
        lastC = nextC;
      }
    }
    count++;
  }
  f.close();
  return count;
}

static bool deleteRFRawSignal(int index) {
  RFRawSignal *buf = new RFRawSignal[MAX_RF_RAW_SIGNALS];
  if (!buf)
    return false;

  int count = loadRFRawSignals(buf, MAX_RF_RAW_SIGNALS);
  if (index < 0 || index >= count) {
    delete[] buf;
    return false;
  }

  File f = SPIFFS.open(RAW_SIGNALS_FILE, FILE_WRITE);
  if (!f) {
    delete[] buf;
    return false;
  }
  for (int i = 0; i < count; i++) {
    if (i == index)
      continue;
    f.printf("%.2f,%d", buf[i].freq, buf[i].count);
    for (int j = 0; j < buf[i].count; j++) {
      f.printf(",%u", buf[i].pulses[j]);
    }
    f.print("\n");
  }
  f.close();
  delete[] buf;
  return true;
}

// ── Inicialização CC1101 ─────────────────────────
bool rfInit() {
  Serial.println("[RF] Iniciando CC1101...");
  Serial.printf("[RF] Pinos: SCK=%d MISO=%d MOSI=%d CS=%d GDO0=%d GDO2=%d\n",
                RF_SCK, RF_MISO, RF_MOSI, RF_CS, RF_GDO0, RF_GDO2);

  ELECHOUSE_cc1101.setSpiPin(RF_SCK, RF_MISO, RF_MOSI, RF_CS);
  ELECHOUSE_cc1101.setGDO(RF_GDO0, RF_GDO2);

  // Init() DEVE vir antes de getCC1101():
  // Init() inicializa o spiCC(HSPI) via spiCC.end()+spiCC.begin().
  // Sem isso, spiCC nao esta "begun" e getCC1101() le 0xFF (SPI inativo).
  ELECHOUSE_cc1101.Init();

  // Agora o spiCC esta pronto — verifica se o chip responde com VERSION valido
  if (!ELECHOUSE_cc1101.getCC1101()) {
    Serial.println("[RF] ERRO: CC1101 nao encontrado! Verifique a fiacao SPI.");
    rfReady = false;
    return false;
  }

  ELECHOUSE_cc1101.setMHZ(433.92);
  ELECHOUSE_cc1101.SetRx();

  // GDO0 e o pino de DADOS do CC1101. Em RX, o CC1101 pilota GDO0 (e OUTPUT
  // do CC1101). O ESP32 deve estar em INPUT para nao conflitar.
  // setGDO() define GDO0 como OUTPUT (padrao da lib para TX). Corrigimos aqui.
  pinMode(RF_GDO0, INPUT);
  rcSwitch.enableReceive(RF_GDO0);
  rcSwitch.setRepeatTransmit(10);

  rfReady = true;

  // Le RSSI inicial para confirmar comunicacao SPI
  float rssiInit = ELECHOUSE_cc1101.getRssi();
  float lqiInit = ELECHOUSE_cc1101.getLqi();
  Serial.println("[RF] CC1101 OK @ 433.92 MHz");
  Serial.printf("[RF] RSSI inicial: %.1f dBm  |  LQI: %.0f\n", rssiInit,
                lqiInit);
  return true;
}

// ════════════════════════════════════════════════
//  Re-inicialização robusta do CC1101
//  Recarrega TODOS os registradores internos e coloca em RX.
//  Deve ser chamada sempre que o barramento HSPI foi compartilhado com o
//  nRF24, ou quando o gráfico indicar RSSI espúrio (todo vermelho).
// ════════════════════════════════════════════════
void rfReinit() {
  if (!hwCC1101_ok) {
    Serial.println("[RF][REINIT] CC1101 não foi inicializado, abortando.");
    return;
  }

  Serial.println("[RF][REINIT] Recarregando registradores do CC1101...");

  // Garante que nenhum outro dispositivo segura o barramento
  // antes de reclamar o CS para o CC1101.
  pinMode(RF_CS, OUTPUT);
  digitalWrite(RF_CS, HIGH);
  delay(5);

  // ── FIX VITAL ──
  // O nRF24 usa spiJam (HSPI) nos mesmos pinos físicos. Quando nrfDeinit()
  // chama spiJam.end(), a matriz de pinos do ESP32 desconecta as GPIOs 33, 19
  // e 13. A biblioteca SmartRC-CC1101 usa o objeto global SPI (VSPI) e possui
  // uma flag 'spi_initialized' que impede chamar SPI.begin() de novo. Portanto,
  // os pinos ficariam "órfãos" e o getCC1101() falharia. A solução é forçar o
  // reinício do VSPI nestes pinos:
  SPI.end();
  SPI.begin(RF_SCK, RF_MISO, RF_MOSI, -1);

  // Init() executa: reset HW + gravação
  // completa de TODOS os registradores de fábrica. Estado completamente limpo.
  ELECHOUSE_cc1101.setSpiPin(RF_SCK, RF_MISO, RF_MOSI, RF_CS);
  ELECHOUSE_cc1101.setGDO(RF_GDO0, RF_GDO2);
  ELECHOUSE_cc1101.Init();
  delay(10); // settling do PLL após reset de hardware

  // Verifica se o chip ainda responde com VERSION válido após o reload
  if (!ELECHOUSE_cc1101.getCC1101()) {
    Serial.println("[RF][REINIT] ERRO: CC1101 não respondeu após reinit!");
    rfReady = false;
    hwCC1101_ok = false;
    rfNeedsReinit = false;
    return;
  }

  // Restaura a frequência previamente detectada (ou padrão 433.92) e RX
  float rxFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
  ELECHOUSE_cc1101.setMHZ(rxFreq);
  ELECHOUSE_cc1101.SetRx();

  // GDO0 é pilotado pelo CC1101 em modo RX → ESP32 deve ficar como INPUT
  pinMode(RF_GDO0, INPUT);
  rcSwitch.enableReceive(RF_GDO0);

  rfReady = true;
  hwCC1101_ok = true;
  rfNeedsReinit = false;

  float rssi = ELECHOUSE_cc1101.getRssi();
  Serial.printf("[RF][REINIT] OK @ %.2f MHz | RSSI: %.1f dBm\n", rxFreq, rssi);
}

// ════════════════════════════════════════════════
//  TELA: Sub-menu RF
//  Padrão visual do projeto (estilo Menu_Attacks):
//   • barra colorida 5px à esquerda
//   • fundo escuro no item selecionado
//   • primeiro item = Voltar
// ════════════════════════════════════════════════
void displayRF() {
  // ── Garante que o CC1101 está inicializado e com registradores válidos ──
  if (!rfInitDone) {
    // Primeira entrada: detecta o chip e configura o barramento
    rfInitDone = true;
    rfReady = false;
    hwCC1101_ok = rfInit();
  } else if (rfNeedsReinit) {
    // Retorno do modo nRF24: HSPI foi compartilhado, registradores podem
    // estar corrompidos. Faz reload completo antes de qualquer leitura.
    rfReinit();
  }

  tft.fillScreen(C_BG);
  tft.setTextSize(1);
  drawHeader(lang->rf_hdr_subghz, true);

  tft.drawFastHLine(0, 26, SCR_W, C_GREY);

  // Lista de itens com drawMenuItem
  for (int i = 0; i < RF_ITEMS; i++) {
    drawMenuItem(0, 28 + i * 19, 128, 18, getRFLabel(i), i == rfOpcao);
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
      drawMenuItem(0, 28 + old * 19, 128, 18, getRFLabel(old), false);
      drawMenuItem(0, 28 + rfOpcao * 19, 128, 18, getRFLabel(rfOpcao), true);
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
        drawMenuItem(0, 28 + old * 19, 128, 18, getRFLabel(old), false);
        drawMenuItem(0, 28 + rfOpcao * 19, 128, 18, getRFLabel(rfOpcao), true);
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
static unsigned int replayDelay = 0; // pulseLength real capturado (us)
static bool replayHasSig = false;

void displayRF_Replay() {
  rfHeader(lang->rf_hdr_replay);

  // Frequência agora exibida junto com os dados capturados

  tft.setTextSize(1);
  if (!replayHasSig) {
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(4, 32);
    tft.print(lang->rf_rpl_aguardando);
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 48);
    tft.print(lang->rf_rpl_hint1);
    tft.setCursor(4, 60);
    tft.print(lang->rf_rpl_hint2);
  } else {
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(4, 32);
    tft.print(lang->rf_rpl_capturado);
    tft.setTextColor(TFT_GREEN);
    char buf[32];
    snprintf(buf, sizeof(buf), "Val: %lu", replayVal);
    tft.setCursor(4, 46);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "Hex: 0x%lX", replayVal);
    tft.setCursor(4, 56);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "Bits:%d  Proto:%d", replayBits, replayProtocol);
    tft.setCursor(4, 66);
    tft.print(buf);
    float txFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
    snprintf(buf, sizeof(buf), "Freq: %.2f", txFreq);
    tft.setCursor(4, 76);
    tft.print(buf);

    tft.setTextColor(TFT_CYAN);
    tft.setCursor(4, 86);
    tft.print(lang->rf_rpl_hint_tx);
  }

  if (!replayHasSig) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, SCR_H - 28);
    tft.print(lang->rf_hint_voltar);
  } else {
    // Indicação do botão salvar no canto direito
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(SCR_W - 40, SCR_H - 28);
    tft.print("Salvar");
  }
  rfFooter();
  batteryDraw();

  // Sempre re-habilita RX ao entrar no modo Replay.
  // Se ja foi feito scan, sintoniza na freq detectada; senao usa 433.92 MHz.
  if (rfReady) {
    float rxFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
    ELECHOUSE_cc1101.setMHZ(rxFreq);
    ELECHOUSE_cc1101.SetRx();
    pinMode(RF_GDO0, INPUT); // CC1101 pilota GDO0 em RX
    rcSwitch.enableReceive(RF_GDO0);
    Serial.printf(
        "[RF][REPLAY] Escutando em %.2f MHz | GDO0=GPIO%d INPUT+INT\n", rxFreq,
        RF_GDO0);
  }
}

void handleRF_Replay() {
  // Recepção contínua
  if (rfReady && rcSwitch.available()) {
    replayVal = rcSwitch.getReceivedValue();
    replayBits = rcSwitch.getReceivedBitlength();
    replayProtocol = rcSwitch.getReceivedProtocol();
    replayDelay = rcSwitch.getReceivedDelay(); // pulseLength real
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
      rcSwitch.disableReceive();
      rcSwitch.enableTransmit(RF_GDO0);
      rcSwitch.setProtocol(replayProtocol, replayDelay > 0 ? replayDelay : 350);
      rcSwitch.send(replayVal, replayBits);
      rcSwitch.disableTransmit();
      pinMode(RF_GDO0, INPUT);
      rcSwitch.enableReceive(RF_GDO0);
      delay(5);
      ELECHOUSE_cc1101.SetRx();
      Serial.println("[RF][REPLAY] TX concluido, voltando para RX.");
      tft.fillRect(4, 92, SCR_W - 8, 12, TFT_BLACK);
      tft.setTextColor(TFT_ORANGE);
      tft.setCursor(4, 92);
      tft.print(lang->rf_rpl_enviado);
      delay(700);
      displayRF_Replay();
    }

    // RIGHT → salva no SPIFFS
    if (digitalRead(BUTTON_RIGHT) == LOW && replayHasSig) {
      lastDebounceTime = millis();
      float saveFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
      bool ok = saveRFSignal(replayVal, replayBits, replayProtocol, saveFreq,
                             replayDelay);
      Serial.printf("[RF][REPLAY] Salvar no SPIFFS: %s\n", ok ? "OK" : "ERRO");
      tft.fillRect(4, 92, SCR_W - 8, 12, TFT_BLACK);
      tft.setTextColor(ok ? TFT_GREEN : TFT_RED);
      tft.setCursor(4, 92);
      tft.print(ok ? lang->rf_rpl_salvo : lang->rf_rpl_erro_salvar);
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
//  Dupla captura simultânea:
//    1) RCSwitch (ISR) → protocolos conhecidos
//    2) Polling GDO0   → qualquer sinal (alarme, FSK...)
// ════════════════════════════════════════════════
static bool rawListening = true;

// ── Estado da captura raw ────────────────────────
static unsigned int rawBuf[RAW_BUF_SIZE]; // durações em µs
static int rawCount = 0;                  // pulsos no buffer atual
static bool rawHasSig = false;            // pacote bruto completo?
static int rawSavedMin = 0;               // duracao minima do pacote salvo
static int rawSavedMax = 0;               // duracao maxima do pacote salvo
static int rawSavedCnt = 0;               // contagem do pacote salvo
static int rawLastPin = HIGH;             // estado anterior do GDO0
static unsigned long rawLastEdge = 0;     // micros() da ultima borda
static unsigned long rawLastAct = 0;      // millis() da ultima atividade

void displayRF_Raw() {
  rfHeader(lang->rf_hdr_raw);

  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(4, 30);
  tft.print(lang->rf_raw_escutando);

  // Separador e label da secao raw
  tft.drawFastHLine(0, 88, SCR_W, 0x2945); // cinza escuro
  tft.setTextColor(0x4A69);                // cinza medio
  tft.setCursor(4, 91);
  tft.print("RAW pulsos:");

  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(4, SCR_H - 28);
  tft.print("< Voltar");
  rfFooter();
  rawListening = true;

  // Reseta estado raw ao entrar na tela
  rawCount = 0;
  rawHasSig = false;
  rawLastPin = digitalRead(RF_GDO0);
  rawLastEdge = micros();
  rawLastAct = millis();

  batteryDraw();

  // Sempre re-habilita RX ao entrar no modo Raw.
  // Se ja foi feito scan, sintoniza na freq detectada; senao usa 433.92 MHz.
  if (rfReady) {
    float rxFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
    ELECHOUSE_cc1101.setMHZ(rxFreq);
    ELECHOUSE_cc1101.SetRx();
    pinMode(RF_GDO0, INPUT);
    rcSwitch.enableReceive(RF_GDO0);
    Serial.printf("[RF][RAW] Escutando em %.2f MHz | GDO0=GPIO%d INPUT+INT\n",
                  rxFreq, RF_GDO0);
  }
}

void handleRF_Raw() {
  static int peakRSSI = -100;
  static unsigned long lastRssiPoll = 0;
  static unsigned int rawBufSaved[RAW_BUF_SIZE];
  int sw = 8 * 6; // "> SALVAR" = 8 chars * 6 pixels

  // ── Polling RSSI a cada 5ms ───────────────────────
  if (rfReady && (millis() - lastRssiPoll > 5)) {
    lastRssiPoll = millis();
    int currentRssi = (int)ELECHOUSE_cc1101.getRssi();
    if (currentRssi > peakRSSI)
      peakRSSI = currentRssi;
  }

  // ── Captura raw de pulsos (polling GDO0) ─────────
  // Roda simultaneamente ao RCSwitch (que usa ISR).
  // O polling le o mesmo pino sem conflitar com a ISR.
  {
    int pinNow = digitalRead(RF_GDO0);
    unsigned long nowUs = micros();

    if (pinNow != rawLastPin) {
      unsigned int dur = (unsigned int)(nowUs - rawLastEdge);
      rawLastEdge = nowUs;
      rawLastPin = pinNow;
      rawLastAct = millis();

      if (dur >= RAW_PULSE_MIN_US && dur <= RAW_PULSE_MAX_US) {
        if (rawCount < RAW_BUF_SIZE)
          rawBuf[rawCount++] = dur;
      }
    }

    // Fim do pacote detectado pelo gap de silencio
    if (rawCount >= RAW_MIN_PULSES && (millis() - rawLastAct) > RAW_GAP_MS) {

      // Calcula min/max do pacote
      int mn = rawBuf[0], mx = rawBuf[0];
      for (int i = 1; i < rawCount; i++) {
        if ((int)rawBuf[i] < mn)
          mn = rawBuf[i];
        if ((int)rawBuf[i] > mx)
          mx = rawBuf[i];
      }
      rawSavedMin = mn;
      rawSavedMax = mx;
      rawSavedCnt = rawCount;
      rawHasSig = true;

      float rxFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
      Serial.printf("[RF][RAW-PULSE] %d pulsos | min:%dus max:%dus @ %.2fMHz\n",
                    rawSavedCnt, rawSavedMin, rawSavedMax, rxFreq);

      // Atualiza secao RAW na tela (area Y=91 a 137)
      tft.fillRect(0, 91, SCR_W, 46, TFT_BLACK);
      tft.setTextSize(1);
      tft.setTextColor(TFT_MAGENTA);
      tft.setCursor(4, 91);
      char rbuf[32];
      snprintf(rbuf, sizeof(rbuf), "RAW pulsos:");
      tft.print(rbuf);

      tft.setTextColor(TFT_WHITE);
      tft.setCursor(4, 102);
      snprintf(rbuf, sizeof(rbuf), "%d pulsos capturados", rawSavedCnt);
      tft.print(rbuf);

      tft.setTextColor(TFT_CYAN);
      tft.setCursor(4, 113);
      snprintf(rbuf, sizeof(rbuf), "Min:%dus", rawSavedMin);
      tft.print(rbuf);
      tft.setCursor(68, 113);
      snprintf(rbuf, sizeof(rbuf), "Max:%dus", rawSavedMax);
      tft.print(rbuf);

      // Hint de salvar no canto direito
      tft.setTextColor(TFT_YELLOW);
      tft.setCursor(SCR_W - sw - 4, 135);
      tft.print("> SALVAR");

      // Salvar os tempos exatos do pacote no buffer secundário
      for (int i = 0; i < rawSavedCnt; i++) {
        rawBufSaved[i] = rawBuf[i];
      }

      // Reseta buffer primário para proximo pacote (mas mantem rawHasSig=true)
      rawCount = 0;
      rawLastAct = millis(); // evita re-trigger imediato
    }
  }

  // ── Salvar RAW manualmente ───────────────────────
  if (rawHasSig && digitalRead(BUTTON_RIGHT) == LOW) {
    if ((millis() - lastDebounceTime) > debounceDelay) {
      lastDebounceTime = millis();
      float rxFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
      bool ok = saveRFRawSignal(rawBufSaved, rawSavedCnt, rxFreq);

      tft.fillRect(0, 135, SCR_W, 10, TFT_BLACK);
      tft.setTextColor(ok ? TFT_GREEN : TFT_RED);
      tft.setCursor(SCR_W - sw - 4, 135);
      tft.print(ok ? "SALVO!  " : "ERRO!   ");
      delay(500);

      tft.fillRect(0, 135, SCR_W, 10, TFT_BLACK);
      tft.setTextColor(TFT_YELLOW);
      tft.setCursor(SCR_W - sw - 4, 135);
      tft.print("> SALVAR");
    }
  }

  // ── RCSwitch: protocolos conhecidos ──────────────
  if (rfReady && rcSwitch.available()) {
    unsigned long val = rcSwitch.getReceivedValue();
    int bits = rcSwitch.getReceivedBitlength();
    int proto = rcSwitch.getReceivedProtocol();
    rcSwitch.resetAvailable();

    int dbm = peakRSSI;
    if (dbm == -100)
      dbm = (int)ELECHOUSE_cc1101.getRssi();
    float lqi = ELECHOUSE_cc1101.getLqi();
    peakRSSI = -100;

    Serial.println("[RF][RAW] Sinal decodificado (RCSwitch):");
    Serial.printf("[RF][RAW]   Dec     : %lu\n", val);
    Serial.printf("[RF][RAW]   Hex     : 0x%lX\n", val);
    Serial.printf("[RF][RAW]   Bits    : %d  |  Protocolo: %d\n", bits, proto);
    Serial.printf("[RF][RAW]   RSSI    : %d dBm  |  LQI: %.0f\n", dbm, lqi);

    // Atualiza area RCSwitch (Y=30 a 87)
    tft.fillRect(0, 30, SCR_W, 58, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN);
    char buf[32];
    snprintf(buf, sizeof(buf), "Dec: %lu", val);
    tft.setCursor(4, 30);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "Hex: 0x%lX", val);
    tft.setCursor(4, 41);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "Bits:%d  Proto:%d", bits, proto);
    tft.setCursor(4, 52);
    tft.print(buf);
    float txFreq = (rfDetectedMHz > 0) ? rfDetectedMHz : 433.92f;
    snprintf(buf, sizeof(buf), "Freq:%.2f", txFreq);
    tft.setCursor(4, 63);
    tft.print(buf);

    // Linha RSSI + barras
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 75);
    char sbuf[12];
    snprintf(sbuf, sizeof(sbuf), "dBm:%d", dbm);
    tft.print(sbuf);
    int bars = constrain(map(dbm, -85, -18, 1, 10), 1, 10);
    for (int b = 0; b < 10; b++) {
      if (b < bars)
        tft.fillRect(55 + b * 7, 75, 6, 7, TFT_CYAN);
      else
        tft.drawRect(55 + b * 7, 75, 6, 7, TFT_DARKGREY);
    }
  }

  // ── Botoes ───────────────────────────────────────
  if ((millis() - lastDebounceTime) > debounceDelay) {

    // LEFT → volta ao sub-menu RF
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

// Frequencias acessiveis com antena de 433 MHz de fabrica:
// - 315 MHz: detectavel a curta distancia (~1m com antena 433 MHz)
// - Banda 433 MHz: faixa nominal com sensibilidade plena
// 868/915 MHz OMITIDOS: antena de 433 MHz perde >10 dB nesses valores.
static const float SCAN_FREQS[] = {315.00, 433.00, 433.42,
                                   433.92, 434.42, 435.00};

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

  // Lê RSSI buscando o pico (peak hold) em vez de média
  // Fundamental para controles OOK/ASK, cujos pulsos rápidos são diluídos na
  // média
  int rssi = -120;
  for (int i = 0; i < 5; i++) {
    int cur = ELECHOUSE_cc1101.getRssi();
    if (cur > rssi)
      rssi = cur;
    delayMicroseconds(300);
  }

  if (rssi > scanBestRSSI) {
    scanBestRSSI = rssi;
    scanBestMHz = SCAN_FREQS[scanIdx];
  }

  scanIdx++;
  if (scanIdx >= (int)SCAN_FREQ_COUNT) {
    scanIdx = 0;

    // Atualiza frequência detectada se o pico é acima do ruído
    // -76 dBm: alinhado com o threshold visual do gráfico (rssiBase > 35)
    if (scanBestRSSI > -76) {
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

#define WF_Y 20
#define WF_H 120
#define WF_CX                                                                  \
  (128 /                                                                       \
   2) // Usando valor fixo 128 para SCR_W temporariamente ou calculando direto

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
    // Escala clássica enquanto escaneia
    tft.setTextColor(0x528A);
    tft.setCursor(0, y + 3);
    tft.print("431");
    tft.setCursor(50, y + 3);
    tft.print("433.9");
    tft.setCursor(104, y + 3);
    tft.print("436");
    tft.drawFastVLine(WF_CX, y, 3, TFT_YELLOW);
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
#define WF_DBM_MIN -85 // sinal muito fraco, proximo do limite
#define WF_DBM_MAX -18 // sinal muito forte, controle colado na antena

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
  // Re-inicializa o CC1101 SEMPRE que o Analisador é aberto.
  // Garante que os registradores AGCCTRL/FSCAL estão íntegros e que o
  // barramento HSPI está corretamente configurado, independente de qual
  // modo RF foi usado antes (nRF24 Jammer, Replay, Raw, etc.).
  rfReinit();

  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(2, 4);
  tft.print(lang->rf_hdr_analyser);

  // Mostra frequência no header (detectada ou padrão)
  tft.setTextColor(rfDetectedMHz > 0 ? TFT_GREEN : TFT_DARKGREY);
  tft.setCursor(68, 4);
  char hdrBuf[16];
  if (rfDetectedMHz > 0)
    snprintf(hdrBuf, sizeof(hdrBuf), "%.2f", rfDetectedMHz);
  else
    snprintf(hdrBuf, sizeof(hdrBuf), "%s", lang->rf_anl_scan);
  tft.print(hdrBuf);
  tft.drawFastHLine(0, 14, SCR_W, TFT_DARKGREY);

  // Barra separadora entre o gráfico e o rodapé
  int axisY = WF_Y + WF_H + 1;
  tft.drawFastHLine(0, axisY, SCR_W, TFT_DARKGREY);

  // Barra de frequência no rodapé (fica entre a linha e o limite inferior)
  drawDetectedFreq(axisY + 2);

  // Ícone de voltar no canto inferior esquerdo
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, SCR_H - 9);
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

      // Atualiza barra de frequência no rodapé (preservando a linha separadora)
      int axisY = WF_Y + WF_H + 1;
      drawDetectedFreq(axisY + 2);

      // Re-habilita o rcSwitch na frequencia detectada (GDO0 = DATA pin)
      pinMode(RF_GDO0, INPUT);
      rcSwitch.enableReceive(RF_GDO0);
    }
  }

  // Detecta sinal novo do rcSwitch (Apenas para contador/log, o grafico usa
  // RSSI bruto)
  if (rfReady && rcSwitch.available()) {
    rcSwitch.resetAvailable();
    wfSigCount++;

    float rssi = ELECHOUSE_cc1101.getRssi();
    float lqi = ELECHOUSE_cc1101.getLqi();
    Serial.printf("[RF][ANALYSER] Sinal RC #%lu  RSSI: %.1f dBm  LQI: %.0f\n",
                  wfSigCount, rssi, lqi);

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

    // O grafico agora reage diretamente a energia RF presente no ar
    if (rssiBase >
        35) { // 35 na escala equivale a aprox -76 dBm no mapa -85/-18
      wfSigActive = true;
      if (wfEnv == ENV_IDLE || wfEnv == ENV_DECAY) {
        wfEnv = ENV_ATTACK;
        wfEnvStep = 0;
      }
    } else {
      wfSigActive = false;
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
      // Passa para decay assim que a energia RF (RSSI) baixar novamente
      if (!wfSigActive) {
        wfEnv = ENV_DECAY;
        wfEnvStep = 0;
      }
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
      pinMode(RF_GDO0, INPUT);
      rcSwitch.enableReceive(RF_GDO0);
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
static float jamSelectedMHz =
    433.92f;               // frequência atualmente selecionada no menu
static int jamFreqSel = 0; // 0 = 433.92, 1 = detectada/expansível

static const float jamExtraFreqs[] = {315.00f, 433.07f, 433.22f,
                                      433.42f, 434.42f, 434.77f,
                                      438.90f, 868.00f, 915.00f};
static const int numExtraFreqs =
    sizeof(jamExtraFreqs) / sizeof(jamExtraFreqs[0]);
static bool jamFreqExpanded = false;
static int jamScroll = 0;

void displayRF_Random(bool fullRedraw) {
  if (fullRedraw) {
    rfHeader(lang->rf_hdr_jammer);
    tft.setTextSize(1);
  }

  if (!rfReady) {
    if (fullRedraw) {
      tft.setTextColor(TFT_RED);
      tft.setCursor(4, 50);
      tft.print(lang->rf_jam_indisponivel);
      rfFooter();
      batteryDraw();
    }
    return;
  }

  if (!jammerAtivo) {
    bool hasTwo = (rfDetectedMHz > 0 && (int)(rfDetectedMHz * 100) != 43392);
    int maisIdx = hasTwo ? 2 : 1;
    int totalItems = maisIdx + 1 + (jamFreqExpanded ? numExtraFreqs : 0);

    if (fullRedraw) {
      // Item 0: Frequência padrão (433.92) FIXA no topo
      drawMenuItem(0, 24, 128, 19, "433.92 (Padrao)", jamFreqSel == 0, true);

      if (hasTwo) {
        char capBuf[24];
        snprintf(capBuf, sizeof(capBuf), "%.2f (Capturada)", rfDetectedMHz);
        // FIXA logo abaixo do Padrão
        drawMenuItem(0, 44, 128, 19, capBuf, jamFreqSel == 1, true);

        // Linha cinza separadora mais pra baixo
        tft.drawLine(10, 65, 118, 65, 0x39C7); // Cinza escuro
        // Limpa a área abaixo da linha
        tft.fillRect(0, 68, SCR_W, SCR_H - 68, C_BG);
      } else {
        // Linha cinza separadora original
        tft.drawLine(10, 48, 118, 48, 0x39C7); // Cinza escuro
        // Limpa a área abaixo da linha
        tft.fillRect(0, 52, SCR_W, SCR_H - 52, C_BG);
      }
    } else {
      // Atualização dinâmica: redesenha os itens fixos para manter a seleção atualizada
      drawMenuItem(0, 24, 128, 19, "433.92 (Padrao)", jamFreqSel == 0, true);
      if (hasTwo) {
        char capBuf[24];
        snprintf(capBuf, sizeof(capBuf), "%.2f (Capturada)", rfDetectedMHz);
        drawMenuItem(0, 44, 128, 19, capBuf, jamFreqSel == 1, true);
      }
    }

    if (jamFreqSel >= totalItems)
      jamFreqSel = totalItems - 1;

    // Define o número de itens roláveis baseado no espaço que sobrou
    int maxVisible = hasTwo ? 3 : 4;

    if (jamScroll < maisIdx)
      jamScroll = maisIdx;

    if (jamFreqSel >= maisIdx) {
      if (jamFreqSel < jamScroll)
        jamScroll = jamFreqSel;
      if (jamFreqSel >= jamScroll + maxVisible)
        jamScroll = jamFreqSel - maxVisible + 1;
    }

    int yPos = hasTwo ? 68 : 52;
    for (int i = 0; i < maxVisible; i++) {
      int idx = jamScroll + i;
      if (idx >= totalItems)
        break;

      char labelBuf[32];
      const char *label;
      if (idx == maisIdx) {
        label = jamFreqExpanded ? "Mais Frequencias v" : "Mais Frequencias >";
      } else {
        snprintf(labelBuf, sizeof(labelBuf), "%.2f",
                 jamExtraFreqs[idx - maisIdx - 1]);
        label = labelBuf;
      }

      // h=20 preenche completamente o espaço, evitando necessidade de clear
      drawMenuItem(0, yPos, 128, 20, label, jamFreqSel == idx, idx != maisIdx);
      yPos += 20;
    }

    // Atualiza a variável com base na seleção
    if (jamFreqSel == 0)
      jamSelectedMHz = 433.92f;
    else if (hasTwo && jamFreqSel == 1)
      jamSelectedMHz = rfDetectedMHz;
    else if (jamFreqSel > maisIdx)
      jamSelectedMHz = jamExtraFreqs[jamFreqSel - maisIdx - 1];
  } else {
    // Status ATIVO
    if (fullRedraw) {
      tft.setTextColor(TFT_RED);
      tft.setCursor(4, 30);
      tft.print(lang->rf_jam_ativo);

      tft.setTextColor(TFT_YELLOW);
      tft.setCursor(4, 48);
      char jfBuf[24];
      snprintf(jfBuf, sizeof(jfBuf), "%.2f MHz bloqueado", jamSelectedMHz);
      tft.print(jfBuf);

      tft.setTextColor(TFT_RED);
      tft.setCursor(4, 96);
      tft.print(lang->rf_jam_hint_stop);
    }

    // Contador de pacotes (atualiza sempre)
    tft.fillRect(4, 66, SCR_W - 8, 12, TFT_BLACK);
    char buf[24];
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(4, 66);
    snprintf(buf, sizeof(buf), "Pacotes: %lu", jammerPackets);
    tft.print(buf);
  }

  if (fullRedraw) {
    rfFooter();
    batteryDraw();
  }
}

void handleRF_Random() {
  if (jammerAtivo) {
    // GDO0 esta em OUTPUT (enableTransmit ativado na inicializacao do jammer).
    // CC1101 permanece em SetTx() durante toda a sessao.
    // Nao mudar enable/disable por pacote: overhead alto e GDO0 ficaria INPUT
    // entre pacotes, suprimindo a portadora.
    rcSwitch.setRepeatTransmit(1);
    rcSwitch.send(esp_random(), 32);
    rcSwitch.setRepeatTransmit(10); // restaura para uso futuro
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
      char buf[24];
      tft.setTextSize(1);
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(4, 66);
      snprintf(buf, sizeof(buf), "Pacotes: %lu", jammerPackets);
      tft.print(buf);

      // Pulso visual animado (barra que cresce e volta)
      static int pulse = 0;
      pulse = (pulse + 4) % (SCR_W - 12);
      tft.fillRect(4, 82, SCR_W - 8, 8, TFT_BLACK);
      tft.fillRect(4, 82, pulse, 8, TFT_RED);
    }

    // SELECT → parar jammer
    if (digitalRead(BUTTON_SELECT) == LOW &&
        (millis() - lastDebounceTime) > debounceDelay) {
      lastDebounceTime = millis();
      jammerAtivo = false;
      ELECHOUSE_cc1101.SetRx();
      rcSwitch.disableTransmit(); // libera GDO0 de OUTPUT
      pinMode(RF_GDO0, INPUT);    // CC1101 volta a pilotar GDO0 em RX
      rcSwitch.enableReceive(RF_GDO0);
      Serial.printf(
          "[RF][JAMMER] Jammer PARADO. Total de pacotes enviados: %lu\n",
          jammerPackets);
      jammerPackets = 0;
      displayRF_Random();
    }

  } else {
    // ─── MODO INATIVO: navegação de frequência + início ───
    bool hasTwo = (rfDetectedMHz > 0 && (int)(rfDetectedMHz * 100) != 43392);
    int maisIdx = hasTwo ? 2 : 1;
    int totalItems = maisIdx + 1 + (jamFreqExpanded ? numExtraFreqs : 0);

    if ((millis() - lastDebounceTime) > debounceDelay) {

      // RIGHT → navega para baixo na lista
      if (digitalRead(BUTTON_RIGHT) == LOW && totalItems > 1) {
        lastDebounceTime = millis();
        jamFreqSel++;
        if (jamFreqSel >= totalItems)
          jamFreqSel = 0; // Volta ao início
        displayRF_Random(false);
        return;
      }

      // LEFT → sobe na lista ou volta
      if (digitalRead(BUTTON_LEFT) == LOW) {
        lastDebounceTime = millis();
        if (jamFreqSel > 0) {
          jamFreqSel--;
          displayRF_Random(false);
        } else {
          jamFreqSel = 0;
          jamScroll = 0;
          jamFreqExpanded = false;
          estadoAtual = MENU_RF;
          displayRF();
        }
        return;
      }

      // SELECT → iniciar jammer ou expandir menu
      if (digitalRead(BUTTON_SELECT) == LOW && rfReady) {
        lastDebounceTime = millis();

        if (jamFreqSel == maisIdx) {
          jamFreqExpanded = !jamFreqExpanded;
          if (!jamFreqExpanded) {
            jamFreqSel = maisIdx;
            jamScroll = maisIdx;
            // Limpa as sub-frequências remanescentes ao fechar o menu
            int clrY = hasTwo ? 88 : 72;
            tft.fillRect(0, clrY, 128, 128 - clrY, C_BG);
          }
          displayRF_Random(false);
          return;
        }

        ELECHOUSE_cc1101.setMHZ(jamSelectedMHz);
        ELECHOUSE_cc1101.SetTx();
        rcSwitch.disableReceive();        // desativa interrupt do RX
        rcSwitch.enableTransmit(RF_GDO0); // GDO0 → OUTPUT para TX continuo
        jammerAtivo = true;
        jammerPackets = 0;
        jammerLastDraw = 0;
        Serial.printf("[RF][JAMMER] Jammer INICIADO em %.2f MHz\n",
                      jamSelectedMHz);
        displayRF_Random();
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
static RFRawSignal savedRawSignals[MAX_RF_RAW_SIGNALS];

static int savedDecCount = 0;
static int savedRawCount = 0;

struct SavedEntry {
  bool isRaw;
  int index; // indice dentro do array respectivo
};

#define MAX_TOTAL_SAVED (MAX_RF_SIGNALS + MAX_RF_RAW_SIGNALS)
static SavedEntry savedEntries[MAX_TOTAL_SAVED];
static int savedTotalCount = 0;

static int savedIndex = 0;  // entry selecionado atualmente
static int savedScroll = 0; // primeiro índice visível
#define SAVED_VISIBLE 8

static void buildSavedEntries() {
  savedTotalCount = 0;
  for (int i = 0; i < savedDecCount; i++) {
    savedEntries[savedTotalCount++] = {false, i};
  }
  for (int i = 0; i < savedRawCount; i++) {
    savedEntries[savedTotalCount++] = {true, i};
  }
}

static void sendRawPulses(int pin, const unsigned int *pulses, int count,
                          int repeatTimes = 3) {
  for (int r = 0; r < repeatTimes; r++) {
    int level = HIGH;
    for (int i = 0; i < count; i++) {
      digitalWrite(pin, level);
      delayMicroseconds(pulses[i]);
      level = !level;
    }
    digitalWrite(pin, LOW);
    delay(10);
  }
}

static void drawSavedItem(int idx, bool sel) {
  if (idx < savedScroll || idx >= savedScroll + SAVED_VISIBLE ||
      idx > savedTotalCount)
    return;
  int i = idx - savedScroll;
  int h = 18;
  int y = 16 + i * h;

  if (idx == 0) {
    drawMenuItem(0, y, SCR_W, h, lang->rf_svd_back, sel, false);
  } else {
    int eIdx = idx - 1;
    SavedEntry e = savedEntries[eIdx];

    tft.fillRect(0, y, SCR_W, h, TFT_BLACK);
    tft.setTextColor(sel ? TFT_WHITE : TFT_DARKGREY);
    tft.setCursor(4, y + 4);
    tft.print(sel ? ">" : " ");

    if (!e.isRaw) {
      tft.setTextColor(sel ? TFT_GREEN : 0x05E0); // verde escuro se não sel
      char buf[28];
      float f = (savedSignals[e.index].freq > 0) ? savedSignals[e.index].freq
                                                 : 433.92f;
      snprintf(buf, sizeof(buf), "D:%lu %.1f", savedSignals[e.index].value, f);
      tft.print(buf);
    } else {
      tft.setTextColor(sel ? TFT_MAGENTA : 0xA014); // magenta escuro se não sel
      char buf[28];
      float f = (savedRawSignals[e.index].freq > 0)
                    ? savedRawSignals[e.index].freq
                    : 433.92f;
      snprintf(buf, sizeof(buf), "R:%dp %.1f", savedRawSignals[e.index].count,
               f);
      tft.print(buf);
    }
  }

  if (i == 0 && savedScroll > 0) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(SCR_W - 10, y + 4);
    tft.print("^");
  }
  if (i == SAVED_VISIBLE - 1 &&
      savedScroll + SAVED_VISIBLE < savedTotalCount + 1) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(SCR_W - 10, y + 4);
    tft.print("v");
  }
}

static void drawSavedList() {
  tft.fillRect(0, 16, SCR_W, SCR_H - 16, TFT_BLACK);
  tft.setTextSize(1);
  if (savedTotalCount == 0) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(8, 50);
    tft.print(lang->rf_svd_nenhum);
    return;
  }
  for (int i = 0; i < SAVED_VISIBLE; i++) {
    int idx = savedScroll + i;
    if (idx > savedTotalCount)
      break;
    drawSavedItem(idx, idx == savedIndex);
  }
}

void displayRF_Saved() {
  rfHeader(lang->rf_hdr_saved);
  savedDecCount = loadRFSignals(savedSignals, MAX_RF_SIGNALS);
  savedRawCount = loadRFRawSignals(savedRawSignals, MAX_RF_RAW_SIGNALS);
  buildSavedEntries();
  savedIndex = 0;
  savedScroll = 0;
  drawSavedList();
  batteryDraw();

  while (digitalRead(BUTTON_SELECT) == LOW)
    vTaskDelay(10 / portTICK_PERIOD_MS);

  if (rfReady && rfDetectedMHz > 0) {
    ELECHOUSE_cc1101.setMHZ(rfDetectedMHz);
    ELECHOUSE_cc1101.SetRx();
  }
}

static void openSavedActionMenu(int listIdx) {
  SavedEntry e = savedEntries[listIdx];
  bool isRaw = e.isRaw;

  int numOpts = isRaw ? 3 : 4;
  int ph = isRaw ? 64 : 78;
  int px = 14, py = 35, pw = 100;

  tft.fillRect(px, py, pw, ph, 0x2104); // fundo igual
  tft.drawRect(px, py, pw, ph, isRaw ? TFT_MAGENTA : TFT_CYAN);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(px + 4, py + 6);
  tft.print(lang->rf_svd_acoes);

  const char *opts_dec[] = {lang->rf_svd_transmitir, lang->rf_svd_excluir,
                            lang->rf_svd_repetir, lang->rf_svd_voltar};
  const char *opts_raw[] = {lang->rf_svd_transmitir, lang->rf_svd_excluir,
                            lang->rf_svd_voltar};

  const char **opts = isRaw ? opts_raw : opts_dec;
  int selOpt = 0;

  auto drawPopupOpts = [&]() {
    for (int i = 0; i < numOpts; i++) {
      tft.fillRect(px + 2, py + 18 + i * 14, pw - 4, 14, 0x2104);
      tft.setTextColor(i == selOpt ? (isRaw ? TFT_MAGENTA : TFT_GREEN)
                                   : TFT_WHITE);
      tft.setCursor(px + 6, py + 21 + i * 14);
      tft.print(i == selOpt ? "> " : "  ");
      tft.print(opts[i]);
    }
  };

  drawPopupOpts();
  while (digitalRead(BUTTON_SELECT) == LOW)
    vTaskDelay(10 / portTICK_PERIOD_MS);

  unsigned long modalDebounce = millis();
  while (true) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    if (millis() - modalDebounce > debounceDelay) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        modalDebounce = millis();
        selOpt = (selOpt - 1 + numOpts) % numOpts;
        drawPopupOpts();
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        modalDebounce = millis();
        selOpt = (selOpt + 1) % numOpts;
        drawPopupOpts();
      }
      if (digitalRead(BUTTON_SELECT) == LOW) {
        modalDebounce = millis();
        while (digitalRead(BUTTON_SELECT) == LOW)
          vTaskDelay(10);

        if (selOpt == 0) { // Transmitir
          float txFreq = isRaw ? (savedRawSignals[e.index].freq > 0
                                      ? savedRawSignals[e.index].freq
                                      : 433.92f)
                               : (savedSignals[e.index].freq > 0
                                      ? savedSignals[e.index].freq
                                      : 433.92f);

          if (!isRaw) {
            ELECHOUSE_cc1101.setMHZ(txFreq);
            ELECHOUSE_cc1101.SetTx();
            rcSwitch.disableReceive();
            rcSwitch.enableTransmit(RF_GDO0);
            unsigned int txDelay = savedSignals[e.index].delay;
            rcSwitch.setProtocol(savedSignals[e.index].protocol,
                                 txDelay > 0 ? txDelay : 350);
            rcSwitch.send(savedSignals[e.index].value,
                          savedSignals[e.index].bits);
            rcSwitch.disableTransmit();
            pinMode(RF_GDO0, INPUT);
            rcSwitch.enableReceive(RF_GDO0);
            ELECHOUSE_cc1101.SetRx();
          } else {
            ELECHOUSE_cc1101.setMHZ(txFreq);
            ELECHOUSE_cc1101.SetTx();
            rcSwitch.disableReceive();
            rcSwitch.enableTransmit(RF_GDO0);
            pinMode(RF_GDO0, OUTPUT);
            sendRawPulses(RF_GDO0, savedRawSignals[e.index].pulses,
                          savedRawSignals[e.index].count);
            rcSwitch.disableTransmit();
            pinMode(RF_GDO0, INPUT);
            rcSwitch.enableReceive(RF_GDO0);
            ELECHOUSE_cc1101.SetRx();
          }
          tft.fillRect(px, py + ph, pw, 12, TFT_BLACK);
          tft.setTextColor(TFT_ORANGE);
          tft.setCursor(px + 4, py + ph + 2);
          tft.print(lang->rf_svd_enviado);
          delay(600);
          break;

        } else if (selOpt == 1) { // Excluir
          if (!isRaw)
            deleteRFSignal(e.index);
          else
            deleteRFRawSignal(e.index);
          tft.fillRect(px, py + ph, pw, 12, TFT_BLACK);
          tft.setTextColor(TFT_RED);
          tft.setCursor(px + 4, py + ph + 2);
          tft.print(lang->rf_svd_deletado);
          delay(600);
          savedDecCount = loadRFSignals(savedSignals, MAX_RF_SIGNALS);
          savedRawCount = loadRFRawSignals(savedRawSignals, MAX_RF_RAW_SIGNALS);
          buildSavedEntries();
          savedIndex = 0;
          savedScroll = 0;
          break;

        } else if (!isRaw && selOpt == 2) { // Repetir (só dec)
          float txFreq = (savedSignals[e.index].freq > 0)
                             ? savedSignals[e.index].freq
                             : 433.92f;
          int sx = px + 7, sy = py + 18 + 2 * 14 + 7, sw = pw - 4, sh = 36;
          tft.fillRect(sx, sy, sw, sh, 0x2104);
          tft.drawRect(sx, sy, sw, sh, TFT_CYAN);
          tft.setTextSize(1);
          unsigned long pulseCount = 0;
          bool repeatRunning = true;

          tft.fillRect(sx + 2, sy + 20, sw - 4, 12, 0x2104);
          tft.setTextColor(TFT_GREEN);
          tft.setCursor(sx + 6, sy + 22);
          tft.print("> ");
          tft.print(lang->rf_svd_parar);

          // Configuração do rádio apenas uma vez antes do loop para ser mais
          // rápido
          ELECHOUSE_cc1101.setMHZ(txFreq);
          ELECHOUSE_cc1101.SetTx();
          rcSwitch.disableReceive();
          rcSwitch.enableTransmit(RF_GDO0);
          unsigned int txDelay2 = savedSignals[e.index].delay;
          rcSwitch.setProtocol(savedSignals[e.index].protocol,
                               txDelay2 > 0 ? txDelay2 : 350);

          // Reduz o número de envios bloqueantes do RCSwitch por loop (default
          // é 10) Isso permite checar os botões mais vezes por segundo
          rcSwitch.setRepeatTransmit(3);

          while (repeatRunning) {
            rcSwitch.send(savedSignals[e.index].value,
                          savedSignals[e.index].bits);
            pulseCount += 3;

            // Checagem super agressiva: QUALQUER botão pressionado para
            if (digitalRead(BUTTON_SELECT) == LOW ||
                digitalRead(BUTTON_LEFT) == LOW ||
                digitalRead(BUTTON_RIGHT) == LOW) {
              repeatRunning = false;
              break;
            }

            // Atualiza a tela a cada 15 pulsos para não engasgar o envio de RF
            if (pulseCount % 15 == 0) {
              tft.fillRect(sx + 2, sy + 4, sw - 4, 12, 0x2104);
              tft.setTextColor(TFT_YELLOW);
              tft.setCursor(sx + 6, sy + 6);
              tft.print(lang->rf_svd_pulsos);
              tft.print(" ");
              tft.print(pulseCount);
            }

            // Checagem secundária
            if (digitalRead(BUTTON_SELECT) == LOW ||
                digitalRead(BUTTON_LEFT) == LOW ||
                digitalRead(BUTTON_RIGHT) == LOW) {
              repeatRunning = false;
              break;
            }

            vTaskDelay(10 / portTICK_PERIOD_MS); // respira a task
          }

          // Aguarda o usuário soltar os botões para não dar duplo-clique
          while (digitalRead(BUTTON_SELECT) == LOW ||
                 digitalRead(BUTTON_LEFT) == LOW ||
                 digitalRead(BUTTON_RIGHT) == LOW) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
          }
          modalDebounce = millis();

          // Restaura CC1101 para recepção
          rcSwitch.disableTransmit();
          pinMode(RF_GDO0, INPUT);
          rcSwitch.enableReceive(RF_GDO0);
          ELECHOUSE_cc1101.SetRx();

          tft.fillRect(sx, sy, sw, sh, TFT_BLACK);
          drawSavedList();
          tft.fillRect(px, py, pw, ph, 0x2104);
          tft.drawRect(px, py, pw, ph, TFT_CYAN);
          tft.setTextSize(1);
          tft.setTextColor(TFT_WHITE);
          tft.setCursor(px + 4, py + 6);
          tft.print(lang->rf_svd_acoes);
          drawPopupOpts();
        } else if ((isRaw && selOpt == 2) || (!isRaw && selOpt == 3)) {
          break;
        }
      }
    }
  }
  drawSavedList();
}

void handleRF_Saved() {
  static unsigned long selectPressStart = 0;
  static bool selectHeld = false;
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      lastDebounceTime = millis();
      int totalItems = savedTotalCount + 1;
      int oldIndex = savedIndex;
      int oldScroll = savedScroll;
      savedIndex = (savedIndex + 1) % totalItems;
      if (savedIndex < savedScroll)
        savedScroll = savedIndex;
      if (savedIndex >= savedScroll + SAVED_VISIBLE)
        savedScroll = savedIndex - SAVED_VISIBLE + 1;
      if (savedScroll != oldScroll)
        drawSavedList();
      else {
        drawSavedItem(oldIndex, false);
        drawSavedItem(savedIndex, true);
      }
    }
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      int totalItems = savedTotalCount + 1;
      int oldIndex = savedIndex;
      int oldScroll = savedScroll;
      savedIndex = (savedIndex - 1 + totalItems) % totalItems;
      if (savedIndex < savedScroll)
        savedScroll = savedIndex;
      if (savedIndex >= savedScroll + SAVED_VISIBLE)
        savedScroll = savedIndex - SAVED_VISIBLE + 1;
      if (savedScroll != oldScroll)
        drawSavedList();
      else {
        drawSavedItem(oldIndex, false);
        drawSavedItem(savedIndex, true);
      }
    }
  }

  if (digitalRead(BUTTON_SELECT) == LOW) {
    if (!selectHeld) {
      selectHeld = true;
      selectPressStart = millis();
    } else if (millis() - selectPressStart > 600) {
      selectHeld = false;
      lastDebounceTime = millis();
      if (savedIndex > 0 && savedIndex <= savedTotalCount) {
        openSavedActionMenu(savedIndex - 1);
      }
    }
  } else {
    if (selectHeld && (millis() - selectPressStart) < 600) {
      selectHeld = false;
      lastDebounceTime = millis();
      if (savedIndex == 0) {
        estadoAtual = MENU_RF;
        displayRF();
      } else {
        int eIdx = savedIndex - 1;
        SavedEntry e = savedEntries[eIdx];
        float txFreq = e.isRaw ? (savedRawSignals[e.index].freq > 0
                                      ? savedRawSignals[e.index].freq
                                      : 433.92f)
                               : (savedSignals[e.index].freq > 0
                                      ? savedSignals[e.index].freq
                                      : 433.92f);

        if (!e.isRaw) {
          ELECHOUSE_cc1101.setMHZ(txFreq);
          ELECHOUSE_cc1101.SetTx();
          rcSwitch.disableReceive();
          rcSwitch.enableTransmit(RF_GDO0);
          unsigned int txDelay3 = savedSignals[e.index].delay;
          rcSwitch.setProtocol(savedSignals[e.index].protocol,
                               txDelay3 > 0 ? txDelay3 : 350);
          rcSwitch.send(savedSignals[e.index].value,
                        savedSignals[e.index].bits);
          rcSwitch.disableTransmit();
          pinMode(RF_GDO0, INPUT);
          rcSwitch.enableReceive(RF_GDO0);
          ELECHOUSE_cc1101.SetRx();
        } else {
          ELECHOUSE_cc1101.setMHZ(txFreq);
          ELECHOUSE_cc1101.SetTx();
          rcSwitch.disableReceive();
          rcSwitch.enableTransmit(RF_GDO0);
          pinMode(RF_GDO0, OUTPUT);
          sendRawPulses(RF_GDO0, savedRawSignals[e.index].pulses,
                        savedRawSignals[e.index].count);
          rcSwitch.disableTransmit();
          pinMode(RF_GDO0, INPUT);
          rcSwitch.enableReceive(RF_GDO0);
          ELECHOUSE_cc1101.SetRx();
        }
        tft.fillRect(4, SCR_H - 22, SCR_W - 8, 12, TFT_BLACK);
        tft.setTextColor(TFT_ORANGE);
        tft.setCursor(4, SCR_H - 22);
        tft.print(">> ENVIADO!");
        delay(500);
        tft.fillRect(4, SCR_H - 22, SCR_W - 8, 12, TFT_BLACK);
      }
    }
    selectHeld = false;
  }
}
