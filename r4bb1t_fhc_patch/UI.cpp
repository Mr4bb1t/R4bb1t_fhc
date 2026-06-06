#include "UI.h"
#include "Globals.h"

// ─────────────────────────────────────────────────────────────
//  drawThickArc — arco pixelado sem float pesado
// ─────────────────────────────────────────────────────────────
void drawThickArc(int cx, int cy, int r, int thickness, uint16_t color) {
  for (int t = 0; t < thickness; t++) {
    int rr = r - t;
    if (rr < 1) break;
    for (float angle = 0; angle <= 180; angle += 0.5f) {
      float rad = angle * PI / 180.0f;
      int px = cx + (int)(rr * cos(rad));
      int py = cy - (int)(rr * sin(rad));
      tft.drawPixel(px, py, color);
    }
  }
}

// ─────────────────────────────────────────────────────────────
//  Ícone WiFi
// ─────────────────────────────────────────────────────────────
void drawWiFiIcon(int x, int y, uint16_t color) {
  int cx = x;
  int cy = y + 13;
  tft.fillCircle(cx, cy, 2, color);
  int radii[] = {5, 9, 13};
  int thickness = 2;
  for (int i = 0; i < 3; i++) {
    int r = radii[i];
    for (int t = 0; t < thickness; t++) {
      int rr = r + t;
      for (float angle = 25; angle <= 155; angle += 0.5f) {
        float rad = angle * PI / 180.0f;
        int px = cx + (int)(rr * cos(rad));
        int py = cy - (int)(rr * sin(rad));
        tft.drawPixel(px, py, color);
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────
//  Ícone Bluetooth
// ─────────────────────────────────────────────────────────────
void drawBluetoothIcon(int x, int y, uint16_t color) {
  int cx = x + 5;
  int cy = y + 10;
  int h  = 10;
  int w  = 6;
  int thick = 2;

#define PX(ox) (cx + (ox))
#define PY(oy) (cy - (oy))

  auto thickLine = [&](int x0, int y0, int x1, int y1) {
    for (int t = -thick / 2; t <= thick / 2; t++) {
      tft.drawLine(PX(x0) + t, PY(y0), PX(x1) + t, PY(y1), color);
      tft.drawLine(PX(x0), PY(y0) + t, PX(x1), PY(y1) + t, color);
    }
  };

  thickLine(0, -h, 0, +h);
  thickLine(0, h,  w, h / 2);
  thickLine(w, h / 2, 0, 0);
  thickLine(0, -h, w, -h / 2);
  thickLine(w, -h / 2, 0, 0);
  thickLine(0, 0, -w, h / 2);
  thickLine(0, 0, -w, -h / 2);

#undef PX
#undef PY
}

// ─────────────────────────────────────────────────────────────
//  Ícone RF / Antena (Estilo Imagem de Referência)
// ─────────────────────────────────────────────────────────────
void drawRFIcon(int x, int y, uint16_t color) {
  int cx = x + 14;
  int cy = y + 16;
  
  // Base triangular da antena
  int w = 6;
  int h = 14;
  tft.drawLine(cx, cy, cx - w, cy + h, color);
  tft.drawLine(cx, cy, cx + w, cy + h, color);
  tft.drawLine(cx - w, cy + h, cx + w, cy + h, color);
  
  // Engrossando o triângulo
  tft.drawLine(cx, cy + 1, cx - w + 1, cy + h - 1, color);
  tft.drawLine(cx, cy + 1, cx + w - 1, cy + h - 1, color);
  tft.drawLine(cx - w + 1, cy + h - 1, cx + w - 1, cy + h - 1, color);
  
  // Círculo no topo da antena
  int circleY = cy - 4;
  tft.drawCircle(cx, circleY, 3, color);
  tft.drawCircle(cx, circleY, 4, color);
  tft.fillCircle(cx, circleY, 2, C_BG);
  tft.fillCircle(cx, circleY, 1, color);
  
  // Ondas de rádio (esquerda e direita) simulando parênteses
  for (int r = 8; r <= 16; r += 7) {
    for (float deg = -50.0f; deg <= 50.0f; deg += 1.0f) {
      float rad = deg * PI / 180.0f;
      int dx = r * cos(rad);
      int dy = r * sin(rad);
      // Direita
      tft.drawPixel(cx + dx, circleY + dy, color);
      tft.drawPixel(cx + dx + 1, circleY + dy, color);
      // Esquerda
      tft.drawPixel(cx - dx, circleY + dy, color);
      tft.drawPixel(cx - dx - 1, circleY + dy, color);
    }
  }
}


// ─────────────────────────────────────────────────────────────
//  Ícone Settings (engrenagem)
// ─────────────────────────────────────────────────────────────
void drawSettingsIcon(int x, int y, uint16_t color, uint16_t bgColor) {
  int cx = x + 11;
  int cy = y + 11;
  const int NUM_TEETH  = 8;
  const int R_INNER    = 5;
  const int R_BODY     = 8;
  const int R_TOOTH    = 11;
  const float TOOTH_HALF_ANG = 13.0f;

  for (float deg = 0.0f; deg < 360.0f; deg += 0.5f) {
    float rad = deg * PI / 180.0f;
    float normDeg = fmod(deg, 360.0f / NUM_TEETH);
    float center  = 360.0f / NUM_TEETH / 2.0f;
    float dist    = fabs(normDeg - center);
    bool  inTooth = (dist < TOOTH_HALF_ANG);
    int rOuter = inTooth ? R_TOOTH : R_BODY;
    for (int r = R_INNER + 1; r <= rOuter; r++) {
      int px = cx + (int)roundf(r * cos(rad));
      int py = cy + (int)roundf(r * sin(rad));
      tft.drawPixel(px, py, color);
    }
  }
  for (int t = 0; t < NUM_TEETH; t++) {
    float ang = t * (360.0f / NUM_TEETH) * PI / 180.0f;
    int tx = cx + (int)roundf(R_TOOTH * cos(ang));
    int ty = cy + (int)roundf(R_TOOTH * sin(ang));
    tft.fillCircle(tx, ty, 2, color);
  }
  tft.fillCircle(cx, cy, R_INNER, bgColor);
  tft.drawCircle(cx, cy, R_INNER, color);
}

// ─────────────────────────────────────────────────────────────
//  drawText — helper simples
// ─────────────────────────────────────────────────────────────
void drawText(int x, int y, const char *text, uint16_t color) {
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.print(text);
}

// ═════════════════════════════════════════════════════════════
//  COMPONENTES CYBER EDITION
// ═════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────
//  drawHeader — barra superior 128×14 px
//    título centrado em C_GOLD, linha dourada na base
//    backArrow = true → "< " à esquerda em C_GOLD_DIM
// ─────────────────────────────────────────────────────────────
void drawHeader(const char *title, bool backArrow) {
  // Fundo do header
  tft.fillRect(0, 0, 128, 14, C_GREY_DARK);

  // Linha inferior dourada
  tft.drawFastHLine(0, 13, 128, C_GOLD);

  tft.setTextSize(1);

  // Back arrow
  if (backArrow) {
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(3, 4);
    tft.print("<");
  }

  // Título centralizado
  int tw = (int)strlen(title) * 6;
  int tx = (128 - tw) / 2;
  if (tx < 2) tx = 2;
  tft.setTextColor(C_GOLD);
  tft.setCursor(tx, 4);
  tft.print(title);
}

// ─────────────────────────────────────────────────────────────
//  drawSeparator — linha horizontal fina
// ─────────────────────────────────────────────────────────────
void drawSeparator(int y, uint16_t color) {
  tft.drawFastHLine(0, y, 128, color);
}

// ─────────────────────────────────────────────────────────────
//  drawMenuItem — item de lista estilo Cyber Edition
//    selecionado: borda lateral esquerda 3px dourada +
//                 fundo muito escuro + texto dourado
//    normal:      texto branco sem fundo
// ─────────────────────────────────────────────────────────────
void drawMenuItem(int x, int y, int w, int h,
                  const char *label,
                  bool selected,
                  bool hasArrow) {
  if (selected) {
    // Fundo escuro da seleção
    tft.fillRect(x, y, w, h, C_GOLD_SEL);
    // Borda lateral esquerda dourada (3px)
    tft.fillRect(x, y, 3, h, C_GOLD);
    // Texto dourado
    tft.setTextColor(C_GOLD);
    tft.setCursor(x + 7, y + (h - 8) / 2 + 1);
    tft.print(label);
    // Seta à direita
    if (hasArrow) {
      tft.setCursor(x + w - 9, y + (h - 8) / 2 + 1);
      tft.print(">");
    }
  } else {
    // Fundo limpo
    tft.fillRect(x, y, w, h, C_BG);
    // Borda lateral esquerda discreta
    tft.fillRect(x, y, 1, h, C_GREY);
    // Texto branco
    tft.setTextColor(C_WHITE);
    tft.setCursor(x + 7, y + (h - 8) / 2 + 1);
    tft.print(label);
    if (hasArrow) {
      tft.setTextColor(C_GREY);
      tft.setCursor(x + w - 9, y + (h - 8) / 2 + 1);
      tft.print(">");
    }
  }
  // Linha separadora inferior fina
  tft.drawFastHLine(x, y + h - 1, w, C_GREY);
}

// ─────────────────────────────────────────────────────────────
//  drawFooter — barra inferior 128×16 com ícones < o >
// ─────────────────────────────────────────────────────────────
void drawFooter() {
  tft.drawFastHLine(0, 144, 128, C_GREY);
  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(5,  148);  tft.print("<");
  tft.setCursor(61, 148);  tft.print("o");
  tft.setCursor(117, 148); tft.print(">");
}

// ─────────────────────────────────────────────────────────────
//  Ícones Menores (Modo Lista)
// ─────────────────────────────────────────────────────────────
void drawWiFiIconSmall(int x, int y, uint16_t color) {
  int cx = x + 8;
  int cy = y + 16;
  tft.fillCircle(cx, cy, 1, color);
  for (int r = 4; r <= 8; r += 4) {
    for (float deg = 35.0f; deg <= 145.0f; deg += 2.0f) {
      float rad = deg * PI / 180.0f;
      int px = cx + (int)(r * cos(rad));
      int py = cy - (int)(r * sin(rad));
      tft.drawPixel(px, py, color);
    }
  }
}

void drawBluetoothIconSmall(int x, int y, uint16_t color) {
  int cx = x + 8;
  int cy = y + 12;
  int h = 6;
  int w = 4;
  tft.drawLine(cx, cy - h, cx, cy + h, color);
  tft.drawLine(cx, cy + h, cx + w, cy + h / 2, color);
  tft.drawLine(cx + w, cy + h / 2, cx, cy, color);
  tft.drawLine(cx, cy - h, cx + w, cy - h / 2, color);
  tft.drawLine(cx + w, cy - h / 2, cx, cy, color);
  tft.drawLine(cx, cy, cx - w, cy + h / 2, color);
  tft.drawLine(cx, cy, cx - w, cy - h / 2, color);
}

void drawRFIconSmall(int x, int y, uint16_t color) {
  int cx = x + 8;
  int cy = y + 13;
  int w = 3;
  int h = 7;
  tft.drawLine(cx, cy, cx - w, cy + h, color);
  tft.drawLine(cx, cy, cx + w, cy + h, color);
  tft.drawLine(cx - w, cy + h, cx + w, cy + h, color);
  
  tft.drawCircle(cx, cy - 2, 2, color);
  for (int r = 5; r <= 9; r += 4) {
    for (float deg = -45.0f; deg <= 45.0f; deg += 3.0f) {
      float rad = deg * PI / 180.0f;
      int dx = r * cos(rad);
      int dy = r * sin(rad);
      tft.drawPixel(cx + dx, cy - 2 + dy, color);
      tft.drawPixel(cx - dx, cy - 2 + dy, color);
    }
  }
}

void drawSettingsIconSmall(int x, int y, uint16_t color, uint16_t bgColor) {
  int cx = x + 8;
  int cy = y + 13;
  tft.drawCircle(cx, cy, 3, color);
  tft.drawCircle(cx, cy, 4, color);
  for (int angle = 0; angle < 360; angle += 45) {
    float rad = angle * PI / 180.0f;
    int px1 = cx + (int)(4 * cos(rad));
    int py1 = cy + (int)(4 * sin(rad));
    int px2 = cx + (int)(6 * cos(rad));
    int py2 = cy + (int)(6 * sin(rad));
    tft.drawLine(px1, py1, px2, py2, color);
  }
}