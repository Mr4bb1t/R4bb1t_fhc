#ifndef MENU_RF_H
#define MENU_RF_H

#include <Arduino.h>

// ── Pinos do CC1101 (HSPI — livre de conflito com o TFT) ──
#define RF_SCK  33  // HSPI SCK
#define RF_MISO 19  // HSPI MISO
#define RF_MOSI 13  // HSPI MOSI
#define RF_CS   25  // CC1101 CS
#define RF_GDO0  2  // CC1101 GDO0 → transmissão (RCSwitch TX)
#define RF_GDO2 32  // GDO2

// Sub-menu RF
void displayRF();
void handleRF();

// Modos RF
void displayRF_Replay();
void handleRF_Replay();
void displayRF_Raw();
void handleRF_Raw();
void displayRF_Analyser();
void handleRF_Analyser();
void displayRF_Random();
void handleRF_Random();
void displayRF_Saved();
void handleRF_Saved();

// Inicializa CC1101 (chamado uma vez no setup)
bool rfInit();

#endif
