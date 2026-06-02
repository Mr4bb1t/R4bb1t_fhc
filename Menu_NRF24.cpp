// Menu_NRF24.cpp — Testes temporários do módulo nRF24L01
//
// ⚠️  REQUER: biblioteca "RF24" por TMRh20
//     Instalar no Arduino IDE:
//     Ferramentas → Gerenciar Bibliotecas → buscar "RF24" → instalar "RF24 by
//     TMRh20"
//
// Pinos (SPI compartilhado com CC1101 via HSPI):
//   SCK  = 33 | MISO = 19 | MOSI = 13
//   CE   = 22 | CSN  = 26

#include "Menu_NRF24.h"
#include "Battery.h"
#include "Globals.h"
#include "HWProbe.h"
#include "Menu_Main.h"

#include <RF24.h>
#include <SPI.h>

// ── Objeto RF24 ───────────────────────────────
static SPIClass spiNRF(HSPI);       // HSPI compartilhado
static RF24 radio(NRF_CE, NRF_CSN); // CE, CSN

// ── Estado interno ────────────────────────────
// nrfReady: espelha hwNRF24_ok após init completa do menu
static bool nrfReady = false;
static bool nrfInitDone = false;

// ── Sub-menu ──────────────────────────────────
// 0=< Voltar  1=Status  2=Ping TX  3=Escutar RX
static const char *nrfLabels[] = {"< Voltar", "Status", "Ping TX",
                                  "Escutar RX"};
static const uint16_t nrfColors[] = {TFT_WHITE, TFT_CYAN, TFT_GREEN,
                                     TFT_YELLOW};
static const int NRF_ITEMS = 4;
static int nrfOpcao = 0;

#define SCR_W 128
#define SCR_H 160

// ── Endereço de rádio fixo para os testes ────
static const byte PIPE_ADDR[6] = "r4bb1";

// ─────────────────────────────────────────────
//  Inicialização (lazy, só na primeira entrada)
// ─────────────────────────────────────────────
static bool nrfInit() {
  Serial.println("[NRF] ====== Iniciando nRF24L01 ======");
  Serial.printf("[NRF] CE=%d  CSN=%d\n", NRF_CE, NRF_CSN);
  Serial.printf("[NRF] SCK=33  MISO=19  MOSI=13\n");

  // 1. Pinos primeiro
  pinMode(NRF_CSN, OUTPUT);
  digitalWrite(NRF_CSN, HIGH);
  pinMode(NRF_CE, OUTPUT);
  digitalWrite(NRF_CE, LOW);
  delay(20);

  // 2. Inicia HSPI (SS=-1 pois RF24 controla CSN manualmente)
  spiNRF.begin(33, 19, 13, -1);
  spiNRF.setFrequency(2000000); // 2 MHz
  delay(10);

  // 3. radio.begin() com retry
  bool ok = false;
  for (int attempt = 1; attempt <= 2 && !ok; attempt++) {
    ok = radio.begin(&spiNRF);
    Serial.printf("[NRF] radio.begin() tentativa %d = %s\n", attempt,
                  ok ? "OK" : "FALHOU");
    if (!ok)
      delay(80);
  }

  if (!ok) {
    Serial.println("[NRF] Modulo nao respondeu.");
    Serial.println(
        "[NRF] Verifique: VCC=3.3V, CE=22, CSN=4, SCK=33, MISO=19, MOSI=13");
    return false;
  }

  // 4. Configuração
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.openWritingPipe(PIPE_ADDR);
  radio.openReadingPipe(1, PIPE_ADDR);
  radio.stopListening();

  Serial.println("[NRF] nRF24L01 configurado com sucesso!");
  Serial.printf("[NRF] Canal: %d  DataRate: 250kbps  PA: MIN\n",
                radio.getChannel());
  radio.printPrettyDetails();
  return true;
}

