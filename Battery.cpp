#include "Battery.h"
#include "Globals.h" // tft

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

  // Contorno do corpo
  tft.drawRect(X, Y, W, H, TFT_WHITE);

  // Polo positivo (+)
  tft.fillRect(X + W, Y + (H - PH) / 2, PW, PH, TFT_WHITE);

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
