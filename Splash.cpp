// Splash.cpp — exibe /r4bb1t.bmp do SPIFFS como splash screen.
//
// Estética Cyber Edition:
//  1. Exibe BMP centralizado (lógica original preservada)
//  2. Após imagem: logo "R4BB1T" pixel art estilizado + barra de loading pixelada

#include "Splash.h"
#include "Globals.h"
#include "Config.h"
#include "Language.h"
#include <SPIFFS.h>

// ── Helpers para ler little-endian do SPIFFS ──────
static uint16_t r16(File &f) {
  return (uint16_t)f.read() | ((uint16_t)f.read() << 8);
}
static uint32_t r32(File &f) {
  uint32_t lo = r16(f);
  return lo | ((uint32_t)r16(f) << 16);
}

// ── Barra de loading pixelada estilo Cyber Edition ──
static void drawLoadingBar(int progress, int total) {
  const int BX = 8;
  const int BY = 148;
  const int BW = 112;
  const int BH = 5;
  const int BLOCK = 4; // largura de cada bloco

  // Fundo da barra
  tft.fillRect(BX, BY, BW, BH, C_GREY_DARK);
  // Borda
  tft.drawRect(BX - 1, BY - 1, BW + 2, BH + 2, C_GOLD_DIM);

  int fill = (BW * progress) / total;
  // Blocos pixelados
  for (int bx = BX; bx < BX + fill - BLOCK; bx += BLOCK + 1) {
    tft.fillRect(bx, BY, BLOCK, BH, C_GOLD);
  }
}

// ── Logo R4BB1T pixel art (5×7 px por letra, escala 2) ─────────
//
//  Cada letra: array de 7 bytes, bit 4 = coluna 0 (esquerda), bit 0 = coluna 4.
//  Pixéis 2×2 dourados com sombra de offset (+1,+1) em dourado escuro.
//
static const uint8_t PROGMEM glyph_R[7] = {
  0b11110, // ████░
  0b10001, // █░░░█
  0b10001, // █░░░█
  0b11110, // ████░
  0b10100, // █░█░░
  0b10010, // █░░█░
  0b10001, // █░░░█
};
static const uint8_t PROGMEM glyph_4[7] = {
  0b10001, // █░░░█
  0b10001, // █░░░█
  0b10001, // █░░░█
  0b11111, // █████
  0b00001, // ░░░░█
  0b00001, // ░░░░█
  0b00001, // ░░░░█
};
static const uint8_t PROGMEM glyph_B[7] = {
  0b11110, // ████░
  0b10001, // █░░░█
  0b10001, // █░░░█
  0b11110, // ████░
  0b10001, // █░░░█
  0b10001, // █░░░█
  0b11110, // ████░
};
static const uint8_t PROGMEM glyph_1[7] = {
  0b00100, // ░░█░░
  0b01100, // ░██░░
  0b00100, // ░░█░░
  0b00100, // ░░█░░
  0b00100, // ░░█░░
  0b00100, // ░░█░░
  0b01110, // ░███░
};
static const uint8_t PROGMEM glyph_T[7] = {
  0b11111, // █████
  0b00100, // ░░█░░
  0b00100, // ░░█░░
  0b00100, // ░░█░░
  0b00100, // ░░█░░
  0b00100, // ░░█░░
  0b00100, // ░░█░░
};

static const uint8_t PROGMEM glyph_space[7] = {
  0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000
};
static const uint8_t PROGMEM glyph_F[7] = {
  0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000
};
static const uint8_t PROGMEM glyph_H[7] = {
  0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001
};
static const uint8_t PROGMEM glyph_C[7] = {
  0b01111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01111
};

// Ponteiros para os 10 glifos na ordem R-4-B-B-1-T-[espaço]-F-H-C
static const uint8_t * const PROGMEM glyphs[10] = {
  glyph_R, glyph_4, glyph_B, glyph_B, glyph_1, glyph_T, glyph_space, glyph_F, glyph_H, glyph_C
};

// Espaçamento: 5 colunas × escala 2 = 10 px/letra + 2 px gap → passo 12 px
// Total: 10×12 - 2 = 118 px → centrado em 128 px → x0 = (128-118)/2 = 5
static void drawR4BB1T_Logo(int screenW, int y) {
  const int SCALE   = 2;  // tamanho de cada pixel em tela
  const int COLS    = 5;  // colunas do glifo
  const int ROWS    = 7;  // linhas do glifo
  const int GAP     = 2;  // espaço entre letras (px de tela)
  const int letterW = COLS * SCALE;
  const int totalW  = 10 * letterW + 9 * GAP;
  const int SW = (screenW > 0) ? screenW : 128;
  int x0 = (SW - totalW) / 2;

  for (int li = 0; li < 10; li++) {
    int lx = x0 + li * (letterW + GAP);
    const uint8_t *g = (const uint8_t *)pgm_read_ptr(&glyphs[li]);
    for (int row = 0; row < ROWS; row++) {
      uint8_t bits = pgm_read_byte(&g[row]);
      for (int col = 0; col < COLS; col++) {
        bool on = (bits >> (4 - col)) & 1;
        if (!on) continue;
        int px = lx + col * SCALE;
        int py = y  + row * SCALE;
        // Sombra (offset +1,+1)
        tft.fillRect(px + 1, py + 1, SCALE, SCALE, C_GOLD_DIM);
        // Pixel principal
        tft.fillRect(px, py, SCALE, SCALE, C_GOLD);
      }
    }
  }
}

