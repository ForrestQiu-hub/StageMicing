#include "App.h"
#include "Assets.h"
#include "Catalog.h"
#include "Draw.h"
#include "L10n.h"
#include "Theme.h"

#include <cmath>

// ---------------------------------------------------------------------------
// top-bar geometry: one right-aligned cursor, every chip as wide as its measured
// label (the global +3 type bump made the old fixed widths too narrow, which is
// exactly how the SRA chip used to run under the tab strip).  Gaps are S(8) and
// the strip never leaves the client area; on a narrow window the SRA chip is the
// first thing dropped instead of letting labels overlap.
// ---------------------------------------------------------------------------
// The top-bar export chip is ONE button whose label follows the current page:
// 通道表 (layer 3) prints 「导出 PDF」, every drawing page 「导出图片」.
static const wchar_t* ExportChipText(int layer) {
    if (layer == 3) return L10n::T(::S::FnPdf);
    // the chip label drops the menu-style ellipsis: 「导出图片」
    static std::wstring s;
    s = L10n::T(::S::FnExportPng);
    while (!s.empty() && (s.back() == L'…' || s.back() == L'.')) s.pop_back();
    return s.c_str();
}

void App::LayoutTopBar() {
    auto S = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    Gdiplus::Bitmap bmp(8, 8, PixelFormat32bppPARGB);
    Gdiplus::Graphics g(&bmp);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    auto tw = [&](const std::wstring& s, float px, bool bold) {
        return (int)std::ceil(dr::TextW(g, s, Font(px, bold)));
    };

    const int padX = S(14);      // inside a chip, left and right
    const int gap = S(8);        // between two chips
    const int chevW = S(12);     // the ▾ that follows 文件 / the language code

    // measured widths of the four (five with SRA) chips — no sample-rate chip
    const std::wstring code = L10n::Code(L10n::Get());
    const int langW = (std::max)(S(60), tw(code, 9.5f, false) + chevW + padX);
    const int presetW = (std::max)(S(64), tw(L10n::T(S::FnPreset), 9.5f, false) +
                                             padX * 2);
    // ONE export chip whose label follows the page: 通道表 → 「导出 PDF」,
    // 乐器摆位 / 话筒摆位 / 布线 → 「导出图片」 (PNG of the drawing).  Its width is
    // measured from the label actually shown, so neighbouring chips never move.
    const wchar_t* exportTxt = ExportChipText(ui_.layer);
    const int pdfW = (std::max)(S(64), tw(exportTxt, 9.5f, false) + padX * 2);
    const std::wstring simpleTxt =
        L10n::T(ui_.simple ? S::FnRestore : S::FnSimplify);
    const int simpleW = (std::max)(S(64), tw(simpleTxt, 9.5f, false) + padX * 2);
    const double sraVal = MainMicSRA();
    const std::wstring sraTxt = sraVal > 0.0 ? dr::Fmt(L"SRA %.0f°", sraVal) : L"SRA";
    const int sraW = (std::max)(S(52), tw(sraTxt, 9.5f, false) + padX * 2);
    const int fileW = (std::max)(S(58), tw(L10n::T(S::FnFile), 9.5f, false) + chevW + padX);

    // the tab strip needs a floor of one 56 logical px cell per page; if the chips
    // would eat into it, the SRA chip (a stage-plot readout) is dropped first
    const int tabsL = S(302);
    const int minTabs = kTabCount * S(56);
    // the app-lock chip sits at the FAR RIGHT, after the language selector
    const int lockW = S(38);
    auto clusterW = [&](bool sra) {
        return lockW + langW + pdfW + presetW + simpleW + fileW + (sra ? sraW : 0) +
               gap * (sra ? 6 : 5) + S(16);
    };
    bool sraChip = (ui_.layer == 0 || ui_.layer == 1);
    if (sraChip && clusterW(true) + gap + tabsL + minTabs > cw_) sraChip = false;

    // right-aligned cursor: place a chip, then step left by its width + the gap
    int rxr = cw_ - S(16);
    auto place = [&](int w, int top, int bottom) {
        RECT r = {rxr - w, top, rxr, bottom};
        rxr = r.left - gap;
        return r;
    };
    L_.lockChip = place(lockW, S(19), S(45));
    L_.langBox = place(langW, S(19), S(45));
    L_.fnPdf = place(pdfW, S(18), S(46));
    // 预设 sits right next to 导出 PDF: both are one-shot functions
    L_.fnPreset = place(presetW, S(18), S(46));
    L_.fnSimple = place(simpleW, S(18), S(46));
    if (sraChip) L_.fnSra = place(sraW, S(18), S(46));
    else L_.fnSra = {};   // hidden: an empty rect can never be hit
    L_.fnFile = place(fileW, S(18), S(46));

    // the tabs fill exactly what is left; they never run under 文件 ▾
    int tabsR = (int)L_.fnFile.left - gap;
    if (tabsR < tabsL + kTabCount) tabsR = tabsL + kTabCount;
    const int tabW = (tabsR - tabsL) / kTabCount;
    for (int i = 0; i < kTabCount; ++i)
        L_.layerTabs[i] = {tabsL + i * tabW, 0, tabsL + (i + 1) * tabW,
                           (int)L_.top.bottom};
}

