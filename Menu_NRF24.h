#ifndef MENU_NRF24_H
#define MENU_NRF24_H

#include <Arduino.h>

// ── Pinos nRF24L01 ─────────────────────────────
// SPI: usa HSPI (mesmo barramento do CC1101), pinos SCK=33 MISO=19 MOSI=13
// TFT usa VSPI separado — sem conflito
#define NRF_CE 22
#define NRF_CSN 4

// ── Pinos Módulo 2 (não soldado ainda) ──────
// Se detectado nestes pinos no boot, sera usado automaticamente.
#define NRF2_CE 12     // GPIO12 — livre (ajustar quando soldar)
#define NRF2_CSN 15    // GPIO15 — livre (ajustar quando soldar)

// ── Funções públicas ────────────────────────────
void displayModoNRF24();
void handleModoNRF24();

// Probe leve chamado no setup() — detecta o NRF24 e seta hwNRF24_ok
// (igual ao rfInit() do CC1101, sem alocar tasks de jamming)
bool nrfProbe();
bool nrfProbe2();

// ── Ícones do NRF24 (usados pelo Menu_Main) ─────
// Modo grid (grande)
void drawNRF24Icon(int x, int y, uint16_t col);
// Modo lista (pequeno)
void drawNRF24IconSmall(int x, int y, uint16_t col);

#endif
