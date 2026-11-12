#include "App.h"
#include "Assets.h"
#include "Catalog.h"
#include "Draw.h"
#include "L10n.h"
#include "Theme.h"

#include <cmath>
#include <cwctype>
#include <string>
#include <vector>

namespace {

std::wstring Lower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)std::towlower((wint_t)c);
    return s;
}

bool Match(const CatItem& it, const std::wstring& q) {
    if (q.empty()) return true;
    std::wstring qq = Lower(q);
    return Lower(it.zh).find(qq) != std::wstring::npos ||
           Lower(it.en).find(qq) != std::wstring::npos;
}

struct Block {
    const CatGroup* grp;
    std::vector<int> items;
};

} // namespace

void App::PaintLeft(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    dr::Fill(g, L_.left, dr::C(theme::kPanel));
    dr::Line(g, (float)(L_.left.right - 1), (float)L_.left.top,
             (float)(L_.left.right - 1), (float)L_.left.bottom,
             dr::C(theme::kBorder), 1.f);

    // --- header -------------------------------------------------------------
    dr::Text(g, L10n::T(S::Library), Font(12.5f, true), dr::C(theme::kText),
             L_.libTitle, 0, true);

    const auto& items = ui_.libTab == 0 ? Instruments() : Mics();
    dr::Text(g, std::to_wstring(items.size()), Font(9.f), dr::C(theme::kTextFaint),
             L_.libCount, 2, true);

    // --- segmented tabs ------------------------------------------------------
    RECT track = {L_.libTabs[0].left, L_.libTabs[0].top,
                  L_.libTabs[1].right, L_.libTabs[0].bottom};
    dr::FillRound(g, dr::RF(track), d(8), dr::C(theme::kPanelAlt));
    dr::StrokeRound(g, dr::RF(track), d(8), dr::C(theme::kBorder), 1.f);

    const S tabIds[2] = {S::TabInstruments, S::TabMics};
    for (int i = 0; i < 2; ++i) {
        if (ui_.libTab == i) {
            RECT r = L_.libTabs[i];
            r.left += d(2);
            r.right -= d(2);
            r.top += d(2);
            r.bottom -= d(2);
            dr::FillRound(g, dr::RF(r), d(6), dr::C(0xFFFFFF));
            dr::StrokeRound(g, dr::RF(r), d(6), dr::C(theme::kBorder), 1.f);
        }
        Gdiplus::Color c = ui_.libTab == i ? dr::C(theme::kText)
                          : ui_.hoverLibTab == i ? dr::C(theme::kTextMuted)
                                                 : dr::C(theme::kTextMuted);
        dr::Text(g, L10n::T(tabIds[i]), Font(10.5f, ui_.libTab == i), c,
                 L_.libTabs[i], 1, true);
    }

    // --- search --------------------------------------------------------------
    bool searchFocus = ui_.focusEdit == ID_SEARCH;
    dr::FillRound(g, dr::RF(L_.searchBox), d(6), dr::C(theme::kField));
    dr::StrokeRound(g, dr::RF(L_.searchBox), d(6),
                    dr::C(searchFocus ? theme::kAccent : theme::kBorder), searchFocus ? 1.5f : 1.f);
    {
        float cx = (float)L_.searchBox.left + d(14);
        float cy = (float)(L_.searchBox.top + L_.searchBox.bottom) * 0.5f;
        dr::Circle(g, cx, cy, d(4.2f), dr::C(theme::kTextMuted), d(1.2f));
        dr::Line(g, cx + d(3.f), cy + d(3.f), cx + d(6.5f), cy + d(6.5f),
                 dr::C(theme::kTextMuted), d(1.2f));
    }

    dr::Line(g, (float)L_.libView.left, (float)(L_.libView.top - d(8)),
             (float)L_.libView.right, (float)(L_.libView.top - d(8)),
             dr::C(theme::kBorder), 1.f);

    // --- layer 03 hint strip --------------------------------------------------
    // The palette stays fully usable on the cabling layer, this strip only
    // explains how devices are added and moved.
    if (ui_.layer == 2 && L_.cbLeftHint.bottom > L_.cbLeftHint.top) {
        dr::Line(g, (float)L_.cbLeftHint.left, (float)(L_.cbLeftHint.top - d(6)),
                 (float)L_.cbLeftHint.right, (float)(L_.cbLeftHint.top - d(6)),
                 dr::C(theme::kBorder), 1.f);
        dr::FillRound(g, Gdiplus::RectF((float)L_.cbLeftHint.left,
                                       (float)(L_.cbLeftHint.top + d(4)),
                                       (float)d(3), (float)d(9)),
                      d(1.5f), dr::C(theme::kAccent, 200));
        dr::TextWrap(g, L10n::T(S::CbLeftHint), Font(9.f), dr::C(theme::kTextMuted),
                     RECT{L_.cbLeftHint.left + d(10), L_.cbLeftHint.top,
                          L_.cbLeftHint.right, L_.cbLeftHint.bottom},
                     0);
    }

    // --- catalogue ------------------------------------------------------------
    const auto& groups = ui_.libTab == 0 ? InstrumentGroups() : MicGroups();
    std::vector<Block> blocks;
    for (const auto& gr : groups) {
        Block b{&gr, {}};
        for (int i = 0; i < (int)items.size(); ++i) {
            if (std::wstring(items[i].group) == gr.id && Match(items[i], ui_.search))
                b.items.push_back(i);
        }
        if (!b.items.empty()) blocks.push_back(std::move(b));
    }

    const int viewH = L_.libView.bottom - L_.libView.top;
    const int viewW = L_.libView.right - L_.libView.left;
    if (viewH <= 0 || viewW <= 0) return;

    const int tileW = (viewW - d(8)) / 2;
    const int tileH = d(80);
    const int rowGap = d(8);
    const int headerH = d(27);

    double total = 0;
    for (const auto& b : blocks) {
        total += headerH;
        int rows = ((int)b.items.size() + 1) / 2;
        total += rows * (tileH + rowGap);
    }

    double maxScroll = std::max(0.0, total - viewH);
    if (ui_.scroll > maxScroll) ui_.scroll = maxScroll;
    if (ui_.scroll < 0) ui_.scroll = 0;

    Gdiplus::GraphicsState st = g.Save();
    g.SetClip(Gdiplus::RectF((float)L_.libView.left, (float)L_.libView.top,
                             (float)viewW, (float)viewH));

    tiles_.clear();

    if (blocks.empty()) {
        RECT r = {L_.libView.left, L_.libView.top + d(24), L_.libView.right,
                  L_.libView.top + d(48)};
        dr::Text(g, L10n::T(S::NoResults), Font(10.f), dr::C(theme::kTextFaint),
                 r, 1, true);
        g.Restore(st);
        return;
    }

    double y = L_.libView.top - ui_.scroll;
    for (const auto& b : blocks) {
        // group header
        RECT hr = {(LONG)L_.libView.left, (LONG)std::lround(y),
                   (LONG)L_.libView.right, (LONG)std::lround(y + headerH)};
        std::wstring gname = GroupName(*b.grp);
        dr::Text(g, gname, Font(8.5f), dr::C(theme::kTextMuted),
                 RECT{hr.left + d(2), hr.top, hr.right, hr.bottom}, 0, true);
        float tw = dr::TextW(g, gname, Font(8.5f));
        float ly = (float)hr.top + headerH * 0.5f;
        dr::Line(g, (float)hr.left + d(2) + tw + d(10), ly,
                 (float)hr.right - d(2), ly, dr::C(theme::kBorder), 1.f);
        y += headerH;

        for (size_t i = 0; i < b.items.size(); i += 2) {
            for (int col = 0; col < 2 && i + col < b.items.size(); ++col) {
                int idx = b.items[i + col];
                RECT tr = {(LONG)(L_.libView.left + col * (tileW + d(8))),
                           (LONG)std::lround(y),
                           (LONG)(L_.libView.left + col * (tileW + d(8)) + tileW),
                           (LONG)std::lround(y + tileH)};
                tiles_.push_back({tr, idx});

                bool hover = ui_.hoverTile == idx;
                bool pressed = ui_.dragCat_ == idx && ui_.dragTab_ == ui_.libTab;
                if (hover || pressed) {
                    RECT h = tr;
                    h.left += d(2);
                    h.right -= d(2);
                    h.top += d(2);
                    h.bottom -= d(2);
                    dr::FillRound(g, dr::RF(h), d(9), dr::C(theme::kHover));
                }

                const CatItem& it = items[idx];
                bool disabled = it.id == std::wstring(L"mainmic") && pr_.HasMainMicItem();
                Rgb ink{(BYTE)((theme::kText >> 16) & 0xFF),
                        (BYTE)((theme::kText >> 8) & 0xFF),
                        (BYTE)(theme::kText & 0xFF)};
                if (disabled) ink = Rgb{0xCF, 0xCB, 0xC3};
                Gdiplus::Bitmap* bmp =
                    Assets::Get().TintIcon(it.id, ui_.libTab == 1, ink);
                if (bmp) {
                    float bw = (float)bmp->GetWidth();
                    float bh = (float)bmp->GetHeight();
                    float box = (float)d(40);
                    float s = std::min(box / bw, box / bh);
                    float w = bw * s, h = bh * s;
                    Gdiplus::RectF dst((float)(tr.left + tr.right) * 0.5f - w * 0.5f,
                                       (float)tr.top + (float)d(10) + (box - h) * 0.5f,
                                       w, h);
                    dr::Icon(g, bmp, dst);
                    if (it.id == std::wstring(L"mic")) {
                        Rgb dk{(BYTE)(ink.r * 0.45), (BYTE)(ink.g * 0.45),
                               (BYTE)(ink.b * 0.45)};
                        Gdiplus::Bitmap* ln = Assets::Get().TintIcon(
                            L"micline", ui_.libTab == 1, dk);
                        if (ln) dr::Icon(g, ln, dst);
                    }
                }

                RECT lr = {tr.left + d(4), tr.top + d(50), tr.right - d(4), tr.top + d(72)};
                dr::Text(g, ItemName(it), Font(10.f),
                         (hover || pressed) ? dr::C(theme::kText) : dr::C(theme::kTextMuted),
                         lr, 1, true);
            }
            y += tileH + rowGap;
        }
    }

    g.Restore(st);

    // --- scrollbar -------------------------------------------------------------
    if (total > viewH) {
        double thumbH = std::max(24.0, (double)viewH * viewH / total);
        double travel = viewH - thumbH;
        double t = maxScroll > 0 ? (ui_.scroll / maxScroll) : 0.0;
        float x = (float)(L_.left.right - d(10));
        Gdiplus::RectF bar(x, (float)L_.libView.top, (float)d(3), (float)viewH);
        dr::FillRound(g, bar, d(2), dr::C(theme::kBorder, 120));
        Gdiplus::RectF thumb(x, (float)(L_.libView.top + travel * t), (float)d(3),
                             (float)thumbH);
        dr::FillRound(g, thumb, d(2), dr::C(theme::kTextFaint));
    }
}
