#include "TrafficMonitor.h"
#include <stdio.h>
#include <string.h>
#include "Battery.h"

// ────────────────────────────────────────────────────────────────────────────
//  Constructor / begin
// ────────────────────────────────────────────────────────────────────────────
TrafficMonitor::TrafficMonitor(TFT_eSPI &tft)
    : _tft(tft), _spr(&tft)
{
    memset(_inBuf,  0, sizeof(_inBuf));
    memset(_outBuf, 0, sizeof(_outBuf));
    _head        = 0;
    _full        = false;
    _maxIn       = 0;
    _maxOut      = 0;
    _sumIn       = 0;
    _sumOut      = 0;
    _sampleCount = 0;
    _peakEver    = 0;
    _yScale      = 1024;   // start at 1 KB/s; auto-grows
    strncpy(_title, "Traffic Monitor - RT", sizeof(_title) - 1);
}

TrafficMonitor::~TrafficMonitor()
{
    _spr.deleteSprite();
}

void TrafficMonitor::begin()
{
    // _tft.init() and rotation are handled by the main project
    // _tft.fillScreen is also handled by the main project UI

    // Allocate 16-bit sprite (full screen)
    _spr.setColorDepth(16);
    _spr.createSprite(TM_W, TM_H);
    _spr.setTextDatum(ML_DATUM);
}

void TrafficMonitor::setTitle(const char *title)
{
    strncpy(_title, title, sizeof(_title) - 1);
    _title[sizeof(_title) - 1] = '\0';
}

void TrafficMonitor::setLabels(const char* labelIn, const char* labelOut, const char* labelMax, const char* labelAvg)
{
    _lblIn = labelIn;
    _lblOut = labelOut;
    _lblMax = labelMax;
    _lblAvg = labelAvg;
}

// ────────────────────────────────────────────────────────────────────────────
//  push() – one new sample (bytes since last call, call at 60 Hz)
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::push(uint32_t inBytes, uint32_t outBytes)
{
    // Convert bytes/frame → bits/second  (frame ≈ 1/60 s → ×60×8)
    uint32_t inBps  = inBytes  * 480UL;   // 60 fps × 8 bits
    uint32_t outBps = outBytes * 480UL;

    _inBuf[_head]  = inBps;
    _outBuf[_head] = outBps;
    _head = (_head + 1) % TM_BUF_LEN;
    if (_head == 0) _full = true;

    // Session peak
    uint32_t combined = inBps + outBps;
    if (combined > _peakEver) _peakEver = combined;

    // Rolling stats (last TM_BUF_LEN samples)
    _sampleCount++;
    _sumIn  += inBps;
    _sumOut += outBps;
    if (_sampleCount > (uint32_t)TM_BUF_LEN) {
        // subtract the sample we're overwriting (already gone from ring)
        _sampleCount = TM_BUF_LEN;   // keep denominator capped
    }

    _updateAutoScale();
}

// ────────────────────────────────────────────────────────────────────────────
//  Auto-scale: pick the smallest power-of-10 × {1,2,5} that fits the peak
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::_updateAutoScale()
{
    // Recompute visible max from ring buffer
    uint32_t vmax = 1024;
    int len = _full ? TM_BUF_LEN : _head;
    for (int i = 0; i < len; i++) {
        uint32_t tot = _inBuf[i] + _outBuf[i];
        if (tot > vmax) vmax = tot;
    }

    // Round up to a "nice" value so labels are clean
    // Sequence: …1k, 2k, 5k, 10k, 20k, 50k, 100k, 200k, 500k, 1M, …
    const uint32_t steps[] = {1, 2, 5};
    uint32_t decade = 1;
    while (true) {
        bool found = false;
        for (int s = 0; s < 3; s++) {
            uint32_t candidate = steps[s] * decade;
            if (candidate >= vmax) {
                _yScale = candidate;
                found = true;
                break;
            }
        }
        if (found) break;
        decade *= 10;
        if (decade > 10000000000UL) { _yScale = vmax; break; }
    }
    
    char buf[12];
    _formatRate(_yScale, buf, sizeof(buf));
    int lenStr = strlen(buf);
    int neededW = lenStr * 6 + 2;
    if (neededW < 30) neededW = 30;
    if (neededW > 38) neededW = 38;
    
    _labelW = neededW;
    _plotX = _labelW;
    _plotW = TM_W - _labelW - TM_RIGHT_W;
}


// ────────────────────────────────────────────────────────────────────────────
//  render() – draw everything into the sprite then push to display
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::render()
{
    _spr.fillSprite(COL_BG);
    _drawTitle();
    _drawBackground();
    _drawGrid();
    _drawBars();
    _drawYAxis();
    _drawXAxis();
    _drawStats();
    _spr.pushSprite(0, 0);
}

