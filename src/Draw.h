#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include "Model.h"

namespace dr {

// ---- deferred text (PNG export) -------------------------------------------
// The PNG exporter paints the picture TILE BY TILE and then runs ONE text pass
// over the whole image, so that every label is laid out (and de-overlapped) in
// global export coordinates exactly once instead of once per tile.
//
// While g_captureText is true, Text / TextHalo / TextWrap only RECORD the call
// into g_textSink (a null sink discards it) and draw nothing.  The record keeps
// a font DESCRIPTOR rather than a Font*, because painters hand over short-lived
// fonts; PaintTextCall rebuilds the font and replays the call verbatim, which
// keeps the on-screen painters the single source of truth for the text layout.
struct TextCall {
    std::wstring s;
    std::wstring family;      // font family name ('' = the generic default)
    float size = 0.f;         // GDI+ em size, in the font's own unit
    int style = 0;            // Gdiplus::FontStyle
    int unit = 0;             // Gdiplus::Unit
    Gdiplus::Color col;
    Gdiplus::Color halo;
    RECT r{};
    int align = 0;
    bool vcenter = true;
    bool useHalo = false;
    bool wrap = false;
};

extern bool g_captureText;
extern std::vector<TextCall>* g_textSink;
// While a PNG export runs the canvas fonts are enlarged by the export factor, so
// the layout rects the painters hand over (sized for screen fonts) must be widened
// to the measured text box, otherwise GDI+ clips the glyphs to a too-small rect.
// 1.0 on screen — set by App::ExportPngTo.
extern float g_textLayoutBoost;

void PaintTextCall(Gdiplus::Graphics& g, const TextCall& t);

Gdiplus::Color C(unsigned rgb, BYTE a = 255);
Gdiplus::Color C(const Rgb& c, BYTE a = 255);
Gdiplus::RectF RF(const RECT& r);

void Fill(Gdiplus::Graphics& g, const RECT& r, const Gdiplus::Color& c);
void FillRound(Gdiplus::Graphics& g, const Gdiplus::RectF& r, float rad,
               const Gdiplus::Color& c);
void StrokeRound(Gdiplus::Graphics& g, const Gdiplus::RectF& r, float rad,
                 const Gdiplus::Color& c, float w);
void Line(Gdiplus::Graphics& g, float x1, float y1, float x2, float y2,
          const Gdiplus::Color& c, float w);
void Circle(Gdiplus::Graphics& g, float cx, float cy, float r,
            const Gdiplus::Color& stroke, float w);

// align: 0 = near, 1 = centre, 2 = far
void Text(Gdiplus::Graphics& g, const std::wstring& s, Gdiplus::Font* f,
          const Gdiplus::Color& c, const RECT& r, int align = 0,
          bool vcenter = true);
void TextHalo(Gdiplus::Graphics& g, const std::wstring& s, Gdiplus::Font* f,
              const Gdiplus::Color& c, const Gdiplus::Color& halo,
              const RECT& r, int align = 1);
float TextW(Gdiplus::Graphics& g, const std::wstring& s, Gdiplus::Font* f);

void TextWrap(Gdiplus::Graphics& g, const std::wstring& s, Gdiplus::Font* f,
              const Gdiplus::Color& c, const RECT& r, int align);
void Icon(Gdiplus::Graphics& g, Gdiplus::Bitmap* bmp, const Gdiplus::RectF& dst,
          float angleDeg = 0.f);

std::wstring Fmt(const wchar_t* fmt, ...);

} // namespace dr
