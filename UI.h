#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <TFT_eSPI.h>

void drawWiFiIcon(int x, int y, uint16_t color);
void drawBluetoothIcon(int x, int y, uint16_t color);
void drawRFIcon(int x, int y, uint16_t color);
void drawSettingsIcon(int x, int y, uint16_t color, uint16_t bgColor);
void drawText(int x, int y, const char *text, uint16_t color);

#endif