// ────────────────────────────────────────────────────────────────────────────
//  Title bar
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::_drawTitle()
{
    _spr.fillRect(0, 0, TM_W, TM_TITLE_H, COL_TITLE_BG);
    _spr.setTextColor(COL_TEXT, COL_TITLE_BG);
    _spr.setTextFont(1);
    _spr.setTextSize(1);
    
    // Title on the Left
    _spr.setTextDatum(ML_DATUM);
    _spr.drawString(_title, 4, TM_TITLE_H / 2);
    
    // Battery on the Right
    int pct = batteryPercent();
    int bx = TM_W - 26;
    int by = (TM_TITLE_H - 10) / 2;
    _spr.drawRect(bx, by, 20, 10, 0xE4A0); // C_GOLD
    _spr.fillRect(bx + 20, by + 3, 3, 4, 0xE4A0);
    
    if (pct > 0) {
        int pw = (pct * 16) / 100;
        uint16_t bcol = pct > 50 ? 0x07E0 : (pct > 20 ? 0xFFE0 : 0xF800);
        _spr.fillRect(bx + 2, by + 2, pw, 6, bcol);
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Plot background
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::_drawBackground()
{
    _spr.fillRect(_plotX, TM_PLOT_Y, _plotW, TM_PLOT_H, COL_PLOT_BG);
    // Thin border around plot
    _spr.drawRect(_plotX - 1, TM_PLOT_Y - 1,
                  _plotW + 2, TM_PLOT_H + 2, COL_GRID);
}

// ────────────────────────────────────────────────────────────────────────────
//  Horizontal grid lines (4 divisions)
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::_drawGrid()
{
    const int divs = 4;
    for (int d = 1; d < divs; d++) {
        int y = TM_PLOT_Y + TM_PLOT_H - (d * TM_PLOT_H / divs);
        _spr.drawFastHLine(_plotX, y, _plotW, COL_GRID);
    }
    // Vertical grid: every 25 % of plot width
    for (int d = 1; d < 4; d++) {
        int x = _plotX + d * _plotW / 4;
        _spr.drawFastVLine(x, TM_PLOT_Y, TM_PLOT_H, COL_GRID);
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Bars – green (in) fills first, blue (out) on top, newest on the right
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::_drawBars()
{
    int len = _full ? TM_BUF_LEN : _head;
    if (len == 0) return;

    for (int col = 0; col < _plotW; col++) {
        int age = (_plotW - 1) - col;
        if (age >= len) continue;

        int idx = ((_head - 1) - age + TM_BUF_LEN) % TM_BUF_LEN;

        uint32_t inBps  = _inBuf[idx];
        uint32_t outBps = _outBuf[idx];

        int hIn  = (int)((float)inBps  / _yScale * TM_PLOT_H);
        int hOut = (int)((float)outBps / _yScale * TM_PLOT_H);
        if (hIn  > TM_PLOT_H) hIn  = TM_PLOT_H;
        if (hOut > TM_PLOT_H) hOut = TM_PLOT_H;

        int x     = _plotX + col;
        int baseY = TM_PLOT_Y + TM_PLOT_H;

        // Desenha o maior primeiro (fica atrás)
        // O menor sobrepõe na frente — igual ao MRTG
        if (hIn >= hOut) {
            if (hIn  > 0) _spr.drawFastVLine(x, baseY - hIn,  hIn,  COL_INBOUND);
            if (hOut > 0) _spr.drawFastVLine(x, baseY - hOut, hOut, COL_OUTBOUND);
        } else {
            if (hOut > 0) _spr.drawFastVLine(x, baseY - hOut, hOut, COL_OUTBOUND);
            if (hIn  > 0) _spr.drawFastVLine(x, baseY - hIn,  hIn,  COL_INBOUND);
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Y-axis labels (0, 25%, 50%, 75%, 100% of _yScale)
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::_drawYAxis()
{
    _spr.setTextFont(1);
    _spr.setTextSize(1);
    _spr.setTextColor(COL_DIM, COL_BG);
    _spr.setTextDatum(MR_DATUM);

    const int divs = 4;
    char buf[12];
    for (int d = 0; d <= divs; d++) {
        uint32_t val = (uint32_t)(_yScale * (float)d / divs);
        int y = TM_PLOT_Y + TM_PLOT_H - (d * TM_PLOT_H / divs);

        int textY = y;
        if (d == divs) textY += 2; // abaixa a métrica mais alta em 2px

        _formatRate(val, buf, sizeof(buf));
        // Truncate label to keep it in the new 38-px column
        // (font1 is 6×8; 38px ÷ 6 = 6 chars max)
        buf[6] = '\0';
        _spr.drawString(buf, _plotX - 2, textY);

        // Tick mark
        _spr.drawPixel(_plotX - 1, y, COL_GRID);
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  X-axis time labels  (ss or mm:ss depending on buffer age)
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::_drawXAxis()
{
    int xAxisY = TM_PLOT_Y + TM_PLOT_H + 2;
    _spr.setTextFont(1);
    _spr.setTextSize(1);
    _spr.setTextColor(COL_DIM, COL_BG);
    _spr.setTextDatum(TC_DATUM);

    // Buffer covers TM_BUF_LEN frames at 60 fps → TM_BUF_LEN/60 seconds
    float totalSecs = (float)TM_BUF_LEN / 60.0f;

    char buf[10];
    
    // Always draw "now" at the right edge
    strncpy(buf, "now", sizeof(buf));
    _spr.setTextDatum(TR_DATUM);
    _spr.drawString(buf, _plotX + _plotW, xAxisY);
    _spr.setTextDatum(TC_DATUM);

    // Determine a reasonable step size for seconds
    int secStep = 1;
    if (totalSecs > 120.0f) secStep = 30;
    else if (totalSecs > 60.0f) secStep = 15;
    else if (totalSecs > 20.0f) secStep = 5;
    else if (totalSecs > 10.0f) secStep = 2;

    for (int s = secStep; s <= (int)totalSecs; s += secStep) {
        int x = _plotX + _plotW - (int)((float)s / totalSecs * _plotW);
        if (x >= _plotX + 8) { // Only draw if it fits within the plot bounds
            if (s >= 60) {
                int m = s / 60;
                int rem = s % 60;
                if (rem == 0) snprintf(buf, sizeof(buf), "-%dm", m);
                else snprintf(buf, sizeof(buf), "-%dm%d", m, rem);
            } else {
                snprintf(buf, sizeof(buf), "-%ds", s);
            }
            _spr.drawString(buf, x, xAxisY);
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────
//  Stats bar: Max | Avg (bottom strip)
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::_drawStats()
{
    int statY = TM_H - 6; // 1px above the physical bottom edge
    _spr.setTextFont(1);
    _spr.setTextSize(1);
    _spr.setTextDatum(ML_DATUM);

    // Recompute visible max & average from ring
    int len = _full ? TM_BUF_LEN : _head;
    uint32_t visMax = 0, visSum = 0;
    for (int i = 0; i < len; i++) {
        uint32_t tot = _inBuf[i] + _outBuf[i];
        if (tot > visMax) visMax = tot;
        visSum += tot;
    }
    uint32_t visAvg = len > 0 ? visSum / len : 0;

    char bufMax[16], bufAvg[16];
    _formatRate(visMax, bufMax, sizeof(bufMax));
    _formatRate(visAvg, bufAvg, sizeof(bufAvg));

    // Single line stats layout
    int bx = 3; // Center of circle at 3, touches left edge (X=0)
    
    // IN
    _spr.fillCircle(bx, statY, 3, COL_INBOUND);
    _spr.setTextColor(COL_INBOUND, COL_BG);
    _spr.drawString(_lblIn, bx + 4, statY);
    
    // OUT
    bx += 20;
    _spr.fillCircle(bx, statY, 3, COL_OUTBOUND);
    _spr.setTextColor(COL_OUTBOUND, COL_BG);
    _spr.drawString(_lblOut, bx + 4, statY);
    
    // Max
    bx += 26;
    _spr.setTextColor(COL_DIM, COL_BG);
    _spr.drawString(_lblMax, bx, statY);
    _spr.setTextColor(COL_PEAK, COL_BG);
    _spr.drawString(bufMax, bx + 24, statY);
    
    // Avg
    bx += 56;
    _spr.setTextColor(COL_DIM, COL_BG);
    _spr.drawString(_lblAvg, bx, statY);
    _spr.setTextColor(COL_INBOUND, COL_BG);
    _spr.drawString(bufAvg, bx + 24, statY);
}

// ────────────────────────────────────────────────────────────────────────────
//  Helpers
// ────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::_formatRate(uint32_t bps, char *buf, size_t len)
{
    if      (bps >= 1000000000UL)  snprintf(buf, len, "%.1fG", bps / 1000000000.0f);
    else if (bps >= 1000000UL)     snprintf(buf, len, "%.1fM", bps / 1000000.0f);
    else if (bps >= 1000UL)        snprintf(buf, len, "%.0fK", bps / 1000.0f);
    else                           snprintf(buf, len, "%uB",   bps);
}
