#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

// ─────────────────────────────────────────────────────────────
//  Pino e atenuação
//  GPIO36 = VP — entrada ADC somente-leitura
//  Divisor medido: bateria 4.2V → 1000 mV no pino
//                  bateria 3.0V →  714 mV no pino
//  ADC_11db: range 0–3600 mV (o mais estável para leituras < 2V)
// ─────────────────────────────────────────────────────────────
#define BAT_ADC_PIN      36
#define BAT_TEMP_PIN     34


// Tensões no pino ADC (em mV) — ajuste se necessário
#define BAT_MV_FULL      1000   // mV quando bateria = 100%  (4.2V)
#define BAT_MV_EMPTY      714   // mV quando bateria =   0%  (3.0V)

// Debounce e intervalo de atualização
#define BAT_DEBOUNCE_PCT   5
#define BAT_UPDATE_MS  60000UL  // 1 minuto

// ─────────────────────────────────────────────────────────────
//  Posição do ícone — canto superior direito
//  Display 128×160: X=105, corpo 20px + polo 3px → borda em 128px
// ─────────────────────────────────────────────────────────────
#define BAT_ICON_X  105
#define BAT_ICON_Y    2

// ─────────────────────────────────────────────────────────────
//  API pública
// ─────────────────────────────────────────────────────────────
void batteryInit();
void batteryUpdate();
void batteryDraw();
int  batteryPercent();
float batteryTemperature();

#endif // BATTERY_H
