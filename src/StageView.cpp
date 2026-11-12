#include "App.h"
#include "Assets.h"
#include "Catalog.h"
#include "Draw.h"
#include "L10n.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

namespace {

// Stage 01 / 02: the main microphone is drawn with its own procedural geometry
// (the L/R bar plus a Decca centre capsule), never with the catalogue icon, so the
// icon-rect scale below cannot grow it.  This factor scales ONLY the line art about
// the item centre — circle radii, pen widths, the M mark and the Decca centre
// capsule's own radius.  It must NEVER multiply a distance in metres: the capsule
// centre-to-centre gap is exactly `it.spacing` × ppm_ (user: 「你放大了话筒的圆圈，
// 但不能放大间距」), so a 0.40 m AB pair really is 0.40 m apart on screen.
constexpr double kMainMicSymScale = 1.6;
// Point mics keep a small catalogue icon so the plot stays readable, but not a
// mosquito: 0.3 × 1.7 = 0.51 (the user asked for a 1.7× enlargement).
constexpr double kPointMicSymScale = 0.3 * 1.7;
// 领夹话筒 is bigger than a normal point mic (user: it shrank to a mosquito, x1.75)
constexpr double kLavalierSymScale = kPointMicSymScale;   // same size as a point mic

static double MicSymScale(const Item& it) {
    if (it.defId == L"mainmic") return kMainMicSymScale;
    if (it.defId == L"lavalier") return kLavalierSymScale;
    return kPointMicSymScale;
}

// Radius of one main-mic capsule circle: the ONLY part of the symbol (together
// with the pen widths) that the 1.6× symbol scale grows.
double MainMicCapRadius(double ppm) {
    return (std::max)(5.0, 0.066 * ppm) * kMainMicSymScale;
}

// Half extents (screen px) of the DRAWN main-mic symbol measured from the item
// centre.  `side` follows the capsule link bar (real metres: spacing/2), `fwd` /
// `back` follow the aiming axis (the Decca centre capsule reaches `center`
// metres).  The hit box, the band selection and the selection halo all use it,
// so what is clickable is exactly what is painted.
struct MainMicExtent {
    double side;
    double fwd;
    double back;
};

MainMicExtent MainMicSymbolExtent(const Item& it, double ppm) {
    const double capR = MainMicCapRadius(ppm);
    MainMicExtent e;
    e.side = (std::max)(0.02, it.spacing) * 0.5 * ppm + capR;
    e.fwd = capR * 1.1;
    e.back = capR * 1.1;
    if (it.mainTech == 3) {
        const double c = (std::max)(0.0, it.center) * ppm;
        e.fwd = (std::max)(e.fwd, c + capR);
    }
    return e;
}

// Axis-aligned half extent (px) of the rotated symbol bounding box.
double MainMicAxisHalf(const MainMicExtent& e, double angleDeg, bool alongX) {
    const double kPi = 3.14159265358979323846;
    const double sa = std::fabs(std::sin(angleDeg * kPi / 180.0));
    const double ca = std::fabs(std::cos(angleDeg * kPi / 180.0));
    const double f = (std::max)(e.fwd, e.back);
    return alongX ? ca * e.side + sa * f : sa * e.side + ca * f;
}

bool PointNearSegment(double x, double y, double x0, double y0, double x1, double y1,
                      double tol) {
    const double vx = x1 - x0, vy = y1 - y0;
    const double len2 = vx * vx + vy * vy;
    double t = 0.0;
    if (len2 > 1e-9) t = std::clamp(((x - x0) * vx + (y - y0) * vy) / len2, 0.0, 1.0);
    return std::hypot(x - (x0 + vx * t), y - (y0 + vy * t)) <= tol;
}

// Is (x, y) on the DRAWN main-mic symbol?  Two capsule circles joined by the link
// bar, plus (Decca) the centre capsule and its stem.  A plain rectangle would
// swallow the neighbouring mics once the capsules are 3 m apart.
bool MainMicSymbolHit(const Item& it, double cx, double cy, double ppm, int x, int y,
                      int pad) {
    const double kPi = 3.14159265358979323846;
    const double rad = it.angle * kPi / 180.0;
    const double fwdX = std::sin(rad), fwdY = -std::cos(rad);
    const double sideX = std::cos(rad), sideY = std::sin(rad);
    const double half = (std::max)(0.02, it.spacing) * 0.5 * ppm;
    const double hitR = MainMicCapRadius(ppm) + pad;
    const double lx = cx - half * sideX, ly = cy - half * sideY;
    const double rx = cx + half * sideX, ry = cy + half * sideY;
    const double px = (double)x - cx, py = (double)y - cy;
    if (std::hypot((double)x - lx, (double)y - ly) <= hitR) return true;
    if (std::hypot((double)x - rx, (double)y - ry) <= hitR) return true;
    if (PointNearSegment((double)x, (double)y, lx, ly, rx, ry, pad + 1.5)) return true;
    // the M mark sits between the capsules, inside the link-bar reach
    if (std::hypot(px, py) <= MainMicCapRadius(ppm) * 1.1 + pad) return true;
    if (it.mainTech == 3) {
        const double mx = cx + fwdX * it.center * ppm;
        const double my = cy + fwdY * it.center * ppm;
        if (std::hypot((double)x - mx, (double)y - my) <= hitR) return true;
        if (PointNearSegment((double)x, (double)y, cx, cy, mx, my, pad + 1.5)) return true;
    }
    return false;
}

void RoundPath(Gdiplus::GraphicsPath& p, const Gdiplus::RectF& r, float rad) {
    float d2 = rad * 2.f;
    if (rad < 0.5f) {
        p.AddRectangle(r);
        return;
    }
    p.AddArc(r.X, r.Y, d2, d2, 180.f, 90.f);
    p.AddArc(r.X + r.Width - d2, r.Y, d2, d2, 270.f, 90.f);
    p.AddArc(r.X + r.Width - d2, r.Y + r.Height - d2, d2, d2, 0.f, 90.f);
    p.AddArc(r.X, r.Y + r.Height - d2, d2, d2, 90.f, 90.f);
    p.CloseFigure();
}

// The muted palette's ID LIST — a FIXED list, never a hash: a defId must keep ONE
// colour across sessions and project files.  Order = catalogue order of
// src/Catalog.cpp kInstruments, then the section targets of kSpotExtras (the same
// order the 02 目标 list uses).  The COLOUR of slot i lives in kMutedPal[i], and
// that array is deliberately NOT in hue order any more: it is the MAX-MIN PATH
// over the same 40 colours (tools/palette_design.py -> _palette_report.txt), so
// two NEIGHBOURING slots — the catalogue neighbours the 02 target list paints one
// under the other — sit as far apart as the fixed set allows.  Achieved: min
// consecutive dE 24.2 (icon 31.9 / drawn ink 24.2); the retired hue-wheel order
// scored 5.5 (mic ink 17/18 nearly the same teal).
const wchar_t* const kMutedOrder[] = {
    L"grand_piano",   L"harpsichord",     L"celesta",          L"upright_piano",
    L"violin",        L"violin2",         L"viola",            L"cello",
    L"bass",          L"harp",            L"flute",            L"oboe",
    L"clarinet",      L"bassoon",         L"horn",             L"trumpet",
    L"trombone",      L"tuba",            L"timpani",          L"bassdrum",
    L"snare",         L"cymbal",          L"xylophone",        L"glockenspiel",
    L"gong",          L"percussion",      L"conductor",        L"solo_singer",
    L"choir_singer",  L"custom",          L"section_surround", L"section_height",
    L"section_side",  L"section_ambience", L"section_strings", L"section_woodwind",
    L"section_brass", L"section_perc"};
constexpr int kMutedOrderCount = (int)(sizeof(kMutedOrder) / sizeof(kMutedOrder[0]));
constexpr int kMutedPaletteSize = 40;

// 40 muted tints — low chroma, Rec.601 luminance 138..158, i.e. the same
// character as the old ten (which sat at 136..161).  These are the ICON tints;
// what a 1.9 px cable / 3 px marker / tag draws is MicInkLine() of them, whose
// luminance cap at 104 throws away most of the lightness difference, so the
// palette is generated to keep the icon tints AND those capped inks apart
// (tools/palette_design.py).  SLOT ORDER = that max-min path, not hue order.
const Rgb kMutedPal[kMutedPaletteSize] = {
    {0xB3, 0x98, 0x86}, {0x9C, 0x7B, 0xA9}, {0x81, 0xA8, 0xB6},
    {0xBA, 0x76, 0x76}, {0x5F, 0x95, 0xC2}, {0x8A, 0xA9, 0x98},
    {0xB5, 0x7C, 0x64}, {0x4E, 0x9E, 0xC0}, {0xA1, 0x88, 0x58},
    {0x75, 0x8B, 0xBD}, {0x4D, 0xA6, 0x9D}, {0x94, 0x8E, 0x5A},
    {0x81, 0xAB, 0xA9}, {0x8B, 0x81, 0xB4}, {0xB9, 0x93, 0x93},
    {0x45, 0xA6, 0xAC}, {0xAD, 0x81, 0x5C}, {0x64, 0xA7, 0xAF},
    {0xB5, 0x74, 0x8B}, {0x6C, 0x9D, 0x76}, {0x45, 0xA4, 0xBC},
    {0xB7, 0x83, 0x8E}, {0x5D, 0xA1, 0x86}, {0x8D, 0xA1, 0xB9},
    {0x80, 0x96, 0x65}, {0xA8, 0x76, 0x9E}, {0x71, 0xA6, 0x93},
    {0xB3, 0x93, 0xA3}, {0x69, 0xA7, 0xA1}, {0xB8, 0x87, 0x7B},
    {0x69, 0xA2, 0xBB}, {0xAB, 0x90, 0x6E}, {0xA3, 0x98, 0xB1},
    {0x97, 0x99, 0x72}, {0x91, 0x8F, 0xB5}, {0xA7, 0x9E, 0x84},
    {0x78, 0x9A, 0xBD}, {0x9A, 0xA4, 0x8A}, {0xA8, 0x87, 0xA6},
    {0x84, 0xA0, 0x7D}};

// ---- CIELab (D65) — the space the min-consecutive-dE check measures in.
// Mirror of tools/palette_design.py's lab()/de()/ink(), so the C++ report and the
// design script agree to the decimal.
double SrgbToLinear(double v) {
    const double s = v / 255.0;
    return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}

struct Lab3 {
    double l = 0.0, a = 0.0, b = 0.0;
};

Lab3 RgbToLab(const Rgb& c) {
    const double r = SrgbToLinear(c.r), g = SrgbToLinear(c.g), b = SrgbToLinear(c.b);
    const double x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047;
    const double y = (0.2126 * r + 0.7152 * g + 0.0722 * b);
    const double z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883;
    auto f = [](double t) {
        return t > 0.008856 ? std::cbrt(t) : (7.787 * t + 16.0 / 116.0);
    };
    const double fx = f(x), fy = f(y), fz = f(z);
    return Lab3{116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

double LabDE(const Lab3& p, const Lab3& q) {
    const double dl = p.l - q.l, da = p.a - q.a, db = p.b - q.b;
    return std::sqrt(dl * dl + da * da + db * db);
}

Rgb MutedColor(const std::wstring& id) {
    static_assert(kMutedOrderCount <= kMutedPaletteSize,
                  "MutedColor: the palette must cover every ordered id");
    for (int i = 0; i < kMutedOrderCount; ++i) {
        if (id == kMutedOrder[i]) return kMutedPal[i];
    }
    // ids outside the list (old project files, a future catalogue entry): keep the
    // old stable hash so such an id still always gets the SAME one of the 40
    unsigned h = 0;
    for (wchar_t c : id) h = h * 131u + (unsigned)c;
    return kMutedPal[h % kMutedPaletteSize];
}

bool ItemRects(const Item& it, double ppm, double ox, double oy, int padX,
               RECT& box, RECT& lbl) {
    const CatItem* c = FindItem(it.defId);
    if (!c) return false;
    double ww = it.w > 0.01 ? it.w : c->w;
    double hh = it.h > 0.01 ? it.h : c->h;

    double rad = it.angle * 3.14159265358979323846 / 180.0;
    double ca = std::fabs(std::cos(rad));
    double sa = std::fabs(std::sin(rad));
    int w = (int)std::lround((ww * ca + hh * sa) * ppm);
    int h = (int)std::lround((ww * sa + hh * ca) * ppm);
    w = (std::max)(w, 8);
    h = (std::max)(h, 8);
    int cx = (int)std::lround(ox + it.x * ppm);
    int cy = (int)std::lround(oy + it.y * ppm);
    box = {cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2};
    lbl = {0, 0, 0, 0};
    (void)padX;
    return true;
}

void UnionRect(RECT& a, const RECT& b) {
    a.left = (std::min)(a.left, b.left);
    a.top = (std::min)(a.top, b.top);
    a.right = (std::max)(a.right, b.right);
    a.bottom = (std::max)(a.bottom, b.bottom);
}

// Per-capsule model text of the main microphone: 0 = L, 1 = C, 2 = R.  This is
// the text the stage caption and the channel table both print; when a capsule
// has no text of its own it falls back exactly like a normal microphone does.
// 型号 cell of ONE main-mic capsule: its per-capsule model, or EMPTY.  It must
// never print a bare "L"/"C"/"R" — in a 型号 column that reads as a model name
// (user report: the main mic's C capsule showed just 「C」).
std::wstring CapsuleModelText(const Item& it, int capsule) {
    if (capsule == 1) return it.modelC;
    return (capsule == 2) ? it.modelR : it.modelL;
}

// The one caption a main microphone prints under its symbol (显示文字).
std::wstring MainMicCaption(const Item& it) {
    if (it.modelL.empty() && it.modelR.empty() && it.modelC.empty()) return it.label;
    const std::wstring a = it.modelL.empty() ? std::wstring(L"L") : it.modelL;
    const std::wstring b = it.modelR.empty() ? std::wstring(L"R") : it.modelR;
    if (it.mainTech == 3) {
        const std::wstring c = it.modelC.empty() ? std::wstring(L"C") : it.modelC;
        return a + L" / " + c + L" / " + b;
    }
    return a + L" / " + b;
}

std::wstring InstTypeName(const std::wstring& id) {
    // 对应乐器 key → localized name: a catalogue instrument, one of the two extra
    // targets (侧展 / 环境), or the raw string of an old / unknown binding
    return SpotTypeName(id);
}

} // namespace

bool StageItemRects(const Item& it, double ppm, double ox, double oy, int padX, RECT& box,
                    RECT& lbl) {
    return ItemRects(it, ppm, ox, oy, padX, box, lbl);
}

Rgb StageMutedColor(const std::wstring& id) {
    return MutedColor(id);
}

// the fixed assignment order itself, so the --dump-colors check can walk exactly
// the ids that own a palette slot (and catch a catalogue id that is missing here)
int StageMutedOrderCount() {
    return kMutedOrderCount;
}

const wchar_t* StageMutedOrderAt(int i) {
    return (i >= 0 && i < kMutedOrderCount) ? kMutedOrder[i] : L"";
}

int StageMutedPaletteSize() {
    return kMutedPaletteSize;
}

// CIELab dE between two palette SLOTS, in the space asked for: drawnInk = false
// is the icon tint, true is what the 1.9 px cable / 3 px marker / tag actually
// paints (MicInkLine caps the luminance at 104, which shrinks lightness
// differences).  --dump-colors reports the minimum over CONSECUTIVE slots, the
// property the max-min ordering of kMutedPal is designed for.
double StageMutedPairDE(int slotA, int slotB, bool drawnInk) {
    if (slotA < 0 || slotB < 0 || slotA >= kMutedPaletteSize ||
        slotB >= kMutedPaletteSize)
        return 0.0;
    const Rgb a = kMutedPal[slotA], b = kMutedPal[slotB];
    if (drawnInk) return LabDE(RgbToLab(MicInkLine(a)), RgbToLab(MicInkLine(b)));
    return LabDE(RgbToLab(a), RgbToLab(b));
}

// ONE ink per microphone = the icon tint of what it is BOUND to (see App.h)
Rgb MicBoundInk(const Project& pr, const Item& it) {
    if (it.kind != ItemKind::Mic) return pr.instColor;
    if (it.spotType.empty()) return pr.micColor;          // unbound: neutral ink
    // the instrument INSTANCE's own 颜色 wins over the palette (its icon and every
    // microphone / cable / tag bound to it read as one custom colour family)
    if (const unsigned ov = InstColorOverrideFor(pr, it.spotType))
        return RgbFromHex(ov);
    if (it.spotType == L"grand_piano") return pr.instColor;   // the piano's own ink
    return MutedColor(it.spotType);                       // instrument / 声部 tint
}

// The ICON tint of one instrument item (see App.h): the per-item override first,
// then the piano / palette rule the 01/02 page has always used.
Rgb ItemIconTint(const Project& pr, const Item& it, bool muted) {
    if (it.colorOverride) return RgbFromHex(it.colorOverride);
    if (it.defId == L"grand_piano" || !muted) return pr.instColor;
    return MutedColor(it.defId);
}

// The override an instrument instance of `defId` carries (0 = none).  A first
// match wins: two instances of one type share one microphone binding, so the
// first one that was given a colour is the family's colour.
unsigned InstColorOverrideFor(const Project& pr, const std::wstring& defId) {
    if (defId.empty()) return 0;
    for (const auto& it : pr.items) {
        if (it.kind != ItemKind::Instrument) continue;
        if (it.defId == defId && it.colorOverride) return it.colorOverride;
    }
    return 0;
}

// that ink at the weight a 1.9 px line / 3 px marker needs on white (see App.h)
Rgb MicInkLine(const Rgb& tint) {
    // luminance (Rec.601 weights) of the old neutral inks: the cable grey 0x6B635A
    // is ~100, the microphone ink 0x554C42 ~77 — the muted palette sits at ~147
    constexpr double kCap = 104.0;
    const double lum =
        0.299 * (double)tint.r + 0.587 * (double)tint.g + 0.114 * (double)tint.b;
    if (lum <= kCap) return tint;      // already dark enough (a neutral / dark ink)
    const double f = kCap / lum;
    Rgb o;
    o.r = (std::uint8_t)std::lround((double)tint.r * f);
    o.g = (std::uint8_t)std::lround((double)tint.g * f);
    o.b = (std::uint8_t)std::lround((double)tint.b * f);
    return o;
}

// the drawn SYMBOL scale of a microphone (main mic 1.6, point / lavalier
// 0.3 × 1.7): it scales the line art only — never any DISTANCE
double MicSymScaleOf(const Item& it) {
    return MicSymScale(it);
}

// ---------------------------------------------------------------------------
// SAME-TYPE (defId) GROUP HULL — item 4
//
// An instrument defId with >= 3 instances gets a thin muted convex hull built
// from the ROTATED footprints of its instances plus ~0.25 m of padding, painted
// UNDER the icons.  When that hull contains no item of another defId, the type's
// per-instance labels are suppressed and ONE 「类型 ×N」 label is centred on the
// hull's bottom edge; a hull that swallows another type keeps the per-instance
// labels (the cluster is not a clean group).  Purely decorative: no hit test ever
// looks at it, and it rides the zoom / pan transform like every other mark.
// ---------------------------------------------------------------------------
namespace {
struct HPt {
    double x = 0, y = 0;
};

double HullCross(const HPt& o, const HPt& a, const HPt& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

std::vector<HPt> ConvexHull(std::vector<HPt> p) {
    std::sort(p.begin(), p.end(), [](const HPt& a, const HPt& b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    if (p.size() < 3) return p;
    std::vector<HPt> h(p.size() * 2);
    size_t k = 0;
    for (size_t i = 0; i < p.size(); ++i) {
        while (k >= 2 && HullCross(h[k - 2], h[k - 1], p[i]) <= 0) --k;
        h[k++] = p[i];
    }
    for (size_t i = p.size() - 1, t = k + 1; i > 0; --i) {
        while (k >= t && HullCross(h[k - 2], h[k - 1], p[i - 1]) <= 0) --k;
        h[k++] = p[i - 1];
    }
    h.resize(k > 0 ? k - 1 : 0);
    return h;
}

// push every edge of a CONVEX polygon outwards by `d` (intersect the offset lines)
std::vector<HPt> OffsetConvex(const std::vector<HPt>& h, double d) {
    const size_t n = h.size();
    if (n < 3) return h;
    double cx = 0, cy = 0;
    for (const auto& q : h) {
        cx += q.x;
        cy += q.y;
    }
    cx /= (double)n;
    cy /= (double)n;
    struct Line {
        double px = 0, py = 0, dx = 1, dy = 0;
    };
    std::vector<Line> ln(n);
    for (size_t i = 0; i < n; ++i) {
        const HPt& a = h[i];
        const HPt& b = h[(i + 1) % n];
        double ex = b.x - a.x, ey = b.y - a.y;
        const double len = std::sqrt(ex * ex + ey * ey);
        if (len < 1e-12) {
            ln[i] = {a.x, a.y, 1, 0};
            continue;
        }
        ex /= len;
        ey /= len;
        double nx = -ey, ny = ex;
        const double mx = (a.x + b.x) * 0.5, my = (a.y + b.y) * 0.5;
        if ((mx - cx) * nx + (my - cy) * ny < 0) {   // the normal must face OUT
            nx = -nx;
            ny = -ny;
        }
        ln[i] = {a.x + nx * d, a.y + ny * d, ex, ey};
    }
    std::vector<HPt> out(n);
    for (size_t i = 0; i < n; ++i) {
        const Line& a = ln[(i + n - 1) % n];
        const Line& b = ln[i];
        const double den = a.dx * b.dy - a.dy * b.dx;
        if (std::fabs(den) < 1e-9) {
            out[i] = {b.px, b.py};
            continue;
        }
        const double t = ((b.px - a.px) * b.dy - (b.py - a.py) * b.dx) / den;
        out[i] = {a.px + a.dx * t, a.py + a.dy * t};
    }
    return out;
}

bool PointInPoly(const std::vector<HPt>& poly, double x, double y) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const HPt& a = poly[i];
        const HPt& b = poly[j];
        if (((a.y > y) != (b.y > y)) &&
            (x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x))
            in = !in;
    }
    return in;
}
} // namespace

// every clean / mixed hull of the current placement, in world metres
std::vector<App::TypeHull> App::BuildTypeHulls() const {
    std::vector<TypeHull> out;
    if (!pr_.showInstruments) return out;
    std::map<std::wstring, int> count;
    for (const auto& it : pr_.items)
        if (it.kind == ItemKind::Instrument) ++count[it.defId];
    for (const auto& kv : count) {
        if (kv.second < 3) continue;
        // 独唱歌手 never forms a section: every solo singer keeps its own
        // per-instance label even when 3+ of them stand on the stage, so there
        // is no 「独唱歌手 ×N」 caption.  The count map keeps the tally on
        // purpose: the mixed-hull test below still sees a soloist as "grouped
        // elsewhere", so one standing inside a big section does not mark that
        // section's hull as mixed.
        if (kv.first == L"solo_singer") continue;
        TypeHull h;
        h.defId = kv.first;
        h.count = kv.second;
        h.name = SpotTypeName(kv.first);
        std::vector<HPt> pts;
        for (const auto& it : pr_.items) {
            if (it.kind != ItemKind::Instrument || it.defId != kv.first) continue;
            const CatItem* c = FindItem(it.defId);
            const double ww = it.w > 0.01 ? it.w : (c ? c->w : 0.5);
            const double hh = it.h > 0.01 ? it.h : (c ? c->h : 0.5);
            const double rad = it.angle * 3.14159265358979323846 / 180.0;
            const double ca = std::cos(rad), sa = std::sin(rad);
            for (int sx = -1; sx <= 1; sx += 2)
                for (int sy = -1; sy <= 1; sy += 2) {
                    const double lx = sx * ww * 0.5, ly = sy * hh * 0.5;
                    pts.push_back(
                        {it.x + lx * ca - ly * sa, it.y + lx * sa + ly * ca});
                }
        }
        const std::vector<HPt> hull = OffsetConvex(ConvexHull(pts), 0.25);
        if (hull.size() < 3) continue;
        double minX = 1e9, maxX = -1e9, maxY = -1e9;
        for (const auto& q : hull) {
            h.pts.push_back({q.x, q.y});
            minX = (std::min)(minX, q.x);
            maxX = (std::max)(maxX, q.x);
            maxY = (std::max)(maxY, q.y);
        }
        // A hull that contains a DIFFERENT defId is not a clean group — but a
        // foreign item whose OWN type is grouped as well (>= 3 instances, so it
        // gets its own single caption) does not pollute: the two big blocks
        // interleaved on the plot each keep ONE caption, which is the whole point
        // of the grouping.  Only a foreign item of a SMALL type makes it mixed,
        // and a mixed hull keeps the per-instance labels.
        for (const auto& other : pr_.items) {
            if (other.kind != ItemKind::Instrument) continue;
            if (other.defId == kv.first) continue;
            auto oc = count.find(other.defId);
            if (oc != count.end() && oc->second >= 3) continue;   // grouped too
            if (PointInPoly(hull, other.x, other.y)) {
                h.clean = false;
                break;
            }
        }
        h.labelX = (minX + maxX) * 0.5;   // centred on the bottom edge
        h.labelY = maxY;
        out.push_back(h);
    }
    return out;
}

// The same-type outline is NOT painted any more (it read as noise): the hull is
// still computed because it is what decides WHERE the single 「类型 ×N」 caption
// goes and which types keep their per-instance labels.
std::vector<App::TypeHull> App::BuildTypeHullLabels() const {
    std::vector<TypeHull> out;
    for (const auto& h : BuildTypeHulls())
        if (h.clean) out.push_back(h);
    return out;
}

// Place one canvas label so it overlaps neither the already placed labels nor a
// drawn symbol / box, and never leaves the canvas.  Candidates: as wanted, then
// above, left, right, and finally a few small nudges; the first collision-free one
// wins, otherwise the least overlapping (a label must never disappear).
RECT App::PlaceCanvasLabel(const RECT& want, const std::vector<RECT>& avoid,
                           const std::vector<RECT>& obstacles,
                           const RECT& canvas, int* overlapScore) const {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    const int w = (std::max)(1, (int)(want.right - want.left));
    const int h = (std::max)(1, (int)(want.bottom - want.top));
    auto clampIn = [&](RECT r) {
        if (r.right > canvas.right - d(2)) {
            r.right = canvas.right - d(2);
            r.left = r.right - w;
        }
        if (r.left < canvas.left + d(2)) {
            r.left = canvas.left + d(2);
            r.right = r.left + w;
        }
        if (r.bottom > canvas.bottom - d(2)) {
            r.bottom = canvas.bottom - d(2);
            r.top = r.bottom - h;
        }
        if (r.top < canvas.top + d(2)) {
            r.top = canvas.top + d(2);
            r.bottom = r.top + h;
        }
        return r;
    };
    auto overlap = [&](const RECT& r) {
        long sc = 0;
        for (const auto& a : avoid) {
            if (a.left < r.right && a.right > r.left && a.top < r.bottom &&
                a.bottom > r.top)
                sc += 4000;   // a label on a label is the worst: never preferred
        }
        for (const auto& a : obstacles) {
            if (a.left >= r.right || a.right <= r.left || a.top >= r.bottom ||
                a.bottom <= r.top)
                continue;
            const long ix = (std::min)(a.right, r.right) - (std::max)(a.left, r.left);
            const long iy = (std::min)(a.bottom, r.bottom) - (std::max)(a.top, r.top);
            if (ix > 0 && iy > 0) sc += ix * iy;
        }
        return sc;
    };
    // below (as asked) → above → left → right → progressively larger nudges
    const int vStep = h + d(6), hStep = w / 2 + d(10), tiny = d(14);
    const int offs[][2] = {{0, 0},         {0, -vStep},      {0, -vStep - tiny},
                           {-hStep, 0},    {hStep, 0},       {0, tiny},
                           {-hStep, -vStep}, {hStep, -vStep}, {0, -2 * vStep},
                           {-2 * hStep, 0}, {2 * hStep, 0},  {0, 2 * tiny}};
    RECT best = clampIn(want);
    int bestScore = overlap(best);
    for (const auto& o : offs) {
        RECT r = want;
        r.left += o[0];
        r.right += o[0];
        r.top += o[1];
        r.bottom += o[1];
        r = clampIn(r);
        const int sc = overlap(r);
        if (sc < bestScore) {
            best = r;
            bestScore = sc;
        }
        if (sc == 0) break;
    }
    if (overlapScore) *overlapScore = bestScore;
    return best;
}

std::vector<std::wstring> g_labelDump;   // last painted labels (verification dump)
// anchor (BEFORE any clamp) of every label request, in the same order as
// g_labelDump: the PNG exporter reads it back to prove where a label WANTED to
// sit, not just where the safety clamp pushed it (temporary diagnostic)
std::vector<std::wstring> g_labelReqDump;
// one line per MIC label with the number of vertical LINE STEPS its block had to
// be pushed down (0 = free, -1 = the sideways fallback ran): the 「话筒界面换行
// 不够多」 fix is verified from this (temporary diagnostic)
std::vector<std::wstring> g_micStackDump;

void App::PaintStage(Gdiplus::Graphics& g) {
    g_labelDump.clear();
    g_labelReqDump.clear();
    g_micStackDump.clear();
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    if (ui_.layer == 3) {
        PaintChannelTable(g);
        return;
    }

    if (ui_.layer == 5) {
        PaintProjectPage(g);
        return;
    }

    // everything the stage draws stays inside the canvas
    Gdiplus::GraphicsState outerClip = g.Save();
    g.SetClip(Gdiplus::RectF((float)L_.canvas.left, (float)L_.canvas.top,
                             (float)(L_.canvas.right - L_.canvas.left),
                             (float)(L_.canvas.bottom - L_.canvas.top)),
              Gdiplus::CombineModeIntersect);

    dr::Fill(g, L_.canvas, dr::C(theme::kOffstage));

    const double sw = pr_.stage.w * ppm_;
    const double sh = pr_.stage.d * ppm_;
    const double ow = pr_.stage.w;
    const double oh = pr_.stage.d;

    Gdiplus::RectF stage((float)ox_, (float)oy_, (float)sw, (float)sh);

    // --- unbounded 1 m grid over the whole canvas ---------------------------
    {
        Gdiplus::GraphicsState gs = g.Save();
        g.SetClip(Gdiplus::RectF((float)L_.canvas.left, (float)L_.canvas.top,
                                 (float)(L_.canvas.right - L_.canvas.left),
                                 (float)(L_.canvas.bottom - L_.canvas.top)),
                  Gdiplus::CombineModeIntersect);
        int x0 = (int)std::floor((L_.canvas.left - ox_) / ppm_);
        int x1 = (int)std::ceil((L_.canvas.right - ox_) / ppm_);
        for (int m = x0; m <= x1; ++m) {
            bool major = (m % 5) == 0;
            float x = (float)(ox_ + m * ppm_);
            dr::Line(g, x, (float)L_.canvas.top, x, (float)L_.canvas.bottom,
                     dr::C(major ? theme::kGridStrong : theme::kGrid), 1.f);
        }
        int y0 = (int)std::floor((L_.canvas.top - oy_) / ppm_);
        int y1 = (int)std::ceil((L_.canvas.bottom - oy_) / ppm_);
        for (int m = y0; m <= y1; ++m) {
            bool major = (m % 5) == 0;
            float y = (float)(oy_ + m * ppm_);
            dr::Line(g, (float)L_.canvas.left, y, (float)L_.canvas.right, y,
                     dr::C(major ? theme::kGridStrong : theme::kGrid), 1.f);
        }
        g.Restore(gs);
    }
    // --- stage ---------------------------------------------------------------
    dr::FillRound(g, stage, 0.f, dr::C(pr_.stageFill));
    {
        // The stage interior grid lives in WORLD metres (0 .. stage.w / stage.d),
        // so its lines are ox_ + m * ppm_ long: they zoom and pan with the stage
        // and the 0 / stage.w / stage.d lines sit exactly on the border at every
        // zoom.  It must be clipped to the VISIBLE part of the stage
        // (stage ∩ canvas): a plain SetClip(stage) REPLACES the canvas clip, so a
        // stage larger than the canvas (zoomed in) would paint grid lines over the
        // library panel and the top bar (user report 「网格出现在左边和上面的区域」).
        const float vx0 = (std::max)(stage.X, (float)L_.canvas.left);
        const float vy0 = (std::max)(stage.Y, (float)L_.canvas.top);
        const float vx1 = (std::min)(stage.X + stage.Width, (float)L_.canvas.right);
        const float vy1 = (std::min)(stage.Y + stage.Height, (float)L_.canvas.bottom);
        if (vx1 > vx0 && vy1 > vy0) {
            Gdiplus::GraphicsState gs2 = g.Save();
            Gdiplus::Region clip(Gdiplus::Rect(
                (INT)std::floor(vx0), (INT)std::floor(vy0),
                (INT)std::ceil(vx1) - (INT)std::floor(vx0),
                (INT)std::ceil(vy1) - (INT)std::floor(vy0)));
            g.SetClip(&clip, Gdiplus::CombineModeIntersect);   // keep the canvas clip
            // only the whole metres that can intersect the visible part
            int x0 = (int)std::floor((vx0 - ox_) / ppm_);
            int x1 = (int)std::ceil((vx1 - ox_) / ppm_);
            x0 = (std::max)(x0, 0);
            x1 = (std::min)(x1, (int)std::ceil(pr_.stage.w));
            for (int m = x0; m <= x1; ++m) {
                float x = (float)(ox_ + m * ppm_);
                dr::Line(g, x, vy0, x, vy1, dr::C(theme::kGrid), 1.f);
            }
            int y0 = (int)std::floor((vy0 - oy_) / ppm_);
            int y1 = (int)std::ceil((vy1 - oy_) / ppm_);
            y0 = (std::max)(y0, 0);
            y1 = (std::min)(y1, (int)std::ceil(pr_.stage.d));
            for (int m = y0; m <= y1; ++m) {
                float y = (float)(oy_ + m * ppm_);
                dr::Line(g, vx0, y, vx1, y, dr::C(theme::kGrid), 1.f);
            }
            g.Restore(gs2);
        }
    }
    {
        Gdiplus::Pen pen(dr::C(theme::kStageBorder), 1.4f);
        g.DrawRectangle(&pen, stage.X, stage.Y, stage.Width, stage.Height);
    }

    // corner ticks
    {
        float t = (float)d(7);
        Gdiplus::Color c = dr::C(theme::kStageBorder);
        float L = stage.X, T = stage.Y, R = stage.X + stage.Width, B = stage.Y + stage.Height;
        dr::Line(g, L - t, T, L - t * 0.3f, T, c, 1.4f);
        dr::Line(g, L, T - t, L, T - t * 0.3f, c, 1.4f);
        dr::Line(g, R, T - t, R, T - t * 0.3f, c, 1.4f);
        dr::Line(g, R + t * 0.3f, T, R + t, T, c, 1.4f);
        dr::Line(g, L, B + t * 0.3f, L, B + t, c, 1.4f);
        dr::Line(g, L - t, B, L - t * 0.3f, B, c, 1.4f);
        dr::Line(g, R + t * 0.3f, B, R + t, B, c, 1.4f);
        dr::Line(g, R, B + t * 0.3f, R, B + t, c, 1.4f);
    }

    // --- dimensions ------------------------------------------------------------
    {
        Gdiplus::Color dim = dr::C(theme::kTextFaint);
        float top = (float)(oy_ - d(15));
        dr::Line(g, (float)ox_, top, (float)(ox_ + sw), top, dim, 1.f);
        dr::Line(g, (float)ox_, top - d(4), (float)ox_, top + d(4), dim, 1.f);
        dr::Line(g, (float)(ox_ + sw), top - d(4), (float)(ox_ + sw), top + d(4), dim, 1.f);
        RECT tr = {(LONG)(ox_ + sw * 0.5 - d(50)), (LONG)(top - d(17)),
                   (LONG)(ox_ + sw * 0.5 + d(50)), (LONG)(top - 1)};
        dr::Text(g, dr::Fmt(L"%.1f m", pr_.stage.w), Font(8.5f), dr::C(theme::kTextMuted),
                 tr, 1, true);

        float left = (float)(ox_ - d(15));
        dr::Line(g, left, (float)oy_, left, (float)(oy_ + sh), dim, 1.f);
        dr::Line(g, left - d(4), (float)oy_, left + d(4), (float)oy_, dim, 1.f);
        dr::Line(g, left - d(4), (float)(oy_ + sh), left + d(4), (float)(oy_ + sh), dim, 1.f);
        RECT lr = {(LONG)(left - d(88)), (LONG)(oy_ + sh * 0.5 - d(8)),
                   (LONG)(left - d(5)), (LONG)(oy_ + sh * 0.5 + d(8))};
        dr::Text(g, dr::Fmt(L"%.1f m", pr_.stage.d), Font(8.5f), dr::C(theme::kTextMuted),
                 lr, 2, true);
    }

    // --- layer 03 (话筒拉线): devices, microphones and cables ------------------
    // Instruments are deliberately invisible on this layer so the plot reads as
    // a wiring plan (docs/CABLING.md).
    if (ui_.layer == 2) {
        PaintCablingLayer(g);
        g.Restore(outerClip);
        return;
    }

    // --- placed objects ------------------------------------------------------------
    std::map<std::wstring, int> instCount;
    bool anyTriple = false;
    for (const auto& it : pr_.items) {
        if (it.kind != ItemKind::Instrument) continue;
        if (++instCount[it.defId] >= 3) anyTriple = true;
    }
    // a CLEAN same-type hull (>= 3 instances, no other defId inside) replaces the
    // per-instance labels of its type with the ONE 「类型 ×N」 caption
    std::map<std::wstring, bool> cleanHull;
    std::vector<TypeHull> groupLabels;          // clean groups → ONE 「类型 ×N」
    if (anyTriple && ui_.layer <= 1) {
        for (const auto& hh : BuildTypeHulls()) {
            cleanHull[hh.defId] = hh.clean;
            if (hh.clean) groupLabels.push_back(hh);
        }
    }
    // every visible symbol is an obstacle for the label pass, every label a
    // request: both are resolved AFTER the icons are painted
    struct LabelReq {
        std::wstring a, b;      // line 1 / line 2 (may be empty)
        int cx = 0, ty = 0;
        Gdiplus::Color col = Gdiplus::Color(255, 0, 0, 0);
        float fontPx = 7.5f;
        bool mic = false;
        int id = 0;
        // PAIRED microphones (a 2-member MicGroup: ORTF / NOS / DIN or a plain
        // stereo pair): 1 = left member, 2 = right member, 0 = not paired
        int pairSide = 0;
        int markerCx = 0;       // marker centre, for the outward split slot
        int pairKey = -1;       // group index (+1), 0 = not paired
    };
    std::vector<RECT> labelObstacles;
    std::vector<LabelReq> labelReqs;

    for (const auto& it : pr_.items) {
        bool mic = it.kind == ItemKind::Mic;
        if (mic) {
            if (!pr_.showMics) continue;
        } else {
            if (ui_.layer > 1 || !pr_.showInstruments) continue;
        }
        bool interactive = mic ? (ui_.layer == 1) : (ui_.layer == 0);

        bool simpleItem = ui_.simple && !mic && it.defId != L"grand_piano";
        Item vis = it;
        if (simpleItem) vis.angle = 0.0;
        RECT box{}, lbl{};
        int pad = d(28);
        if (!ItemRects(vis, ppm_, ox_, oy_, pad, box, lbl)) continue;

        // the procedural main-mic symbol has its own footprint (the catalogue box
        // only feeds the zoom fit), so its highlight follows the DRAWN capsules
        RECT all = box;
        if (mic && it.defId == L"mainmic") {
            // the halo hugs the drawn symbol — capsule spacing in REAL metres plus
            // the 1.6× scaled radii — and follows its rotation
            const int gcx = (box.left + box.right) / 2;
            const int gcy = (box.top + box.bottom) / 2;
            const MainMicExtent me = MainMicSymbolExtent(it, ppm_);
            const int gw = (int)std::lround(2.0 * MainMicAxisHalf(me, it.angle, true));
            const int gh = (int)std::lround(2.0 * MainMicAxisHalf(me, it.angle, false));
            all = {gcx - gw / 2, gcy - gh / 2, gcx + gw / 2, gcy + gh / 2};
        }
        all.left -= d(4);
        all.right += d(4);
        all.top -= d(4);
        all.bottom += d(4);

        bool sel = pr_.IsSel(it.id) || it.id == pr_.selected;
        if (sel) {
            if (mic) {
                // microphones are marked with a compact accent halo around the
                // symbol instead of a box (the box looked huge over a tiny dot)
                const int hcx = (all.left + all.right) / 2;
                const int hcy = (all.top + all.bottom) / 2;
                int hr = (all.right - all.left) / 2 + d(2);
                if (hr < d(11)) hr = d(11);
                dr::FillRound(g, Gdiplus::RectF((float)(hcx - hr), (float)(hcy - hr),
                                                (float)(hr * 2), (float)(hr * 2)),
                              (float)(hr * 0.55f), dr::C(theme::kAccent, 34));
                Gdiplus::Pen ring(dr::C(theme::kAccent, 190), 1.4f);
                g.DrawEllipse(&ring, (float)(hcx - hr), (float)(hcy - hr),
                              (float)(hr * 2), (float)(hr * 2));
            } else {
                dr::FillRound(g, dr::RF(all), (float)d(6), dr::C(theme::kAccent, 32));
            }
        }

        const CatItem* cat = FindItem(it.defId);
        double ww = it.w > 0.01 ? it.w : (cat ? cat->w : 0.5);
        double hh = it.h > 0.01 ? it.h : (cat ? cat->h : 0.5);
        // stage symbols: point mics (incl. the intensity-difference pairs) much
        // smaller than the catalogue icon; the main mic's symbol is procedural and
        // scales itself at kMainMicSymScale (its icon rect is not used)
        if (mic) {
            const double ms = MicSymScale(it);
            ww *= ms;
            hh *= ms;
        }
        int iw = (int)std::lround(ww * ppm_);
        int ih = (int)std::lround(hh * ppm_);
        iw = (std::max)(iw, 6);
        ih = (std::max)(ih, 6);
        int ccx = (box.left + box.right) / 2;
        int ccy = (box.top + box.bottom) / 2;
        Gdiplus::RectF irect((float)(ccx - iw * 0.5), (float)(ccy - ih * 0.5),
                             (float)iw, (float)ih);
        // ONE colour family per 声部: a microphone wears the ink of what it is
        // BOUND to (its instrument / section icon tint, at DRAWN weight so the tiny
        // symbol stays legible), an UNBOUND one keeps the neutral microphone colour.
        // Instruments keep their own rule below.
        Rgb ink = mic ? MicInkLine(MicBoundInk(pr_, it)) : pr_.instColor;
        if (!mic) ink = ItemIconTint(pr_, it, anyTriple);

        bool simplified = simpleItem;
        if (simplified) {
            int sp = (std::max)(6, (int)std::lround(
                (std::min)(0.55, (std::min)(ww, hh)) * ppm_));
            Gdiplus::RectF sq((float)(ccx - sp / 2), (float)(ccy - sp / 2),
                              (float)sp, (float)sp);
            dr::FillRound(g, sq, (float)d(2), dr::C(ink, 16));
            dr::StrokeRound(g, sq, (float)d(2), dr::C(ink, 210), 1.2f);
        } else if (mic && it.defId == L"mainmic") {
            const double kPi = 3.14159265358979323846;
            double rad = it.angle * kPi / 180.0;
            double fwdX = std::sin(rad), fwdY = -std::cos(rad);
            double sideX = std::cos(rad), sideY = std::sin(rad);
            // The capsule CENTRE-TO-CENTRE distance is the real spacing in metres
            // (it.spacing × ppm_): the 1.6× symbol scale grows the line art — the
            // circle radii and the pen widths — and NEVER this distance, so an
            // 0.40 m AB pair is 0.40 m apart against the 1 m stage grid and a 3.00 m
            // pair is 3.00 m apart.
            double half = (std::max)(0.02, it.spacing) * 0.5 * ppm_;
            double capR = MainMicCapRadius(ppm_);
            Gdiplus::Pen bar(dr::C(ink, 210), (float)(1.6 * kMainMicSymScale));
            Gdiplus::Pen cap(dr::C(ink, 235), (float)(1.6 * kMainMicSymScale));
            g.DrawLine(&bar, (float)(ccx - half * sideX), (float)(ccy - half * sideY),
                       (float)(ccx + half * sideX), (float)(ccy + half * sideY));
            for (int s = -1; s <= 1; s += 2) {
                double mx = ccx + s * half * sideX;
                double my = ccy + s * half * sideY;
                g.DrawEllipse(&cap, (float)(mx - capR), (float)(my - capR),
                              (float)(2 * capR), (float)(2 * capR));
                // the user asked for the two diagonal orientation ticks to go:
                // the main-mic symbol is just the link bar + capsules (+ M mark)
            }
            // M mark
            double ms = capR * 1.05;
            g.DrawLine(&bar, (float)(ccx - ms), (float)(ccy + ms * 0.9),
                       (float)(ccx - ms), (float)(ccy - ms * 0.4));
            g.DrawLine(&bar, (float)(ccx - ms), (float)(ccy - ms * 0.4),
                       (float)ccx, (float)(ccy + ms * 0.4));
            g.DrawLine(&bar, (float)ccx, (float)(ccy + ms * 0.4),
                       (float)(ccx + ms), (float)(ccy - ms * 0.4));
            g.DrawLine(&bar, (float)(ccx + ms), (float)(ccy - ms * 0.4),
                       (float)(ccx + ms), (float)(ccy + ms * 0.9));
            // Decca Tree centre microphone (protrudes forward by `center` metres —
            // a real distance, so it is never multiplied by the symbol scale)
            if (it.mainTech == 3) {
                double mx = ccx + fwdX * it.center * ppm_;
                double my = ccy + fwdY * it.center * ppm_;
                Gdiplus::Pen cpen(dr::C(ink, 235), (float)(1.6 * kMainMicSymScale));
                // line from the centre mic back to the pair midpoint
                g.DrawLine(&cpen, (float)mx, (float)my, (float)ccx, (float)ccy);
                g.DrawEllipse(&cpen, (float)(mx - capR), (float)(my - capR),
                              (float)(2 * capR), (float)(2 * capR));
                g.DrawLine(&cpen, (float)mx, (float)(my - capR),
                           (float)(mx + fwdX * capR * 1.6),
                           (float)(my + fwdY * capR * 1.6));
            }
        } else {
            // intensity-difference technique pairs reuse the palette symbol,
            // drawn ONCE for the whole pair (first member only)
            std::wstring techIcon;
            int techGroup = -1;
            if (mic && IsIntensityTech(it.defId)) {
                // ONE stage object for XY / MS / Blumlein: its own pair glyph
                techIcon = it.defId;
            } else if (mic && it.defId == L"mic") {
                techGroup = pr_.GroupOf(it.id);
                if (techGroup >= 0 && pr_.micGroups[techGroup].kind == 2) {
                    const std::wstring& ty = pr_.micGroups[techGroup].tech;
                    if (ty == L"xy" || ty == L"ms" || ty == L"blumlein")
                        techIcon = ty;
                }
            }
            // MIXED-DIFFERENCE technique pairs (ORTF / NOS / DIN) are TWO mic items
            // joined by the CROSSBAR of their technique glyph: that connecting line
            // is drawn ONCE for the group, capsule to capsule, under the two icons
            // (it is the bar the ORTF / NOS / DIN glyph shows).
            bool drawPairIcon = !techIcon.empty() &&
                                (techGroup < 0 ||
                                 (!pr_.micGroups[techGroup].ids.empty() &&
                                  pr_.micGroups[techGroup].ids.front() == it.id));
            if (!techIcon.empty() && !drawPairIcon) {
                // second member draws nothing (the pair shares one symbol)
            } else if (!techIcon.empty()) {
                Gdiplus::Bitmap* bmp = Assets::Get().TintIcon(techIcon, true, ink);
                double pairScale = (techIcon == L"blumlein") ? 0.32 : 0.64;
                // a STANDALONE technique object (techGroup < 0) has no pair of items
                // to spread over: the glyph IS the symbol, so it is drawn at the
                // visual weight the two-item pair used to have
                if (techGroup < 0) pairScale *= 1.8;
                double cw = (box.right - box.left) * pairScale;
                double ch = (box.bottom - box.top) * pairScale;
                Gdiplus::RectF tr2((float)(ccx - cw * 0.5), (float)(ccy - ch * 0.5),
                                   (float)cw, (float)ch);
                dr::Icon(g, bmp, tr2, (float)it.angle);
            } else {
                // every other object draws its own symbol — an ORTF / NOS / DIN
                // pair member is just an ordinary point microphone
                Gdiplus::Bitmap* bmp = Assets::Get().TintIcon(it.defId, mic, ink);
                dr::Icon(g, bmp, irect, (float)it.angle);
            }
        }

        // the microphone's direction line is drawn in a darker tone of the SAME
        // ink, so the symbol stays one colour family
        // (skipped for technique pairs - their symbol is drawn as a whole)
        int lineGroup = mic ? pr_.GroupOf(it.id) : -1;
        bool techPairMic = lineGroup >= 0 && pr_.micGroups[lineGroup].kind == 2;
        if (mic && it.defId == L"mic" && !techPairMic) {
            Rgb dk;
            dk.r = (BYTE)(std::clamp((int)(ink.r * 0.42), 0, 255));
            dk.g = (BYTE)(std::clamp((int)(ink.g * 0.42), 0, 255));
            dk.b = (BYTE)(std::clamp((int)(ink.b * 0.42), 0, 255));
            Gdiplus::Bitmap* ln = Assets::Get().TintIcon(L"micline", true, dk);
            dr::Icon(g, ln, irect, (float)it.angle);
        }

        // generic "custom" square: the user's text sits inside the box
        if (!mic && it.defId == L"custom" && !it.label.empty() &&
            pr_.showInstrumentText) {
            RECT tr = box;
            int inset = d(5);
            tr.left += inset;
            tr.right -= inset;
            tr.top += inset;
            tr.bottom -= inset;
            dr::TextWrap(g, it.label, Font(7.5f), dr::C(pr_.textColor), tr, 1);
        }

        // 吊装 (flown) badge: recognisable on 01 / 02 as well as on 05
        if (mic && it.flown) {
            RECT br = {(LONG)ccx + d(4), (LONG)ccy - d(20), (LONG)ccx + d(20),
                       (LONG)ccy - d(6)};
            dr::FillRound(g, dr::RF(br), (float)d(3), dr::C(0x6A5F7A, 235));
            dr::Text(g, L10n::T(S::CbFlyBadge), Font(7.f, true), dr::C(0xFFFFFF), br, 1,
                     true);
        }

        // ---- the label is only REQUESTED here; the placement pass below decides
        // where it goes so it never covers a symbol or another label ----------
        labelObstacles.push_back(all);
        if (mic) {
            if (pr_.showMicText) {
                // a microphone prints its model text; the main microphone prints
                // its per-capsule texts (L / C / R), the label when all are empty
                LabelReq r;
                r.a = (it.defId == L"mainmic") ? MainMicCaption(it)
                                               : (it.model.empty() ? it.label
                                                                   : it.model);
                if (r.a.empty()) r.a = SpotTypeName(it.defId);
                if (!it.spotType.empty())
                    r.b = L"(" + SpotTypeName(it.spotType) + L")";
                // GLUED to the symbol: 3-4 px under its drawn bottom edge, centred
                // on the microphone itself (the item anchor IS the symbol centre and
                // MicMarkerRadiusF is the radius the 02 circle / capsules use)
                r.cx = (int)ccx;
                r.ty = (int)(ccy + MicMarkerRadiusF(it) + d(3));
                // the label wears the microphone's own drawing colour
                r.col = dr::C(ink);
                r.mic = true;
                r.id = it.id;
                // a 2-member group (ORTF / NOS / DIN / stereo pair) may split
                // its two labels outwards when the glued slots collide
                {
                    const int gi = pr_.GroupOf(it.id);
                    if (gi >= 0 && pr_.micGroups[(size_t)gi].ids.size() == 2) {
                        const auto& gids = pr_.micGroups[(size_t)gi].ids;
                        const int otherId = gids[0] == it.id ? gids[1] : gids[0];
                        const Item* other = nullptr;
                        for (const auto& oi : pr_.items)
                            if (oi.id == otherId) {
                                other = &oi;
                                break;
                            }
                        if (other) {
                            r.markerCx = (int)ccx;
                            r.pairKey = gi + 1;
                            if (it.x < other->x)
                                r.pairSide = 1;   // this one is the LEFT member
                            else if (it.x > other->x)
                                r.pairSide = 2;   // RIGHT member
                            else
                                r.pairSide = (it.id < otherId) ? 1 : 2;
                        }
                        g_micStackDump.push_back(dr::Fmt(
                            L"pairinfo <%s> id %d group %d n %d pair %d",
                            r.a.c_str(), it.id, gi,
                            (int)pr_.micGroups[(size_t)gi].ids.size(), r.pairSide));
                    } else {
                        g_micStackDump.push_back(dr::Fmt(
                            L"pairinfo <%s> id %d group %d n %d pair %d",
                            r.a.c_str(), it.id, gi,
                            gi >= 0 ? (int)pr_.micGroups[(size_t)gi].ids.size() : 0,
                            r.pairSide));
                    }
                }
                labelReqs.push_back(r);
            }
        } else if (pr_.showInstrumentText) {
            // the per-instance label of a type that owns a CLEAN group is
            // suppressed: that type carries the single 「类型 ×N」 caption
            auto ch = cleanHull.find(it.defId);
            const bool suppressed = (ch != cleanHull.end() && ch->second);
            if (!suppressed) {
                LabelReq r;
                r.a = it.label.empty() ? SpotTypeName(it.defId) : it.label;
                r.cx = (int)ccx;
                r.ty = (int)(box.bottom + d(2));
                // the label wears the instrument icon's own ink (per-type tint)
                r.col = dr::C(ink);
                labelReqs.push_back(r);
            }
        }

        if (sel) {
            dr::StrokeRound(g, dr::RF(all), (float)d(6),
                            dr::C(theme::kAccent, it.id == pr_.selected ? 220 : 150),
                            it.id == pr_.selected ? 1.4f : 1.0f);
        }
    }

    // --- canvas labels --------------------------------------------------------
    // COLOUR: a label is painted in the colour of the object it belongs to — the
    // instrument's icon ink (per-type tint) for instrument labels and the
    // 「类型 ×N」 caption, the mic ink for microphone labels.
    // PLACEMENT: fixed anchors (caption at the type area's bottom edge, instrument
    // label just under its footprint, mic label glued under its marker), plus the
    // two safety clamps — inside the canvas, and flipped above the symbol when it
    // would start past the bottom edge.
    // OVERLAP RELIEF: horizontal only — clashing INSTRUMENT labels (and captions)
    // are pushed apart sideways in small steps until they clear; mic labels may
    // side-shift by one label width.  No vertical moves, no icon avoidance.
    {
        struct Lab {
            RECT r{};
            std::wstring a, b;
            Gdiplus::Color col = Gdiplus::Color(255, 0, 0, 0);
            float fontPx = 7.5f;
            int id = -1;
            bool mic = false;
            int anchorLeft = 0;
            int pairSide = 0;   // 1 = left member, 2 = right member
            int markerCx = 0;
            int pairKey = -1;
            bool pairClash = false;   // the pair's glued labels collide
        };
        auto clampRect = [&](RECT q) {
            if (q.right > L_.canvas.right - d(2)) {
                q.left -= q.right - (L_.canvas.right - d(2));
                q.right = L_.canvas.right - d(2);
            }
            if (q.left < L_.canvas.left + d(2)) {
                q.right += L_.canvas.left + d(2) - q.left;
                q.left = L_.canvas.left + d(2);
            }
            if (q.bottom > L_.canvas.bottom - d(2)) {
                q.top -= q.bottom - (L_.canvas.bottom - d(2));
                q.bottom = L_.canvas.bottom - d(2);
            }
            if (q.top < L_.canvas.top + d(2)) {
                q.bottom += L_.canvas.top + d(2) - q.top;
                q.top = L_.canvas.top + d(2);
            }
            return q;
        };
        std::vector<Lab> labs;
        // label metrics in TEXT lines, not in d(): a PNG export draws the fonts
        // scaleMul× larger (dr::g_textLayoutBoost, App::Font), so the two lines of a
        // bound mic's block have to grow with them or the second line lands on the
        // first.  At 1× (the screen) lboost is 1 and these are exactly d(15)/d(13).
        const float lboost =
            (dr::g_textLayoutBoost > 1.001f) ? dr::g_textLayoutBoost : 1.f;
        auto bl = [&](float v) { return d(v * lboost); };
        const int lineH = bl(15.f);      // one label line
        // line to line inside a two-line block.  In an export the measured glyph
        // box is a hair taller than one layout line, so the pitch has to be at
        // least a full line there (1× keeps the historical d(13) exactly).
        const int lineStep =
            (lboost > 1.001f) ? (std::max)(bl(13.f), lineH) : d(13.f);
        auto build = [&](const LabelReq& q) {
            Gdiplus::Font* f = Font(q.fontPx);
            const int tw1 = (int)std::ceil(dr::TextW(g, q.a, f));
            const int tw2 = q.b.empty() ? 0 : (int)std::ceil(dr::TextW(g, q.b, f));
            const int w = (std::max)(tw1, tw2) + d(10);
            const int lines = q.b.empty() ? 1 : 2;
            const int hh = lineH + (lines == 1 ? 0 : lineStep);
            int top = q.ty;
            if (top + lineStep > L_.canvas.bottom - d(2))
                top = q.ty - hh - d(6);
            Lab l;
            l.r = {(LONG)(q.cx - w / 2), (LONG)top, (LONG)(q.cx - w / 2 + w),
                   (LONG)(top + hh)};
            l.r = clampRect(l.r);
            l.a = q.a;
            l.b = q.b;
            l.col = q.col;
            l.fontPx = q.fontPx;
            l.id = q.mic ? q.id : -1;
            l.mic = q.mic;
            l.anchorLeft = (LONG)(q.cx - w / 2);
            l.pairSide = q.pairSide;
            l.markerCx = q.markerCx;
            l.pairKey = q.pairKey;
            labs.push_back(l);
            g_labelReqDump.push_back(dr::Fmt(
                L"lreq <%s> cx %d ty %d w %d hh %d -> %ld %ld %ld %ld canvas %ld %ld %ld %ld",
                q.a.c_str(), q.cx, q.ty, w, hh, l.r.left, l.r.top, l.r.right, l.r.bottom,
                L_.canvas.left, L_.canvas.top, L_.canvas.right, L_.canvas.bottom));
        };
        // the group captions first, then the per-object labels
        if (pr_.showInstrumentText) {
            for (const auto& h : groupLabels) {
                LabelReq r;
                r.a = dr::Fmt(L10n::T(S::HullCountFmt), h.name.c_str(), h.count);
                r.cx = (int)std::lround(ox_ + h.labelX * ppm_);
                r.ty = (int)std::lround(oy_ + h.labelY * ppm_) + d(2);
                // the caption wears the type's own ink (a per-item 颜色 override of
                // any instance of that type tints the family caption too)
                r.col = dr::C(InstColorOverrideFor(pr_, h.defId)
                                  ? RgbFromHex(InstColorOverrideFor(pr_, h.defId))
                                  : MutedColor(h.defId));
                r.fontPx = 8.f;
                build(r);
            }
        }
        for (const auto& r : labelReqs) build(r);

        // ---- overlap relief: label pairs of the same kind -----------------------
        auto hit = [](const RECT& p, const RECT& q) {
            return p.left < q.right && p.right > q.left && p.top < q.bottom &&
                   p.bottom > q.top;
        };
        // MIC labels STACK VERTICALLY.  Each mic label is placed in list order
        // against the labels already placed: the glued slot under its marker first,
        // then the SAME slot pushed DOWN by whole LINE STEPS — one step is a text
        // line plus the small gap the bound mic's second line 「(对应乐器)」 uses — so
        // a dense cluster of microphones reads as a column instead of piling up.
        // Only when no free slot turns up within four steps are the OLD sideways
        // shift sizes tried (then sideways together with the same down steps), and
        // when even those stay blocked the candidate with the least overlap is
        // taken.  A label already placed is never moved, so the stack cannot push a
        // neighbour into a third label.  The two-line block always moves as ONE (the
        // second line is drawn off the same rect, so it can never be separated from
        // its name), and the whole block stays inside the canvas.
        // A PAIR whose two glued labels collide splits outwards (both members),
        // even when one of them would still fit under its own marker.
        for (size_t i = 0; i < labs.size(); ++i) {
            if (!labs[i].mic || labs[i].pairKey < 0 || labs[i].pairClash) continue;
            for (size_t k = i + 1; k < labs.size(); ++k) {
                if (!labs[k].mic || labs[k].pairKey != labs[i].pairKey) continue;
                if (hit(labs[i].r, labs[k].r)) {
                    labs[i].pairClash = true;
                    labs[k].pairClash = true;
                }
                break;
            }
        }
        const int micStep = lineH;   // one label line + the small gap
        std::vector<int> micDown(labs.size(), 0);   // line steps the block was pushed
        std::vector<int> micSide(labs.size(), 0);   // 0 none, 1 right, 2 left
        std::vector<int> micDx(labs.size(), 0);     // px the block was shifted right
        std::vector<int> micPairSlot(labs.size(), 0);   // 1 = pair split slot
        for (size_t j = 0; j < labs.size(); ++j) {
            if (!labs[j].mic) continue;
            auto hitsPlaced = [&](const RECT& q) {
                for (size_t i = 0; i < j; ++i)
                    if (labs[i].mic && hit(labs[i].r, q)) return true;
                return false;
            };
            if (!hitsPlaced(labs[j].r) && !labs[j].pairClash)
                continue;   // the glued slot is free
            const RECT base = labs[j].r;
            // 0 = still glued, 1 = the pair's outward split slot, 2 = generic
            int pairSlot = 0;
            const int ownW = (int)(base.right - base.left);
            // The budget is ONE label width sideways and THREE lines down: a mic
            // label must stay close to its microphone (a slight overlap beats a
            // label far from its marker).  Candidate order:
            //   1) SIDEWAYS first — the user wants the horizontal room used while
            //      it is there: half a label width, then a whole one, right before
            //      left, at the glued height;
            //   2) then DOWN one line, and the same sideways offsets one line down;
            //   3) then two lines down, then three, each with the sideways variants.
            // A label already placed is never moved, so the search cannot shove a
            // neighbour into a third label; the two-line block always moves as ONE
            // (its second line is drawn off the same rect) and stays in the canvas.
            std::vector<RECT> cands;
            std::vector<int> cDown, cSide;
            auto addCand = [&](RECT q, int down, int side) {
                q = clampRect(q);
                // a block pushed past the bottom edge slides back up whole, never
                // flipping to the other side of its marker
                if (q.bottom > L_.canvas.bottom - d(2)) {
                    q.top -= q.bottom - (L_.canvas.bottom - d(2));
                    q.bottom = L_.canvas.bottom - d(2);
                }
                if (q.top < L_.canvas.top + d(2)) return;
                if (q.bottom > L_.canvas.bottom - d(2)) return;
                cands.push_back(q);
                cDown.push_back(down);
                cSide.push_back(side);
            };
            // PAIRED microphones: a pair is placed OUTWARDS before anything else
            // — the left member's block hangs to the BOTTOM-LEFT of the left mic,
            // the right member's to the BOTTOM-RIGHT of its own mic (user
            // request), so the two labels never stack on each other
            if (labs[j].pairSide) {
                RECT sd = base;
                if (labs[j].pairSide == 1) {   // left member: flush left of the mic
                    sd.right = labs[j].markerCx - d(3);
                    sd.left = sd.right - ownW;
                } else {                       // right member: flush right of it
                    sd.left = labs[j].markerCx + d(3);
                    sd.right = sd.left + ownW;
                }
                addCand(sd, 0, labs[j].pairSide == 1 ? 2 : 1);
            }
            for (int lvl = 0; lvl <= 3; ++lvl) {
                const int dy = lvl * micStep;
                if (lvl == 0) {
                    addCand(base, 0, 0);   // the glued slot under the marker
                } else {
                    RECT dn = base;        // straight down whole line steps
                    dn.top += dy;
                    dn.bottom += dy;
                    addCand(dn, lvl, 0);
                }
                // sideways at this height: half a label width, then a whole one
                for (int lvlW = 1; lvlW <= 2; ++lvlW) {
                    const int off = lvlW == 1 ? ownW / 2 : ownW;
                    if (off <= 0) continue;
                    for (int dir = 0; dir < 2; ++dir) {
                        RECT sd = base;
                        const int dx = dir ? -off : off;
                        sd.left += dx;
                        sd.right += dx;
                        sd.top += dy;
                        sd.bottom += dy;
                        addCand(sd, lvl, dir ? 2 : 1);
                    }
                }
            }
            size_t pick = cands.size();
            for (size_t c = 0; c < cands.size(); ++c) {
                if (!hitsPlaced(cands[c])) {
                    pick = c;
                    break;
                }
            }
            if (pick == cands.size()) {   // no free slot: accept the least overlap
                long long bestOv = -1;
                for (size_t c = 0; c < cands.size(); ++c) {
                    long long ov = 0;
                    for (size_t i = 0; i < j; ++i) {
                        if (!labs[i].mic) continue;
                        const int ow = (std::min)(labs[i].r.right, cands[c].right) -
                                       (std::max)(labs[i].r.left, cands[c].left);
                        const int oh = (std::min)(labs[i].r.bottom, cands[c].bottom) -
                                       (std::max)(labs[i].r.top, cands[c].top);
                        if (ow > 0 && oh > 0) ov += (long long)ow * oh;
                    }
                    if (bestOv < 0 || ov < bestOv) {
                        bestOv = ov;
                        pick = c;
                    }
                }
            }
            if (pick < cands.size()) {
                labs[j].r = cands[pick];
                micDown[j] = cDown[pick];
                micSide[j] = cSide[pick];
                pairSlot = (labs[j].pairSide && pick == 0)
                               ? 1
                               : ((cSide[pick] != 0 || cDown[pick] != 0) ? 2 : 1);
                micPairSlot[j] = pairSlot;
                micDx[j] = (int)(labs[j].r.left - base.left);
            }
        }
        // INSTRUMENT labels + group captions keep the plain horizontal relief
        // (identical to before: same pair order, same 2-px steps, same 2.5 m cap)
        for (size_t i = 0; i < labs.size(); ++i) {
            for (size_t j = i + 1; j < labs.size(); ++j) {
                if (labs[i].mic || labs[j].mic) continue;   // mics stack above
                if (!hit(labs[i].r, labs[j].r)) continue;
                // the horizontal de-overlap cap is POSITIONING: 2.5 m of drawing,
                // so it scales with the export (d(140) at 100 %)
                const int cap = (int)std::lround(2.5 * ppm_);
                const int step = d(2);
                int moved = 0;
                while (moved < cap && hit(labs[i].r, labs[j].r)) {
                    labs[i].r.left -= step;
                    labs[i].r.right -= step;
                    labs[j].r.left += step;
                    labs[j].r.right += step;
                    labs[i].r = clampRect(labs[i].r);
                    labs[j].r = clampRect(labs[j].r);
                    moved += step;
                }
            }
        }
        for (size_t j = 0; j < labs.size(); ++j) {
            if (!labs[j].mic) continue;
            g_micStackDump.push_back(dr::Fmt(
                L"micstack <%s> %ld %ld %ld %ld b <%s> %ld %ld %ld %ld step %d side %d dx %d pair %d",
                labs[j].a.c_str(), labs[j].r.left, labs[j].r.top, labs[j].r.right,
                labs[j].r.bottom, labs[j].b.c_str(), labs[j].r.left,
                labs[j].r.top + lineStep, labs[j].r.right, labs[j].r.bottom + lineStep,
                micDown[j], micSide[j], micDx[j], micPairSlot[j]));
        }
        // ---- paint -------------------------------------------------------------
        for (const auto& l : labs) {
            Gdiplus::Font* f = Font(l.fontPx);
            dr::Text(g, l.a, f, l.col, l.r, 1, true);
            g_labelDump.push_back(
                dr::Fmt(L"label <%s> %ld %ld %ld %ld id %d dx %d", l.a.c_str(),
                        l.r.left, l.r.top, l.r.right, l.r.bottom, l.id,
                        (int)(l.r.left - l.anchorLeft)));
            if (!l.b.empty()) {
                RECT r2 = l.r;
                r2.top += lineStep;
                r2.bottom += lineStep;
                dr::Text(g, l.b, f, dr::C(theme::kAccent), r2, 1, true);
            }
        }
    }

    // --- rotation handle: the single selected object, or the whole selection ----
    {
        double hx = 0, hy = 0;
        bool show = false;
        if (pr_.selIds.size() >= 2) {
            // multi-selection: the handle sits straight above the bbox centre and
            // turns every selected object about that centre
            show = RotateHandlePosMulti(hx, hy);
        } else if (pr_.selIds.size() == 1 && pr_.Selected()) {
            const Item& one = *pr_.Selected();
            bool mic = one.kind == ItemKind::Mic;
            bool interactive = mic ? (ui_.layer == 1) : (ui_.layer == 0);
            bool simpleSel = ui_.simple && !mic && one.defId != L"grand_piano";
            show = interactive && !simpleSel && RotateHandlePos(one, hx, hy);
        }
        if (show) {
            // circular-arrow rotation handle
            const double r = 7.5 * dpi_ * uiScale_;
            Gdiplus::SolidBrush wf(dr::C(0xFDFCFA));
            g.FillEllipse(&wf, (float)(hx - r), (float)(hy - r),
                          (float)(2 * r), (float)(2 * r));
            float cx = (float)hx, cy = (float)hy;
            float ar = (float)(r * 0.70);
            Gdiplus::RectF er(cx - ar, cy - ar, ar * 2.f, ar * 2.f);
            Gdiplus::Pen arc(dr::C(theme::kAccent, 235), 1.7f);
            g.DrawArc(&arc, er, -55.f, 275.f);
            {
                const float kPi2 = 3.14159265358979f;
                float ae = (-55.f + 275.f) * kPi2 / 180.f;
                float ex = cx + ar * std::cos(ae);
                float ey = cy + ar * std::sin(ae);
                float tx = -std::sin(ae), ty = std::cos(ae);
                float nx = std::cos(ae), ny = std::sin(ae);
                Gdiplus::PointF tri[3] = {
                    Gdiplus::PointF(ex + tx * (float)r * 0.42f, ey + ty * (float)r * 0.42f),
                    Gdiplus::PointF(ex - nx * (float)r * 0.30f - tx * (float)r * 0.10f,
                                    ey - ny * (float)r * 0.30f - ty * (float)r * 0.10f),
                    Gdiplus::PointF(ex + nx * (float)r * 0.30f - tx * (float)r * 0.10f,
                                    ey + ny * (float)r * 0.30f - ty * (float)r * 0.10f)};
                Gdiplus::SolidBrush ab(dr::C(theme::kAccent, 235));
                g.FillPolygon(&ab, tri, 3);
            }
        }
    }

    // --- bound microphone groups (2 / 3 mic arrays) ----------------------------
    if (ui_.layer == 1 && pr_.showMics) {
        for (const auto& grp : pr_.micGroups) {
            if (grp.ids.size() < 2) continue;
            std::vector<Gdiplus::PointF> pts;
            for (int id : grp.ids) {
                const Item* it = pr_.Find(id);
                if (!it) continue;
                pts.push_back(Gdiplus::PointF((float)(ox_ + it->x * ppm_),
                                              (float)(oy_ + it->y * ppm_)));
            }
            if (pts.size() < 2) continue;
            // the dashes joining a bound group wear the group's own ink (first
            // resolvable member), so the array reads as one colour family too
            Rgb gink = pr_.micColor;
            for (int id : grp.ids) {
                const Item* it = pr_.Find(id);
                if (!it) continue;
                gink = MicInkLine(MicBoundInk(pr_, *it));
                break;
            }
            Gdiplus::Pen pen(dr::C(gink, 150), 1.2f);
            pen.SetDashStyle(Gdiplus::DashStyleDash);
            for (size_t i = 1; i < pts.size(); ++i)
                g.DrawLine(&pen, pts[i - 1], pts[i]);
            Gdiplus::SolidBrush br(dr::C(gink, 200));
            for (const auto& pt : pts)
                g.FillEllipse(&br, pt.X - 2.5f, pt.Y - 2.5f, 5.f, 5.f);
        }
    }

    // --- main microphone: SRA cone ------------------------------------------
    if (ui_.showSra) {
        double sra = MainMicSRA();
        std::vector<const Item*> mm;
        for (int id : pr_.mainMics)
            if (const Item* m = pr_.Find(id)) mm.push_back(m);
        // a 180° (or wider) recording angle covers the whole front: the cone lines
        // would just be two horizontal rays, so they are not drawn at all
        if (sra > 0.0 && sra < 179.5 && !mm.empty()) {
            double sx = 0, sy = 0, asum = 0;
            for (const Item* m : mm) {
                sx += m->x;
                sy += m->y;
                asum += m->angle;
            }
            double cx = ox_ + (sx / mm.size()) * ppm_;
            double cy = oy_ + (sy / mm.size()) * ppm_;
            double ang = asum / mm.size();
            double half = sra * 0.5 * 3.14159265358979323846 / 180.0;
            double len = 7.0 * ppm_;
            Gdiplus::Pen pen(dr::C(pr_.micColor, 190), 1.5f);
            pen.SetDashStyle(Gdiplus::DashStyleDash);
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                double a = ang * 3.14159265358979323846 / 180.0 + sgn * half;
                float ex = (float)(cx + std::sin(a) * len);
                float ey = (float)(cy - std::cos(a) * len);
                g.DrawLine(&pen, (float)cx, (float)cy, ex, ey);
            }
            Gdiplus::SolidBrush dot(dr::C(pr_.micColor, 220));
            g.FillEllipse(&dot, (float)cx - 3.f, (float)cy - 3.f, 6.f, 6.f);
            // highlighted SRA chip in the top-left corner of the canvas: it is a
            // MICROPHONE readout, so 显示话筒文字 governs it (chip + text together)
            if (pr_.showMicText) {
                Gdiplus::RectF chip((float)(L_.canvas.left + d(14)),
                                    (float)(L_.canvas.top + d(32)),
                                    (float)d(150), (float)d(24));
                dr::FillRound(g, chip, (float)d(6), dr::C(theme::kAccent, 235));
                RECT lr = {(LONG)(chip.X + d(10)), (LONG)chip.Y,
                           (LONG)(chip.X + chip.Width), (LONG)(chip.Y + chip.Height)};
                dr::Text(g, dr::Fmt(L"%s SRA %.0f°", L10n::T(S::MainMic), sra),
                         Font(9.f), dr::C(0xFFFFFF), lr, 0, true);
            }
        }
    }

    g.Restore(outerClip);
}

// Screen-space bounding box of the current item selection (union of the drawn
// rects), used both for the multi-selection handle and for its pivot.
bool App::SelectionBBox(RECT& box) const {
    bool any = false;
    for (int id : pr_.selIds) {
        const Item* it = pr_.Find(id);
        if (!it) continue;
        RECT b{}, lbl{};
        if (!ItemRects(*it, ppm_, ox_, oy_, 0, b, lbl)) continue;
        if (!any) {
            box = b;
            any = true;
        } else {
            UnionRect(box, b);
        }
    }
    return any;
}

// The rotation handle of a MULTI selection: straight above the bounding-box
// centre, at the same standoff the single-object handle uses.  Only a homogeneous
// selection on its own layer gets a handle (mics never mix with instruments), and
// 简化模式 still excludes instruments that are not a grand piano.
bool App::RotateHandlePosMulti(double& hx, double& hy) const {
    if (pr_.selIds.size() < 2) return false;
    RECT box{};
    if (!SelectionBBox(box)) return false;
    bool anyMic = false, anyInst = false;
    for (int id : pr_.selIds) {
        const Item* it = pr_.Find(id);
        if (!it) continue;
        if (it->kind == ItemKind::Mic) anyMic = true;
        else anyInst = true;
        if (ui_.simple && it->kind == ItemKind::Instrument && it->defId != L"grand_piano")
            return false;
    }
    if (anyMic == anyInst) return false;   // mixed / nothing selectable
    if (anyMic) {
        if (ui_.layer != 1) return false;
    } else if (ui_.layer != 0) {
        return false;
    }
    const double cx = (box.left + box.right) * 0.5;
    const double cy = (box.top + box.bottom) * 0.5;
    const double halfH = std::max(1.0, (box.bottom - box.top) * 0.5);
    const double dist = std::max(halfH + 16.0 * dpi_ * uiScale_, 30.0 * dpi_ * uiScale_);
    hx = cx;
    hy = cy - dist;
    return true;
}

bool App::RotateHandlePos(const Item& it, double& hx, double& hy) const {
    RECT box{}, lbl{};
    if (!ItemRects(it, ppm_, ox_, oy_, 0, box, lbl)) return false;
    double cx = (box.left + box.right) * 0.5;
    double cy = (box.top + box.bottom) * 0.5;
    double halfH = std::max(1.0, (box.bottom - box.top) * 0.5);
    double dist = std::max(halfH + 16.0 * dpi_ * uiScale_, 30.0 * dpi_ * uiScale_);
    double a = it.angle * 3.14159265358979323846 / 180.0;
    hx = cx + std::sin(a) * dist;
    hy = cy - std::cos(a) * dist;

    return true;
}

bool App::RotateHandleHit(int x, int y) const {
    if (rotating_) return true;
    double hx = 0, hy = 0;
    if (pr_.selIds.size() >= 2) {
        // the multi-selection handle is only live for a homogeneous selection on
        // its own layer (RotateHandlePosMulti already enforces that)
        if (!RotateHandlePosMulti(hx, hy)) return false;
    } else if (pr_.selIds.size() == 1 && pr_.Selected()) {
        const Item& one = *pr_.Selected();
        bool mic = one.kind == ItemKind::Mic;
        if (mic ? (ui_.layer != 1) : (ui_.layer != 0)) return false;
        if (ui_.simple && !mic && one.defId != L"grand_piano") return false;
        if (!RotateHandlePos(one, hx, hy)) return false;
    } else {
        return false;
    }
    double dx = x - hx, dy = y - hy;
    double r = 9.0 * dpi_ * uiScale_;

    return dx * dx + dy * dy <= r * r;
}

void App::RotateGroupBy(int itemId, double daDeg) {
    Item* it = pr_.Find(itemId);
    if (!it) return;
    // A microphone that belongs to a group rotates the WHOLE group (stereo pair,
    // Decca Tree, technique pair — every kind) about the group centroid by the
    // same angle delta, so the geometry of the array is preserved: every member
    // (including the grabbed one, and any bound spot mic inside the group) orbits
    // the centroid and turns by the same delta.
    const int rotGroup = pr_.GroupOf(itemId);
    std::vector<int> ids;
    if (rotGroup >= 0 && pr_.micGroups[rotGroup].ids.size() >= 2)
        ids = pr_.micGroups[rotGroup].ids;
    if (ids.empty()) ids.push_back(itemId);

    double gx = 0, gy = 0;
    int n = 0;
    for (int id : ids) {
        if (const Item* m = pr_.Find(id)) {
            gx += m->x;
            gy += m->y;
            ++n;
        }
    }
    if (n == 0) return;
    gx /= n;
    gy /= n;
    double da = std::fmod(daDeg, 360.0);
    if (da > 180.0) da -= 360.0;      // never jump the long way round
    if (da < -180.0) da += 360.0;
    const double rad = da * 3.14159265358979323846 / 180.0;
    const double ca = std::cos(rad), sa = std::sin(rad);
    for (int id : ids) {
        Item* m = pr_.Find(id);
        if (!m) continue;
        const double rx = m->x - gx, ry = m->y - gy;
        m->x = gx + rx * ca - ry * sa;
        m->y = gy + rx * sa + ry * ca;
        m->angle = std::fmod(m->angle + da, 360.0);
        if (m->angle < 0.0) m->angle += 360.0;
    }
    RebuildAllCablePaths(pr_, MicRadiusM());   // links follow the rotated members
    SyncXYEdits();                             // the group's members all moved
    SyncAngleEdit();
    InvalidateAll();
}

// Rotate EVERY selected item about a fixed pivot by the same angle delta: each
// item's position orbits the pivot and its own angle advances by exactly daDeg.
// This is the multi-selection counterpart of RotateGroupBy (same maths) with the
// selection's bounding-box centre as the pivot instead of a group centroid.
void App::RotateSelectionBy(double px, double py, double daDeg) {
    // a mixed microphone / instrument selection is never rotated: the two kinds
    // live on different pages, so turning the set as one would move locked objects
    if (SelectionMixedKinds()) return;
    double da = std::fmod(daDeg, 360.0);
    if (da > 180.0) da -= 360.0;
    if (da < -180.0) da += 360.0;
    const double rad = da * 3.14159265358979323846 / 180.0;
    const double ca = std::cos(rad), sa = std::sin(rad);
    bool any = false;
    for (int id : pr_.selIds) {
        Item* m = pr_.Find(id);
        if (!m) continue;
        const double rx = m->x - px, ry = m->y - py;
        m->x = px + rx * ca - ry * sa;
        m->y = py + rx * sa + ry * ca;
        m->angle = std::fmod(m->angle + da, 360.0);
        if (m->angle < 0.0) m->angle += 360.0;
        any = true;
    }
    if (!any) return;
    RebuildAllCablePaths(pr_, MicRadiusM());
    SyncXYEdits();
    SyncAngleEdit();
    InvalidateAll();
}

void App::RotateTo(int x, int y) {
    // a mixed microphone / instrument selection never rotates: nothing turns, no
    // snapshot is recorded and the press already reported the status line once
    if (SelectionMixedKinds()) return;
    // ---- multi selection: every selected item turns about the frozen bbox centre
    if (rotating_ && pr_.selIds.size() >= 2) {
        const double sx = ox_ + rotPivotX_ * ppm_;
        const double sy = oy_ + rotPivotY_ * ppm_;
        const double dx = (double)x - sx;
        const double dy = (double)y - sy;
        if (dx * dx + dy * dy < 4.0) return;
        double a = std::atan2(dx, -dy) * 180.0 / 3.14159265358979323846;
        if (GetKeyState(VK_SHIFT) & 0x8000) a = std::round(a / 15.0) * 15.0;
        // the delta is what the gesture has not applied yet, so the pivot never
        // drifts even after a hundred mouse moves
        double step = a - rotBase_ - rotApplied_;
        step = std::fmod(step, 360.0);
        if (step > 180.0) step -= 360.0;
        if (step < -180.0) step += 360.0;
        if (std::fabs(step) < 1e-9) return;
        GestureUndo();   // one snapshot for the whole rotation gesture
        RotateSelectionBy(rotPivotX_, rotPivotY_, step);
        rotApplied_ += step;
        return;
    }
    Item* it = pr_.Find(rotId_);
    if (!it) return;

    // the reference point is the centroid of the rotating set (the group centroid
    // for a grouped mic, the mic itself otherwise), so a drag cannot feed back
    const int rotGroup = pr_.GroupOf(it->id);
    std::vector<int> ids;
    if (rotGroup >= 0 && pr_.micGroups[rotGroup].ids.size() >= 2)
        ids = pr_.micGroups[rotGroup].ids;
    if (ids.empty()) ids.push_back(it->id);
    double gx = 0, gy = 0;
    int n = 0;
    for (int id : ids) {
        if (const Item* m = pr_.Find(id)) {
            gx += m->x;
            gy += m->y;
            ++n;
        }
    }
    if (n == 0) return;
    gx /= n;
    gy /= n;
    const double sx = ox_ + gx * ppm_;   // the same point in screen space
    const double sy = oy_ + gy * ppm_;

    const double dx = (double)x - sx;
    const double dy = (double)y - sy;
    if (dx * dx + dy * dy < 4.0) return;
    double a = std::atan2(dx, -dy) * 180.0 / 3.14159265358979323846;
    if (GetKeyState(VK_SHIFT) & 0x8000) a = std::round(a / 15.0) * 15.0;
    a = std::fmod(a, 360.0);
    if (a < 0.0) a += 360.0;

    GestureUndo();   // one snapshot for the whole rotation gesture
    RotateGroupBy(it->id, a - it->angle);
    it->angle = a;   // exact, so a sub-degree delta cannot drift
    SyncAngleEdit();
    InvalidateAll();
}

int App::StageHitTest(int x, int y, bool& lockedHit) {
    lockedHit = false;
    // the cabling layer (03) hosts its own objects, never items
    if (ui_.layer > 1) return -1;
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    POINT p = {x, y};
    int pad = (int)std::lround(28.0 * dpi_ * uiScale_);
    int lockedId = -1;
    // microphones are small and usually sit ON an instrument: they are hit-tested
    // in their OWN pass first, so a click near a mic always prefers the mic and a
    // neighbouring mic stays reachable
    const bool micFirst = (ui_.layer == 1);   // page 03 prefers mics, page 02 instruments
    for (int pass = 0; pass < 2; ++pass) {
    for (auto it = pr_.items.rbegin(); it != pr_.items.rend(); ++it) {
        bool mic = it->kind == ItemKind::Mic;
        if (micFirst ? (pass == 1) == mic : (pass == 0) == mic) continue;
        bool visible = mic ? pr_.showMics
                           : (ui_.layer <= 1 && pr_.showInstruments);
        if (!visible) continue;
        Item vis = *it;
        if (ui_.simple && !mic && it->defId != L"grand_piano") vis.angle = 0.0;
        RECT box{}, lbl{};
        if (!ItemRects(vis, ppm_, ox_, oy_, pad, box, lbl)) continue;
        if (mic && it->defId == L"mainmic") {
            // the main mic's click box IS the drawn symbol: the two capsule
            // circles, the link bar and (Decca) the centre capsule.  A rectangle
            // around a 3 m AB pair would swallow every neighbouring mic.
            const int hcx = (box.left + box.right) / 2;
            const int hcy = (box.top + box.bottom) / 2;
            if (!MainMicSymbolHit(*it, (double)hcx, (double)hcy, ppm_, x, y, d(5)))
                continue;
        } else {
            if (mic) {
                // click box = the DRAWN symbol (a point mic is drawn at 0.51×, the
                // lavalier at 0.8925×) — using the catalogue box stole clicks
                const CatItem* hc = FindItem(it->defId);
                double hw = it->w > 0.01 ? it->w : (hc ? hc->w : 0.35);
                double hh2 = it->h > 0.01 ? it->h : (hc ? hc->h : 0.35);
                const double hs = MicSymScale(*it);
                int hr = (int)std::lround((std::max)(hw, hh2) * hs * ppm_ * 0.5) + d(5);
                if (hr < d(9)) hr = d(9);
                const int hcx = (box.left + box.right) / 2;
                const int hcy = (box.top + box.bottom) / 2;
                box = {hcx - hr, hcy - hr, hcx + hr, hcy + hr};
            }
            if (!PtInRect(&box, p)) continue;
        }
        bool draggable = mic ? (ui_.layer == 1) : (ui_.layer == 0);
        if (!draggable) lockedHit = true;
        return it->id;
    }
    }
    lockedHit = false;
    return -1;
}

// ---------------------------------------------------------------------------
// rubber-band multi-select (see App.h)
//
// A left-drag that starts on EMPTY canvas space draws a selection rectangle and,
// on release, selects every object of the CURRENT layer whose rotated footprint
// intersects it.  The band lives in SCREEN space, so an object is tested through
// the very rectangle the painter draws for it (ItemRects / DeviceRects geometry,
// the same one StageHitTest uses).
// ---------------------------------------------------------------------------
namespace {

// Exact separating-axis test between the band rectangle and one rotated rectangle
// (centre, half extents, angle in degrees — the dr::Icon convention, a POSITIVE
// angle turns clockwise on screen).  Touching counts as a hit, so a band dragged
// flat across a row still catches the icons it runs through.
bool BandHitsRotRect(const RECT& band, double cx, double cy, double hw, double hh,
                     double angleDeg) {
    const double rad = angleDeg * 3.14159265358979323846 / 180.0;
    const double ca = std::cos(rad), sa = std::sin(rad);
    // the object's four corners: centre +- the two local half axes
    const double ux = ca * hw, uy = sa * hw;    // local +x axis, rotated
    const double vx = -sa * hh, vy = ca * hh;   // local +y axis, rotated
    const double qx[4] = {cx - ux - vx, cx + ux - vx, cx + ux + vx, cx - ux + vx};
    const double qy[4] = {cy - uy - vy, cy + uy - vy, cy + uy + vy, cy - uy + vy};
    const double bx[4] = {(double)band.left, (double)band.right, (double)band.right,
                          (double)band.left};
    const double by[4] = {(double)band.top, (double)band.top, (double)band.bottom,
                          (double)band.bottom};
    const double axes[4][2] = {{1.0, 0.0}, {0.0, 1.0}, {ca, sa}, {-sa, ca}};
    for (const auto& a : axes) {
        double q0 = 1e18, q1 = -1e18, r0 = 1e18, r1 = -1e18;
        for (int i = 0; i < 4; ++i) {
            const double p = qx[i] * a[0] + qy[i] * a[1];
            q0 = (std::min)(q0, p);
            q1 = (std::max)(q1, p);
            const double m = bx[i] * a[0] + by[i] * a[1];
            r0 = (std::min)(r0, m);
            r1 = (std::max)(r1, m);
        }
        if ((std::min)(q1, r1) - (std::max)(q0, r0) < -0.001) return false;
    }
    return true;
}

} // namespace

bool App::BandHitsItem(const Item& it, const RECT& band) const {
    const CatItem* c = FindItem(it.defId);
    if (!c) return false;
    const double ww = it.w > 0.01 ? it.w : c->w;
    const double hh = it.h > 0.01 ? it.h : c->h;
    const double cx = ox_ + it.x * ppm_;
    const double cy = oy_ + it.y * ppm_;
    double angle = it.angle;
    // 简洁模式 draws instruments unrotated (except the grand piano): the band must
    // test the footprint the user actually sees
    if (ui_.simple && it.kind == ItemKind::Instrument && it.defId != L"grand_piano")
        angle = 0.0;
    if (it.kind == ItemKind::Mic) {
        // a microphone is drawn as a small symbol (point mics 0.51×, the lavalier
        // 1.75× of that): its footprint is the drawn square, exactly like
        // StageHitTest — the main mic follows its procedural symbol instead
        if (it.defId == L"mainmic") {
            const MainMicExtent me = MainMicSymbolExtent(it, ppm_);
            const double ext = (std::max)(me.side, (std::max)(me.fwd, me.back));
            return BandHitsRotRect(band, cx, cy, ext, ext, angle);
        }
        const double side = (std::max)(ww, hh) * MicSymScale(it) * ppm_;
        return BandHitsRotRect(band, cx, cy, side * 0.5, side * 0.5, angle);
    }
    return BandHitsRotRect(band, cx, cy, ww * ppm_ * 0.5, hh * ppm_ * 0.5, angle);
}

RECT App::BandRect() const {
    RECT r;
    r.left = (std::min)(bandStart_.x, bandPt_.x);
    r.top = (std::min)(bandStart_.y, bandPt_.y);
    r.right = (std::max)(bandStart_.x, bandPt_.x);
    r.bottom = (std::max)(bandStart_.y, bandPt_.y);
    // a purely horizontal / vertical drag still has to catch what it runs across
    if (r.right <= r.left) r.right = r.left + 1;
    if (r.bottom <= r.top) r.bottom = r.top + 1;
    return r;
}

void App::BandBegin(int x, int y, bool add) {
    band_ = true;
    bandMoved_ = false;
    bandAdd_ = add;
    bandStart_ = POINT{x, y};
    bandPt_ = bandStart_;
    downX_ = x;
    downY_ = y;
    // NOTHING moves and the old selection survives until the button comes up
    SetCapture(hwnd_);
    InvalidateAll();
}

void App::BandTo(int x, int y) {
    bandPt_ = POINT{x, y};
    if (!bandMoved_) {
        // under the 3 px threshold the gesture is still a plain click
        if (std::abs(x - bandStart_.x) <= 3 && std::abs(y - bandStart_.y) <= 3) return;
        bandMoved_ = true;
    }
    InvalidateAll();
}

void App::BandCancel() {
    if (!band_) return;
    band_ = false;
    bandMoved_ = false;
    bandAdd_ = false;
    bandStart_ = POINT{};
    bandPt_ = POINT{};
    if (GetCapture() == hwnd_) ReleaseCapture();
    InvalidateAll();   // the selection is left exactly as it was
}

void App::BandEnd(int x, int y) {
    if (!band_) return;
    bandPt_ = POINT{x, y};
    const bool moved = bandMoved_;
    const bool add = bandAdd_;
    const RECT band = BandRect();
    band_ = false;
    bandMoved_ = false;
    bandAdd_ = false;
    bandStart_ = POINT{};
    bandPt_ = POINT{};
    if (GetCapture() == hwnd_) ReleaseCapture();

    // a gesture under the 3 px threshold is a plain click: on empty canvas space
    // that only drops the selection (the behaviour before the band existed)
    if (!moved) {
        if (!add) {
            if (ui_.layer == 2) pr_.ClearDeviceSelection();
            pr_.Deselect();
            RefreshAfterProjectChange();
        } else {
            InvalidateAll();
        }
        return;
    }

    // ---- layer 03: the cabling devices are their own selection model ---------
    if (ui_.layer == 2) {
        if (!add) pr_.ClearDeviceSelection();
        int n = 0;
        for (Device& dv : pr_.devices) {
            if (!DeviceHitsBand(dv, band)) continue;
            dv.selected = true;
            ++n;
        }
        ui_.hoverCbRow = -1;
        ui_.hoverCbDevice = -1;
        if (n > 0) StatusMsg(dr::Fmt(L10n::T(S::BandSelFmt), n));
        RefreshAfterProjectChange();
        return;
    }

    // ---- layers 01 / 02: instruments on 0, microphones on 1 ------------------
    if (ui_.layer != 0 && ui_.layer != 1) return;
    if (!add) {
        pr_.selIds.clear();
        pr_.selected = -1;
    }
    pr_.ClearDeviceSelection();   // items and devices never mix in one selection
    int n = 0;
    for (const Item& it : pr_.items) {
        const bool mic = it.kind == ItemKind::Mic;
        if (mic != (ui_.layer == 1)) continue;   // the CURRENT layer only
        if (mic ? !pr_.showMics : !pr_.showInstruments) continue;   // not drawn
        if (!BandHitsItem(it, band)) continue;
        if (!pr_.IsSel(it.id)) pr_.selIds.push_back(it.id);
        ++n;
    }
    pr_.selected = pr_.selIds.empty() ? -1 : pr_.selIds.back();
    if (n > 0) StatusMsg(dr::Fmt(L10n::T(S::BandSelFmt), n));
    RefreshAfterProjectChange();
}

void App::PaintRubberBand(Gdiplus::Graphics& g) {
    if (!band_ || !bandMoved_) return;
    const RECT r = BandRect();
    // clipped to the canvas: dragging out over the side panels must not paint there
    Gdiplus::GraphicsState gs = g.Save();
    Gdiplus::Region clip(Gdiplus::Rect(L_.canvas.left, L_.canvas.top,
                                       L_.canvas.right - L_.canvas.left,
                                       L_.canvas.bottom - L_.canvas.top));
    g.SetClip(&clip);
    dr::Fill(g, r, dr::C(theme::kAccent, 36));   // translucent accent fill
    const float penW = (float)(std::max)(1, D(1));
    Gdiplus::Pen pen(dr::C(theme::kAccent, 220), penW);
    pen.SetDashStyle(Gdiplus::DashStyleDash);
    const float w = (float)(std::max)(1, (int)(r.right - r.left));
    const float h = (float)(std::max)(1, (int)(r.bottom - r.top));
    g.DrawRectangle(&pen, Gdiplus::RectF((float)r.left, (float)r.top, w, h));
    g.Restore(gs);
}

ChColSpan App::ChannelColumns(double x0, double x1, double chW, double gap,
                                   double pad) const {
    ChColSpan s;
    double x = x0;
    if (ui_.chColVis[0]) {
        s.l[0] = x;
        s.r[0] = x + chW;
        x = s.r[0] + gap;
    }
    int nFlex = 0;
    for (int i = 1; i < kChCols; ++i)
        if (ui_.chColVis[i]) ++nFlex;
    if (nFlex > 0) {
        // 型号 / 对应乐器 / 备注 / 链路 / 长度 — the link column needs room for
        // "点话筒 1 → 8ch 声卡" (device labels, no protocol prefix), the length
        // column for "12.4 m"
        // 长度 now carries the longer caption 「话筒线长度」 / 「Mikrofonkabel」, so it
        // takes a little width from 备注 (short remarks); the caption still shrinks
        // one step in the header pass if a translation does not fit
        const double wts[kChCols] = {0.0, 1.6, 1.8, 1.6, 2.8, 2.4, 1.8, 1.7};
        double tot = 0.0;
        for (int i = 1; i < kChCols; ++i)
            if (ui_.chColVis[i]) tot += wts[i];
        double avail = (x1 - x) - gap * (nFlex - 1);
        if (avail < 40.0) avail = 40.0;
        for (int i = 1; i < kChCols; ++i) {
            if (!ui_.chColVis[i]) continue;
            const double w = avail * wts[i] / tot;
            s.l[i] = x;
            s.r[i] = x + w;
            x = s.r[i] + gap;
        }
    }
    for (int i = 0; i < kChCols; ++i) {
        if (!ui_.chColVis[i]) {
            s.l[i] = 0;
            s.r[i] = 0;
            continue;
        }
        s.l[i] += pad;
        s.r[i] -= pad;
        if (s.r[i] < s.l[i] + 8.0) s.r[i] = s.l[i] + 8.0;
    }
    return s;
}

ChTableGeom App::ChannelTableGeom() const {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    ChTableGeom gt;
    gt.x0 = L_.canvas.left + d(36);
    gt.x1 = L_.canvas.right - d(36);
    gt.titleY = L_.canvas.top + d(24);
    gt.titleH = d(28);
    gt.chipY = gt.titleY + d(34);
    gt.chipH = d(26);
    gt.colLabel = {gt.x0, gt.chipY, gt.x0 + d(64), gt.chipY + gt.chipH};
    // 「自动排序」 sits at the right end of the toolbar, level with the 通道表 title
    // (the chip band keeps its full width, so the six 显示列 chips still fit one row).
    // The pager 「◀ 第 i / N 页 ▶」 sits immediately to its left.
    {
        const int bw = d(104), bh = d(24);
        const int by = gt.titleY + (gt.titleH - bh) / 2;
        gt.autoSort = {gt.x1 - bw, by, gt.x1, by + bh};
        const int btn = d(24), lblW = d(112), gap = d(6);
        gt.pageNext = {gt.autoSort.left - gap - btn, by, gt.autoSort.left - gap, by + bh};
        gt.pageLabel = {gt.pageNext.left - lblW, by, gt.pageNext.left, by + bh};
        gt.pagePrev = {gt.pageLabel.left - btn, by, gt.pageLabel.left, by + bh};
    }
    // 显示列 chips: ALWAYS ONE ROW.  Every chip is as wide as its own label; the
    // band owns the full toolbar width (the pager and 自动排序 sit on the title row
    // above).  If it still does not fit, the padding / gap shrink first and then
    // the chip font drops 1 pt — the band never wraps and never scrolls.
    {
        const int chipsX = gt.x0 + d(68);
        const int availW = (std::max)(d(120), (int)(gt.x1 - chipsX));
        const S chipNames[kChCols] = {S::ChNum,     S::ChModel, S::ChInst, S::ChRemark,
                                      S::CbLink,    S::CbLen,   S::ChToMainShort,
                                      S::ChAzimuth};
        Gdiplus::Bitmap probe(1, 1, PixelFormat32bppARGB);
        Gdiplus::Graphics pg(&probe);
        auto textW = [&](const std::wstring& t, float fpx) -> int {
            Gdiplus::Font f(family_, fpx * dpi_ * uiScale_,
                            Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::RectF rr{};
            pg.MeasureString(t.c_str(), (INT)t.size(), &f, Gdiplus::PointF(0, 0), &rr);
            return (int)std::ceil(rr.Width);
        };
        int padX = d(8), gapX = d(8);
        float chipFont = 8.5f;
        auto measureAll = [&]() {
            int sum = 0;
            for (int i = 0; i < kChCols; ++i)
                sum += d(12) + d(6) + textW(L10n::T(chipNames[i]), chipFont) + 2 * padX;
            return sum + (kChCols - 1) * gapX;
        };
        int tw = measureAll();
        if (tw > availW) {
            padX = d(5);
            gapX = d(5);
            tw = measureAll();
        }
        if (tw > availW) {
            chipFont = 7.5f;   // chips only — the table itself keeps its type
            tw = measureAll();
        }
        if (tw > availW) {
            padX = d(2);
            gapX = d(3);
            tw = measureAll();
        }
        int x = chipsX;
        for (int i = 0; i < kChCols; ++i) {
            const int cw = d(12) + d(6) + textW(L10n::T(chipNames[i]), chipFont) +
                           2 * padX + d(6);   // slack: the label is never trimmed
            gt.chip[i] = {x, gt.chipY, x + cw, gt.chipY + gt.chipH};
            x += cw + gapX;
        }
        gt.chipFont = chipFont;
        gt.headerY = gt.chipY + gt.chipH + d(12);
    }
    gt.headerH = d(28);
    gt.rowsY = gt.headerY + gt.headerH + d(4);
    gt.rowH = d(28);
    gt.cols = ChannelColumns((double)gt.x0, (double)gt.x1, (double)d(64),
                             (double)d(12), (double)d(6));
    const int bottom = L_.canvas.bottom - d(10);
    const int fit = gt.rowH > 0 ? (bottom - gt.rowsY) / gt.rowH : 0;
    gt.visibleRows = fit > 0 ? fit : 0;
    // one SCREEN page = exactly the whole rows that fit: the table pages (never
    // scrolls) and a page never shows a half row
    gt.pageRows = gt.visibleRows;
    return gt;
}

int App::ChannelPickerMaxRows(int yTop) const {
    const int rowH = D(22);
    const int avail = (L_.canvas.bottom - D(12)) - yTop;
    int n = rowH > 0 ? avail / rowH : 0;
    if (n < 1) n = 1;
    if (n > 16) n = 16;
    return n;
}

// The 布线 tail block: every INTERFACE BOX → SOUND CARD cable (a device-to-device
// link) becomes ONE summary row — the 通道 cell carries its channel count
// (「8 通道」), the 链路 cell the device route (「接口箱 3 → 声卡 1」) and the 长度 cell
// the REAL cable length, exactly the number the 05 tag shows.
std::vector<App::ChRow> App::BuildSummaryRows() const {
    std::vector<ChRow> rows;
    auto devName = [](const Device& dv) {
        return dv.label.empty() ? DeviceKindTitle(dv.kind) : dv.label;
    };
    auto rank = [](DevKind k) {   // 接口箱 first, then 声卡, then 接收器
        switch (k) {
        case DevKind::SoundCard: return 1;
        case DevKind::Receiver: return 2;
        default: return 0;
        }
    };
    for (const auto& ln : pr_.links) {
        if (ln.aKind != 0 || ln.bKind != 0) continue;   // device → device only
        const Device* da = nullptr;
        const Device* db = nullptr;
        for (const auto& dv : pr_.devices) {
            if (dv.id == ln.a) da = &dv;
            if (dv.id == ln.b) db = &dv;
        }
        if (!da || !db) continue;
        const Device* from = da;
        const Device* to = db;
        if (rank(from->kind) > rank(to->kind)) std::swap(from, to);
        ChRow r;
        r.summary = true;
        r.chText = dr::Fmt(L"%d 通道", ln.channels);
        r.link = devName(*from) + L" → " + devName(*to);
        const CableIssue is = CheckCable(pr_, ln);
        r.len = CableRealLen(pr_, ln);
        r.custom = CableOverrideLen(pr_, ln) > 0.0;
        r.bad = is.overChannels;
        rows.push_back(r);
    }
    return rows;
}

// the table's paint order: the numbered channel rows, then the 布线 tail block
std::vector<App::ChRow> App::BuildChannelDisplayRows() const {
    std::vector<ChRow> out = BuildChannelRows();
    std::vector<ChRow> tail = BuildSummaryRows();
    if (tail.empty()) return out;
    ChRow sep;
    sep.separator = true;
    out.push_back(sep);
    for (auto& r : tail) out.push_back(r);
    return out;
}

// 通道表 「话筒线长度」 column caption + the active mode:
// 「话筒线长度(推荐)」 (standard cable size) / 「话筒线长度(实际)」 (drawn run).
// The header cell is also the switch, see App::MouseDown — screen and PDF both read
// this one string, so the printed table always shows the mode it was printed in.
std::wstring App::ChannelLenHeader() const {
    return dr::Fmt(L"%s(%s)", L10n::T(S::CbLen),
                   L10n::T(pr_.lenMode == 1 ? S::LenModeReal : S::LenModeReserved));
}

void App::PaintChannelTable(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    dr::Fill(g, L_.canvas, dr::C(theme::kBg));

    const std::vector<ChRow> rows = BuildChannelDisplayRows();
    const ChTableGeom gt = ChannelTableGeom();
    const S colNames[kChCols] = {S::ChNum,   S::ChModel, S::ChInst, S::ChRemark,
                                 S::CbLink, S::CbLen,   S::ChToMain, S::ChAzimuth};
    // the chip labels are the short form of the header names
    const S chipNames[kChCols] = {S::ChNum,     S::ChModel, S::ChInst, S::ChRemark,
                                  S::CbLink,    S::CbLen,   S::ChToMainShort,
                                  S::ChAzimuth};

    // --- pages (no scrolling): whole pages of rows, ◀ ▶ flip between them -----
    const int pageRows = (std::max)(1, gt.pageRows);
    const int pages = (std::max)(1, ((int)rows.size() + pageRows - 1) / pageRows);
    const int page = std::clamp(ui_.chPage, 0, pages - 1);
    if (ui_.chPage != page) ui_.chPage = page;   // clamp after the row count changed
    const int rowBase = page * pageRows;

    dr::Text(g, L10n::T(S::ChTitle), Font(12.f, true), dr::C(theme::kText),
             {gt.x0, gt.titleY, gt.x1, gt.titleY + gt.titleH}, 0, true);

    // --- column on/off chips -------------------------------------------------
    dr::Text(g, L10n::T(S::ColVisible), Font(8.5f), dr::C(theme::kTextMuted),
             gt.colLabel, 0, true);
    for (int i = 0; i < kChCols; ++i) {
        const RECT& c = gt.chip[i];
        const bool on = ui_.chColVis[i];
        const bool hover = ui_.hoverChChip == i;
        dr::FillRound(g, dr::RF(c), (float)d(5),
                      dr::C(on ? theme::kAccentSoft : (hover ? theme::kHover : theme::kPanel)));
        dr::StrokeRound(g, dr::RF(c), (float)d(5),
                        dr::C(on ? theme::kAccent
                                 : (hover ? theme::kBorderStrong : theme::kBorder)),
                        1.f);
        RECT cb = {c.left + d(6), (c.top + c.bottom) / 2 - d(6), c.left + d(18),
                   (c.top + c.bottom) / 2 + d(6)};
        dr::FillRound(g, dr::RF(cb), (float)d(3), dr::C(on ? theme::kAccent : theme::kField));
        dr::StrokeRound(g, dr::RF(cb), (float)d(3),
                        dr::C(on ? theme::kAccent : theme::kBorderStrong), 1.f);
        if (on) {
            const float ax = (float)cb.left + d(2.6f);
            const float ay = (float)cb.top + d(6.2f);
            const float bx = (float)cb.left + d(5.0f);
            const float by = (float)cb.top + d(8.6f);
            const float ex = (float)cb.right - d(2.6f);
            const float ey = (float)cb.top + d(3.6f);
            dr::Line(g, ax, ay, bx, by, dr::C(0xFFFFFF), 1.3f);
            dr::Line(g, bx, by, ex, ey, dr::C(0xFFFFFF), 1.3f);
        }
        RECT tr = {cb.right + d(6), c.top, c.right - d(2), c.bottom};
        dr::Text(g, L10n::T(chipNames[i]), Font(gt.chipFont),
                 dr::C(on ? theme::kText : theme::kTextMuted), tr, 0, true);
    }

    // --- 「自动排序」: clears the manual row order -------------------------------
    {
        const RECT& b = gt.autoSort;
        const bool manual = !pr_.channelOrder.empty();
        const bool hov = ui_.hoverChAutoSort != 0;
        dr::FillRound(g, dr::RF(b), (float)d(5),
                      dr::C(hov ? theme::kHover : theme::kPanel));
        dr::StrokeRound(g, dr::RF(b), (float)d(5),
                        dr::C(hov ? theme::kBorderStrong : theme::kBorder), 1.f);
        RECT br = {b.left + d(10), b.top, b.right - d(8), b.bottom};
        dr::Text(g, L10n::T(S::ChAutoSort), Font(8.5f),
                 dr::C(manual ? theme::kText : theme::kTextFaint), br, 0, true);
        // a tiny "sort" glyph: three bars of decreasing length, left of the text
        const int gx = b.left + d(4);
        const int gy = (b.top + b.bottom) / 2;
        for (int k = 0; k < 3; ++k) {
            const int wl = d(4) + (2 - k) * d(2);
            dr::Fill(g, RECT{gx, gy - d(3) + k * d(3), gx + wl, gy - d(2) + k * d(3)},
                     dr::C(manual ? theme::kTextMuted : theme::kTextFaint));
        }
    }

    // --- pager 「◀ 第 i / N 页 ▶」 (left of 自动排序) ---------------------------
    {
        const bool hasPrev = page > 0, hasNext = page + 1 < pages;
        auto pbtn = [&](const RECT& b, bool on, bool hov, bool left) {
            dr::FillRound(g, dr::RF(b), (float)d(4),
                          dr::C(hov && on ? theme::kHover : theme::kPanel));
            dr::StrokeRound(g, dr::RF(b), (float)d(4),
                            dr::C(on ? (hov ? theme::kBorderStrong : theme::kBorder)
                                     : theme::kBorder), 1.f);
            const Gdiplus::Color c =
                dr::C(on ? theme::kTextMuted : theme::kTextFaint, on ? 230 : 120);
            const float cx2 = (float)((b.left + b.right) / 2);
            const float cy2 = (float)((b.top + b.bottom) / 2);
            const float hh = (float)d(4), hw = (float)d(3);
            if (left) {
                dr::Line(g, cx2 + hw, cy2 - hh, cx2 - hw, cy2, c, 1.5f);
                dr::Line(g, cx2 - hw, cy2, cx2 + hw, cy2 + hh, c, 1.5f);
            } else {
                dr::Line(g, cx2 - hw, cy2 - hh, cx2 + hw, cy2, c, 1.5f);
                dr::Line(g, cx2 + hw, cy2, cx2 - hw, cy2 + hh, c, 1.5f);
            }
        };
        pbtn(gt.pagePrev, hasPrev, ui_.hoverChPageBtn == 1, true);
        pbtn(gt.pageNext, hasNext, ui_.hoverChPageBtn == 2, false);
        dr::Text(g, dr::Fmt(L10n::T(S::ChPageFmt), page + 1, pages), Font(8.5f),
                 dr::C(pages > 1 ? theme::kText : theme::kTextFaint), gt.pageLabel, 1,
                 true);
    }
    // --- header band ---------------------------------------------------------
    dr::FillRound(g, Gdiplus::RectF((float)gt.x0, (float)gt.headerY,
                                    (float)(gt.x1 - gt.x0), (float)gt.headerH),
                  (float)d(4), dr::C(theme::kPanelAlt));
    for (int i = 0; i < kChCols; ++i) {
        if (!ui_.chColVis[i]) continue;
        RECT r = {(LONG)std::lround(gt.cols.l[i]), gt.headerY,
                  (LONG)std::lround(gt.cols.r[i]), gt.headerY + gt.headerH};
        // a caption may be longer than its column (「话筒线长度(推荐)」 /
        // 「Mikrofonkabel」): it shrinks in half-point steps before GDI+ would elide
        // it.  The 长度 column's caption also shows the active 推荐 / 实际 mode.
        const std::wstring cap =
            (i == 5) ? ChannelLenHeader() : L10n::T(colNames[i]);
        float hf = 9.f;
        while (hf > 7.5f && dr::TextW(g, cap, Font(hf, true)) > (float)(r.right - r.left))
            hf -= 0.5f;
        dr::Text(g, cap, Font(hf, true), dr::C(theme::kTextMuted), r, 0, true);
    }

    // --- rows ---------------------------------------------------------------
    const int n = (std::min)((int)rows.size() - rowBase, pageRows);
    for (int i = 0; i < n; ++i) {
        const int ri = rowBase + i;
        const int y = gt.rowsY + i * gt.rowH;
        if (i % 2 == 1)
            dr::FillRound(g, Gdiplus::RectF((float)gt.x0, (float)y, (float)(gt.x1 - gt.x0),
                                            (float)gt.rowH),
                          (float)d(3), dr::C(theme::kPanel, 120));
        // the row being dragged (or hovered) gets a tint: it reads as picked up
        const bool dragging = ui_.chDragRow == i && ui_.chDragActive;
        if (dragging)
            dr::FillRound(g, Gdiplus::RectF((float)gt.x0, (float)y,
                                            (float)(gt.x1 - gt.x0), (float)gt.rowH),
                          (float)d(3), dr::C(theme::kAccent, 40));
        // the 「布线」 separator: a hairline plus its label, no cells and no
        // drag handle (the summary rows are not numbered channels)
        if (rows[ri].separator) {
            dr::Line(g, (float)gt.x0, (float)y, (float)gt.x1, (float)y,
                     dr::C(theme::kBorderStrong, 200), 1.4f);
            RECT sr = {(LONG)gt.cols.l[0], y, (LONG)gt.cols.r[0], y + gt.rowH};
            dr::Text(g, L"布线", Font(9.f, true), dr::C(theme::kTextMuted), sr, 0,
                     true);
            continue;
        }
        // drag handle in the 通道 column: two columns of three dots (grabbing the
        // 通道 or 链路 cell starts a row move, see App::MouseDown)
        if (ui_.chColVis[0] && !rows[ri].summary) {
            const int hx = gt.x0 + d(2);
            for (int cxi = 0; cxi < 2; ++cxi)
                for (int cyi = -1; cyi <= 1; ++cyi) {
                    const float px = (float)(hx + cxi * d(3));
                    const float py = (float)(y + gt.rowH / 2 + cyi * d(3) - d(1));
                    dr::Fill(g, RECT{(LONG)px, (LONG)py, (LONG)(px + d(1.4f)),
                                     (LONG)(py + d(1.4f))},
                             dr::C(theme::kTextFaint, dragging ? 200 : 130));
                }
        }
        auto cell = [&](int c, const std::wstring& s, const Gdiplus::Color& col) {
            if (!ui_.chColVis[c] || s.empty()) return;
            RECT r = {(LONG)std::lround(gt.cols.l[c]), y, (LONG)std::lround(gt.cols.r[c]),
                      y + gt.rowH};
            dr::Text(g, s, Font(9.5f), col, r, 0, true);
        };
        cell(0,
             rows[ri].summary ? rows[ri].chText : dr::Fmt(L"%02d", rows[ri].ch),
             dr::C(rows[ri].summary ? theme::kTextMuted : theme::kText));
        cell(1, rows[ri].model, dr::C(theme::kText));
        const Item* rowItem = rows[ri].summary ? nullptr : pr_.Find(rows[ri].itemId);
        const bool mainRow = rowItem && rowItem->defId == L"mainmic";
        cell(2, rows[ri].inst,
             dr::C(rows[ri].inst.empty()
                       ? theme::kTextFaint
                       : (mainRow ? theme::kTextMuted : theme::kAccent)));
        cell(3, rows[ri].remark, dr::C(theme::kTextMuted));
        // 链路 / 长度: read-only, filled from the stored routing topology.  With
        // no routing (or after a cancelled snapshot) they read as "—".  A row
        // whose stored cable breaks a capacity / length rule starts with "!".
        if (!rows[ri].link.empty() && rows[ri].bad) {
            cell(4, L"! " + rows[ri].link, dr::C(theme::kDanger));
        } else {
            cell(4, rows[ri].link.empty() ? std::wstring(L"—") : rows[ri].link,
                 dr::C(rows[ri].link.empty() ? theme::kTextFaint : theme::kTextMuted));
        }
        // 长度: the RESERVED length (rounded up to a standard cable size), or the
        // manual override, marked 「自定义」 — click the cell to edit it
        if (rows[ri].len > 0.0) {
            std::wstring len = dr::Fmt(L"%.1f m", rows[ri].len);
            if (rows[ri].custom) len += std::wstring(L" ") + L10n::T(S::CbCustom);
            cell(5, len,
                 dr::C(rows[ri].bad ? theme::kDanger
                                   : (rows[ri].custom ? theme::kAccent : theme::kTextMuted)));
        } else {
            cell(5, std::wstring(L"—"), dr::C(theme::kTextFaint));
        }
        // 距主话筒: plan distance to the main mic's L/R capsule on this side
        if (rows[ri].toMain >= 0.0) {
            cell(6, dr::Fmt(L"%.1f m", rows[ri].toMain), dr::C(theme::kTextMuted));
        } else {
            cell(6, std::wstring(L"—"), dr::C(theme::kTextFaint));
        }
        // 幅角: azimuth from the upstage axis, positive to the right
        if (rows[ri].hasAz) {
            const double a = rows[ri].azimuth;
            const std::wstring txt = (a >= -0.049 && a <= 0.049)
                                         ? std::wstring(L"0.0°")
                                         : dr::Fmt(a > 0 ? L"+%.1f°" : L"−%.1f°",
                                                   std::fabs(a));
            cell(7, txt, dr::C(theme::kTextMuted));
        } else {
            cell(7, std::wstring(L"—"), dr::C(theme::kTextFaint));
        }
    }
    // live row drag: an insertion line under the row the drop would land on
    if (ui_.chDragRow >= 0 && ui_.chDragActive && n > 0) {
        const int drop = std::clamp(ui_.chDropRow, 0, n - 1);
        const int ly = gt.rowsY + drop * gt.rowH;
        dr::Line(g, (float)gt.x0, (float)ly, (float)gt.x1, (float)ly,
                 dr::C(theme::kAccent, 220), 2.f);
        Gdiplus::SolidBrush dot(dr::C(theme::kAccent, 220));
        const float r0 = (float)d(3);
        g.FillEllipse(&dot, (float)gt.x0 - r0, (float)ly - r0, r0 * 2.f, r0 * 2.f);
    }
    // page-break cue at the BOTTOM edge of the table: a dashed rule plus
    // 「第 i 页 / 共 N 页 · 下页从第 X 行开始」 so the pagination is visible
    if (pages > 1) {
        const int cueY = gt.rowsY + (std::max)(n, 0) * gt.rowH;
        Gdiplus::Pen dash(dr::C(theme::kBorderStrong, 220), 1.1f);
        dash.SetDashStyle(Gdiplus::DashStyleDash);
        g.DrawLine(&dash, (float)gt.x0, (float)cueY, (float)gt.x1, (float)cueY);
        std::wstring cue = dr::Fmt(L10n::T(S::ChPageFmt), page + 1, pages);
        if (page + 1 < pages)
            cue += L"   ·   " +
                   dr::Fmt(L10n::T(S::ChPageNextFmt), (page + 1) * pageRows + 1);
        RECT cr = {(LONG)gt.x0, (LONG)(cueY + d(2)), (LONG)gt.x1,
                   (LONG)(cueY + d(2) + d(16))};
        dr::Text(g, cue, Font(8.f), dr::C(theme::kTextFaint), cr, 2, true);
    }
    if (rows.empty())
        dr::Text(g, L"—", Font(10.f), dr::C(theme::kTextFaint),
                 {gt.x0, gt.rowsY, gt.x1, gt.rowsY + d(40)}, 1, true);
}

// Floating 对应乐器 picker: a small list of the instrument types on stage, with
// their counts, anchored next to the clicked channel-table cell.  Drawn in the
// overlay pass and hit-tested at the top of MouseDown (like the spot popup).
void App::PaintChannelPicker(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    L_.chPickRow.clear();
    L_.spotSubRow.clear();
    L_.spotSubPanel = {};
    if (ui_.chSpotOpen < 0) return;
    const std::vector<SpotRow> rows = SpotTopRows(ui_.chSpotOpen);
    const int total = (int)rows.size();
    const int rowH = D(22);
    const int w = D(212);
    int x = ui_.chSpotAnchorX;
    if (x + w > L_.canvas.right - D(8)) x = L_.canvas.right - D(8) - w;
    if (x < L_.canvas.left + D(8)) x = L_.canvas.left + D(8);
    const int y = ui_.chSpotAnchorY + D(4);
    const int maxRows = ChannelPickerMaxRows(y);
    const int maxScroll = total > maxRows ? total - maxRows : 0;
    if (ui_.chSpotScroll > maxScroll) ui_.chSpotScroll = maxScroll;
    if (ui_.chSpotScroll < 0) ui_.chSpotScroll = 0;
    const int n = (std::min)(total - ui_.chSpotScroll, maxRows);
    RECT box = {x, y, x + w, y + n * rowH + D(8)};
    L_.chPickList = box;
    dr::FillRound(g, dr::RF(box), (float)d(7), dr::C(theme::kPanel));
    dr::StrokeRound(g, dr::RF(box), (float)d(7), dr::C(theme::kBorderStrong), 1.f);
    L_.chPickRow.assign((size_t)n, RECT{});
    for (int i = 0; i < n; ++i) {
        const int idx = ui_.chSpotScroll + i;
        RECT r = {x + d(4), y + d(4) + i * rowH, x + w - d(4), y + d(4) + (i + 1) * rowH};
        L_.chPickRow[(size_t)i] = r;
        const SpotRow& srow = rows[(size_t)idx];
        const bool hover = ui_.chSpotHover == i;
        if (hover) dr::FillRound(g, dr::RF(r), (float)d(4), dr::C(theme::kHover));
        const bool dim = (srow.kind == 0);
        RECT tr = {r.left + d(10), r.top, r.right - d(58), r.bottom};
        dr::Text(g, srow.label, Font(9.f),
                 dr::C(dim ? theme::kTextMuted : theme::kText), tr, 0, true);
        if (!srow.right.empty()) {
            RECT rr = {r.right - d(56), r.top, r.right - d(8), r.bottom};
            dr::Text(g, srow.right, Font(8.f), dr::C(theme::kTextFaint), rr, 2, true);
        }

    }
    // ---- second level -------------------------------------------------------
    if (ui_.spotGroup < 0 || ui_.spotGroup >= SpotGroupCount()) return;
    const std::vector<SpotRow> sub = SpotSubRows(ui_.spotGroup);
    if (sub.empty()) return;
    const int sw = D(188);
    const int sh = (int)sub.size() * rowH + D(8);
    int sx = box.right + D(4);
    if (sx + sw > L_.canvas.right - D(8)) sx = box.left - D(4) - sw;   // other side
    if (sx < L_.canvas.left + D(8)) sx = box.left + D(4);
    int sy = box.top;
    if (sy + sh > L_.canvas.bottom - D(8)) sy = L_.canvas.bottom - D(8) - sh;
    if (sy < L_.canvas.top + D(8)) sy = L_.canvas.top + D(8);
    L_.spotSubPanel = {sx, sy, sx + sw, sy + sh};
    dr::FillRound(g, dr::RF(L_.spotSubPanel), (float)d(7), dr::C(theme::kPanel));
    dr::StrokeRound(g, dr::RF(L_.spotSubPanel), (float)d(7),
                    dr::C(theme::kBorderStrong), 1.f);
    L_.spotSubRow.assign(sub.size(), RECT{});
    for (size_t k = 0; k < sub.size(); ++k) {
        RECT r = {sx + d(4), sy + d(4) + (int)k * rowH, sx + sw - d(4),
                  sy + d(4) + (int)(k + 1) * rowH};
        L_.spotSubRow[k] = r;
        const bool hover = ui_.hoverSpotSub == (int)k;
        if (hover) dr::FillRound(g, dr::RF(r), (float)d(4), dr::C(theme::kHover));
        dr::Text(g, sub[k].label, Font(9.f), dr::C(theme::kText),
                 RECT{r.left + d(10), r.top, r.right - d(8), r.bottom}, 0, true);
    }
}
namespace {

// ---------------------------------------------------------------------------
// Channel-table DEFAULT row order (通道表行顺序)
//
// The table is grouped by the section a microphone is bound to, in the order the
// user asked for:
//   主话筒 → 侧展 → 三角钢琴 → 第一小提琴 … 钢片琴 → 其它乐器 → 自定义 →
//   未分配 → 独唱 (独唱歌手) → 合唱 (合唱歌手) → 环境
// A bound microphone sorts under its bound section (`Item::spotType` = a
// catalogue defId, or one of the extra 对应乐器 keys section_side /
// section_ambience).  An instrument that is not in the list at all (竖琴, 大键琴,
// 打击乐, …) sorts after the known sections and before 自定义; a microphone with
// no binding is 未分配; the two singing characters own the last buckets before
// 环境, which is always LAST.  The order is STABLE inside a section (source
// order), so the channels of one group stay together.
// ---------------------------------------------------------------------------
const wchar_t* const kChannelSectionOrder[] = {
    L"violin",   L"violin2",  L"viola",     L"cello",   L"bass",
    L"flute",    L"clarinet", L"oboe",      L"bassoon", L"horn",
    L"trumpet",  L"trombone", L"tuba",      L"timpani", L"snare",
    L"bassdrum", L"gong",     L"cymbal",    L"xylophone", L"glockenspiel",
    L"celesta",
};
constexpr int kChannelSectionCount =
    (int)(sizeof(kChannelSectionOrder) / sizeof(kChannelSectionOrder[0]));

// bucket ranks — the order the 03 table is built in (and the order 「自动排序」
// re-applies after a manual drag).  The blocks follow the big categories of the
// binding picker; a mic bound to a CATEGORY (section_strings …) sits at the START
// of its block, ahead of the mics bound to one instrument of that block:
//   主话筒 → 环绕声道 → 天空声道 → 侧展 → 三角钢琴 → 立式钢琴 → 大键琴 →
//   弦乐组 → 木管组 → 铜管组 → 打击乐组 → 其余乐器 (钢片琴 / 竖琴 / …) →
//   自定义 → 独唱 → 合唱 → 环境 → 未分配 (the very LAST bucket)
struct ChanSec { const wchar_t* id; int rank; };
const ChanSec kChanSecs[] = {
    // keyboard instruments lead the non-category block: the three pianos have
    // their own buckets (三角钢琴 50 → 立式钢琴 60 → 大键琴 70) and the rest of the
    // keyboard/harp family follows as 其余乐器
    {L"upright_piano", 60}, {L"harpsichord", 70},
    // strings
    {L"violin", 100}, {L"violin2", 101}, {L"viola", 102}, {L"cello", 103},
    {L"bass", 104},
    // woodwind
    {L"flute", 200}, {L"clarinet", 201}, {L"oboe", 202}, {L"bassoon", 203},
    // brass
    {L"horn", 300}, {L"trumpet", 301}, {L"trombone", 302}, {L"tuba", 303},
    // percussion FAMILY (the 打击乐组 block)
    {L"timpani", 400}, {L"snare", 401}, {L"bassdrum", 402}, {L"gong", 403},
    {L"cymbal", 404}, {L"xylophone", 405}, {L"glockenspiel", 406},
    {L"percussion", 407},
    // 其余乐器: the keyboard / harp / voice instruments that own no bucket
    {L"celesta", 500},
    {L"harp", 600},
    {L"conductor", 800},
};
int ChannelSectionRank(const std::wstring& spotType) {
    if (spotType.empty()) return 1500;                 // 未分配: the VERY last bucket
    // 环绕声道 / 天空声道 lead the non-main sections, then 侧展
    if (spotType == L"section_surround") return 5;
    if (spotType == L"section_height") return 7;
    if (spotType == L"section_side") return 10;        // 侧展: right after 主话筒
    if (spotType == L"grand_piano") return 50;         // 三角钢琴 (own bucket)
    // the four CATEGORY targets sit at the START of their block, ahead of the
    // instruments bound to one player of that section
    if (spotType == L"section_strings") return 99;
    if (spotType == L"section_woodwind") return 199;
    if (spotType == L"section_brass") return 299;
    if (spotType == L"section_perc") return 399;
    // the keyboard / harp / voice families have NO category target any more: an
    // OLD file that still binds section_keyboard / _harp / _voice sorts with the
    // other instruments of its family (ahead of 其余乐器) instead of owning a block
    if (spotType == L"section_keyboard") return 499;
    if (spotType == L"section_harp") return 599;
    if (spotType == L"section_voice") return 699;
    // 独唱 / 合唱 own their buckets AFTER 未分配 and just before 环境 (the user's
    // order: …其余乐器 → 自定义 → 未分配 → 独唱 → 合唱 → 环境)
    if (spotType == L"solo_singer") return 1200;       // 独唱
    if (spotType == L"choir_singer") return 1210;      // 合唱
    for (const auto& s : kChanSecs)
        if (spotType == s.id) return s.rank;
    if (spotType == L"custom") return 1000;            // 自定义
    if (spotType == L"section_ambience") return 1400;  // 环境: right before 未分配
    return 900;   // an instrument outside every category
}
} // namespace
std::vector<App::ChRow> App::BuildChannelRows() const {
    std::vector<ChRow> rows;
    int ch = 1;
    const std::wstring mainName = L10n::T(S::MainMic);
    auto modelOf = [&](const Item& q) -> std::wstring {
        return q.model.empty() ? q.label : q.model;
    };
    // 主话筒 leads, then every other microphone by its bound section
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto& it : pr_.items) {
            if (it.kind != ItemKind::Mic) continue;
            const bool isMain = it.defId == L"mainmic";
            if ((pass == 0) != isMain) continue;
            if (isMain) {
                // one row per capsule: L, C (only for a Decca tree), R.  The model
                // column carries the per-capsule text, exactly like a normal mic.
                // the 乐器 column reads 「主话筒」 on every capsule row (the main mic is
                // its own section); 型号 carries the real per-capsule model or is empty
                rows.push_back({ch++, it.id, 0, CapsuleModelText(it, 0), mainName,
                                it.remark});
                if (it.mainTech == 3)
                    rows.push_back({ch++, it.id, 1, CapsuleModelText(it, 1), mainName, it.remark});
                rows.push_back({ch++, it.id, 2, CapsuleModelText(it, 2), mainName, it.remark});
                continue;
            }
            if (IsIntensityTech(it.defId)) {
                // ONE stage object, TWO channels: the L row carries 型号 L and the
                // R row 型号 R (M / S for MS) — exactly like the main mic capsules.
                // The 乐器 column names the bound instrument, else the technique.
                std::wstring nm = SpotTypeName(it.spotType);
                if (nm.empty()) {
                    const CatItem* c = FindItem(it.defId);
                    nm = c ? ItemName(*c) : it.defId;
                }
                rows.push_back({ch++, it.id, 0, it.modelL, nm, it.remark});
                rows.push_back({ch++, it.id, 2, it.modelR, nm, it.remark});
                continue;
            }
            int gi = pr_.GroupOf(it.id);
            if (gi >= 0) {
                const MicGroup& grp = pr_.micGroups[gi];
                if (!grp.ids.empty() && grp.ids.front() != it.id) continue;
                // 乐器 column: the bound instrument, else — for a technique pair —
                // the technique's own name (ORTF / NOS / DIN), so the pairing is
                // visible in the 03 table exactly like on the stage
                std::wstring inst = SpotTypeName(it.spotType);
                if (inst.empty() && grp.kind == 2 && !grp.tech.empty())
                    inst = SpotTypeName(grp.tech);
                for (int id : grp.ids) {
                    const Item* m = nullptr;
                    for (const auto& q : pr_.items)
                        if (q.id == id) { m = &q; break; }
                    rows.push_back({ch++, id, -1,
                                    m ? modelOf(*m) : std::wstring(), inst,
                                    m ? m->remark : std::wstring()});
                }
            } else {
                rows.push_back({ch++, it.id, -1, modelOf(it), SpotTypeName(it.spotType),
                                it.remark});
            }
        }
    }
    // sort by section (STABLE: the microphone order inside a section is kept),
    // then number the channels in row order so the 通道 column reads 01, 02, …
    {
        std::stable_sort(rows.begin(), rows.end(), [&](const ChRow& a, const ChRow& b) {
            const Item* ia = a.itemId > 0 ? pr_.Find(a.itemId) : nullptr;
            const Item* ib = b.itemId > 0 ? pr_.Find(b.itemId) : nullptr;
            const int ra = ia ? (ia->defId == L"mainmic" ? 0 : ChannelSectionRank(ia->spotType))
                              : 1100;
            const int rb = ib ? (ib->defId == L"mainmic" ? 0 : ChannelSectionRank(ib->spotType))
                              : 1100;
            return ra < rb;
        });
        ch = 1;
        for (auto& r : rows) r.ch = ch++;
    }
    // MANUAL order (rows the user dragged) OVERRIDES the automatic one: it is a
    // list of item ids in the dragged order.  Rows of one item (the main mic's
    // capsules) keep their capsule order, and anything the list does not name
    // keeps its automatic place — after the listed rows.
    if (!pr_.channelOrder.empty()) {
        auto manualPos = [&](int itemId) {
            auto k = std::find(pr_.channelOrder.begin(), pr_.channelOrder.end(), itemId);
            return k == pr_.channelOrder.end() ? (int)pr_.channelOrder.size() + 1
                                               : (int)(k - pr_.channelOrder.begin());
        };
        std::stable_sort(rows.begin(), rows.end(), [&](const ChRow& a, const ChRow& b) {
            return manualPos(a.itemId) < manualPos(b.itemId);
        });
        ch = 1;
        for (auto& r : rows) r.ch = ch++;
    }
    // 链路 / 长度 come from the STORED routing topology, not from the cables that
    // happen to be drawn: a cancelled snapshot keeps the last result readable
    // until the next 「自动布线」 run.  No routing yet = empty cells.
    if (pr_.topo.valid) {
        for (auto& r : rows) {
            // group-aware: a bound pair shares ONE cable, so both rows show it
            if (const CableLink* ln =
                    FindMicCableForItem(pr_, pr_.topo.links, r.itemId)) {
                r.link = CableChainText(pr_, pr_.topo.links, *ln);
                const CableIssue is = CheckCable(pr_, *ln);
                // a radio link is not a cable: the 长度 cell reads "—"
                // 推荐 = the standard cable size (the original numbers); 实际 = the
                // real drawn run for THIS row's microphone/capsule, the same number
                // the 05 tag prints.  Switched by clicking the column header.
                if (is.radio)
                    r.len = 0.0;
                else if (pr_.lenMode == 1)
                    r.len = CableRunRealLen(pr_, *ln, r.itemId, r.capsule);
                else
                    r.len = CableReservedLen(pr_, *ln);
                r.custom = CableOverrideLen(pr_, *ln) > 0.0;
                r.bad = is.overChannels;
            }
        }
    }
    // ---- 距主话筒: plan distance from each POINT microphone (or its group's
    // convergence point) to the MAIN microphone capsule on ITS side ------------
    // Only the L and R capsules take part; the centre capsule never does.  A mic
    // right of the main mic's centre measures to R, left measures to L, and one
    // exactly on the centre line takes the NEARER capsule.  Main-mic rows and
    // single-object intensity rows stay 「—」 (toMain < 0).
    {
        const Item* mainMic = nullptr;
        for (const auto& it : pr_.items)
            if (it.kind == ItemKind::Mic && it.defId == L"mainmic") {
                mainMic = &it;
                break;
            }
        if (mainMic) {
            const std::vector<std::pair<double, double>> caps = MicCapsulePos(*mainMic);
            if (caps.size() >= 2) {
                const std::pair<double, double> capL = caps.front();
                const std::pair<double, double> capR = caps.back();
                for (auto& r : rows) {
                    const bool rowIsMain = (r.itemId == mainMic->id);
                    if (rowIsMain) continue;
                    const Item* m = pr_.Find(r.itemId);
                    if (!m) continue;
                    if (IsIntensityTech(m->defId)) continue;   // single-object rows: 「—」
                    // a grouped mic measures from the group's convergence point
                    std::pair<double, double> p = {m->x, m->y};
                    if (pr_.GroupOf(m->id) >= 0)
                        p = GroupConvergencePos(pr_, m->id);
                    const std::pair<double, double>* target = &capL;
                    if (p.first > mainMic->x + 1e-9) {
                        target = &capR;
                    } else if (p.first >= mainMic->x - 1e-9) {
                        // exactly on the centre line: the NEARER capsule wins
                        const double dL = std::hypot(p.first - capL.first,
                                                     p.second - capL.second);
                        const double dR = std::hypot(p.first - capR.first,
                                                     p.second - capR.second);
                        target = (dR < dL) ? &capR : &capL;
                    }
                    r.toMain = std::hypot(p.first - target->first,
                                          p.second - target->second);
                    // 幅角 from the L/R MIDPOINT of the main microphone (the centre
                    // capsule never takes part), 0° = upstage = screen up = world −Y,
                    // positive = to the right (+X)
                    const double ox0 = (capL.first + capR.first) * 0.5;
                    const double oy0 = (capL.second + capR.second) * 0.5;
                    const double dx = p.first - ox0;
                    const double dy = p.second - oy0;      // + = toward the audience
                    double az = std::atan2(dx, -dy) * 180.0 / 3.14159265358979323846;
                    r.azimuth = az;
                    r.hasAz = true;
                }
            }
        }
    }
    return rows;
}
// ---------------------------------------------------------------------------
// layer 5 — 01 项目信息
//
// The project header (项目名称 / 场地 / 指挥 / 系统 / 录音 / 助理 / 备注), the stage
// size (宽度 / 深度 / 层高, the three presets) and the four 外观颜色 rows used to be
// the top half of the right inspector; they are facts about the PROJECT, not about
// the selected object, so they get their own page with a panel centred in the
// canvas.  Every field box comes from App::FieldRect() — the same rhythm the
// inspector uses — and every EDIT is placed by App::PlaceInspectorEdit() from that
// same rect, so a label can never sit on a box and two fields can never overlap.
// ---------------------------------------------------------------------------
void App::LayoutProjectPage() {
    auto S = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    // this page owns no inspector rows: clear everything the stage pages use, so
    // no stale EDIT hit-rect survives a page switch
    L_.secSel = L_.secLayers = {};
    for (int i = 0; i < 4; ++i) L_.divider[i] = {};
    L_.itemColorRow = L_.itemColorLabel = L_.itemColorSwatch = {};
    L_.itemColorHex = L_.itemColorReset = {};
    L_.selInfo = L_.spotBtn = L_.spotList = {};
    L_.selName = L_.xField = L_.yField = L_.angleField = L_.lenField = {};
    L_.micModel = L_.micHeight = L_.spaceField = L_.centerField = {};
    L_.modelL = L_.modelR = L_.modelC = L_.flyField = L_.oField = {};
    L_.delBtn = L_.bindBtn = L_.treeBtn = {};
    L_.flownRow = L_.flownCheck = {};
    for (int i = 0; i < 4; ++i) { L_.techBtn[i] = {}; L_.patternBtn[i] = {}; }
    for (int i = 0; i < 3; ++i) L_.techPreset[i] = {};
    for (int i = 0; i < 2; ++i) { L_.layerRow[i] = {}; L_.layerCheck[i] = {}; }
    for (int i = 0; i < 2; ++i) L_.textRow[i] = L_.textCheck[i] = {};
    // the inspector on this page is empty, so there is nothing to scroll
    L_.rightContent = 0;
    ui_.rightScroll = 0;

    const RECT& c = L_.canvas;
    const int pad = S(34);
    const int colGap = S(36);
    const int titleH = S(30);
    const int gap = S(6);

    // two columns inside one panel: project fields | stage size + colours
    const int cwpx = (int)(c.right - c.left);
    int panelW = (std::min)(cwpx - S(96), S(760));
    if (panelW < S(520)) panelW = (std::min)(cwpx - S(48), S(520));
    if (panelW < S(240)) panelW = cwpx - S(16);
    int colW = (panelW - 2 * pad - colGap) / 2;

    // --- measured heights (the panel is centred, so measure before placing) ---
    const int secH = S(16) + S(24);                       // header + the gap under it
    const int fieldGap = S(22) + gap;                     // label line + gap
    // left column: the page title already says 项目信息, so the fields start right
    // under it (no second header)
    const int leftH = (fieldGap + S(32)) * 3 +           // 项目名称 / 场地 / 指挥
                      (fieldGap + S(28)) * 3 +           // 系统 / 录音 / 助理
                      S(22) + S(64);                     // 备注
    const int rightH = secH + (fieldGap + S(32)) +       // 舞台尺寸 + 宽度/深度
                       (fieldGap + S(32) + S(8)) +        // 层高
                       S(34) + S(14) +                    // the three presets
                       S(16) + S(8) + S(32) * 2;          // 外观颜色 + 2 rows
    const int contentH = (std::max)(leftH, rightH);
    int panelH = pad + titleH + S(5) + S(16) + contentH + pad;
    const int availH = (c.bottom - c.top) - S(32);
    if (panelH > availH) panelH = availH;

    int panelLeft = c.left + ((c.right - c.left) - panelW) / 2;
    int panelTop = c.top + ((c.bottom - c.top) - panelH) / 2;
    if (panelTop < c.top + S(8)) panelTop = c.top + S(8);
    L_.p1Panel = {panelLeft, panelTop, panelLeft + panelW, panelTop + panelH};

    // --- page title ----------------------------------------------------------
    L_.p1Title = {panelLeft + pad, panelTop + pad, panelLeft + panelW - pad,
                  panelTop + pad + titleH};
    L_.p1Rule = {L_.p1Title.left, L_.p1Title.bottom + S(4),
                 L_.p1Title.right, L_.p1Title.bottom + S(5)};

    // --- left column: the project header -------------------------------------
    int lx = panelLeft + pad;
    int rxCol = lx + colW + colGap;
    int y = L_.p1Rule.bottom + S(16);
    L_.secProject = {};
    L_.piece = FieldRect(lx, colW, 32, y);
    y = L_.piece.box.bottom + gap;
    L_.venue = FieldRect(lx, colW, 32, y);
    y = L_.venue.box.bottom + gap;
    L_.conductor = FieldRect(lx, colW, 32, y);
    y = L_.conductor.box.bottom + gap;
    L_.sysEng = FieldRect(lx, colW, 28, y);
    y = L_.sysEng.box.bottom + gap;
    L_.recEng = FieldRect(lx, colW, 28, y);
    y = L_.recEng.box.bottom + gap;
    L_.asstEng = FieldRect(lx, colW, 28, y);
    y = L_.asstEng.box.bottom + gap;
    L_.notes = FieldRect(lx, colW, 64, y);

    // --- right column: 舞台尺寸 + 外观颜色 -----------------------------------
    int ry = L_.p1Rule.bottom + S(16);
    L_.secStage = {rxCol, ry, rxCol + colW, ry + S(16)};
    ry += S(24);
    int halfW = (colW - S(10)) / 2;
    L_.wField = FieldRect(rxCol, halfW, 32, ry);
    L_.dField = FieldRect(rxCol + halfW + S(10), colW - halfW - S(10), 32, ry);
    ry = L_.wField.box.bottom + gap;
    L_.ceilingField = FieldRect(rxCol, halfW, 32, ry);
    ry = L_.ceilingField.box.bottom + S(8);
    int pw = (colW - S(16)) / 3;
    for (int i = 0; i < 3; ++i) {
        int px = rxCol + i * (pw + S(8));
        L_.presets[i] = {px, ry, px + pw, ry + S(34)};
    }
    ry += S(34) + S(14);
    L_.secAppearance = {rxCol, ry, rxCol + colW, ry + S(16)};
    ry += S(8) + S(16);
    for (int i = 0; i < 2; ++i) {   // 文字 + 舞台底色 only (the two lost rows closed up)
        L_.colorRow[i] = {rxCol, ry, rxCol + colW, ry + S(32)};
        L_.colorSwatch[i] = {rxCol + colW - S(100), ry + S(7), rxCol + colW - S(64),
                             ry + S(25)};
        L_.colorHex[i] = {rxCol + colW - S(60), ry, rxCol + colW, ry + S(32)};
        ry += S(32);
    }
    L_.secLayers = {};

    // --- child controls: same placement path as the inspector -----------------
    auto place = [&](HWND h, const RECT& r) {
        PlaceInspectorEdit(h, r, true, L_.p1Panel);
    };
    place(ePiece_, L_.piece.edit);
    place(eVenue_, L_.venue.edit);
    place(eConductor_, L_.conductor.edit);
    place(eSysEng_, L_.sysEng.edit);
    place(eRecEng_, L_.recEng.edit);
    place(eAsstEng_, L_.asstEng.edit);
    place(eNotes_, L_.notes.edit);
    place(eWidth_, L_.wField.edit);
    place(eDepth_, L_.dField.edit);
    place(eCeiling_, L_.ceilingField.edit);
    // every field that belongs to another page is hidden here, from an empty rect
    place(eOff_, {});
    place(eFlyH_, {});
    place(eSelName_, {});
    place(eX_, {});
    place(eY_, {});
    place(eAngle_, {});
    place(ePianoLen_, {});
    place(eMicH_, {});
    place(eMicModel_, {});
    place(eSpace_, {});
    place(eCenter_, {});
    place(eModelL_, {});
    place(eModelR_, {});
    place(eModelC_, {});
    place(eDevName_, {});
    place(eDevCh_, {});
    place(eDevX_, {});
    place(eDevY_, {});
    PlaceInspectorEdit(eSearch_, L_.searchEdit, true, L_.left);

    UpdateStageXf();
}

void App::PaintProjectPage(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    // the page is a document, not a plan: a plain canvas, no grid, no stage
    dr::Fill(g, L_.canvas, dr::C(theme::kCanvas));

    const RECT& p = L_.p1Panel;
    if (p.right <= p.left || p.bottom <= p.top) return;
    dr::FillRound(g, dr::RF(p), (float)d(12), dr::C(theme::kPanel));
    dr::StrokeRound(g, dr::RF(p), (float)d(12), dr::C(theme::kBorder), 1.f);

    // --- title ---------------------------------------------------------------
    dr::Text(g, L10n::T(S::SectionProject), Font(13.f, true), dr::C(theme::kText),
             L_.p1Title, 0, true);
    if (L_.p1Rule.right > L_.p1Rule.left)
        dr::Line(g, (float)L_.p1Rule.left, (float)L_.p1Rule.top,
                 (float)L_.p1Rule.right, (float)L_.p1Rule.top,
                 dr::C(theme::kBorder), 1.f);

    auto section = [&](const RECT& r, const wchar_t* txt) {
        if (r.right <= r.left) return;
        dr::FillRound(g, Gdiplus::RectF((float)r.left, (float)(r.top + d(3)),
                                       (float)d(3), (float)d(9)),
                      d(1.5f), dr::C(theme::kAccent, 200));
        dr::Text(g, txt, Font(9.f), dr::C(theme::kTextMuted),
                 RECT{r.left + d(10), r.top, r.right, r.bottom}, 0, true);
    };
    auto label = [&](const FieldBox& f, const std::wstring& txt) {
        if (f.box.right <= f.box.left) return;
        dr::Text(g, txt, Font(9.f), dr::C(theme::kTextMuted), f.label, 0, true);
    };
    auto numLabel = [&](const FieldBox& f, S id) {
        label(f, dr::Fmt(L"%s (m)", L10n::T(id)));
    };

    section(L_.secProject, L10n::T(S::SectionProject));
    PaintFieldBox(g, L_.piece, ID_PIECE, true);
    label(L_.piece, L10n::T(S::FieldPiece));
    PaintFieldBox(g, L_.venue, ID_VENUE, true);
    label(L_.venue, L10n::T(S::FieldVenue));
    PaintFieldBox(g, L_.conductor, ID_CONDUCTOR, true);
    label(L_.conductor, L10n::T(S::FieldConductor));
    PaintFieldBox(g, L_.sysEng, ID_SYSENG, true);
    label(L_.sysEng, L10n::T(S::SysEngineer));
    PaintFieldBox(g, L_.recEng, ID_RECENG, true);
    label(L_.recEng, L10n::T(S::RecEngineer));
    PaintFieldBox(g, L_.asstEng, ID_ASSTENG, true);
    label(L_.asstEng, L10n::T(S::AsstEngineer));
    PaintFieldBox(g, L_.notes, ID_NOTES, true);
    label(L_.notes, L10n::T(S::FieldNotes));

    // --- 舞台尺寸 ------------------------------------------------------------
    section(L_.secStage, L10n::T(S::SectionStage));
    PaintFieldBox(g, L_.wField, ID_WIDTH, true);
    PaintFieldBox(g, L_.dField, ID_DEPTH, true);
    numLabel(L_.wField, S::FieldWidth);
    numLabel(L_.dField, S::FieldDepth);
    PaintFieldBox(g, L_.ceilingField, ID_CEILING, true);
    // 「层高（吊装设备高度）」: the unit "(m)" is replaced by the parenthetical, which is
    // drawn as a SECOND text run at ~72 % size in a fainter colour
    if (L_.ceilingField.box.right > L_.ceilingField.box.left) {
        Gdiplus::Font* fMain = Font(9.f);
        Gdiplus::Font* fSub = Font(6.5f);
        const RECT& lr = L_.ceilingField.label;
        const std::wstring mainTxt = L10n::T(S::FieldCeiling);
        const std::wstring subTxt = L10n::T(S::FieldCeilingSub);
        dr::Text(g, mainTxt, fMain, dr::C(theme::kTextMuted), lr, 0, true);
        const float wMain = dr::TextW(g, mainTxt, fMain);
        RECT sr = {(LONG)(lr.left + std::lround(wMain)), lr.top + d(1), lr.right,
                   lr.bottom};
        dr::Text(g, subTxt, fSub, dr::C(theme::kTextFaint), sr, 0, true);
    }
    const S pids[3] = {S::PresetHall, S::PresetChurch, S::PresetSalon};
    for (int i = 0; i < 3; ++i) {
        const bool hover = ui_.hoverPreset == i;
        dr::FillRound(g, dr::RF(L_.presets[i]), d(5),
                      dr::C(hover ? theme::kField : theme::kPanelAlt));
        dr::StrokeRound(g, dr::RF(L_.presets[i]), d(5),
                        dr::C(hover ? theme::kBorderStrong : theme::kBorder), 1.f);
        dr::Text(g, L10n::T(pids[i]), Font(9.f), dr::C(theme::kTextMuted),
                 L_.presets[i], 1, true);
    }

    // --- 外观颜色 ------------------------------------------------------------
    section(L_.secAppearance, L10n::T(S::SectionAppearance));
    // 乐器线条 / 话筒线条 are gone: the two colours still exist in the project (a
    // saved file keeps them) but nothing draws or edits them — only 文字 + 舞台底色
    Rgb* cols[2] = {&pr_.textColor, &pr_.stageFill};
    const S cids[2] = {S::ColorText, S::ColorStage};
    for (int i = 0; i < 2; ++i) {
        if (L_.colorRow[i].right <= L_.colorRow[i].left) continue;
        const bool hover = ui_.hoverColor == i;
        if (hover) {
            RECT r = L_.colorRow[i];
            r.left -= d(6);
            r.right += d(6);
            dr::FillRound(g, dr::RF(r), d(5), dr::C(theme::kHover));
        }
        dr::Text(g, L10n::T(cids[i]), Font(10.f), dr::C(theme::kText), L_.colorRow[i],
                 0, true);
        dr::FillRound(g, dr::RF(L_.colorSwatch[i]), d(3), dr::C(*cols[i]));
        dr::StrokeRound(g, dr::RF(L_.colorSwatch[i]), d(3), dr::C(0x000000, 48), 1.f);
        dr::Text(g, dr::Fmt(L"#%02X%02X%02X", cols[i]->r, cols[i]->g, cols[i]->b),
                 Font(8.5f), dr::C(theme::kTextFaint), L_.colorHex[i], 2, true);
    }
}
