// =====================================================
//  DIAGNOSTICO CC1101 v2 - R4bb1t FHC
//
//  HSPI explicito (igual ao projeto): SCK=33 MISO=19 MOSI=13
//  CS=25  GDO0=2  GDO2=32
//
//  Este sketch usa SPIClass(HSPI) em vez do SPI global
//  (VSPI) para replicar exatamente o ambiente do projeto.
//  Tambem faz escrita/leitura de registradores para
//  confirmar comunicacao bidirecional real.
//
//  Libs necessarias:
//    SmartRC-CC1101-Driver-Lib (lsatan)
//  Monitor Serial: 115200 baud
// =====================================================

#include <SPI.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

#define RF_SCK   33
#define RF_MISO  19
#define RF_MOSI  13
#define RF_CS    25
#define RF_GDO0   2
#define RF_GDO2  32

// Usa HSPI dedicado - identico ao SPIClass spiJam(HSPI) do NRF24
SPIClass spiHSPI(HSPI);

// ── SPI raw via HSPI ──────────────────────────────────
static uint8_t cc1101ReadReg(uint8_t addr) {
  digitalWrite(RF_CS, LOW);
  delayMicroseconds(5);
  spiHSPI.transfer(addr | 0x80);   // bit7=1 leitura de config
  uint8_t val = spiHSPI.transfer(0x00);
  delayMicroseconds(5);
  digitalWrite(RF_CS, HIGH);
  delayMicroseconds(10);
  return val;
}

static uint8_t cc1101ReadStatus(uint8_t addr) {
  // Status registers: addr | 0xC0
  digitalWrite(RF_CS, LOW);
  delayMicroseconds(5);
  spiHSPI.transfer(addr | 0xC0);
  uint8_t val = spiHSPI.transfer(0x00);
  delayMicroseconds(5);
  digitalWrite(RF_CS, HIGH);
  delayMicroseconds(10);
  return val;
}

static void cc1101WriteReg(uint8_t addr, uint8_t val) {
  digitalWrite(RF_CS, LOW);
  delayMicroseconds(5);
  spiHSPI.transfer(addr & 0x3F);   // bit7=0 escrita
  spiHSPI.transfer(val);
  delayMicroseconds(5);
  digitalWrite(RF_CS, HIGH);
  delayMicroseconds(10);
}

