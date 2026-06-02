#include "UI.h"
#include "Globals.h"

// Desenha arco grosso usando múltiplos drawArc ou círculos sobrepostos
// Como TFT_eSPI pode não ter drawArc espesso nativo, usamos fillCircle +
// fillCircle menor (anel)
void drawThickArc(int cx, int cy, int r, int thickness, uint16_t color) {
  // Desenha apenas a metade superior do círculo (arco de 180°)
  for (int t = 0; t < thickness; t++) {
    int rr = r - t;
    if (rr < 1)
      break;
    // Itera pelos ângulos da metade superior (0° a 180°)
    for (float angle = 0; angle <= 180; angle += 0.5f) {
      float rad = angle * PI / 180.0f;
      int px = cx + (int)(rr * cos(rad));
      int py = cy - (int)(rr * sin(rad));
      tft.drawPixel(px, py, color);
    }
  }
}

void drawWiFiIcon(int x, int y, uint16_t color) {
  // Ponto central (base do ícone)
  int cx = x;
  int cy = y + 13; // menor offset → ícone mais compacto

  // Ponto sólido na base
  tft.fillCircle(cx, cy, 2, color);

  // Três arcos concêntricos menores (radii reduzidos)
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

void drawBluetoothIcon(int x, int y, uint16_t color) {
  int cx = x + 5;  // centro horizontal
  int cy = y + 10; // centro vertical
  int h = 10;      // altura do símbolo (menor)
  int w = 6;       // largura do símbolo (menor)

  int thick = 2; // espessura das linhas

// === FUNÇÃO AUXILIAR (lambda via macro) ===
// Converte coordenadas matemáticas (ox, oy) para pixels da tela
// Eixo y invertido pois tela cresce para baixo
#define PX(ox) (cx + (ox))
#define PY(oy) (cy - (oy))

  // Desenha linha grossa entre dois pontos matemáticos
  auto thickLine = [&](int x0, int y0, int x1, int y1) {
    for (int t = -thick / 2; t <= thick / 2; t++) {
      tft.drawLine(PX(x0) + t, PY(y0), PX(x1) + t, PY(y1), color);
      tft.drawLine(PX(x0), PY(y0) + t, PX(x1), PY(y1) + t, color);
    }
  };

  // ============================================
  // v1: Segmento vertical central — (0,-h) até (0,+h)
  // ============================================
  thickLine(0, -h, 0, +h);

  // ============================================
  // TRIÂNGULO SUPERIOR (lado direito)
  // Vértices: Centro(0,0), Topo(0,h), DirSup(w, h/2)
  // ============================================

  // v3: Topo(0,h) → DirSup(w, h/2)
  thickLine(0, h, w, h / 2);

  // v4: DirSup(w, h/2) → Centro(0,0)
  thickLine(w, h / 2, 0, 0);

  // ============================================
  // TRIÂNGULO INFERIOR (lado direito)
  // Vértices: Centro(0,0), Base(0,-h), DirInf(w,-h/2)
  // ============================================

  // v6: Base(0,-h) → DirInf(w, -h/2)
  thickLine(0, -h, w, -h / 2);

  // v7: DirInf(w,-h/2) → Centro(0,0)
  thickLine(w, -h / 2, 0, 0);

  // ============================================
  // LADO ESQUERDO — perninhas
  // ============================================

  // v8: Centro(0,0) → EsqSup(-w, h/2)
  thickLine(0, 0, -w, h / 2);

  // v9: Centro(0,0) → EsqInf(-w, -h/2)
  thickLine(0, 0, -w, -h / 2);

#undef PX
#undef PY
}

void drawRFIcon(int x, int y, uint16_t color) {
  // Coordenada base: centro inferior do ícone
  // O ícone ocupa aproximadamente 30×30 px
  int cx = x + 5;  // centro horizontal da antena
  int cy = y + 20; // base da antena (reduzida)
  int thick = 2;

  auto thickLine = [&](int x0, int y0, int x1, int y1) {
    for (int t = -thick / 2; t <= thick / 2; t++) {
      tft.drawLine(x0 + t, y0, x1 + t, y1, color);
      tft.drawLine(x0, y0 + t, x1, y1 + t, color);
    }
  };

  // ── ANTENA (menor) ───────────────────────────
  // Base horizontal (cx-4 .. cx+4)
  thickLine(cx - 4, cy, cx + 4, cy);

  // Haste vertical (cy .. cy-10)
  thickLine(cx, cy, cx, cy - 10);

  // Ponta diagonal: sobe 5, vai 3 para a direita
  int tipX = cx + 3;
  int tipY = cy - 15;
  thickLine(cx, cy - 10, tipX, tipY);

  // Traço horizontal na ponta
  thickLine(tipX - 1, tipY, tipX + 3, tipY);

  // ── ONDAS RF (leque direita, raios menores) ───
  int ox = tipX + 3;
  int oy = tipY;

  int radii[] = {4, 7, 11};
  for (int i = 0; i < 3; i++) {
    int r = radii[i];
    for (int t = 0; t < thick; t++) {
      int rr = r + t;
      for (float deg = -65.0f; deg <= 65.0f; deg += 0.8f) {
        float rad = deg * PI / 180.0f;
        int px = ox + (int)(rr * cos(rad));
        int py = oy + (int)(rr * sin(rad));
        tft.drawPixel(px, py, color);
      }
    }
  }

  // Ponto origem do sinal
  tft.fillCircle(ox, oy, 1, color);
}

void drawSettingsIcon(int x, int y, uint16_t color, uint16_t bgColor) {
  int cx = x + 11;
  int cy = y + 11;

  const int NUM_TEETH  = 8;
  const int R_INNER    = 5;   // raio do buraco central
  const int R_BODY     = 8;   // raio do corpo da engrenagem
  const int R_TOOTH    = 11;  // ponta do dente
  const float TOOTH_HALF_ANG = 13.0f;  // meia-largura do dente em graus

  // --- Preenche toda a engrenagem (corpo + dentes) ---
  for (float deg = 0.0f; deg < 360.0f; deg += 0.5f) {
    float rad = deg * PI / 180.0f;

    // Descobre se este ângulo está "dentro" de um dente
    float normDeg = fmod(deg, 360.0f / NUM_TEETH);        // posição dentro do período
    float center  = 360.0f / NUM_TEETH / 2.0f;            // centro do dente
    float dist    = fabs(normDeg - center);
    bool  inTooth = (dist < TOOTH_HALF_ANG);

    int rOuter = inTooth ? R_TOOTH : R_BODY;

    // Desenha raio do centro até rOuter — mas pula o buraco central
    for (int r = R_INNER + 1; r <= rOuter; r++) {
      int px = cx + (int)roundf(r * cos(rad));
      int py = cy + (int)roundf(r * sin(rad));
      tft.drawPixel(px, py, color);
    }
  }

  // --- Arredonda as pontas dos dentes com um círculo pequeno em cada ponta ---
  for (int t = 0; t < NUM_TEETH; t++) {
    float ang = t * (360.0f / NUM_TEETH) * PI / 180.0f;
    int tx = cx + (int)roundf(R_TOOTH * cos(ang));
    int ty = cy + (int)roundf(R_TOOTH * sin(ang));
    tft.fillCircle(tx, ty, 2, color);
  }

  // --- Buraco central vazado ---
  tft.fillCircle(cx, cy, R_INNER, bgColor);

  // --- Anel do buraco (opcional — dá profundidade) ---
  tft.drawCircle(cx, cy, R_INNER, color);
}

void drawText(int x, int y, const char *text, uint16_t color) {
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.print(text);
}