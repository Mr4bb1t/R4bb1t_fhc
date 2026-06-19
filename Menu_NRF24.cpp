// Menu_NRF24.cpp — NRF24 Jammer real (FreeRTOS non-blocking)
// Módulo 1: CE=22 CSN=4  HSPI: SCK=33 MISO=19 MOSI=13
// Módulo 2: opcional — mesmo HSPI, CE/CSN configuráveis
// ⚠️ Não mexe no CC1101 (CS=25, GDO0=2, GDO2=32)

#include "Menu_NRF24.h"
#include "Battery.h"
#include "Globals.h"
#include "HWProbe.h"
#include "Menu_Main.h"
#include "Menu_RF.h" // Necessário para controlar RF_CS
#include "UI.h"
#include <RF24.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ── Pinos Módulo 2 (opcional, não soldado ainda) ──────
// ⚠️ NRF2_CE estava em GPIO18 = TFT_SCLK → destruía o display!
// Quando for soldar o módulo 2, escolha GPIOs livres e mude NRF2_ENABLED para 1.
#define NRF2_ENABLED 0    // 0 = módulo 2 desativado (não soldado)
#define NRF2_CE   0   // GPIO0  — livre pós-boot (ajustar quando soldar)
#define NRF2_CSN  15  // GPIO15 — livre (ajustar quando soldar)

// ── Layout ────────────────────────────────────────────
#define SCR_W 128
#define SCR_H 160

