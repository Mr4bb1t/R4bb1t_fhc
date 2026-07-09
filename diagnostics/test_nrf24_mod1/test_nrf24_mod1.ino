// =====================================================
//  DIAGNOSTICO NRF24L01 MODULO 1 v2 - R4bb1t FHC
//  STATUS=0x0C confirmou modulo presente no barramento.
//  Este sketch testa multiplas velocidades SPI e faz
//  leitura/escrita direta de registradores para
//  identificar por que begin() falha.
//
//  HSPI: SCK=33  MISO=19  MOSI=13
//  CE=22  CSN=4
//  Lib: RF24 (nrf24/RF24 by TMRh20)
//  Monitor Serial: 115200 baud
// =====================================================

#include <SPI.h>
#include <RF24.h>

#define NRF_SCK  33
#define NRF_MISO 19
#define NRF_MOSI 13
#define NRF_CE   22
#define NRF_CSN   4
#define RF_CS    25

SPIClass spiHSPI(HSPI);
RF24 radio(NRF_CE, NRF_CSN);

// Leitura direta de registrador do NRF24 via SPI raw
static uint8_t nrfReadReg(uint8_t reg) {
  digitalWrite(NRF_CSN, LOW);
  delayMicroseconds(5);
  spiHSPI.transfer(reg & 0x1F);   // comando READ (bit7=0)
  uint8_t val = spiHSPI.transfer(0xFF);
  delayMicroseconds(5);
  digitalWrite(NRF_CSN, HIGH);
  delayMicroseconds(5);
  return val;
}

// Escrita direta em registrador do NRF24 via SPI raw
static void nrfWriteReg(uint8_t reg, uint8_t val) {
  digitalWrite(NRF_CSN, LOW);
  delayMicroseconds(5);
  spiHSPI.transfer(0x20 | (reg & 0x1F));  // comando WRITE (bit5=1)
  spiHSPI.transfer(val);
  delayMicroseconds(5);
  digitalWrite(NRF_CSN, HIGH);
  delayMicroseconds(5);
}

// Envia CE pulse para resetar estado do NRF24
static void nrfCEpulse() {
  digitalWrite(NRF_CE, LOW);
  delay(5);
}

