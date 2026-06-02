#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pinos dos botões
#define BUTTON_LEFT 14
#define BUTTON_RIGHT 27
#define BUTTON_SELECT 26

// Pinos do display
#define TFT_MOSI 23
#define TFT_SCLK 5
#define TFT_CS   -1
#define TFT_DC   17
#define TFT_RST  16
#define TFT_BL   21

// Configurações Gerais
#define MAX_REDES 10
#define DEBOUNCE_DELAY 200

#endif