void displaySplash(unsigned long delayMs) {
  tft.fillScreen(TFT_BLACK);

  File f = SPIFFS.open("/r4bb1t.bmp", "r");
  if (!f) {
    tft.setTextColor(C_RED);
    tft.setTextSize(1);
    tft.setCursor(4, 72);
    tft.print("SPIFFS: r4bb1t.bmp");
    tft.setCursor(4, 84);
    tft.print(lang->spl_bmp_nao);
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

  if (bpp != 24 || comp != 0) {
    tft.setTextColor(C_GOLD);
    tft.setTextSize(1);
    tft.setCursor(4, 72);
    tft.print(lang->spl_bmp_formato);
    tft.setCursor(4, 84);
    tft.print(lang->spl_bmp_uncompressed);
    f.close();
    delay(delayMs);
    return;
  }

  bool flipY = (bmpH > 0);
  if (bmpH < 0) bmpH = -bmpH;

  // ── Escala proporcional — imagem ocupa y=0..139 (deixa rodapé para texto) ──
  const int16_t scrW = (int16_t)tft.width();
  const int16_t scrH = 140; // reserva 20px no rodapé

  uint32_t sx = (uint32_t)scrW * 256 / (uint32_t)bmpW;
  uint32_t sy = (uint32_t)scrH * 256 / (uint32_t)bmpH;
  uint32_t sc = (sx < sy) ? sx : sy;
  if (sc > 256) sc = 256;

  int16_t dW = (int16_t)((uint32_t)bmpW * sc / 256);
  int16_t dH = (int16_t)((uint32_t)bmpH * sc / 256);
  if (dW < 1) dW = 1;
  if (dH < 1) dH = 1;

  int16_t ox = (scrW - dW) / 2;
  int16_t oy = (scrH - dH) / 2;

  // ── Aloca framebuffer ─────────────────────────
  size_t fbSize = (size_t)dW * (size_t)dH;
  uint16_t *fb = (uint16_t *)malloc(fbSize * sizeof(uint16_t));

  uint32_t rowBytes = ((uint32_t)(bmpW * 3 + 3) / 4) * 4;
  uint8_t  *row = (uint8_t *)malloc(rowBytes);

  if (!fb || !row) {
    if (fb)  free(fb);
    if (row) free(row);
    f.close();
    tft.setTextColor(C_RED);
    tft.setTextSize(1);
    tft.setCursor(4, 72);
    tft.print(lang->spl_memoria);
    delay(delayMs);
    return;
  }

  // ── Leitura sequencial ─────────────────────────
  f.seek(dataOffset);

  for (int32_t fileRow = 0; fileRow < bmpH; fileRow++) {
    if (f.read(row, rowBytes) != (int)rowBytes) break;

    int32_t imgY = flipY ? (bmpH - 1 - fileRow) : fileRow;
    int16_t sY   = (int16_t)((int32_t)imgY * dH / bmpH);
    if (sY < 0 || sY >= dH) continue;

    uint16_t *fbRow = &fb[(size_t)sY * (size_t)dW];
    for (int16_t x = 0; x < dW; x++) {
      int32_t sX = (int32_t)x * bmpW / dW;
      uint8_t b  = row[sX * 3 + 0];
      uint8_t g  = row[sX * 3 + 1];
      uint8_t r2 = row[sX * 3 + 2];
      fbRow[x] = ((uint16_t)(r2 & 0xF8) << 8) |
                 ((uint16_t)(g  & 0xFC) << 3) |
                 (b >> 3);
    }
  }

  free(row);
  f.close();

  // ── Envia framebuffer ─────────────────────────
  tft.setSwapBytes(true);
  tft.pushImage(ox, oy, dW, dH, fb);
  tft.setSwapBytes(false);
  free(fb);

  // ── Rodapé Cyber Edition — logo pixel art ────
  drawR4BB1T_Logo(0, 124);

  // Barra de loading animada (blocos pixelados)
  if (delayMs > 0) {
    const int STEPS = 20;
    unsigned long stepMs = delayMs / STEPS;
    for (int i = 1; i <= STEPS; i++) {
      drawLoadingBar(i, STEPS);
      delay(stepMs);
    }
  }
}
