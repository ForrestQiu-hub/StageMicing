#include "App.h"
#include "Draw.h"
#include "L10n.h"
#include "Theme.h"

#include <cmath>

void App::PaintStatus(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    dr::Fill(g, L_.status, dr::C(theme::kPanel));
    dr::Line(g, 0.f, (float)L_.status.top, (float)cw_, (float)L_.status.top,
             dr::C(theme::kBorder), 1.f);

    std::wstring left =
        dr::Fmt(L"%s  %.1f × %.1f m", L10n::T(S::StatusStage), pr_.stage.w,
                pr_.stage.d) +
        L"     ·     " +
        L"     ·     " +
        dr::Fmt(L10n::T(S::MetricCount), (int)pr_.items.size());

    RECT lr = {d(22), L_.status.top, (LONG)(cw_ * 0.44), L_.status.bottom};
    dr::Text(g, left, Font(9.f), dr::C(theme::kTextMuted), lr, 0, true);

    // ---- middle: transient messages > hovered-object info > hint -----------
    std::wstring hint;
    Gdiplus::Color hintCol = dr::C(theme::kTextFaint);
    if (!ui_.msg.empty() && GetTickCount() < ui_.msgUntil) {
        hint = ui_.msg;
        hintCol = dr::C(ui_.msgWarn ? theme::kDanger : theme::kTextMuted);
    } else if (ui_.layer == 2 && ui_.hoverCable >= 0 &&
               ui_.hoverCable < (int)pr_.links.size()) {
        // hovering a cable explains it, including a capacity or length problem
        const CableLink& ln = pr_.links[(size_t)ui_.hoverCable];
        const CableEnd a = ResolveCableEnd(pr_, ln.aKind, ln.a);
        const CableEnd b = ResolveCableEnd(pr_, ln.bKind, ln.b);
        std::wstring why;
        if (CableProblemText(pr_, ln, &why)) {
            hintCol = dr::C(theme::kDanger);
        } else if (ln.radio) {
            // a radio link has no length: only its channels
            why = dr::Fmt(L10n::T(S::CbRadioTagFmt), ln.channels);
        } else if (CableOverrideLen(pr_, ln) > 0.0) {
            // a manual length: shown verbatim on every page
            why = dr::Fmt(L10n::T(S::CbCableTagFmt), ln.channels,
                          CableOverrideLen(pr_, ln)) +
                  std::wstring(L"  ") + L10n::T(S::CbCustom);
        } else if (ln.flown) {
            // flown cable: the four legs, composed — no reserve, no slack
            why = dr::Fmt(L10n::T(S::CbFlyComposeFmt), ln.legUp, ln.legCeil,
                          ln.legWall, ln.legFloor,
                          FlownRunLength(ln.legUp, ln.legCeil, ln.legWall, ln.legFloor));
        } else {
            // 「高度 1.5 + 地面 7.4 = 8.9 m」 — the real cable, composed.  A converged
            // group adds its collector stubs between the height and the main run.
            const double collectors = CollectorLen(ln);
            if (collectors > 0.0)
                why = dr::Fmt(L10n::T(S::CbLenConvergeFmt), CableMicHeight(pr_, ln),
                              collectors, CablePolylineLen(ln), CableRawLen(pr_, ln));
            else
                why = dr::Fmt(L10n::T(S::CbLenComposeFmt), CableMicHeight(pr_, ln),
                              CablePolylineLen(ln), CableRawLen(pr_, ln));
        }
        hint = a.name + L"  →  " + b.name + L"     ·     " + why;
    } else if (ui_.layer == 3 && ui_.hoverChRow >= 0 && pr_.topo.valid) {
        // channel table: hovering a row explains its 链路 / 长度, and flags a
        // link that breaks the channel-capacity rule
        const std::vector<ChRow> rows = BuildChannelRows();
        if (ui_.hoverChRow < (int)rows.size()) {
            const ChRow& r = rows[(size_t)ui_.hoverChRow];
            const CableLink* ln = FindMicCableForItem(pr_, pr_.topo.links, r.itemId);
            std::wstring why;
            if (ln && CableProblemText(pr_, *ln, &why)) {
                hint = why;
                hintCol = dr::C(theme::kDanger);
            } else if (!r.link.empty()) {
                hint = r.link;
                // 03 shows the RESERVED length: raw → standard, or the manual one
                if (r.len > 0.0) {
                    hint += L"     ·     " + dr::Fmt(L"%.1f m", r.len);
                    if (r.custom)
                        hint += L"     ·     " + std::wstring(L10n::T(S::CbCustom));
                }
            }
        }
    } else if (ui_.layer == 5) {
        // 项目信息: the page has no canvas objects, so the hint names its sections
        hint = std::wstring(L10n::T(S::SectionProject)) + L"     ·     " +
               L10n::T(S::SectionStage) + L"     ·     " +
               L10n::T(S::SectionAppearance);
    } else {
        hint = L10n::T(S::StatusHint);
    }
    RECT mr = {(LONG)(cw_ * 0.30), L_.status.top, (LONG)(cw_ * 0.78), L_.status.bottom};
    dr::Text(g, hint, Font(9.f), hintCol, mr, 1, true);

    RECT rr = {(LONG)(cw_ - d(240)), L_.status.top, (LONG)(cw_ - d(22)),
               L_.status.bottom};
    dr::Text(g, std::wstring(L"100%   ·   ") + L10n::T(S::StatusFit), Font(9.f),
             dr::C(theme::kTextFaint), rr, 2, true);
}