// ─────────────────────────────────────────────
//  Helpers de tela
// ─────────────────────────────────────────────
static void nrfHeader(const char *titulo) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  char buf[24];
  snprintf(buf, sizeof(buf), "[ %s ]", titulo);
  tft.setCursor((SCR_W - strlen(buf) * 6) / 2, 5);
  tft.print(buf);
  tft.drawFastHLine(0, 17, SCR_W, TFT_DARKGREY);
}

static void nrfFooter() {
  tft.drawFastHLine(0, SCR_H - 16, SCR_W, TFT_DARKGREY);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, SCR_H - 10);
  tft.print("<");
  tft.setCursor(SCR_W / 2 - 2, SCR_H - 10);
  tft.print("o");
  tft.setCursor(SCR_W - 11, SCR_H - 10);
  tft.print(">");
}

// ─────────────────────────────────────────────
//  TELA: Sub-menu nRF24
// ─────────────────────────────────────────────
void displayModoNRF24() {
  if (!nrfInitDone) {
    nrfInitDone = true;
    nrfReady = nrfInit();
    hwNRF24_ok = nrfReady;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);

  if (!nrfReady) {
    // ── Tela de ERRO: mostra pinos e botão de retry ──
    tft.setTextColor(TFT_RED);
    tft.setCursor(2, 4);
    tft.print("nRF24: NAO DETECTADO");
    tft.drawFastHLine(0, 16, SCR_W, TFT_DARKGREY);

    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(4, 24);
    tft.print("Pinos esperados:");
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 36);
    tft.print("CE=22   CSN=4");
    tft.setCursor(4, 48);
    tft.print("SCK=33  MISO=19");
    tft.setCursor(4, 60);
    tft.print("MOSI=13 VCC=3.3V");

    tft.drawFastHLine(0, 78, SCR_W, 0x2104);
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(4, 86);
    tft.print("[o] Tentar novamente");
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 100);
    tft.print("[<] Voltar");

    nrfFooter();
    batteryDraw();
    return;
  }

  // ── Módulo OK: cabeçalho + lista de opções ──
  tft.setCursor(2, 2);
  tft.setTextColor(TFT_GREEN);
  tft.print("nRF24L01  OK  2.4GHz");
  tft.setCursor(32, 14);
  tft.setTextColor(TFT_YELLOW);
  tft.print("2.4 GHz");
  tft.drawFastHLine(0, 24, SCR_W, TFT_DARKGREY);

  const int ITEM_H = 20;
  const int ITEM_Y0 = 28;
  for (int i = 0; i < NRF_ITEMS; i++) {
    int y = ITEM_Y0 + i * ITEM_H;
    bool sel = (i == nrfOpcao);
    if (sel) {
      tft.fillRect(0, y, 5, ITEM_H - 2, nrfColors[i]);
      tft.fillRect(5, y, SCR_W - 5, ITEM_H - 2, 0x1082);
      tft.setTextColor(TFT_WHITE);
    } else {
      tft.setTextColor(nrfColors[i]);
    }
    tft.setCursor(10, y + 5);
    tft.print(nrfLabels[i]);
  }

  nrfFooter();
  batteryDraw();
}

// ─────────────────────────────────────────────
//  TESTE 1: Status / Informações do módulo
// ─────────────────────────────────────────────
static void runStatus() {
  nrfHeader("NRF STATUS");
  tft.setTextSize(1);

  if (!nrfReady) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(4, 40);
    tft.print("Modulo nao detectado!");
    tft.setCursor(4, 56);
    tft.print("Verifique fiacao.");
    nrfFooter();
    batteryDraw();
    return;
  }

  int y = 24;
  auto row = [&](const char *lbl, String val, uint16_t col = TFT_WHITE) {
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(2, y);
    tft.print(lbl);
    tft.setTextColor(col);
    tft.setCursor(60, y);
    tft.print(val);
    y += 12;
  };

  row("Canal", String(radio.getChannel()));
  row("PA Level", radio.isPVariant() ? "nRF24L01+" : "nRF24L01");
  row("DataRate", "250 kbps");
  row("FIFO TX", radio.isFifo(true, true) ? "FULL" : "OK");
  row("FIFO RX", radio.isFifo(false, true) ? "FULL" : "OK");
  row("Chip", radio.isChipConnected() ? "CONN" : "FAIL",
      radio.isChipConnected() ? TFT_GREEN : TFT_RED);
  row("Variant", radio.isPVariant() ? "PA+LNA" : "basic");

  tft.setTextColor(TFT_CYAN);
  tft.setCursor(4, y + 4);
  tft.print("Ver Serial p/ detalhes");

  nrfFooter();
  batteryDraw();
}

