#include "Draw.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace dr {

using namespace Gdiplus;

bool g_captureText = false;
std::vector<TextCall>* g_textSink = nullptr;
float g_textLayoutBoost = 1.f;

// Grow a layout rect around its centre to the measured text box.  Only used while a
// PNG export runs (g_textLayoutBoost > 1), where the fonts are enlarged by the same
// factor but the rects still come from screen-sized constants; without this GDI+
// clips the glyphs to the (too small) rect.
static RECT BoostRect(Graphics& g, const std::wstring& s, Font* f, const RECT& r,
                      int align, bool vcenter, bool wrap) {
    if (g_textLayoutBoost <= 1.001f || !f || s.empty()) return r;
    StringFormat fmt;
    fmt.SetAlignment(align == 0 ? StringAlignmentNear
                    : align == 1 ? StringAlignmentCenter : StringAlignmentFar);
    fmt.SetLineAlignment(vcenter ? StringAlignmentCenter : StringAlignmentNear);
    if (!wrap) fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    RectF box;
    g.MeasureString(s.c_str(), (INT)s.size(), f, RectF(0, 0, 1.0e6f, 1.0e6f), &fmt,
                    &box);
    const float pad = 4.f * g_textLayoutBoost;
    const float w = box.Width + pad, h = box.Height + pad;
    const float cx = (r.left + r.right) * 0.5f, cy = (r.top + r.bottom) * 0.5f;
    RECT o{};
    o.left = (LONG)std::lround(cx - w * 0.5f);
    o.top = (LONG)std::lround(cy - h * 0.5f);
    o.right = (LONG)std::lround(cx + w * 0.5f);
    o.bottom = (LONG)std::lround(cy + h * 0.5f);
    return o;
}

// record one text call (font -> descriptor) so the exporter can replay it later
static void RecordText(const std::wstring& s, Font* f, const Color& c,
                       const Color& halo, const RECT& r, int align, bool vcenter,
                       bool useHalo, bool wrap) {
    if (!g_textSink) return;
    TextCall t;
    t.s = s;
    t.col = c;
    t.halo = halo;
    t.r = r;
    t.align = align;
    t.vcenter = vcenter;
    t.useHalo = useHalo;
    t.wrap = wrap;
    if (f) {
        t.size = f->GetSize();
        t.style = f->GetStyle();
        t.unit = (int)f->GetUnit();
        // Font::GetFamily takes an OUT param (it is not a zero-arg getter)
        FontFamily ff;
        WCHAR nm[LF_FACESIZE] = {0};
        if (f->GetFamily(&ff) == Ok && ff.GetFamilyName(nm) == Ok) t.family = nm;
    }
    g_textSink->push_back(t);
}

static void DrawRecorded(Graphics& g, const TextCall& t, Font* f) {
    const bool saveCap = g_captureText;
    g_captureText = false;      // replay always paints, even while capturing
    if (t.wrap)
        TextWrap(g, t.s, f, t.col, t.r, t.align);
    else if (t.useHalo)
        TextHalo(g, t.s, f, t.col, t.halo, t.r, t.align);
    else
        Text(g, t.s, f, t.col, t.r, t.align, t.vcenter);
    g_captureText = saveCap;
}

void PaintTextCall(Graphics& g, const TextCall& t) {
    if (t.s.empty()) return;
    if (t.family.empty()) {
        const FontFamily* def = FontFamily::GenericSansSerif();
        if (!def) return;
        Font f(def, t.size, t.style, (Unit)t.unit);
        if (f.GetLastStatus() != Ok) return;
        DrawRecorded(g, t, &f);
        return;
    }
    FontFamily fam(t.family.c_str());
    if (fam.GetLastStatus() != Ok) return;
    Font f(&fam, t.size, t.style, (Unit)t.unit);
    if (f.GetLastStatus() != Ok) return;
    DrawRecorded(g, t, &f);
}

Color C(unsigned rgb, BYTE a) {
    return Color(a, (BYTE)((rgb >> 16) & 0xFF), (BYTE)((rgb >> 8) & 0xFF),
                 (BYTE)(rgb & 0xFF));
}
Color C(const Rgb& c, BYTE a) { return Color(a, c.r, c.g, c.b); }

RectF RF(const RECT& r) {
    return RectF((float)r.left, (float)r.top, (float)(r.right - r.left),
                 (float)(r.bottom - r.top));
}

static void RoundPath(GraphicsPath& p, const RectF& r, float rad) {
    float d = rad * 2.f;
    if (rad < 0.5f) {
        p.AddRectangle(r);
        return;
    }
    p.AddArc(r.X, r.Y, d, d, 180.f, 90.f);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270.f, 90.f);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0.f, 90.f);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90.f, 90.f);
    p.CloseFigure();
}