// Envia strobe SRES e aguarda CHIP_RDY (MISO baixo)
static bool cc1101Reset() {
  digitalWrite(RF_CS, HIGH);
  delayMicroseconds(5);
  digitalWrite(RF_CS, LOW);
  delayMicroseconds(10);
  digitalWrite(RF_CS, HIGH);
  delayMicroseconds(41);
  digitalWrite(RF_CS, LOW);
  delayMicroseconds(5);

  // Aguarda CHIP_RDY (MISO deve ir LOW quando pronto)
  int timeout = 200;
  while (digitalRead(RF_MISO) == HIGH && timeout-- > 0) {
    delayMicroseconds(10);
  }
  bool ready = (timeout > 0);

  spiHSPI.transfer(0x30);  // SRES strobe
  delayMicroseconds(5);
  digitalWrite(RF_CS, HIGH);

  // Aguarda CC1101 completar reset (MISO deve voltar LOW)
  delay(5);
  return ready;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=========================================");
  Serial.println("  DIAGNOSTICO CC1101 v2 - R4bb1t FHC");
  Serial.println("  Usando HSPI (igual ao projeto)");
  Serial.println("=========================================");
  Serial.printf("  HSPI: SCK=%d  MISO=%d  MOSI=%d\n", RF_SCK, RF_MISO, RF_MOSI);
  Serial.printf("  CS=%d  GDO0=%d  GDO2=%d\n", RF_CS, RF_GDO0, RF_GDO2);
  Serial.println("-----------------------------------------");

  // Configura pinos
  pinMode(RF_CS, OUTPUT);  digitalWrite(RF_CS, HIGH);
  pinMode(RF_GDO0, INPUT);
  pinMode(RF_GDO2, INPUT);
  pinMode(RF_MISO, INPUT);

  // Inicia HSPI (igual ao nrfProbe/nrfInit do projeto)
  spiHSPI.begin(RF_SCK, RF_MISO, RF_MOSI, -1);
  spiHSPI.setFrequency(1000000);  // 1MHz para diagnostico
  spiHSPI.setDataMode(SPI_MODE0);
  spiHSPI.setBitOrder(MSBFIRST);
  delay(10);

  // ── PASSO 1: STATUS antes do reset ─────────────────
  Serial.println("\n[PASSO 1] STATUS antes do reset (NOP):");
  digitalWrite(RF_CS, LOW);
  delayMicroseconds(5);
  uint8_t status_pre = spiHSPI.transfer(0xFF);
  delayMicroseconds(5);
  digitalWrite(RF_CS, HIGH);
  Serial.printf("  STATUS = 0x%02X\n", status_pre);
  if (status_pre == 0xFF) {
    Serial.println("  [ERRO] 0xFF: MISO travado HIGH - fio MISO solto ou sem VCC");
    Serial.println("  -> Verifique: ESP32 GPIO19 conectado ao pino MISO/SO do CC1101");
    Serial.println("  -> Verifique: 3.3V no pino VCC do CC1101");
  } else if (status_pre == 0x00) {
    Serial.println("  [ATENCAO] 0x00: MISO em LOW - possivelmente curto");
  } else {
    Serial.printf("  [OK] CC1101 respondeu: CHIP_RDY=%d STATE=%d\n",
                  (status_pre >> 7) & 1, (status_pre >> 4) & 0x07);
  }

  // ── PASSO 2: Reset com CHIP_RDY check ──────────────
  Serial.println("\n[PASSO 2] Enviando SRES com verificacao de CHIP_RDY:");
  bool resetOk = cc1101Reset();
  Serial.printf("  CHIP_RDY antes do SRES: %s\n",
                resetOk ? "LOW (pronto)" : "TIMEOUT HIGH (MISO sem resposta!)");

  // ── PASSO 3: PARTNUM e VERSION via HSPI raw ─────────
  Serial.println("\n[PASSO 3] Lendo PARTNUM e VERSION via HSPI raw:");
  delay(10);
  uint8_t partnum = cc1101ReadReg(0x30);
  uint8_t version = cc1101ReadReg(0x31);

  Serial.printf("  PARTNUM (0x30): 0x%02X  (esperado: 0x00)\n", partnum);
  Serial.printf("  VERSION (0x31): 0x%02X  (esperado: 0x14)\n", version);

  if (partnum == 0x00 && version == 0x14) {
    Serial.println("  [OK] CC1101 genuino confirmado!");
  } else if (partnum == 0xFF && version == 0xFF) {
    Serial.println("  [ERRO] 0xFF/0xFF: MISO HIGH fixo - fio MISO solto ou VCC ausente");
  } else if (partnum == 0x00 && version == 0x00) {
    Serial.println("  [AVISO] 0x00/0x00: MISO LOW fixo - possivel MISO com curto a GND");
    Serial.println("  Mas STATUS lido acima era diferente de 0x00?");
    Serial.println("  -> Pode ser problema no pino MISO do modulo CC1101 (similar ao NRF24)");
  } else {
    Serial.printf("  [AVISO] Valores inesperados: PART=0x%02X VER=0x%02X\n", partnum, version);
    Serial.println("  Possivel: modulo clone, VSPI x HSPI conflict na v1, ou pino MISO fraco");
  }

  // ── PASSO 4: Escrita e leitura de registrador (bidirecional) ───
  Serial.println("\n[PASSO 4] Teste bidirecional (escrita + leitura de registrador):");
  Serial.println("  Escrevendo FSCTRL1 (0x0B) com valor 0x0F...");
  cc1101WriteReg(0x0B, 0x0F);
  delay(2);
  uint8_t fsctrl1 = cc1101ReadReg(0x0B);
  Serial.printf("  Leitura FSCTRL1: 0x%02X  %s\n", fsctrl1,
                fsctrl1 == 0x0F ? "[OK - SPI bidirecional funcional!]" :
                fsctrl1 == 0x00 ? "[FALHA - MISO vai LOW apos 1 byte (mesmo bug NRF24?)]" :
                fsctrl1 == 0xFF ? "[FALHA - MISO travado HIGH]" :
                "[FALHA - dado diferente]");

  Serial.println("  Escrevendo MCSM0 (0x22) com valor 0x3A...");
  cc1101WriteReg(0x22, 0x3A);
  delay(2);
  uint8_t mcsm0 = cc1101ReadReg(0x22);
  Serial.printf("  Leitura MCSM0:   0x%02X  %s\n", mcsm0,
                mcsm0 == 0x3A ? "[OK]" : "[FALHA]");

  // ── PASSO 5: RSSI lido via status register ──────────
  Serial.println("\n[PASSO 5] Leitura RSSI via status register (0x34):");
  ELECHOUSE_cc1101.setSpiPin(RF_SCK, RF_MISO, RF_MOSI, RF_CS);
  ELECHOUSE_cc1101.setGDO(RF_GDO0, RF_GDO2);

  if (ELECHOUSE_cc1101.getCC1101()) {
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(433.92);
    ELECHOUSE_cc1101.SetRx();
    delay(50);

    // Leitura crua do registrador RSSI (0x34 via status read 0xF4)
    uint8_t rssi_raw = cc1101ReadStatus(0x34);
    int rssi_dbm;
    if (rssi_raw >= 128) rssi_dbm = (rssi_raw - 256) / 2 - 74;
    else rssi_dbm = rssi_raw / 2 - 74;

    Serial.printf("  RSSI raw: 0x%02X  ->  %d dBm\n", rssi_raw, rssi_dbm);
    Serial.printf("  RSSI lib: %.1f dBm\n", ELECHOUSE_cc1101.getRssi());

    if (rssi_raw == 0x00) {
      Serial.println("  [AVISO] RSSI=0x00: CC1101 respondendo mas Init() pode ter falhado");
      Serial.println("  -> Mesmo padrao do NRF24: MISO cai no 2o byte da transacao");
      Serial.println("  -> VERIFIQUE A SOLDA DO PINO MISO no modulo CC1101");
    } else if (rssi_dbm > -10) {
      Serial.println("  [AVISO] RSSI muito alto (possivel erro de leitura)");
    } else {
      Serial.println("  [OK] RSSI valido - CC1101 em RX e lendo barulho de fundo");
    }

    Serial.println("\n  Varredura RSSI @ 433.92 MHz por 3 segundos:");
    unsigned long t0 = millis();
    while (millis() - t0 < 3000) {
      uint8_t r = cc1101ReadStatus(0x34);
      int dbm = (r >= 128) ? (r-256)/2-74 : r/2-74;
      Serial.printf("  [%4dms] raw=0x%02X -> %d dBm\n", (int)(millis()-t0), r, dbm);
      delay(300);
    }
  } else {
    Serial.println("  [ERRO] getCC1101() falhou mesmo com HSPI!");
    Serial.println("  -> Confirma que o problema nao e so VSPI vs HSPI");
    Serial.println("  -> Verifique VCC, GND e MISO fisicamente no modulo");
  }

  // ── PASSO 6: GDO state ──────────────────────────────
  Serial.println("\n[PASSO 6] Estado pinos GDO apos Init+SetRx:");
  Serial.printf("  GDO0 GPIO%d: %s\n", RF_GDO0, digitalRead(RF_GDO0) ? "HIGH" : "LOW");
  Serial.printf("  GDO2 GPIO%d: %s\n", RF_GDO2, digitalRead(RF_GDO2) ? "HIGH" : "LOW");
  Serial.println("  (Esperado: GDO2 HIGH quando CC1101 em idle/RX sem sinal)");

  // ── PASSO 7: Resumo ─────────────────────────────────
  Serial.println("\n[RESUMO]");
  Serial.println("  Se PASSO 3 deu 0x3F/0x00 no sketch anterior (VSPI)");
  Serial.println("  e agora PASSO 3 deu 0x00/0x14 (HSPI) -> problema era VSPI vs HSPI");
  Serial.println("  Se PASSO 3 deu 0x00/0x00 aqui (HSPI) -> MISO fraco igual NRF24");
  Serial.println("  Se PASSO 4 falhou (readback != valor escrito) -> mesmo bug MISO");
  Serial.println("  Todos modulos falham no 2o byte? -> MISO GPIO19 com problema fisico");
  Serial.println("  Apenas NRF24 falha? -> problema especifico no modulo NRF24");

  Serial.println("\n=========================================");
  Serial.println("  FIM DIAGNOSTICO CC1101 v2");
  Serial.println("=========================================");
}

