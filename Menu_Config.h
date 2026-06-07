#ifndef MENU_CONFIG_H
#define MENU_CONFIG_H

#include <Arduino.h>

void initConfig(); // Inicializa as preferências salvas
void displayConfiguracoes();
void handleConfiguracoes();
void displaySobre();
void handleSobre();
void displayMudarMAC();
void handleMudarMAC();
void displayBrilho();
void handleBrilho();
void displayModoMenu();
void handleModoMenu();
void blOff(); // apaga o backlight (LEDC → duty 0)

void displayArmazenamento();
void handleArmazenamento();

#endif
