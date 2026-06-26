#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pinos dos botões
#define BUTTON_LEFT 14
#define BUTTON_RIGHT 27
#define BUTTON_SELECT 26

// Pinos do display
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   17
#define TFT_RST  16
#define TFT_BL   21

// Configurações Gerais
#define MAX_REDES 10
#define DEBOUNCE_DELAY 200

// ─────────────────────────────────────────────────────────────
//  R4BB1T — Paleta Cyber Edition (RGB565)
// ─────────────────────────────────────────────────────────────
#define C_BG         0x0000   // Preto puro — fundo
#define C_GOLD       0xE4A0   // Dourado âmbar — destaque principal
#define C_GOLD_DIM   0x7220   // Dourado escuro — inativos / secondary
#define C_GOLD_SEL   0x2104   // Fundo seleção — cinza escuro neutro para evitar tom esverdeado
#define C_WHITE      0xFFFF   // Branco — texto principal
#define C_GREY       0x4208   // Cinza escuro — separadores / borda
#define C_GREY_DARK  0x18C3   // Cinza muito escuro — fundo header
#define C_RED        0xF800   // Vermelho — erros / ataques
#define C_GREEN      0x07E0   // Verde — status OK / conectado
#define C_YELLOW     0xFFE0   // Amarelo — avisos
#define C_CYAN       0x07FF   // Ciano — info
#define C_BLACK      0x0000   // Preto — textos
#define C_BLUE       0x001F   // Azul — graficos

#endif
