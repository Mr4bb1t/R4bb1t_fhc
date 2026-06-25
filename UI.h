#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Config.h"

// ── Ícones primitivos ────────────────────────────────────────
void drawWiFiIcon(int x, int y, uint16_t color);
void drawBluetoothIcon(int x, int y, uint16_t color);
void drawRFIcon(int x, int y, uint16_t color);
void drawSettingsIcon(int x, int y, uint16_t color, uint16_t bgColor);
void drawText(int x, int y, const char *text, uint16_t color);

// Ícones menores para o Modo Lista
void drawWiFiIconSmall(int x, int y, uint16_t color);
void drawBluetoothIconSmall(int x, int y, uint16_t color);
void drawRFIconSmall(int x, int y, uint16_t color);
void drawSettingsIconSmall(int x, int y, uint16_t color, uint16_t bgColor);

// ── Componentes Cyber Edition ─────────────────────────────────
// Header: barra superior com título centralizado + borda dourada inferior
//   backArrow = true → desenha "←" à esquerda (telas filhas)
void drawHeader(const char *title, bool backArrow = false);

// Separador horizontal dourado
void drawSeparator(int y, uint16_t color = C_GREY);

// Item de menu em lista vertical
//   x, y        → canto superior esquerdo
//   w           → largura (normalmente 128)
//   h           → altura do item (normalmente 18)
//   label       → texto do item
//   selected    → true = destaque dourado
//   hasArrow    → true = exibe ">" à direita
void drawMenuItem(int x, int y, int w, int h,
                  const char *label,
                  bool selected,
                  bool hasArrow = true,
                  bool disabled = false,
                  bool isSaved = false);

// Rodapé com ícones de botão (< o >)
void drawFooter();

#endif