// ── LOOP: leitura continua de RSSI ────────────────────
// Mostra RSSI raw e em dBm a cada 500ms para monitorar
// o barramento SPI em tempo real apos o diagnostico.
static unsigned long lastPrint = 0;
static uint32_t loopCount = 0;

void loop() {
  if (millis() - lastPrint >= 500) {
    lastPrint = millis();
    loopCount++;

    // Leitura raw via HSPI direto (registrador STATUS RSSI = 0x34)
    uint8_t rssi_raw = cc1101ReadStatus(0x34);
    int rssi_dbm = (rssi_raw >= 128) ? (rssi_raw - 256) / 2 - 74
                                      : rssi_raw / 2 - 74;

    // Leitura via biblioteca (compara com raw)
    float rssi_lib = ELECHOUSE_cc1101.getRssi();

    Serial.printf("[%6lu ms] RSSI raw=0x%02X -> %4d dBm  |  lib=%.1f dBm",
                  millis(), rssi_raw, rssi_dbm, rssi_lib);

    // Indicador visual simples de nivel
    int bars = 0;
    if      (rssi_dbm > -60) bars = 5;
    else if (rssi_dbm > -70) bars = 4;
    else if (rssi_dbm > -80) bars = 3;
    else if (rssi_dbm > -90) bars = 2;
    else if (rssi_dbm > -100) bars = 1;

    Serial.print("  [");
    for (int i = 0; i < 5; i++) Serial.print(i < bars ? "#" : ".");
    Serial.print("]");

    if (rssi_raw == 0x00)
      Serial.print("  MISO=0 (SPI ruim)");
    else if (rssi_raw == 0xFF)
      Serial.print("  MISO=FF (sem resposta)");

    Serial.println();
  }
}