// ─────────────────────────────────────────────
//  TESTE 2: Ping TX — envia pacotes e conta ACK
// ─────────────────────────────────────────────
static unsigned long pingCount = 0;
static unsigned long pingSuccess = 0;
static unsigned long pingLastMs = 0;
static bool pingRunning = false;

static void displayPingTX(bool update = false) {
  if (!update)
    nrfHeader("PING TX");
  tft.setTextSize(1);

  if (!nrfReady) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(4, 40);
    tft.print("Modulo indisponivel");
    nrfFooter();
    batteryDraw();
    return;
  }

  if (!update) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 26);
    tft.print("Canal 76  addr: r4bb1");
    tft.setTextColor(pingRunning ? TFT_RED : TFT_GREEN);
    tft.setCursor(4, 40);
    tft.print(pingRunning ? "TRANSMITINDO..." : "o = INICIAR");
  }

  // Atualiza contadores
  tft.fillRect(4, 60, SCR_W - 8, 56, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(4, 62);
  tft.print("Enviados: ");
  tft.print(pingCount);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(4, 76);
  tft.print("ACK OK : ");
  tft.print(pingSuccess);
  tft.setTextColor(TFT_RED);
  tft.setCursor(4, 90);
  tft.print("Falhas : ");
  tft.print(pingCount - pingSuccess);

  if (pingCount > 0) {
    int pct = (int)((float)pingSuccess / pingCount * 100);
    tft.setTextColor(pct > 80 ? TFT_GREEN : TFT_YELLOW);
    tft.setCursor(4, 106);
    char buf[20];
    snprintf(buf, sizeof(buf), "Taxa: %d%%", pct);
    tft.print(buf);
  }

  if (!update) {
    nrfFooter();
    batteryDraw();
  }
}

// ─────────────────────────────────────────────
//  TESTE 3: Escutar RX — recebe pacotes
// ─────────────────────────────────────────────
static unsigned long rxCount = 0;
static unsigned long rxLastMs = 0;
static bool rxListening = false;
static char rxLastMsg[33] = "--";

static void displayEscutar(bool update = false) {
  if (!update)
    nrfHeader("ESCUTAR RX");
  tft.setTextSize(1);

  if (!nrfReady) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(4, 40);
    tft.print("Modulo indisponivel");
    nrfFooter();
    batteryDraw();
    return;
  }

  if (!update) {
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(4, 26);
    tft.print(rxListening ? "Escutando..." : "o = INICIAR");
    nrfFooter();
    batteryDraw();
  }

  tft.fillRect(4, 42, SCR_W - 8, 72, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(4, 44);
  tft.print("Recebidos: ");
  tft.print(rxCount);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(4, 60);
  tft.print("Ultimo:");
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(4, 72);
  tft.print(rxLastMsg);
}

// ─────────────────────────────────────────────
//  Estado da tela atual dentro do menu NRF
//  Prefixo NSC_ (NRF Screen) para não conflitar
//  com macros do nRF24L01.h (NRF_STATUS etc)
// ─────────────────────────────────────────────
enum NrfScreen { NSC_MENU, NSC_STATUS, NSC_PING, NSC_LISTEN };
static NrfScreen nrfScreen = NSC_MENU;

