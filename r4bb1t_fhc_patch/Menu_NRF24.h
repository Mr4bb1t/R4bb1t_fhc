#ifndef MENU_NRF24_H
#define MENU_NRF24_H

#include <Arduino.h>

// ── Pinos nRF24L01 ─────────────────────────────
// SPI compartilhado com CC1101 (HSPI: SCK=33, MISO=19, MOSI=13)
#define NRF_CE   22
#define NRF_CSN   4   // GPIO 26 = BUTTON_SELECT (CONFLITO) → usar GPIO 4

void displayModoNRF24();
void handleModoNRF24();

#endif