// ── Canais ────────────────────────────────────────────
static const uint8_t BT_CH[]  = {32,34,46,48,50,52,0,1,2,4,6,8,22,24,26,28,30,74,76,78,80};
static const uint8_t BLE_CH[] = {2, 26, 80};
static const char    JAM_TEXT[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

// ── Objetos de rádio (alocados dinamicamente) ─────────
// NRF24 usa HSPI (SPI2) — MESMO barramento do CC1101, mas DIFERENTE do TFT!
// TFT usa VSPI (SPI3) com MOSI=23 SCLK=18 — barramentos SEPARADOS.
// ⚠️ NUNCA use o objeto global `SPI` (VSPI) aqui — isso destrói o TFT!
static SPIClass  spiJam(HSPI); // HSPI dedicado, não compartilha com o TFT
static RF24     *radio[2]  = {nullptr, nullptr};
static int       radioCount = 0;

// ── Estado geral ──────────────────────────────────────
static volatile bool nrfInitDone  = false;
static bool nrfReady     = false;

// ── Task FreeRTOS de jamming ──────────────────────────
static TaskHandle_t  jamTaskHandle = nullptr;
static volatile bool jamStop       = false;
static volatile bool jamRunning    = false;

// ── Task FreeRTOS de init ─────────────────────────────
static volatile bool nrfInitRunning = false;
static volatile bool nrfNeedsRedraw = false;

static bool nrfInit(); // Forward declaration

static void nrfInitTask(void *param) {
  nrfReady = nrfInit();
  hwNRF24_ok = nrfReady;
  nrfNeedsRedraw = true;
  nrfInitRunning = false;
  Serial.printf("[NRF] Init task concluido, nrfReady=%d\n", nrfReady);
  vTaskDelete(nullptr);
}

// ── Contadores (atualizados pela task, lidos pela UI) ─
static volatile unsigned long jamPktCount = 0;
static volatile uint8_t       jamCurChan  = 0;

// ── Sub-ataques disponíveis ───────────────────────────
struct NrfAttack { const char *label; const char *desc; uint16_t color; };
static const NrfAttack ATTACKS[] = {
  {"< Voltar",        "",                          TFT_WHITE},
  {"BT Jammer",       "Bluetooth Classic 2.4GHz",  TFT_RED  },
  {"Drone Jammer",    "Drones 2.4GHz",             TFT_RED  },
  {"BLE Adv Jammer",  "BLE Adv Channels",          TFT_YELLOW},
  {"BLE Data Jammer", "BLE Data Channels",         TFT_YELLOW},
  {"WiFi Jammer",     "802.11 b/g/n 2.4GHz",       TFT_CYAN },
  {"Zigbee Jammer",   "IEEE 802.15.4",             0x07C0   },
  {"Misc Jammer",     "Canal livre 0-124",          0x967F   },
};
static const int ATK_COUNT = (int)(sizeof(ATTACKS)/sizeof(ATTACKS[0]));

// ── Cursor / scroll do menu ───────────────────────────
static int nrfCursor = 0;
static int nrfScroll = 0;
static const int ITEM_H  = 18;
static const int ITEM_Y0 = 38;
static const int MAX_VIS = (SCR_H - ITEM_Y0 - 16) / ITEM_H;

// ── Tela interna ──────────────────────────────────────
static int nrfActiveAtk = -1;
enum NrfScreen { NSC_MENU, NSC_ATTACK };
static NrfScreen nrfScreen = NSC_MENU;
static unsigned long lastUIUpdate = 0;

// Probe leve chamado no setup() — detecta o NRF24 e seta hwNRF24_ok
// (igual ao rfInit() do CC1101, sem alocar tasks de jamming)
bool nrfProbe() {
  if (hwNRF24_ok) return true;

  Serial.println("[NRF] Fazendo probe leve...");

  // Garante pinos do módulo 1 e desativa CC1101 no barramento
  pinMode(RF_CS, OUTPUT);    digitalWrite(RF_CS, HIGH); // CS do CC1101 inativo
  pinMode(NRF_CSN, OUTPUT);  digitalWrite(NRF_CSN, HIGH);
  pinMode(NRF_CE,  OUTPUT);  digitalWrite(NRF_CE,  LOW);
  delay(10);

  // Inicializa o HSPI dedicado para o NRF24.
  spiJam.begin(33, 19, 13, -1); // SCK=33, MISO=19, MOSI=13
  spiJam.setFrequency(8000000);

  RF24 probeRadio(NRF_CE, NRF_CSN);
  bool ok = false;
  for (int t = 0; t < 3 && !ok; t++) {
    ok = probeRadio.begin(&spiJam);
    if (!ok) delay(20);
  }

  if (ok && probeRadio.isChipConnected()) {
    hwNRF24_ok = true;
    Serial.println("[NRF] Probe detectou modulo 1 com sucesso!");
  } else {
    hwNRF24_ok = false;
    Serial.println("[NRF] Probe falhou: modulo 1 ausente.");
  }
  return hwNRF24_ok;
}

// ─────────────────────────────────────────────────────
//  Init / Deinit de rádios
// ─────────────────────────────────────────────────────
static bool nrfInit() {
  if (nrfReady) return true;

  Serial.println("[NRF] Iniciando HSPI...");

  // Garante pinos do módulo 1 e desativa CC1101 no barramento
  pinMode(RF_CS, OUTPUT);    digitalWrite(RF_CS, HIGH); // CS do CC1101 inativo
  pinMode(NRF_CSN, OUTPUT);  digitalWrite(NRF_CSN, HIGH);
  pinMode(NRF_CE,  OUTPUT);  digitalWrite(NRF_CE,  LOW);
  delay(20);

  // Inicializa o HSPI dedicado para o NRF24.
  // NUNCA usar SPI (VSPI) aqui — o VSPI pertence ao TFT_eSPI (pinos 23/18/5)!
  // SS=-1: o barramento não gerencia CS. CE=22 e CSN=4 são controlados pelo RF24.
  spiJam.begin(33, 19, 13, -1); // SCK=33, MISO=19, MOSI=13
  spiJam.setFrequency(8000000);
  Serial.println("[NRF] HSPI iniciado (SCK=33 MISO=19 MOSI=13 | CE=22 CSN=4 gerenciados pelo RF24)");

  // Módulo 1 (obrigatório)
  radio[0] = new RF24(NRF_CE, NRF_CSN);
  bool ok0 = false;
  for (int t = 0; t < 3 && !ok0; t++) {
    Serial.printf("[NRF] Tentativa %d...\n", t + 1);
    ok0 = radio[0]->begin(&spiJam);
    Serial.printf("[NRF] begin()=%d\n", ok0);
    if (!ok0) delay(80);
  }
  if (!ok0) {
    Serial.println("[NRF] Modulo 1 nao respondeu.");
    delete radio[0]; radio[0] = nullptr;
    return false;
  }
  radioCount = 1;

#if NRF2_ENABLED
  // Módulo 2 (opcional — só tenta se pinos diferentes e NRF2_ENABLED=1)
  pinMode(NRF2_CSN, OUTPUT); digitalWrite(NRF2_CSN, HIGH);
  pinMode(NRF2_CE,  OUTPUT); digitalWrite(NRF2_CE,  LOW);
  delay(10);
  radio[1] = new RF24(NRF2_CE, NRF2_CSN);
  bool ok1 = radio[1]->begin(&spiJam);
  if (ok1 && radio[1]->isChipConnected()) {
    radioCount = 2;
    Serial.println("[NRF] Modulo 2 detectado!");
  } else {
    delete radio[1]; radio[1] = nullptr;
    Serial.println("[NRF] Modulo 2 ausente (opcional).");
  }
#else
  Serial.println("[NRF] Modulo 2 desativado (NRF2_ENABLED=0).");
#endif

  // Configura todos os módulos presentes
  for (int i = 0; i < radioCount; i++) {
    radio[i]->setAutoAck(false);
    radio[i]->stopListening();
    radio[i]->setRetries(0, 0);
    radio[i]->setPayloadSize(32);
    radio[i]->setAddressWidth(3);
    radio[i]->setPALevel(RF24_PA_MAX, true);
    radio[i]->setDataRate(RF24_2MBPS);
    radio[i]->setCRCLength(RF24_CRC_DISABLED);
    radio[i]->disableCRC();
    radio[i]->disableAckPayload();
    radio[i]->disableDynamicPayloads();
  }

  hwNRF24_ok = true;
  nrfReady   = true;
  Serial.printf("[NRF] Pronto. Modulos: %d\n", radioCount);
  return true;
}

static void nrfDeinit() {
  // Para a task de init se estiver rodando
  nrfInitRunning = false;
  nrfNeedsRedraw = false;

  for (int i = 0; i < radioCount; i++) {
    if (radio[i]) {
      radio[i]->stopConstCarrier();
      radio[i]->powerDown();
      delete radio[i]; radio[i] = nullptr;
    }
  }
  radioCount = 0;
  nrfReady   = false;
  hwNRF24_ok = false;
  nrfInitDone = false;
}

static void toggleCeLow() {
  digitalWrite(NRF_CE, LOW);
#if NRF2_ENABLED
  digitalWrite(NRF2_CE, LOW);
#endif
}
static void toggleCeHigh() {
  digitalWrite(NRF_CE, HIGH);
#if NRF2_ENABLED
  digitalWrite(NRF2_CE, HIGH);
#endif
}

// ─────────────────────────────────────────────────────
//  Task FreeRTOS — loop de jamming
// ─────────────────────────────────────────────────────
static void jamTask(void *param) {
  int atkId = (int)(intptr_t)param;
  jamPktCount = 0;
  jamCurChan  = 0;

  switch (atkId) {

    // ── 1: BT Jammer — portadora constante varrendo BT Classic
    case 1:
      Serial.println("[NRF] Iniciando BT Jammer (Const Carrier)");
      for (int i = 0; i < radioCount; i++)
        radio[i]->startConstCarrier(RF24_PA_MAX, BT_CH[0]);
      while (!jamStop) {
        for (int ch = 0; ch < 21 && !jamStop; ch++) {
          toggleCeLow(); // OBRIGATORIO: baixar CE para o PLL mudar de canal
          for (int i = 0; i < radioCount; i++)
            radio[i]->setChannel(BT_CH[ch]);
          toggleCeHigh(); // Levanta CE para voltar a transmitir
          jamCurChan = BT_CH[ch];
          jamPktCount++;
          // Serial.printf("[NRF] BT Jammer -> Canal %d\n", jamCurChan); // Log desativado para não poluir
          vTaskDelay(5);
        }
      }
      for (int i = 0; i < radioCount; i++) radio[i]->stopConstCarrier();
      Serial.println("[NRF] BT Jammer Parado.");
      break;

    // ── 2: Drone Jammer — varrendo 0-124 aleatório + portadora
    case 2:
      for (int i = 0; i < radioCount; i++)
        radio[i]->startConstCarrier(RF24_PA_MAX, 45);
      while (!jamStop) {
        uint8_t ch = (uint8_t)(random(125));
        toggleCeLow();
        for (int i = 0; i < radioCount; i++)
          radio[i]->setChannel(ch);
        toggleCeHigh();
        jamCurChan = ch;
        jamPktCount++;
        vTaskDelay(1);
      }
      for (int i = 0; i < radioCount; i++) radio[i]->stopConstCarrier();
      break;

    // ── 3: BLE Adv Jammer — Const Carrier Flood nos 3 canais ADV
    case 3:
      Serial.println("[NRF] Iniciando BLE Adv Jammer (Const Carrier)");
      for (int i = 0; i < radioCount; i++)
        radio[i]->startConstCarrier(RF24_PA_MAX, BLE_CH[0]);
      while (!jamStop) {
        for (int ch = 0; ch < 3 && !jamStop; ch++) {
          toggleCeLow();
          for (int i = 0; i < radioCount; i++) radio[i]->setChannel(BLE_CH[ch]);
          toggleCeHigh();
          jamCurChan = BLE_CH[ch];
          jamPktCount++;
          vTaskDelay(1);
        }
      }
      for (int i = 0; i < radioCount; i++) radio[i]->stopConstCarrier();
      Serial.println("[NRF] BLE Adv Jammer Parado.");
      break;

    // ── 4: BLE Data Jammer — portadora nos canais BLE data (2-80 pares)
    case 4:
      for (int i = 0; i < radioCount; i++)
        radio[i]->startConstCarrier(RF24_PA_MAX, 45);
      while (!jamStop) {
        for (uint8_t ch = 2; ch <= 80 && !jamStop; ch += 2) {
          toggleCeLow();
          for (int i = 0; i < radioCount; i++)
            radio[i]->setChannel(ch);
          toggleCeHigh();
          jamCurChan = ch;
          jamPktCount++;
          vTaskDelay(1);
        }
      }
      for (int i = 0; i < radioCount; i++) radio[i]->stopConstCarrier();
      break;

    // ── 5: WiFi Jammer — todos os 14 canais WiFi (faixas de 23 sub-canais)
    case 5:
      Serial.println("[NRF] Iniciando WiFi Jammer (Const Carrier)");
      for (int i = 0; i < radioCount; i++)
        radio[i]->startConstCarrier(RF24_PA_MAX, 1);
      while (!jamStop) {
        for (int wch = 0; wch < 14 && !jamStop; wch++) {
          int base = (wch * 5) + 1;
          for (int sub = base; sub <= base + 22 && !jamStop; sub++) {
            if (sub < 1 || sub > 125) continue;
            
            toggleCeLow();
            for (int i = 0; i < radioCount; i++) radio[i]->setChannel((uint8_t)sub);
            toggleCeHigh();
            
            jamCurChan = (uint8_t)sub;
            jamPktCount++;
          }
          vTaskDelay(1);
        }
      }
      for (int i = 0; i < radioCount; i++) radio[i]->stopConstCarrier();
      Serial.println("[NRF] WiFi Jammer Parado.");
      break;

    // ── 6: Zigbee Jammer — canais 11-26 IEEE 802.15.4 → NRF offset
    case 6:
      Serial.println("[NRF] Iniciando Zigbee Jammer (Const Carrier)");
      for (int i = 0; i < radioCount; i++)
        radio[i]->startConstCarrier(RF24_PA_MAX, 4);
      while (!jamStop) {
        for (int zch = 11; zch < 27 && !jamStop; zch++) {
          uint8_t nrfCh = (uint8_t)(4 + 5 * (zch - 11));
          for (int sub = 0; sub < 3 && !jamStop; sub++) {
            toggleCeLow();
            for (int i = 0; i < radioCount; i++) radio[i]->setChannel(nrfCh + sub);
            toggleCeHigh();
            jamCurChan = nrfCh + sub;
            jamPktCount++;
          }
          vTaskDelay(1);
        }
      }
      for (int i = 0; i < radioCount; i++) radio[i]->stopConstCarrier();
      Serial.println("[NRF] Zigbee Jammer Parado.");
      break;

    // ── 7: Misc Jammer — portadora varrendo 0-124 sequencialmente
    case 7:
      for (int i = 0; i < radioCount; i++)
        radio[i]->startConstCarrier(RF24_PA_MAX, 0);
      while (!jamStop) {
        for (uint8_t ch = 0; ch < 125 && !jamStop; ch++) {
          toggleCeLow();
          for (int i = 0; i < radioCount; i++)
            radio[i]->setChannel(ch);
          toggleCeHigh();
          jamCurChan = ch;
          jamPktCount++;
          vTaskDelay(1);
        }
      }
      for (int i = 0; i < radioCount; i++) radio[i]->stopConstCarrier();
      break;

    default:
      break;
  }

  jamRunning = false;
  vTaskDelete(nullptr);
}

static void startJamTask(int atkId) {
  jamStop    = false;
  jamRunning = true;
  xTaskCreatePinnedToCore(
    jamTask, "nrfJam",
    4096,
    (void *)(intptr_t)atkId,
    5,                // prioridade alta para RF
    &jamTaskHandle,
    0               // core 0 (UI no core 1)
  );
}

static void stopJamTask() {
  if (!jamRunning) return;
  jamStop = true;
  // Aguarda até 500ms para a task terminar
  for (int t = 0; t < 50 && jamRunning; t++) delay(10);
  jamTaskHandle = nullptr;
}

// ─────────────────────────────────────────────────────
//  Ícone Proibido + sinais RF
// ─────────────────────────────────────────────────────
static void drawProhibitedRFIcon(int cx, int cy, int r, uint16_t col) {
  tft.drawCircle(cx, cy, r,     col);
  tft.drawCircle(cx, cy, r - 1, col);
  float ang = 45.0f * PI / 180.0f;
  int dx = (int)(r * cos(ang)), dy = (int)(r * sin(ang));
  tft.drawLine(cx - dx, cy + dy, cx + dx, cy - dy, col);
  tft.drawLine(cx - dx + 1, cy + dy, cx + dx + 1, cy - dy, col);
  for (int arc = 6; arc <= 14; arc += 4) {
    for (float a = -50.0f; a <= 50.0f; a += 2.0f) {
      float rad = a * PI / 180.0f;
      int px = cx + (int)(arc * cos(rad)) + r + 2;
      int py = cy + (int)(arc * sin(rad));
      if (px < SCR_W && py >= 0 && py < SCR_H) tft.drawPixel(px, py, col);
    }
  }
  for (int arc = 6; arc <= 14; arc += 4) {
    for (float a = 130.0f; a <= 230.0f; a += 2.0f) {
      float rad = a * PI / 180.0f;
      int px = cx + (int)(arc * cos(rad)) - r - 2;
      int py = cy + (int)(arc * sin(rad));
      if (px >= 0 && py >= 0 && py < SCR_H) tft.drawPixel(px, py, col);
    }
  }
}

void drawNRF24IconSmall(int x, int y, uint16_t col) {
  // (x,y) = ponto de ref do item de lista; cx/cy = centro do ícone
  drawProhibitedRFIcon(x + 10, y + 13, 7, col);
}

void drawNRF24Icon(int x, int y, uint16_t col) {
  // (x,y) = centro direto (vindo de iconX/iconY do Menu_Main)
  drawProhibitedRFIcon(x, y, 11, col);
}

// ─────────────────────────────────────────────────────
//  Menu principal
// ─────────────────────────────────────────────────────
static void nrfDrawItem(int idx, bool sel) {
  int slot = idx - nrfScroll;
  if (slot < 0 || slot >= MAX_VIS) return;
  drawMenuItem(0, ITEM_Y0 + slot * ITEM_H, SCR_W, ITEM_H,
               ATTACKS[idx].label, sel, idx > 0);
}

static void nrfUpdateItems(int oldC, int newC) {
  int oldScroll = nrfScroll;
  if (newC < nrfScroll) nrfScroll = newC;
  if (newC >= nrfScroll + MAX_VIS) nrfScroll = newC - MAX_VIS + 1;
  if (nrfScroll != oldScroll) {
    tft.fillRect(0, ITEM_Y0, SCR_W, SCR_H - ITEM_Y0 - 16, C_BG);
    for (int i = 0; i < MAX_VIS; i++) {
      int idx = nrfScroll + i;
      if (idx >= ATK_COUNT) break;
      nrfDrawItem(idx, idx == newC);
    }
    // scroll indicators
    tft.fillRect(SCR_W - 10, ITEM_Y0 - 10, 10, 20, C_BG);
    if (nrfScroll > 0) { tft.setTextColor(C_GOLD_DIM); tft.setCursor(SCR_W-8, ITEM_Y0-8); tft.print("^"); }
    if (nrfScroll + MAX_VIS < ATK_COUNT) { tft.setTextColor(C_GOLD_DIM); tft.setCursor(SCR_W-8, ITEM_Y0 + MAX_VIS * ITEM_H); tft.print("v"); }
    return;
  }
  nrfDrawItem(oldC, false);
  nrfDrawItem(newC, true);
}

void displayModoNRF24() {
  tft.fillScreen(C_BG);
  tft.setTextSize(1);
  drawHeader("NRF24 JAMMER", false);
  drawProhibitedRFIcon(64, 27, 9, C_RED);
  tft.drawFastHLine(0, 37, SCR_W, C_GREY);

  if (nrfCursor < nrfScroll) nrfScroll = nrfCursor;
  if (nrfCursor >= nrfScroll + MAX_VIS) nrfScroll = nrfCursor - MAX_VIS + 1;

  for (int i = 0; i < MAX_VIS; i++) {
    int idx = nrfScroll + i;
    if (idx >= ATK_COUNT) break;
    nrfDrawItem(idx, idx == nrfCursor);
  }
  if (nrfScroll > 0) { tft.setTextColor(C_GOLD_DIM); tft.setCursor(SCR_W-8, ITEM_Y0-8); tft.print("^"); }
  if (nrfScroll + MAX_VIS < ATK_COUNT) { tft.setTextColor(C_GOLD_DIM); tft.setCursor(SCR_W-8, ITEM_Y0 + MAX_VIS*ITEM_H); tft.print("v"); }

  drawFooter();
  batteryDraw();
}

// ─────────────────────────────────────────────────────
//  Tela de ataque
// ─────────────────────────────────────────────────────
static void nrfDrawAttackFull() {
  tft.fillScreen(C_BG);
  drawHeader("NRF24 ATTACK", true);
  tft.setTextSize(1);

  const NrfAttack &atk = ATTACKS[nrfActiveAtk];

  // Nome
  tft.setTextColor(atk.color);
  int tx = (SCR_W - (int)strlen(atk.label) * 6) / 2;
  tft.setCursor(tx < 0 ? 0 : tx, 17);
  tft.print(atk.label);

  // Desc
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(2, 27);
  tft.print(atk.desc);

  tft.drawFastHLine(0, 37, SCR_W, C_GREY);

  // Módulos
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(2, 40);
  tft.print("Modulos:");
  tft.setTextColor(nrfReady ? C_GREEN : C_RED);
  char mbuf[12];
  snprintf(mbuf, sizeof(mbuf), "%d detect.", radioCount);
  tft.setCursor(56, 40);
  tft.print(nrfReady ? mbuf : "init...");

  // Banner status (y=52..64)
  tft.fillRect(0, 52, SCR_W, 14, jamRunning ? C_RED : 0x18C3);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor((SCR_W - 78) / 2, 55);
  tft.print(jamRunning ? "  [ ATIVO ]  " : "  [INATIVO]  ");

  // Etiquetas fixas
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(2, 72); tft.print("Canal:");
  tft.setCursor(2, 86); tft.print("Pacotes:");

  // Valores dinâmicos
  tft.fillRect(50, 70, 78, 10, C_BG);
  tft.setTextColor(C_CYAN);
  tft.setCursor(50, 72);
  char buf[12]; snprintf(buf, sizeof(buf), "%d", (int)jamCurChan);
  tft.print(buf);

  tft.fillRect(50, 84, 78, 10, C_BG);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(50, 86);
  tft.print((unsigned long)jamPktCount);

  // Instruções
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(2, 108); tft.print("o = Iniciar/Parar");
  tft.setCursor(2, 120); tft.print("< = Voltar");

  drawFooter();
  batteryDraw();
}

// Atualização pontual: só banner + contadores (sem fillScreen)
static void nrfUpdateAttackDyn() {
  // Banner
  tft.fillRect(0, 52, SCR_W, 14, jamRunning ? C_RED : 0x18C3);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor((SCR_W - 78) / 2, 55);
  tft.print(jamRunning ? "  [ ATIVO ]  " : "  [INATIVO]  ");

  // Canal
  tft.fillRect(50, 70, 78, 10, C_BG);
  tft.setTextColor(C_CYAN);
  tft.setCursor(50, 72);
  char buf[12]; snprintf(buf, sizeof(buf), "%d", (int)jamCurChan);
  tft.print(buf);

  // Pacotes
  tft.fillRect(50, 84, 78, 10, C_BG);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(50, 86);
  tft.print((unsigned long)jamPktCount);
}

// ─────────────────────────────────────────────────────
//  Handler principal
// ─────────────────────────────────────────────────────
void handleModoNRF24() {

  // Atualiza contadores a cada 300ms enquanto jamming ativo
  if (nrfScreen == NSC_ATTACK && jamRunning) {
    if (millis() - lastUIUpdate >= 300) {
      lastUIUpdate = millis();
      nrfUpdateAttackDyn();
    }
  }

  // Redesenha quando init assíncrono completa
  if (nrfNeedsRedraw) {
    nrfNeedsRedraw = false;
    nrfDrawAttackFull();
  }

  // Debounce mais curto na tela de ataque (inicio/parada rapidos)
  // Menu de navegacao usa debounce global (200ms) — evita duplo clique
  static const unsigned long NRF_ATK_DEBOUNCE = 300; // ms
  unsigned long nrfDebounce = (nrfScreen == NSC_ATTACK) ? NRF_ATK_DEBOUNCE : debounceDelay;
  if ((millis() - lastDebounceTime) <= nrfDebounce) return;

  // ── MENU ─────────────────────────────────────────
  if (nrfScreen == NSC_MENU) {

    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int old = nrfCursor;
      nrfCursor = (nrfCursor + 1) % ATK_COUNT;
      lastDebounceTime = millis();
      nrfUpdateItems(old, nrfCursor);
      return;
    }
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      if (nrfCursor == 0) { estadoAtual = MENU_INICIAL; displayMenuInicial(); return; }
      int old = nrfCursor;
      nrfCursor = (nrfCursor - 1 + ATK_COUNT) % ATK_COUNT;
      nrfUpdateItems(old, nrfCursor);
      return;
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      if (nrfCursor == 0) { estadoAtual = MENU_INICIAL; displayMenuInicial(); return; }

      nrfActiveAtk = nrfCursor;
      nrfScreen    = NSC_ATTACK;
      estadoAtual  = TELA_NRF_ATTACK;
      jamPktCount  = 0;
      jamCurChan   = 0;

      // Desenha a tela de ataque
      nrfDrawAttackFull();

      // Inicializa o hardware de forma ASSÍNCRONA (task no core 0)
      if (!nrfInitDone) {
        nrfInitDone = true;
        nrfInitRunning = true;
        xTaskCreatePinnedToCore(
          nrfInitTask, "nrfInit",
          4096,
          nullptr,
          5,
          nullptr,
          0  // core 0 — deixa core 1 livre para o TFT
        );
      }
      return;
    }
  }

  // ── ATTACK ───────────────────────────────────────
  else {

    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      stopJamTask();
      nrfDeinit();
      nrfScreen = NSC_MENU;
      estadoAtual = MENU_NRF24;
      displayModoNRF24();
      return;
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      if (!nrfReady && !nrfInitRunning) {
        // Tenta init novamente se falhou (assíncrono)
        nrfInitRunning = true;
        nrfInitDone = false;
        xTaskCreatePinnedToCore(
          nrfInitTask, "nrfInit",
          4096, nullptr, 5, nullptr, 0
        );
        return;
      }
      if (jamRunning) {
        stopJamTask();
      } else {
        jamPktCount = 0;
        startJamTask(nrfActiveAtk);
      }
      lastUIUpdate = millis();
      nrfUpdateAttackDyn();
      return;
    }
  }
}