// ─────────────────────────────────────────────
//  Handler principal
// ─────────────────────────────────────────────
void handleModoNRF24() {
  // ── PING TX: loop não-bloqueante ──────────
  if (nrfScreen == NSC_PING && pingRunning && nrfReady) {
    if (millis() - pingLastMs > 500) { // 1 ping a cada 500ms
      pingLastMs = millis();
      radio.stopListening();

      uint32_t payload = millis();
      bool ok = radio.write(&payload, sizeof(payload));
      pingCount++;
      if (ok)
        pingSuccess++;

      Serial.printf("[NRF] Ping %lu → %s\n", pingCount, ok ? "ACK" : "FAIL");
      displayPingTX(true);
    }
  }

  // ── RX: verifica buffer ───────────────────
  if (nrfScreen == NSC_LISTEN && rxListening && nrfReady) {
    if (radio.available()) {
      uint32_t payload = 0;
      radio.read(&payload, sizeof(payload));
      rxCount++;
      snprintf(rxLastMsg, sizeof(rxLastMsg), "0x%08lX (#%lu)", payload,
               rxCount);
      Serial.printf("[NRF] RX: 0x%08lX (#%lu)\n", payload, rxCount);
      displayEscutar(true);
    }
  }

  // ── Botões ────────────────────────────────
  if ((millis() - lastDebounceTime) > debounceDelay) {

    // ── Se módulo não detectado: só LEFT (voltar) e SELECT (retry) ──
    if (!nrfReady) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        lastDebounceTime = millis();
        estadoAtual = MENU_INICIAL;
        displayMenuInicial();
        return;
      }
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        // Reseta flags para forçar nova tentativa de init
        nrfInitDone = false;
        nrfReady = false;
        displayModoNRF24(); // vai chamar nrfInit() novamente
        return;
      }
      return; // ignora demais botões quando módulo ausente
    }

    // ── Na tela MENU principal do NRF ────────
    if (nrfScreen == NSC_MENU) {
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        nrfOpcao = (nrfOpcao + 1) % NRF_ITEMS;
        lastDebounceTime = millis();
        displayModoNRF24();
      }
      if (digitalRead(BUTTON_LEFT) == LOW) {
        lastDebounceTime = millis();
        if (nrfOpcao == 0) {
          estadoAtual = MENU_INICIAL;
          displayMenuInicial();
          return;
        }
        nrfOpcao = (nrfOpcao - 1 + NRF_ITEMS) % NRF_ITEMS;
        displayModoNRF24();
      }
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        switch (nrfOpcao) {
        case 0:
          estadoAtual = MENU_INICIAL;
          displayMenuInicial();
          break;
        case 1:
          nrfScreen = NSC_STATUS;
          runStatus();
          break;
        case 2:
          nrfScreen = NSC_PING;
          pingCount = pingSuccess = 0;
          pingRunning = false;
          displayPingTX();
          break;
        case 3:
          nrfScreen = NSC_LISTEN;
          rxCount = 0;
          rxListening = false;
          displayEscutar();
          break;
        }
      }
    }

    // ── Em sub-telas ─────────────────────────
    else {
      // LEFT → volta ao menu NRF
      if (digitalRead(BUTTON_LEFT) == LOW) {
        lastDebounceTime = millis();
        // Para transmissão/recepção ativas
        if (pingRunning) {
          pingRunning = false;
          radio.stopListening();
        }
        if (rxListening) {
          rxListening = false;
          radio.stopListening();
        }
        nrfScreen = NSC_MENU;
        displayModoNRF24();
      }

      // SELECT → toggle na tela de Ping ou RX
      if (digitalRead(BUTTON_SELECT) == LOW) {
        lastDebounceTime = millis();
        if (nrfScreen == NSC_PING && nrfReady) {
          pingRunning = !pingRunning;
          if (!pingRunning)
            radio.stopListening();
          displayPingTX();
        }
        if (nrfScreen == NSC_LISTEN && nrfReady) {
          rxListening = !rxListening;
          if (rxListening) {
            radio.startListening();
          } else {
            radio.stopListening();
          }
          displayEscutar();
        }
      }
    }
  }
}
