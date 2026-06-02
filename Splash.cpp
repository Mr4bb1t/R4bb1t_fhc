// Splash.cpp — exibe /r4bb1t.bmp do SPIFFS como splash screen.
//
// Estratégia:
//  1. Lê o BMP de forma sequencial (1 seek inicial) → rápido no SPIFFS
//  2. Aplica escala proporcional + centralização em framebuffer na RAM
//  3. Envia tudo de uma vez com pushImage() → sem flickering, mais rápido

#include "Splash.h"
#include "Globals.h"
#include <SPIFFS.h>

// ── Helpers para ler little-endian do SPIFFS ──────
static uint16_t r16(File &f) {
  return (uint16_t)f.read() | ((uint16_t)f.read() << 8);
}
static uint32_t r32(File &f) {
  uint32_t lo = r16(f);
  return lo | ((uint32_t)r16(f) << 16);
}

void displaySplash(unsigned long delayMs) {
  tft.fillScreen(TFT_BLACK);

  File f = SPIFFS.open("/r4bb1t.bmp", "r");
  if (!f) {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(1);
    tft.setCursor(4, 72);
    tft.print("SPIFFS: r4bb1t.bmp");
    tft.setCursor(4, 84);
    tft.print("nao encontrado");
    delay(delayMs);
    return;
  }

  // ── Cabeçalho BMP ─────────────────────────────
  if (r16(f) != 0x4D42) {
    f.close();
    delay(delayMs);
    return;
  }                             // "BM"
  r32(f);                       // filesize
  r32(f);                       // reservado
  uint32_t dataOffset = r32(f); // offset dos pixels
  r32(f);                       // tamanho DIB header
  int32_t bmpW = (int32_t)r32(f);
  int32_t bmpH = (int32_t)r32(f);
  r16(f); // planes
  uint16_t bpp = r16(f);
  uint32_t comp = r32(f);

  if (bpp != 24 || comp != 0) { // só suporta 24-bit RGB sem compressão
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(4, 72);
    tft.print("BMP: must be 24-bit");
    tft.setCursor(4, 84);
    tft.print("uncompressed");
    f.close();
    delay(delayMs);
    return;
  }

  bool flipY = (bmpH > 0); // true = armazenado de baixo pra cima (padrão)
  if (bmpH < 0)
    bmpH = -bmpH;

  // ── Escala proporcional para caber na tela ─────
  const int16_t scrW = (int16_t)tft.width();  // ex: 128
  const int16_t scrH = (int16_t)tft.height(); // ex: 160

  // fator como inteiro ×256 para evitar float
  uint32_t sx = (uint32_t)scrW * 256 / (uint32_t)bmpW;
  uint32_t sy = (uint32_t)scrH * 256 / (uint32_t)bmpH;
  uint32_t sc = (sx < sy) ? sx : sy;
  if (sc > 256)
    sc = 256; // não ampliar

  int16_t dW = (int16_t)((uint32_t)bmpW * sc / 256);
  int16_t dH = (int16_t)((uint32_t)bmpH * sc / 256);
  if (dW < 1)
    dW = 1;
  if (dH < 1)
    dH = 1;

  int16_t ox = (scrW - dW) / 2; // offset X para centralizar
  int16_t oy = (scrH - dH) / 2; // offset Y para centralizar

  // ── Aloca framebuffer para a imagem escalada ───
  size_t fbSize = (size_t)dW * (size_t)dH;
  uint16_t *fb = (uint16_t *)malloc(fbSize * sizeof(uint16_t));

  uint32_t rowBytes =
      ((uint32_t)(bmpW * 3 + 3) / 4) * 4; // linha BMP alinhada a 4 bytes
  uint8_t *row = (uint8_t *)malloc(rowBytes);

  if (!fb || !row) {
    if (fb)
      free(fb);
    if (row)
      free(row);
    f.close();
    tft.setTextColor(TFT_RED);
    tft.setTextSize(1);
    tft.setCursor(4, 72);
    tft.print("Splash: sem memoria");
    delay(delayMs);
    return;
  }

  // ── Leitura sequencial (1 seek) ────────────────
  f.seek(dataOffset);

  for (int32_t fileRow = 0; fileRow < bmpH; fileRow++) {
    if (f.read(row, rowBytes) != (int)rowBytes)
      break;

    // BMP padrão (flipY=true): fileRow 0 = linha INFERIOR da imagem
    int32_t imgY =
        flipY ? (bmpH - 1 - fileRow) : fileRow;        // Y de cima pra baixo
    int16_t sY = (int16_t)((int32_t)imgY * dH / bmpH); // Y no framebuffer
    if (sY < 0 || sY >= dH)
      continue;

    uint16_t *fbRow = &fb[(size_t)sY * (size_t)dW];
    for (int16_t x = 0; x < dW; x++) {
      int32_t sX =
          (int32_t)x * bmpW / dW;  // coluna de origem (nearest-neighbor)
      uint8_t b = row[sX * 3 + 0]; // BMP armazena BGR
      uint8_t g = row[sX * 3 + 1];
      uint8_t r2 = row[sX * 3 + 2];
      // RGB565: R[4:0] em bits 15..11, G em 10..5, B em 4..0
      fbRow[x] =
          ((uint16_t)(r2 & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
    }
  }

  free(row);
  f.close();

  // ── Envia framebuffer para o display de uma vez ─
  tft.setSwapBytes(true);
  tft.pushImage(ox, oy, dW, dH, fb);
  tft.setSwapBytes(false);

  free(fb);
  delay(delayMs);
}