void Fill(Graphics& g, const RECT& r, const Color& c) {
    SolidBrush b(c);
    g.FillRectangle(&b, (INT)r.left, (INT)r.top, (INT)(r.right - r.left),
                    (INT)(r.bottom - r.top));
}

void FillRound(Graphics& g, const RectF& r, float rad, const Color& c) {
    GraphicsPath p;
    RoundPath(p, r, rad);
    SolidBrush b(c);
    g.FillPath(&b, &p);
}

void StrokeRound(Graphics& g, const RectF& r, float rad, const Color& c,
                 float w) {
    GraphicsPath p;
    RoundPath(p, r, rad);
    Pen pen(c, w);
    g.DrawPath(&pen, &p);
}

void Line(Graphics& g, float x1, float y1, float x2, float y2, const Color& c,
          float w) {
    Pen pen(c, w);
    g.DrawLine(&pen, x1, y1, x2, y2);
}

void Circle(Graphics& g, float cx, float cy, float r, const Color& stroke,
            float w) {
    Pen pen(stroke, w);
    g.DrawEllipse(&pen, cx - r, cy - r, r * 2.f, r * 2.f);
}

void TextWrap(Graphics& g, const std::wstring& s, Font* f, const Color& c,
              const RECT& r, int align) {
    if (!f || s.empty() || r.right <= r.left) return;
    if (g_captureText) {
        RecordText(s, f, c, Color(255, 0, 0, 0), r, align, true, false, true);
        return;
    }
    const RECT rr = BoostRect(g, s, f, r, align, true, true);
    StringFormat fmt;
    fmt.SetAlignment(align == 0 ? StringAlignmentNear
                    : align == 1 ? StringAlignmentCenter : StringAlignmentFar);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisWord);
    SolidBrush b(c);
    g.DrawString(s.c_str(), (INT)s.size(), f, RF(r), &fmt, &b);
}

void Text(Graphics& g, const std::wstring& s, Font* f, const Color& c,
          const RECT& r, int align, bool vcenter) {
    if (!f || s.empty() || r.right <= r.left) return;
    if (g_captureText) {
        RecordText(s, f, c, Color(255, 0, 0, 0), r, align, vcenter, false, false);
        return;
    }
    const RECT rr = BoostRect(g, s, f, r, align, vcenter, false);
    StringFormat fmt;
    fmt.SetAlignment(align == 0 ? StringAlignmentNear
                    : align == 1 ? StringAlignmentCenter
                                 : StringAlignmentFar);
    fmt.SetLineAlignment(vcenter ? StringAlignmentCenter : StringAlignmentNear);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush b(c);
    g.DrawString(s.c_str(), (INT)s.size(), f, RF(rr), &fmt, &b);
}

void TextHalo(Graphics& g, const std::wstring& s, Font* f, const Color& c,
              const Color& halo, const RECT& r, int align) {
    if (!f || s.empty() || r.right <= r.left) return;
    if (g_captureText) {
        // one record for the whole halo call — Text() below must not re-capture
        RecordText(s, f, c, halo, r, align, true, true, false);
        return;
    }
    const long hd = (std::max)(1L, std::lround(1.0f * g_textLayoutBoost));
    RECT offs[4] = {
        {r.left - hd, r.top, r.right - hd, r.bottom},
        {r.left + hd, r.top, r.right + hd, r.bottom},
        {r.left, r.top - hd, r.right, r.bottom - hd},
        {r.left, r.top + hd, r.right, r.bottom + hd},
    };
    for (auto& o : offs) Text(g, s, f, halo, o, align, true);
    Text(g, s, f, c, r, align, true);
}

float TextW(Graphics& g, const std::wstring& s, Font* f) {
    if (!f || s.empty()) return 0.f;
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    fmt.SetTrimming(StringTrimmingNone);
    RectF bounds;
    g.MeasureString(s.c_str(), (INT)s.size(), f, PointF(0, 0), &fmt, &bounds);
    return bounds.Width;
}

void Icon(Graphics& g, Bitmap* bmp, const RectF& dst, float angleDeg) {
    if (!bmp) return;
    if (angleDeg != 0.f) {
        GraphicsState st = g.Save();
        g.TranslateTransform(dst.X + dst.Width * 0.5f, dst.Y + dst.Height * 0.5f);
        g.RotateTransform(angleDeg);
        g.TranslateTransform(-(dst.X + dst.Width * 0.5f),
                             -(dst.Y + dst.Height * 0.5f));
        g.DrawImage(bmp, dst);
        g.Restore(st);
        return;
    }
    g.DrawImage(bmp, dst);
}

std::wstring Fmt(const wchar_t* fmt, ...) {
    wchar_t buf[512];
    va_list ap;
    va_start(ap, fmt);
    vswprintf_s(buf, fmt, ap);
    va_end(ap);
    return std::wstring(buf);
}

} // namespace dr