void testSPIFreq(uint32_t freq, const char* label) {
  Serial.printf("\n--- Testando SPI @ %s ---\n", label);
  spiHSPI.setFrequency(freq);
  delay(10);

  // Le STATUS (NOP = 0xFF)
  digitalWrite(NRF_CSN, LOW);
  delayMicroseconds(5);
  uint8_t status = spiHSPI.transfer(0xFF);
  delayMicroseconds(5);
  digitalWrite(NRF_CSN, HIGH);
  Serial.printf("  STATUS: 0x%02X ", status);
  if (status == 0xFF) Serial.println("(ERRO: MISO travado HIGH)");
  else if (status == 0x00) Serial.println("(ERRO: MISO travado LOW)");
  else Serial.printf("(modulo presente, bits=%s)\n", status == 0x0E ? "POR OK" : "estado prev");

  // Escreve e le registro CONFIG (0x00)
  // POR value = 0x08. Tentamos escrever 0x0F e ler de volta.
  nrfWriteReg(0x00, 0x0F);
  delay(2);
  uint8_t config_readback = nrfReadReg(0x00);
  Serial.printf("  CONFIG write=0x0F readback=0x%02X %s\n",
                config_readback,
                config_readback == 0x0F ? "[OK - SPI escrita/leitura funcional!]" :
                config_readback == 0x08 ? "[FALHA escrita - apenas leitura do POR]" :
                "[AVISO - valor inesperado]");

  // Escreve e le registro EN_AA (0x01)
  nrfWriteReg(0x01, 0x3F);
  delay(2);
  uint8_t enaa = nrfReadReg(0x01);
  Serial.printf("  EN_AA  write=0x3F readback=0x%02X %s\n",
                enaa, enaa == 0x3F ? "[OK]" : "[FALHA]");

  // Tenta radio.begin() com esta frequencia
  bool ok = radio.begin(&spiHSPI);
  Serial.printf("  radio.begin() = %s\n", ok ? "TRUE [SUCESSO!]" : "false");
  if (ok) {
    Serial.printf("    isChipConnected = %s\n",
                  radio.isChipConnected() ? "true" : "false");
    Serial.println("    >> FREQUENCIA FUNCIONAL ENCONTRADA! <<");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=========================================");
  Serial.println("  DIAGNOSTICO NRF24 MOD1 v2 - FREQ TEST");
  Serial.println("=========================================");
  Serial.printf("  SCK=%d MISO=%d MOSI=%d CE=%d CSN=%d\n",
                NRF_SCK, NRF_MISO, NRF_MOSI, NRF_CE, NRF_CSN);
  Serial.println("-----------------------------------------");

  // Garante CC1101 inativo
  pinMode(RF_CS, OUTPUT); digitalWrite(RF_CS, HIGH);

  // Configura pinos NRF24
  pinMode(NRF_CSN, OUTPUT); digitalWrite(NRF_CSN, HIGH);
  pinMode(NRF_CE, OUTPUT);  digitalWrite(NRF_CE, LOW);

  // PASSO 1: STATUS inicial
  Serial.println("\n[PASSO 1] STATUS inicial (NOP 0xFF):");
  spiHSPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI, -1);
  spiHSPI.setFrequency(1000000);
  delay(10);

  digitalWrite(NRF_CSN, LOW);
  delayMicroseconds(5);
  uint8_t status = spiHSPI.transfer(0xFF);
  delayMicroseconds(5);
  digitalWrite(NRF_CSN, HIGH);

  Serial.printf("  STATUS = 0x%02X\n", status);
  if (status == 0x0E)
    Serial.println("  -> POR (0x0E): modulo recentemente energizado, estado limpo");
  else if (status == 0x0C)
    Serial.println("  -> 0x0C: modulo presente, estado da sessao anterior (normal apos reset ESP32 sem desligar NRF)");
  else if (status == 0xFF)
    Serial.println("  -> 0xFF: MISO travado HIGH - fio solto ou sem VCC!");
  else
    Serial.printf("  -> 0x%02X: modulo presente em estado diverso\n", status);

  // PASSO 2: Leitura dos registradores POR
  Serial.println("\n[PASSO 2] Valores POR esperados dos registradores:");
  Serial.println("  Reg  | Esperado | Lido     | Status");
  Serial.println("  -----|----------|----------|-------");
  // CONFIG=0x08, EN_AA=0x3F, EN_RXADDR=0x03, SETUP_AW=0x03
  uint8_t regs[][3] = {{0x00,0x08,0}, {0x01,0x3F,0}, {0x02,0x03,0}, {0x03,0x03,0}};
  const char* regNames[] = {"CONFIG","EN_AA ","ENRXAD","SETUP_"};
  bool allMatch = true;
  for (int i = 0; i < 4; i++) {
    uint8_t val = nrfReadReg(regs[i][0]);
    bool match = (val == regs[i][1]);
    if (!match) allMatch = false;
    Serial.printf("  0x%02X | 0x%02X     | 0x%02X     | %s\n",
                  regs[i][0], regs[i][1], val,
                  match ? "OK" : (val == 0xFF ? "MISO=FF (sem comunicacao)" :
                  val == 0x00 ? "MISO=00 (curto)" : "DIFERENTE"));
  }
  if (allMatch)
    Serial.println("  >> Todos registros POR corretos! SPI funcionando a 1MHz <<");
  else
    Serial.println("  >> Registros POR incorretos - verificar hardware <<");

  // PASSO 3: Teste de escrita/leitura + begin() em multiplas frequencias
  Serial.println("\n[PASSO 3] Varredura de frequencias SPI:");
  testSPIFreq(250000,  "250kHz");
  testSPIFreq(500000,  "500kHz");
  testSPIFreq(1000000, "1MHz");
  testSPIFreq(2000000, "2MHz");
  testSPIFreq(4000000, "4MHz");
  testSPIFreq(8000000, "8MHz");
  testSPIFreq(10000000,"10MHz");
  testSPIFreq(16000000,"16MHz");

  // PASSO 4: Voltagem (estimada via status - limitacao sem ADC)
  Serial.println("\n[PASSO 4] Informacoes adicionais:");
  Serial.println("  Capacitores: 22uF 16V (OK - suficiente)");
  Serial.println("  Se begin() falhou em TODAS frequencias:");
  Serial.println("  -> Modulo clone/fake: responde ao SPI mas nao passa init check");
  Serial.println("  -> Meça a tensao VCC do modulo com multimetro ENQUANTO roda");
  Serial.println("  -> VCC deve ser 3.3V +/- 0.1V sob carga");
  Serial.println("  Se begin() OK em alguma frequencia:");
  Serial.println("  -> Usar essa frequencia no projeto (editar nrfProbe/nrfInit)");

  Serial.println("\n=========================================");
  Serial.println("  FIM DIAGNOSTICO NRF24 MOD1 v2");
  Serial.println("=========================================");
}

void loop() {
  delay(5000);
  Serial.println("[NRF24] Reinicie para novo teste.");
}