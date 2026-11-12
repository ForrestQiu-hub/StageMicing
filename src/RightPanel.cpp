#include "App.h"
#include "Catalog.h"
#include "Draw.h"
#include "L10n.h"
#include "Theme.h"

#include <cmath>

// The painted chrome of one labelled field box.  The right inspector and the
// 项目信息 page both call it, so a field looks identical on both pages and the
// EDIT always sits in the box this draws (FieldBox::box, the same rect the EDIT
// rect is derived from in App::FieldRect).
void App::PaintFieldBox(Gdiplus::Graphics& g, const FieldBox& f, int ctrlId,
                        bool enabled) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    if (f.box.right <= f.box.left || f.box.bottom <= f.box.top) return;
    const bool focus = (ctrlId != 0 && ui_.focusEdit == ctrlId);
    dr::FillRound(g, dr::RF(f.box), (float)d(4),
                  dr::C(enabled ? theme::kField : theme::kPanelAlt));
    dr::StrokeRound(g, dr::RF(f.box), (float)d(4),
                    dr::C(focus ? theme::kAccent
                         : enabled ? theme::kBorderStrong : theme::kBorder),
                    focus ? 1.4f : 1.f);
}

void App::PaintRight(Gdiplus::Graphics& g) {
    // layer 03 (话筒拉线) has its own inspector: devices instead of the mic panel
    if (ui_.layer == 2) {
        PaintCablingRight(g);
        return;
    }
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    dr::Fill(g, L_.right, dr::C(theme::kPanel));
    dr::Line(g, (float)L_.right.left, (float)L_.right.top,
             (float)L_.right.left, (float)L_.right.bottom,
             dr::C(theme::kBorder), 1.f);

    // layer 5 (项目信息) has no inspector rows at all: 项目名称 / 场地 / 指挥 /
    // 系统 / 录音 / 助理 / 备注, 舞台尺寸 and 外观颜色 live in the centred panel on
    // the canvas.  The panel stays empty — and therefore never scrolls.
    if (ui_.layer == 5) return;

    // only the rules Layout actually placed: the 项目信息 / 舞台尺寸 / 外观颜色
    // sections left this panel for page 01
    for (int i = 0; i < 4; ++i) {
        if (L_.divider[i].right <= L_.divider[i].left) continue;
        dr::Line(g, (float)L_.divider[i].left, (float)L_.divider[i].top,
                 (float)L_.divider[i].right, (float)L_.divider[i].top,
                 dr::C(theme::kBorder), 1.f);
    }

    auto section = [&](const RECT& r, const wchar_t* txt) {
        if (r.right <= r.left) return;
        dr::FillRound(g, Gdiplus::RectF((float)r.left, (float)(r.top + d(3)),
                                       (float)d(3), (float)d(9)),
                      d(1.5f), dr::C(theme::kAccent, 200));
        dr::Text(g, txt, Font(9.f), dr::C(theme::kTextMuted),
                 RECT{r.left + d(10), r.top, r.right, r.bottom}, 0, true);
    };

    bool hasSel = pr_.Selected() != nullptr;

    auto fieldBox = [&](const FieldBox& f, int ctrlId, bool enabled) {
        PaintFieldBox(g, f, ctrlId, enabled);
    };
    auto numLabel = [&](const FieldBox& f, S id) {
        if (f.box.right <= f.box.left) return;
        dr::Text(g, dr::Fmt(L"%s (m)", L10n::T(id)), Font(9.f),
                 dr::C(theme::kTextMuted), f.label, 0, true);
    };

    // --- selection --------------------------------------------------------------
    section(L_.secSel, L10n::T(S::SectionSelection));
    fieldBox(L_.selName, ID_SELNAME, hasSel);
    dr::Text(g, L10n::T(S::FieldName), Font(9.f), dr::C(theme::kTextMuted), L_.selName.label, 0, true);

    fieldBox(L_.xField, ID_X, hasSel);
    fieldBox(L_.yField, ID_Y, hasSel);
    dr::Text(g, L10n::T(S::XLabel), Font(9.f), dr::C(theme::kTextMuted), L_.xField.label, 0, true);
    dr::Text(g, L10n::T(S::YLabel), Font(9.f), dr::C(theme::kTextMuted), L_.yField.label, 0, true);

    fieldBox(L_.angleField, ID_ANGLE, hasSel);
    dr::Text(g, L10n::T(S::AngleLabel), Font(9.f), dr::C(theme::kTextMuted), L_.angleField.label, 0, true);

    bool pianoSel = hasSel && pr_.Selected()->defId == L"grand_piano";
    if (pianoSel) {
        fieldBox(L_.lenField, ID_PIANOLEN, true);
        dr::Text(g, dr::Fmt(L"%s (m)", L10n::T(S::FieldPianoLength)), Font(9.f),
                 dr::C(theme::kTextMuted), L_.lenField.label, 0, true);
    }
    bool micSel = hasSel && pr_.Selected()->kind == ItemKind::Mic;
    // an intensity-difference technique object (XY / MS / Blumlein) is ONE object
    // with TWO capsule models: it shows 型号 L / 型号 R (M / S for MS) instead of
    // the single 型号 field, and it has no 间距 row at all
    const bool techObj = micSel && IsIntensityTech(pr_.Selected()->defId);
    if (micSel && pr_.Selected()->defId != L"mainmic" && !techObj) {
        fieldBox(L_.micModel, ID_MICMODEL, true);
        dr::Text(g, L10n::T(S::FieldModel), Font(9.f), dr::C(theme::kTextMuted),
                 L_.micModel.label, 0, true);
    }
    if (techObj) {
        const bool ms = pr_.Selected()->defId == L"ms";
        fieldBox(L_.modelL, ID_MODELL, true);
        fieldBox(L_.modelR, ID_MODELR, true);
        dr::Text(g, L10n::T(ms ? S::ModelM : S::ModelL), Font(8.5f),
                 dr::C(theme::kTextMuted), L_.modelL.label, 0, true);
        dr::Text(g, L10n::T(ms ? S::ModelS : S::ModelR), Font(8.5f),
                 dr::C(theme::kTextMuted), L_.modelR.label, 0, true);
    }
    if (micSel) {
        fieldBox(L_.micHeight, ID_MICHEIGHT, true);
        dr::Text(g, L10n::T(S::FieldMicHeight), Font(9.f),
                 dr::C(theme::kTextMuted), L_.micHeight.label, 0, true);
    }
    if (micSel && L_.flownRow.right > L_.flownRow.left) {
        const Item* mi = pr_.Selected();
        const bool on = mi && mi->flown;
        // hover uses the generic row slot (9 is the 显示文字 row on this panel)
        if (ui_.hoverRow == 10)
            dr::FillRound(g, dr::RF(L_.flownRow), d(5), dr::C(theme::kHover));
        dr::FillRound(g, dr::RF(L_.flownCheck), d(3.5f),
                      dr::C(on ? theme::kAccent : theme::kField));
        dr::StrokeRound(g, dr::RF(L_.flownCheck), d(3.5f),
                        dr::C(on ? theme::kAccent : theme::kBorderStrong), 1.f);
        if (on) {
            const RECT& cb = L_.flownCheck;
            float x0 = (float)cb.left + d(3.5f);
            float y0 = (float)cb.top + d(8.f);
            float x1 = (float)cb.left + d(6.8f);
            float y1 = (float)cb.top + d(11.2f);
            float x2 = (float)cb.right - d(3.5f);
            float y2 = (float)cb.top + d(4.5f);
            dr::Line(g, x0, y0, x1, y1, dr::C(0xFFFFFF), 1.4f);
            dr::Line(g, x1, y1, x2, y2, dr::C(0xFFFFFF), 1.4f);
        }
        dr::Text(g, L10n::T(S::FieldFlown), Font(10.f), dr::C(theme::kText),
                 RECT{L_.flownCheck.right + d(8), L_.flownRow.top, L_.flownRow.right,
                      L_.flownRow.bottom},
                 0, true);
        if (L_.flyField.box.right > L_.flyField.box.left) {
            fieldBox(L_.flyField, ID_FLYH, true);
            numLabel(L_.flyField, S::FieldFlyHeight);
        }
    }
    // ---- 颜色: the per-item tint override (instruments / 声部 targets) ---------
    if (L_.itemColorRow.right > L_.itemColorRow.left && hasSel &&
        pr_.Selected()->kind == ItemKind::Instrument) {
        const Item* cit = pr_.Selected();
        if (ui_.hoverRow == 12)
            dr::FillRound(g, dr::RF(L_.itemColorRow), d(5), dr::C(theme::kHover));
        dr::Text(g, L10n::T(S::Color), Font(9.f), dr::C(theme::kTextMuted),
                 L_.itemColorLabel, 0, true);
        // the swatch IS the current tint: the override when set, else the palette
        // slot this defId owns — so the row never lies about what is drawn
        const Rgb tint = ItemIconTint(pr_, *cit, true);
        dr::FillRound(g, dr::RF(L_.itemColorSwatch), d(4), dr::C(tint));
        dr::StrokeRound(g, dr::RF(L_.itemColorSwatch), d(4),
                        dr::C(cit->colorOverride ? theme::kAccent : theme::kBorderStrong),
                        cit->colorOverride ? 1.4f : 1.f);
        dr::Text(g, dr::Fmt(L"#%02X%02X%02X", (int)tint.r, (int)tint.g, (int)tint.b),
                 Font(8.5f), dr::C(theme::kTextMuted), L_.itemColorHex, 0, true);
        if (L_.itemColorReset.right > L_.itemColorReset.left) {
            const bool rh = ui_.hoverRow == 13;
            dr::FillRound(g, dr::RF(L_.itemColorReset), d(5),
                          dr::C(rh ? theme::kHover : theme::kPanelAlt));
            dr::StrokeRound(g, dr::RF(L_.itemColorReset), d(5),
                            dr::C(rh ? theme::kBorderStrong : theme::kBorder), 1.f);
            dr::Text(g, L10n::T(S::ColorReset), Font(8.5f),
                     dr::C(rh ? theme::kText : theme::kTextMuted), L_.itemColorReset,
                     1, true);
        }
    }

    Item* spotMic = SpotMicInSel();
    std::wstring selInst = SelectedInstType();
    if (spotMic) {
        bool on = !spotMic->spotType.empty() && selInst.empty();
        bool hover = ui_.hoverDel == 2;
        // bind action = dark sage, unbind action = dark clay
        const DWORD kBindCol = 0x6E7F71;
        const DWORD kUnbindCol = 0x8A6252;
        DWORD col = on ? kUnbindCol : (hover ? 0x7A8B7D : kBindCol);
        dr::FillRound(g, dr::RF(L_.spotBtn), d(5), dr::C(col));
        dr::StrokeRound(g, dr::RF(L_.spotBtn), d(5), dr::C(col), 1.f);
        std::wstring lab;
        if (!selInst.empty()) {
            lab = (L10n::IsZh() ? L"绑定到 " : L"Bind to ") + SpotTypeName(selInst);
        } else {
            lab = L10n::T(on ? S::SpotClear : S::SpotMic);
        }
        dr::Text(g, lab, Font(9.f), dr::C(0xFFFFFF), L_.spotBtn, 1, true);
    }

    bool mainSel = hasSel && pr_.Selected()->defId == L"mainmic";
    if (mainSel) {
        const Item* m = pr_.Selected();
        const S tids[4] = {S::TechAB, S::TechMixed, S::TechIntensity, S::TechDecca};
        for (int i = 0; i < 4; ++i) {
            bool on = m->mainTech == i;
            bool hover = ui_.hoverTech == i;
            dr::FillRound(g, dr::RF(L_.techBtn[i]), d(5),
                          dr::C(on ? theme::kAccent : (hover ? theme::kHover : theme::kPanel)));
            dr::StrokeRound(g, dr::RF(L_.techBtn[i]), d(5),
                            dr::C(on ? theme::kAccent : theme::kBorder), 1.f);
            dr::Text(g, L10n::T(tids[i]), Font(8.f),
                     dr::C(on ? 0xFFFFFF : theme::kTextMuted), L_.techBtn[i], 1, true);
        }
        if (m->mainTech == 1) {
            const wchar_t* pl[3] = {L"ORTF", L"NOS", L"DIN"};
            for (int i = 0; i < 3; ++i) {
                dr::FillRound(g, dr::RF(L_.techPreset[i]), d(5), dr::C(theme::kPanel));
                dr::StrokeRound(g, dr::RF(L_.techPreset[i]), d(5), dr::C(theme::kBorder), 1.f);
                dr::Text(g, pl[i], Font(8.5f), dr::C(theme::kTextMuted),
                         L_.techPreset[i], 1, true);
            }
        }
        int pats[3] = {0, 0, 0};
        S plabels[3] = {S::PatOmni, S::PatOmni, S::PatOmni};
        int np = 0;
        if (m->mainTech == 0) {
            pats[0] = 0; plabels[0] = S::PatOmni; np = 1;
        } else if (m->mainTech == 1) {
            pats[0] = 4; plabels[0] = S::PatWideCard;
            pats[1] = 1; plabels[1] = S::PatCard; np = 2;
        } else if (m->mainTech == 2) {
            pats[0] = 1; plabels[0] = S::PatCard;
            pats[1] = 2; plabels[1] = S::PatSuper;
            pats[2] = 3; plabels[2] = S::PatFig8; np = 3;
        }
        for (int i = 0; i < np; ++i) {
            bool on = m->pattern == pats[i];
            bool hover = ui_.hoverPattern == i;
            dr::FillRound(g, dr::RF(L_.patternBtn[i]), d(5),
                          dr::C(on ? theme::kAccent : (hover ? theme::kHover : theme::kPanel)));
            dr::StrokeRound(g, dr::RF(L_.patternBtn[i]), d(5),
                            dr::C(on ? theme::kAccent : theme::kBorder), 1.f);
            dr::Text(g, L10n::T(plabels[i]), Font(8.5f),
                     dr::C(on ? 0xFFFFFF : theme::kTextMuted), L_.patternBtn[i], 1, true);
        }
        fieldBox(L_.spaceField, ID_MAINSPACE, m->mainTech != 2);
        dr::Text(g, L10n::T(S::FieldSpacing), Font(9.f),
                 dr::C(theme::kTextMuted), L_.spaceField.label, 0, true);
        if (m->mainTech == 2) {
            dr::Text(g, L"0.00", Font(9.f), dr::C(theme::kTextFaint),
                     L_.spaceField.box, 1, true);
        }
        if (m->mainTech == 3) {
            fieldBox(L_.centerField, ID_MAINCENTER, true);
            dr::Text(g, L10n::T(S::FieldCenter), Font(9.f),
                     dr::C(theme::kTextMuted), L_.centerField.label, 0, true);
        }
        fieldBox(L_.modelL, ID_MODELL, true);
        fieldBox(L_.modelR, ID_MODELR, true);
        dr::Text(g, L10n::T(S::ModelL), Font(8.5f), dr::C(theme::kTextMuted),
                 L_.modelL.label, 0, true);
        dr::Text(g, L10n::T(S::ModelR), Font(8.5f), dr::C(theme::kTextMuted),
                 L_.modelR.label, 0, true);
        if (m->mainTech == 3) {
            fieldBox(L_.modelC, ID_MODELC, true);
            dr::Text(g, L10n::T(S::ModelC), Font(8.5f), dr::C(theme::kTextMuted),
                     L_.modelC.label, 0, true);
        }
    }    if (hasSel) {
        const Item* it = pr_.Selected();
        std::wstring info;
        if (it->kind == ItemKind::Mic) {
            info = dr::Fmt(L"%s %.0f°   ·   %s %.2f m", L10n::T(S::DirLabel), it->angle,
                           L10n::T(S::FieldMicHeight), it->height);
            int g = pr_.GroupOf(it->id);
            if (g >= 0) {
                if (pr_.micGroups[g].kind == 1) {
                    info += L"   ·   Decca Tree";
                } else if (pr_.micGroups[g].kind == 2 &&
                           !pr_.micGroups[g].tech.empty()) {
                    // a 混合差 pair names its technique (ORTF / NOS / DIN)
                    info += L"   ·   " + SpotTypeName(pr_.micGroups[g].tech);
                } else {
                    int n = 0;
                    for (int k = 0; k <= g; ++k)
                        if (pr_.micGroups[k].kind == 0) ++n;
                    info += dr::Fmt(L"   ·   %s %d", L10n::IsZh() ? L"组" : L"Group", n);
                }
            }
            // 对应乐器: the instrument the microphone — or its WHOLE bound group —
            // is bound to, exactly what the 03 inspector and channel table print
            if (!it->spotType.empty()) {
                const std::wstring nm = SpotTypeName(it->spotType);
                info += dr::Fmt(L"   ·   %s %s", L10n::T(S::CbMiInst), nm.c_str());
            }
        } else {
            info = dr::Fmt(L"%.2f × %.2f m", it->w, it->h);
        }
        dr::Text(g, info, Font(8.5f), dr::C(theme::kTextMuted), L_.selInfo, 0, true);
    } else {
        dr::Text(g, L10n::T(S::NoSelection), Font(9.f), dr::C(theme::kTextFaint),
                 L_.selName.box, 1, true);
    }

    {
        bool hover = ui_.hoverDel == 1 && hasSel;
        dr::FillRound(g, dr::RF(L_.delBtn), d(5),
                      dr::C(hover ? 0xF6EFEC : theme::kPanel));
        dr::StrokeRound(g, dr::RF(L_.delBtn), d(5),
                        dr::C(hover ? 0xD9C7BF : theme::kBorder), 1.f);
        dr::Text(g, L10n::T(S::DeleteItem), Font(9.f),
                 dr::C(hasSel ? (hover ? theme::kDanger : theme::kTextMuted)
                              : theme::kTextFaint, hasSel ? 255 : 120),
                 L_.delBtn, 1, true);
    }
    {
        bool allMics = !pr_.selIds.empty();
        for (int id : pr_.selIds) {
            const Item* q = pr_.Find(id);
            if (!q || q->kind != ItemKind::Mic) allMics = false;
        }
        bool anyGroup = false;
        for (int id : pr_.selIds)
            if (pr_.GroupOf(id) >= 0) anyGroup = true;
        bool canBind = allMics && pr_.selIds.size() >= 2 && !anyGroup;
        // an intensity-difference technique object is ONE object: there is no
        // "unbind" for it, so the button stays inactive
        bool techSel = hasSel && ItemSRA(pr_.Selected()->defId) > 0.0 &&
                       !IsIntensityTech(pr_.Selected()->defId);
        bool inGroup = hasSel && pr_.GroupOf(pr_.Selected()->id) >= 0;
        bool active = canBind || inGroup || techSel;
        S lab = techSel ? S::UnbindMic : canBind ? S::LinkGroup
                            : inGroup ? S::UnbindMic : S::BindMic;
        dr::FillRound(g, dr::RF(L_.bindBtn), d(5),
                      dr::C(active ? (canBind ? theme::kAccent : theme::kPanel)
                                   : theme::kPanel));
        dr::StrokeRound(g, dr::RF(L_.bindBtn), d(5),
                        dr::C(active ? (canBind ? theme::kAccent : theme::kBorderStrong)
                                     : theme::kBorder), 1.f);
        dr::Text(g, L10n::T(lab), Font(9.f),
                 dr::C(canBind ? 0xFFFFFF
                      : active ? theme::kTextMuted : theme::kTextFaint,
                      active ? 255 : 120),
                 L_.bindBtn, 1, true);

        // Decca Tree button
        bool allMicsT = !pr_.selIds.empty();
        for (int id : pr_.selIds) {
            const Item* q = pr_.Find(id);
            if (!q || q->kind != ItemKind::Mic) allMicsT = false;
        }
        bool canTree = allMicsT && pr_.selIds.size() >= 3 && !pr_.HasDeccaTree();
        dr::FillRound(g, dr::RF(L_.treeBtn), d(5),
                      dr::C(canTree ? theme::kAccent : theme::kPanel));
        dr::StrokeRound(g, dr::RF(L_.treeBtn), d(5),
                        dr::C(canTree ? theme::kAccent : theme::kBorder), 1.f);
        dr::Text(g, L10n::T(S::DeccaTree), Font(9.f),
                 dr::C(canTree ? 0xFFFFFF : theme::kTextFaint, canTree ? 255 : 120),
                 L_.treeBtn, 1, true);
    }

    // 外观颜色 moved to page 01 项目信息 (it belongs to the project, not to a
    // selected object), so the inspector ends with 图层 and stays scroll-free.

    // --- 同类乐器: 「全选 <类型> (N)」 -------------------------------------------
    if (L_.secSameType.right > L_.secSameType.left) {
        section(L_.secSameType, L10n::T(S::SectionSameType));
        for (size_t si = 0; si < L_.sameTypeBtn.size(); ++si) {
            const RECT& br = L_.sameTypeBtn[si];
            const int n = (int)std::count_if(
                pr_.items.begin(), pr_.items.end(), [&](const Item& q) {
                    return q.kind == ItemKind::Instrument &&
                           q.defId == L_.sameTypeDef[si];
                });
            const std::wstring nm = SpotTypeName(L_.sameTypeDef[si]);
            bool hover = PtInRect(&br, ui_.dragPt_);
            dr::FillRound(g, dr::RF(br), d(5),
                          dr::C(hover ? theme::kHover : theme::kPanel));
            dr::StrokeRound(g, dr::RF(br), d(5), dr::C(theme::kBorder), 1.f);
            dr::Text(g, dr::Fmt(L10n::T(S::SameTypeBtnFmt), nm.c_str(), n),
                     Font(9.f), dr::C(theme::kTextMuted), br, 1, true);
        }
    }

    // --- layers ------------------------------------------------------------------
    section(L_.secLayers, L10n::T(S::SectionLayers));
    bool checks[4] = {pr_.showInstruments, pr_.showMics, pr_.showInstrumentText,
                      pr_.showMicText};
    const S lids[4] = {S::ShowInstruments, S::ShowMics, S::ShowInstrText,
                       S::ShowMicText};
    for (int i = 0; i < 4; ++i) {
        RECT row = (i < 2) ? L_.layerRow[i] : L_.textRow[i - 2];
        RECT cb = (i < 2) ? L_.layerCheck[i] : L_.textCheck[i - 2];
        bool hover = (i < 2) ? ui_.hoverRow == i : ui_.hoverRow == 9 + (i - 2);
        if (hover) {
            RECT r = row;
            r.left -= d(6);
            r.right += d(6);
            dr::FillRound(g, dr::RF(r), d(5), dr::C(theme::kHover));
        }
        dr::FillRound(g, dr::RF(cb), d(3.5f),
                      dr::C(checks[i] ? theme::kAccent : theme::kField));
        dr::StrokeRound(g, dr::RF(cb), d(3.5f),
                        dr::C(checks[i] ? theme::kAccent : theme::kBorderStrong), 1.f);
        if (checks[i]) {
            float x0 = (float)cb.left + d(3.5f);
            float y0 = (float)cb.top + d(8.f);
            float x1 = (float)cb.left + d(6.8f);
            float y1 = (float)cb.top + d(11.2f);
            float x2 = (float)cb.right - d(3.5f);
            float y2 = (float)cb.top + d(4.5f);
            dr::Line(g, x0, y0, x1, y1, dr::C(0xFFFFFF), 1.4f);
            dr::Line(g, x1, y1, x2, y2, dr::C(0xFFFFFF), 1.4f);
        }
        RECT lr = {(LONG)(cb.right + d(8)), (LONG)row.top,
                   (LONG)row.right, (LONG)row.bottom};
        dr::Text(g, L10n::T(lids[i]), Font(10.f), dr::C(theme::kText), lr, 0, true);
    }
}