void App::PaintTopBar(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    dr::Fill(g, L_.top, dr::C(theme::kPanel));
    dr::Line(g, 0.f, (float)(L_.top.bottom - 1), (float)cw_,
             (float)(L_.top.bottom - 1), dr::C(theme::kBorder), 1.f);

    // --- mark -------------------------------------------------------------
    Gdiplus::RectF logo = dr::RF(L_.logo);
    dr::FillRound(g, logo, d(8), dr::C(theme::kText));
    float ccx = logo.X + logo.Width * 0.5f;
    float ccy = logo.Y + logo.Height * 0.60f;
    float rr = logo.Width * 0.20f;
    dr::Circle(g, ccx, ccy, rr, dr::C(0xFFFFFF, 242), d(1.9f));
    dr::Line(g, ccx - rr * 1.38f, ccy - rr, ccx + rr * 1.38f,
             ccy - rr, dr::C(0xFFFFFF, 242), d(1.9f));

    // --- wordmark ---------------------------------------------------------
    {
        RECT r = {L_.brand.left, L_.brand.top + d(2), L_.brand.right, L_.brand.top + d(22)};
        dr::Text(g, L"STAGE MICING", Font(13.f, true), dr::C(theme::kText), r, 0, true);
        dr::Text(g, L10n::T(S::AppSubtitle), Font(9.f), dr::C(theme::kTextMuted),
                 L_.brandSub, 0, true);
        dr::Line(g, (float)d(276), (float)(L_.top.top + d(16)),
                 (float)d(276), (float)(L_.top.bottom - d(16)),
                 dr::C(theme::kBorder), 1.f);
    }

    // --- layer tabs ---------------------------------------------------------
    // The strip shares the bar with the function cluster on the right; Layout()
    // gives it whatever space is left, so on a narrow window the tabs get
    // tighter (and drop their number) instead of running under the buttons.
    const S* ids = LayerNameIds();
    for (int i = 0; i < kTabCount; ++i) {
        const bool enabled = true;   // all six pages exist (01 = 项目信息)
        const bool active = (LayerOfTab(i) == ui_.layer);
        const bool hover = (ui_.hoverLayer == i) && enabled;
        const int tabL = L_.layerTabs[i].left;
        const int tabR = L_.layerTabs[i].right;
        const int tabW = tabR - tabL;
        const bool compact = tabW < d(96);

        if (hover && !active) {
            RECT hr = L_.layerTabs[i];
            const int pad = compact ? d(3) : d(6);
            hr.left += pad;
            hr.right -= pad;
            hr.top += d(10);
            hr.bottom -= d(10);
            if (hr.right > hr.left)
                dr::FillRound(g, dr::RF(hr), d(7), dr::C(theme::kHover));
        }
        if (active) {
            const int inset = compact ? d(6) : d(14);
            const int uw = tabW - inset * 2;
            if (uw > d(4)) {
                dr::FillRound(g, Gdiplus::RectF((float)(tabL + inset),
                                                (float)(L_.top.bottom - d(2)),
                                                (float)uw, (float)d(2)),
                              d(1), dr::C(theme::kAccent));
            }
        }

        Gdiplus::Color numCol = active ? dr::C(theme::kAccent)
                              : enabled ? dr::C(theme::kTextFaint)
                                        : dr::C(theme::kTextFaint, 150);
        Gdiplus::Color txtCol = active ? dr::C(theme::kText)
                              : enabled ? dr::C(theme::kTextMuted)
                                        : dr::C(theme::kTextFaint);
        // The number and the name are one centred label: with the global +3 type
        // bump a separate number column would leave the name too little room.
        const std::wstring label = compact
            ? std::wstring(L10n::T(ids[i]))
            : L10n::T(ids[i]);   // no numbering on the tabs
        RECT tr = {tabL + d(4), L_.layerTabs[i].top, tabR - d(4),
                   L_.layerTabs[i].bottom - d(2)};
        // never clip: measure first and step the size down only if the tab is too
        // narrow for the enlarged type (long translations, narrow windows)
        const float avail = (float)(tr.right - tr.left);
        float size = compact ? 9.5f : 11.5f;
        for (int k = 0; k < 4; ++k) {
            const float trySize = (compact ? 9.5f : 11.5f) - (float)k;
            if (dr::TextW(g, label, Font(trySize, active)) <= avail) {
                size = trySize;
                break;
            }
            size = trySize;
        }
        dr::Text(g, label, Font(size, active), txtCol, tr, 1, true);
        (void)numCol;
    }

    // --- function bar -------------------------------------------------------
    {
        // 文件 ▾ : 保存项目 / 另存为 / 打开项目 live in the floating list that
        // PaintOverlay draws, modelled on the language dropdown
        {
            const bool hv = (ui_.hoverFn == 5) || ui_.fileOpen;
            dr::FillRound(g, dr::RF(L_.fnFile), d(8),
                          dr::C(ui_.fileOpen ? theme::kAccentSoft
                                             : (hv ? theme::kHover : theme::kPanelAlt)));
            dr::StrokeRound(g, dr::RF(L_.fnFile), d(8),
                            dr::C(ui_.fileOpen ? theme::kAccent
                                 : (hv ? theme::kBorderStrong : theme::kBorder)), 1.f);
            Gdiplus::Font* f = Font(9.5f);
            const std::wstring label = L10n::T(S::FnFile);
            const float tw = dr::TextW(g, label, f);
            const int chevW = d(9);
            const int groupW = (int)tw + d(5) + chevW;
            const int startX = (L_.fnFile.left + L_.fnFile.right - groupW) / 2;
            dr::Text(g, label, f, dr::C(hv ? theme::kText : theme::kTextMuted),
                     RECT{startX, L_.fnFile.top, startX + (int)tw + d(4),
                          L_.fnFile.bottom},
                     0, true);
            const float ax = (float)(startX + (int)tw + d(5) + chevW / 2);
            const float ay = (float)((L_.fnFile.top + L_.fnFile.bottom) / 2);
            const float hw = (float)d(4);
            const float hh = (float)d(2);
            const Gdiplus::Color ac(hv ? dr::C(theme::kText)
                                       : dr::C(theme::kTextMuted));
            dr::Line(g, ax - hw, ay - hh, ax, ay + hh, ac, 1.3f);
            dr::Line(g, ax, ay + hh, ax + hw, ay - hh, ac, 1.3f);
        }

        // the SRA chip only exists on the two stage plots (layers 0 and 1); Layout() leaves
        // an empty rectangle on the channel-list, topology and cabling pages
        bool hoverSra = ui_.hoverFn == 1;
        if (L_.fnSra.right > L_.fnSra.left) {
            double sraVal = MainMicSRA();
            bool hasSra = sraVal > 0.0;
            dr::FillRound(g, dr::RF(L_.fnSra), d(8),
                          dr::C(hasSra ? theme::kAccent
                                       : (hoverSra ? theme::kHover : theme::kPanelAlt)));
            dr::StrokeRound(g, dr::RF(L_.fnSra), d(8),
                            dr::C(hasSra ? theme::kAccent
                                 : (hoverSra ? theme::kBorderStrong : theme::kBorder)), 1.f);
            dr::Text(g, hasSra ? dr::Fmt(L"SRA %.0f°", sraVal) : L"SRA", Font(9.5f),
                     dr::C(hasSra ? 0xFFFFFF : theme::kTextMuted), L_.fnSra, 1, true);
        }

        bool hover = ui_.hoverFn == 2;
        dr::FillRound(g, dr::RF(L_.fnSimple), d(8),
                      dr::C(ui_.simple ? theme::kAccent : (hover ? theme::kHover : theme::kPanelAlt)));
        dr::StrokeRound(g, dr::RF(L_.fnSimple), d(8),
                        dr::C(ui_.simple ? theme::kAccent
                             : (hover ? theme::kBorderStrong : theme::kBorder)), 1.f);
        dr::Text(g, L10n::T(ui_.simple ? S::FnRestore : S::FnSimplify), Font(9.5f),
                 dr::C(ui_.simple ? 0xFFFFFF : theme::kTextMuted), L_.fnSimple, 1, true);

        bool hoverPdf = ui_.hoverFn == 3;
        dr::FillRound(g, dr::RF(L_.fnPdf), d(8),
                      dr::C(hoverPdf ? theme::kHover : theme::kPanelAlt));
        dr::StrokeRound(g, dr::RF(L_.fnPdf), d(8),
                        dr::C(hoverPdf ? theme::kBorderStrong : theme::kBorder), 1.f);
        dr::Text(g, ExportChipText(ui_.layer), Font(9.5f),
                 dr::C(theme::kTextMuted), L_.fnPdf, 1, true);

        // 预设: opens the preset modal (src/Presets.cpp)
        bool hoverPreset = ui_.hoverFn == 6;
        dr::FillRound(g, dr::RF(L_.fnPreset), d(8),
                      dr::C(hoverPreset ? theme::kHover : theme::kPanelAlt));
        dr::StrokeRound(g, dr::RF(L_.fnPreset), d(8),
                        dr::C(hoverPreset ? theme::kBorderStrong : theme::kBorder),
                        1.f);
        dr::Text(g, L10n::T(S::FnPreset), Font(9.5f), dr::C(theme::kTextMuted),
                 L_.fnPreset, 1, true);
    }

    // --- language dropdown ---------------------------------------------------
    // One rounded box showing the current language code and a small chevron;
    // clicking it opens the floating list that PaintOverlay draws.
    {
        const bool hover = (ui_.hoverLang == 0);
        const bool open = ui_.langOpen;
        Gdiplus::RectF box = dr::RF(L_.langBox);
        dr::FillRound(g, box, d(7),
                      dr::C(open ? theme::kAccentSoft : (hover ? theme::kHover : theme::kPanelAlt)));
        dr::StrokeRound(g, box, d(7),
                        dr::C(open ? theme::kAccent
                             : (hover ? theme::kBorderStrong : theme::kBorder)), 1.f);

        Gdiplus::Font* f = Font(9.5f, open);
        const std::wstring code = L10n::Code(L10n::Get());
        const float tw = dr::TextW(g, code, f);
        const int chevW = d(9);
        const int groupW = (int)tw + d(5) + chevW;
        const int startX = (L_.langBox.left + L_.langBox.right - groupW) / 2;
        RECT tr = {startX, L_.langBox.top, startX + (int)tw + d(4), L_.langBox.bottom};
        dr::Text(g, code, f,
                 dr::C((open || hover) ? theme::kText : theme::kTextMuted),
                 tr, 0, true);

        const float ax = (float)(startX + (int)tw + d(5) + chevW / 2);
        const float ay = (float)((L_.langBox.top + L_.langBox.bottom) / 2);
        const float hw = (float)d(4);
        const float hh = (float)d(2);
        const Gdiplus::Color ac((open || hover) ? dr::C(theme::kText) : dr::C(theme::kTextMuted));
        dr::Line(g, ax - hw, ay - hh, ax, ay + hh, ac, 1.3f);
        dr::Line(g, ax, ay + hh, ax + hw, ay - hh, ac, 1.3f);
    }

    // --- app lock (the ONLY entry point, far right of the language selector) --
    PaintLockChip(g);
}
