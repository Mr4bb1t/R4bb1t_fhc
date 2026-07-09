// =====================================================
//  DIAGNOSTICO NRF24L01 MODULO 2 - R4bb1t FHC
//  Pinos identicos ao Menu_NRF24.h do projeto:
//  HSPI: SCK=33  MISO=19  MOSI=13
//  NRF24 Mod2: CE=12  CSN=15
//
//  ATENCAO: GPIO12 e GPIO15 sao pinos de strapping!
//  GPIO12 (MTDI): deve estar LOW no boot (CE=LOW, OK)
//  GPIO15 (MTDO): HIGH no boot silencia UART (normal)
//
//  Lib necessaria: RF24 (nrf24/RF24 by TMRh20)
//  Monitor Serial: 115200 baud
// =====================================================

#include <SPI.h>
#include <RF24.h>

#define NRF_SCK   33
#define NRF_MISO  19
#define NRF_MOSI  13
#define NRF2_CE   12
#define NRF2_CSN  15
#define RF_CS     25
#define NRF_CSN    4

SPIClass spiHSPI(HSPI);
RF24 radio2(NRF2_CE, NRF2_CSN);

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=========================================");
  Serial.println("  DIAGNOSTICO NRF24 - MODULO 2");
  Serial.println("=========================================");
  Serial.printf("  HSPI: SCK=%d  MISO=%d  MOSI=%d\n", NRF_SCK, NRF_MISO, NRF_MOSI);
  Serial.printf("  CE=%d  CSN=%d\n", NRF2_CE, NRF2_CSN);
  Serial.println("-----------------------------------------");

  pinMode(RF_CS,   OUTPUT); digitalWrite(RF_CS,   HIGH);
  pinMode(NRF_CSN, OUTPUT); digitalWrite(NRF_CSN, HIGH);
  Serial.printf("[INFO] CC1101 CS GPIO%d + NRF1 CSN GPIO%d = HIGH\n", RF_CS, NRF_CSN);
  Serial.println("[AVISO] GPIO12=CE do Mod2 (fica LOW = sem conflito no boot)");
  Serial.println("[AVISO] GPIO15=CSN do Mod2 (HIGH no boot = UART boot silenciado, normal)\n");

  pinMode(NRF2_CSN, OUTPUT);
  digitalWrite(NRF2_CSN, HIGH);
  pinMode(NRF2_CE, OUTPUT);
  digitalWrite(NRF2_CE, LOW);
  delay(10);

  Serial.println("[PASSO 1] STATUS register via SPI raw...");
  spiHSPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI, -1);
  spiHSPI.setFrequency(1000000);
  delay(5);

  digitalWrite(NRF2_CSN, LOW);
  delayMicroseconds(5);
  uint8_t status = spiHSPI.transfer(0xFF);
  delayMicroseconds(5);
  digitalWrite(NRF2_CSN, HIGH);

  Serial.printf("  STATUS: 0x%02X\n", status);
  if (status == 0x0E) {
    Serial.println("  [OK] 0x0E = Power-on Reset. Modulo PRESENTE!");
  } else if (status == 0xFF) {
    Serial.println("  [ERRO] 0xFF = MISO travado HIGH. Fio solto ou sem VCC.");
  } else if (status == 0x00) {
    Serial.println("  [ERRO] 0x00 = MISO em curto com GND.");
  } else {
    Serial.printf("  [INFO] 0x%02X = modulo provavelmente presente\n", status);
  }

  spiHSPI.setFrequency(16000000);

  Serial.println("\n[PASSO 2] radio2.begin() (ate 3 tentativas)...");
  bool ok = false;
  for (int t = 0; t < 3 && !ok; t++) {
    Serial.printf("  Tentativa %d... ", t + 1);
    ok = radio2.begin(&spiHSPI);
    Serial.printf("begin()=%s\n", ok ? "true" : "false");
    if (!ok) delay(80);
  }

  if (!ok) {
    Serial.println("\n[ERRO] radio2.begin() falhou!");
    Serial.println("  Causas: fio solto, VCC != 3.3V, sem capacitores,");
    Serial.println("  GPIO12 interferindo, modulo ruim");
    Serial.println("  GPIO33->SCK  GPIO19->MISO  GPIO13->MOSI");
    Serial.println("  GPIO15->CSN  GPIO12->CE    3.3V->VCC    GND->GND");
  } else {
    bool conn = radio2.isChipConnected();
    Serial.printf("  isChipConnected()=%s\n", conn ? "true" : "false");
    if (conn) {
      Serial.println("\n[OK] NRF24 MOD2 DETECTADO!\n");
      radio2.printPrettyDetails();
      Serial.println("\n[PASSO 3] Teste setChannel/getChannel:");
      uint8_t canais[] = {0, 40, 80, 100, 124};
      for (int i = 0; i < 5; i++) {
        radio2.setChannel(canais[i]);
        uint8_t got = radio2.getChannel();
        Serial.printf("  set(%3d)->get()=%d  %s\n", canais[i], got,
                      canais[i] == got ? "[OK]" : "[ERRO]");
      }
    } else {
      Serial.println("  [AVISO] begin=true mas isChipConnected=false");
      Serial.println("  Soldagem ruim ou sem capacitor de desacoplamento.");
    }
  }

  Serial.println("\n=========================================");
  Serial.println("  FIM DIAGNOSTICO NRF24 MOD2");
  Serial.println("=========================================");
}

void loop() {
  delay(5000);
  Serial.println("[NRF24 Mod2] Reinicie para novo teste.");
}