#pragma once
#include <TFT_eSPI.h>
#include <Arduino.h>

// ─── Display dimensions ────────────────────────────────────────────────────
#define TM_W        162
#define TM_H        131

// ─── Layout zones ──────────────────────────────────────────────────────────
#define TM_TITLE_H   14    // Title bar height
#define TM_RIGHT_W    4   // thin right padding
#define TM_BOTTOM_H  24   // X-axis + stats bar height

// Derived plot area
#define TM_PLOT_Y    TM_TITLE_H
#define TM_PLOT_H   (TM_H - TM_TITLE_H - TM_BOTTOM_H)

// ─── Colour palette (RGB565) ───────────────────────────────────────────────
#define COL_BG       0x18C3   // near-black background  #183060 approx
#define COL_PLOT_BG  0x0000   // black plot area
#define COL_GRID     0x2965   // dark-blue grid lines
#define COL_INBOUND  0x07E0   // bright green  (fill)
#define COL_OUTBOUND 0x001F   // blue  (fill, drawn on top)
#define COL_TITLE_BG 0x2104   // very dark panel
#define COL_TEXT     0xFFFF   // white labels
#define COL_DIM      0xAD75   // grey secondary text
#define COL_PEAK     0xF800   // red peak marker

// ─── Ring-buffer length = plot width ──────────────────────────────────────
#define TM_BUF_LEN 128

// ─── Unit suffixes ────────────────────────────────────────────────────────
enum TM_Unit { TM_UNIT_BPS, TM_UNIT_KBPS, TM_UNIT_MBPS, TM_UNIT_GBPS };

struct TrafficSample {
    uint32_t inBytes;   // bytes IN  since last call
    uint32_t outBytes;  // bytes OUT since last call
};

class TrafficMonitor {
public:
    TrafficMonitor(TFT_eSPI &tft);
    ~TrafficMonitor(); // Destructor to clear sprite from RAM

    // Call once in setup()
    void begin();

    // Push a new sample every ~16 ms (60 fps); bytes transferred since last call
    void push(uint32_t inBytes, uint32_t outBytes);

    // Render the full frame (call every loop iteration after push())
    void render();

    // Optional: set a custom title (max 24 chars)
    void setTitle(const char *title);
    void setLabels(const char* labelIn, const char* labelOut, const char* labelMax, const char* labelAvg);

private:
    int _labelW = 30;
    int _plotX = 30;
    int _plotW = TM_W - 30 - TM_RIGHT_W;
    const char* _lblIn = "IN";
    const char* _lblOut = "OUT";
    const char* _lblMax = "Max:";
    const char* _lblAvg = "Avg:";
    TFT_eSPI &_tft;
    TFT_eSprite _spr;          // full-screen sprite → flicker-free

    // Ring buffers
    uint32_t _inBuf[TM_BUF_LEN];
    uint32_t _outBuf[TM_BUF_LEN];
    int      _head;            // write pointer
    bool     _full;            // ring is filled at least once

    // Running stats
    uint32_t _maxIn;
    uint32_t _maxOut;
    uint32_t _sumIn;
    uint32_t _sumOut;
    uint32_t _sampleCount;
    uint32_t _peakEver;        // overall session peak

    char _title[28];

    // Internal helpers
    uint32_t _yScale;          // current full-scale value (auto-range)
    void _updateAutoScale();
    void _drawBackground();
    void _drawGrid();
    void _drawBars();
    void _drawYAxis();
    void _drawXAxis();
    void _drawStats();
    void _drawTitle();

    static void _formatRate(uint32_t bps, char *buf, size_t len);
    static const char *_unitStr(TM_Unit u);
};
