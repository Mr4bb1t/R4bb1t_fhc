#include "Battery.h"
#include "Globals.h" // tft
#include "Config.h"  // C_GOLD, paleta Cyber Edition

// ─────────────────────────────────────────────────────────────
//  Estado interno
// ─────────────────────────────────────────────────────────────
static int           s_lastDisplayedPct = -1;
static unsigned long s_lastReadMs       = 0;

// ─────────────────────────────────────────────────────────────
//  Leitura ADC
//  analogReadMilliVolts() usa a calibração eFuse do ESP32 →
//  resultado em mV, bem mais preciso que calcular por raw/VREF.
// ─────────────────────────────────────────────────────────────
static int readBatMv(int samples = 16) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(BAT_ADC_PIN);
    delayMicroseconds(200);
  }
  return (int)(sum / samples);
}

// Converte mV no pino → porcentagem (0–100)
static int mvToPercent(int mv) {
  if (mv >= BAT_MV_FULL)  return 100;
  if (mv <= BAT_MV_EMPTY) return   0;
  return (int)(((float)(mv - BAT_MV_EMPTY) /
                (float)(BAT_MV_FULL - BAT_MV_EMPTY)) * 100.0f);
}

// ─────────────────────────────────────────────────────────────
//  Ícone de pilha — SEM texto de porcentagem
//  Corpo 20×10 px + polo + 3×4 px, encostado canto direito
// ─────────────────────────────────────────────────────────────
static void drawBatteryIcon(int pct) {
  const int X   = BAT_ICON_X;
  const int Y   = BAT_ICON_Y;
  const int W   = 20;
  const int H   = 10;
  const int PAD = 2;
  const int PW  = 3;
  const int PH  = 4;

  // Fundo limpo
  tft.fillRect(X - 1, Y - 1, W + PW + 2, H + 2, TFT_BLACK);

  // Contorno do corpo em dourado
  tft.drawRect(X, Y, W, H, C_GOLD);

  // Polo positivo (+) em dourado
  tft.fillRect(X + W, Y + (H - PH) / 2, PW, PH, C_GOLD);

  // Cor da barra conforme nível
  uint16_t barColor;
  if      (pct > 50) barColor = TFT_GREEN;
  else if (pct > 30) barColor = TFT_YELLOW;
  else if (pct > 10) barColor = TFT_ORANGE;
  else               barColor = TFT_RED;

  // Preenchimento proporcional
  int innerW = W - 2 * PAD;
  int fillW  = (innerW * pct) / 100;

  tft.fillRect(X + PAD, Y + PAD, innerW, H - 2 * PAD, TFT_BLACK);
  if (fillW > 0) {
    tft.fillRect(X + PAD, Y + PAD, fillW, H - 2 * PAD, barColor);
  }
}

// ─────────────────────────────────────────────────────────────
//  API pública
// ─────────────────────────────────────────────────────────────

void batteryInit() {
  // ADC_11db: range 0-3600 mV, funciona bem para leituras < 2V
  analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);
  analogSetPinAttenuation(BAT_TEMP_PIN, ADC_11db);

  // Primeira leitura com analogReadMilliVolts (calibração interna)
  int mv  = analogReadMilliVolts(BAT_ADC_PIN);
  int pct = mvToPercent(mv);

  // Debug para Serial Monitor — essencial para ajuste fino
  Serial.println("[BAT] ==================");
  Serial.print("[BAT] mV no pino : "); Serial.print(mv); Serial.println(" mV");
  Serial.print("[BAT] Porcentagem : "); Serial.print(pct); Serial.println("%");
  Serial.print("[BAT] MV_FULL=");  Serial.print(BAT_MV_FULL);
  Serial.print("  MV_EMPTY="); Serial.println(BAT_MV_EMPTY);
  Serial.println("[BAT] ==================");

  s_lastDisplayedPct = pct;
  s_lastReadMs       = millis();
  // Ícone NÃO é desenhado aqui — será desenhado pelo batteryDraw()
  // após o splash screen, chamado por displayMenuInicial().
}

void batteryUpdate() {
  unsigned long now = millis();
  if ((now - s_lastReadMs) < BAT_UPDATE_MS) return;
  s_lastReadMs = now;

  int mv  = readBatMv();
  int pct = mvToPercent(mv);

  int delta = pct - s_lastDisplayedPct;
  if (abs(delta) >= BAT_DEBOUNCE_PCT) {
    s_lastDisplayedPct = pct;
    drawBatteryIcon(pct);
    Serial.print("[BAT] Update: "); Serial.print(mv);
    Serial.print("mV -> "); Serial.print(pct); Serial.println("%");
  }
}

void batteryDraw() {
  if (s_lastDisplayedPct < 0) return;
  drawBatteryIcon(s_lastDisplayedPct);
}

int batteryPercent() {
  return (s_lastDisplayedPct < 0) ? 0 : s_lastDisplayedPct;
}

float batteryTemperature() {
  int mv = analogReadMilliVolts(BAT_TEMP_PIN);
  
  // Se assumirmos um resistor pull-up para 3.3V (ex: 10k), se o fio amarelo não estiver conectado,
  // a tensão lida será bem alta, próxima a 3300mV.
  // Se nada estiver conectado (nem resistor nem fio), o pino flutua e pode ler valores baixos (ex: 200mV = ~101°C).
  // Se a tensão for menor que 400mV (equivalente a >70°C num NTC de 10k) ignoramos a leitura.
  if (mv > 3100 || mv < 400) return -999.0f;

  float vcc = 3300.0f;
  float r_pullup = 10000.0f;
  
  // Vout = Vcc * R_ntc / (R_pullup + R_ntc)
  // R_ntc = R_pullup * Vout / (Vcc - Vout)
  float r_ntc = r_pullup * mv / (vcc - (float)mv);
  if (r_ntc <= 0) return -999.0f;

  // Parâmetros típicos NTC 10K
  float b = 3950.0f;
  float t0 = 298.15f; // 25°C
  float r0 = 10000.0f;

  float temp = 1.0f / (1.0f / t0 + (1.0f / b) * log(r_ntc / r0));
  temp -= 273.15f; // Converte Kelvin para Celsius
  
  return temp;
}
