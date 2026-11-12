#include "App.h"
#include "Catalog.h"
#include "Draw.h"
#include "L10n.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

// layout diagnostics: one line per placed label/pill (defined in StageView.cpp,
// printed by --dump and by the export's SM_PNG_TEXTLOG)
extern std::vector<std::wstring> g_micStackDump;

// ---------------------------------------------------------------------------
// Layer 03 — 话筒拉线 (cabling).
//
// The device objects of the cabling model: the 「接口箱 / Stagebox」, the sound
// card (a hub: unlimited inputs) and the wireless receiver.  There is one box
// kind, no protocol and no port budget — the only capacity rule is the plain
// 输入通道数 of the device against what the wiring actually delivers.
// ---------------------------------------------------------------------------

namespace {

// The PNG export draws its text scaleMul× larger (App::Font multiplies the em
// size and sets dr::g_textLayoutBoost).  Every pill, badge or box that HOSTS that
// text — and every placement step measured against it — has to be enlarged by the
// same factor: with screen-sized pills a boosted tag spills out of its white box
// and two tags pile up instead of dodging each other.  At 1× (the screen) this is
// exactly 1, so the on-screen page stays untouched.
float CablingBoost() {
    return (dr::g_textLayoutBoost > 1.001f) ? dr::g_textLayoutBoost : 1.f;
}

// UI pending state: 0 = none, 1 = device, 2 = microphone.  The model uses
// 0 = device / 1 = mic for cable endpoints, hence the offset.
int ModelEndpointKind(int uiKind) { return uiKind == 2 ? 1 : 0; }

// microphone marker radius (device px).  The cable router uses the same value in
// metres (App::MicRadiusM) so a cable really ends on the circle.
const double kMicMarkerR = 8.0;

// DRAWN footprint in metres — shared with the router (Model.h) so the cable lands
// exactly on the rectangle boundary instead of 0.65 m in front of it.
const double kDevW = kCableDevW;
const double kDevH = kCableDevH;

bool DeviceRects(const Device& dv, double ppm, double ox, double oy, int padX,
                 RECT& box, RECT& lbl) {
    if (ppm <= 0.0) return false;
    const double w = (std::max)(18.0, kDevW * ppm);
    const double h = (std::max)(15.0, kDevH * ppm);
    const double cx = ox + dv.x * ppm;
    const double cy = oy + dv.y * ppm;
    box.left = (LONG)std::lround(cx - w * 0.5);
    box.top = (LONG)std::lround(cy - h * 0.5);
    box.right = (LONG)std::lround(cx + w * 0.5);
    box.bottom = (LONG)std::lround(cy + h * 0.5);
    lbl.top = box.bottom + 1;
    const LONG cxi = (box.left + box.right) / 2;
    const LONG half = (std::max)((box.right - box.left) / 2 + padX, (LONG)padX * 8);
    lbl.left = cxi - half;
    lbl.right = cxi + half;
    lbl.bottom = box.bottom + (LONG)(padX * 2 + 10);
    return true;
}

Gdiplus::Color DevInk(DevKind k) {
    switch (k) {
    case DevKind::Mic: return dr::C(0x554C42);
    case DevKind::AnalogBox: return dr::C(0x554C42);
    case DevKind::DigitalBox: return dr::C(0x554C42);   // merged into the stagebox
    case DevKind::SoundCard: return dr::C(0x8A6252);
    case DevKind::Receiver: return dr::C(0x6A5F7A);
    }
    return dr::C(theme::kText);
}

// the three "add device" buttons, in paint order: 接口箱 / 声卡 / 无线接收器
DevKind AddKindAt(int i) {
    switch (i) {
    case 0: return DevKind::AnalogBox;
    case 1: return DevKind::SoundCard;
    default: return DevKind::Receiver;
    }
}

// Full, unambiguous device name for the canvas tag / device list / inspector.
std::wstring DevLabelText(const Device& dv) {
    return dv.label.empty() ? DeviceKindTitle(dv.kind) : dv.label;
}

// Every cable of this microphone.  A grouped microphone is wired ONCE for the
// whole MicGroup and that one cable may hang off any member, so every member
// resolves to it (the 05 inspector shows the group's single cable).
std::vector<int> MicCableIndices(const Project& pr, const Item& mic) {
    std::vector<int> members{mic.id};
    const int g = pr.GroupOf(mic.id);
    if (g >= 0 && pr.micGroups[(size_t)g].ids.size() >= 2)
        members = pr.micGroups[(size_t)g].ids;
    std::vector<int> out;
    for (size_t i = 0; i < pr.links.size(); ++i) {
        const CableLink& ln = pr.links[i];
        const int mid = (ln.aKind == 1) ? ln.a : (ln.bKind == 1 ? ln.b : -1);
        if (mid < 0) continue;
        if (std::find(members.begin(), members.end(), mid) != members.end())
            out.push_back((int)i);
    }
    return out;
}

// REAL cable length of one drawn run (05 布线): a manual override wins, otherwise
// the microphone height plus that run's floor polyline, or the four flown legs.
// A CONVERGED group cable draws ONE main run plus its collector stubs, so the tag
// of that run is the whole cable (stubs included) — see CollectorLen.
double RunRealLen(const Project& pr, const CableLink& ln,
                  const std::vector<std::pair<double, double>>& run) {
    const double ov = CableOverrideLen(pr, ln);
    if (ov > 0.0) return ov;
    if (ln.flown) return CableRawLen(pr, ln);
    if (CollectorLen(ln) > 0.0) return CableRawLen(pr, ln);
    return CableMicHeight(pr, ln) + PolyLen(run);
}

} // namespace

// ---------------------------------------------------------------------------
// the drawn microphone marker — and the same radius in metres, which the cable
// router uses so every cable terminal sits exactly on the circle edge
// ---------------------------------------------------------------------------
int App::MicMarkerRadiusPx() const {
    return (int)std::lround(kMicMarkerR * dpi_ * uiScale_);
}

// per-microphone marker radius: the 03 circle is the point mic's drawn symbol, so
// it uses the SAME factor as the cable terminal (MicMarkerScaleOf): the main mic
// 1.3×, a point mic 0.5 × 1.7 = 0.85, the lavalier 1.75× of that.  The line ends
// exactly on the circle it is drawn around.
int App::MicMarkerRadiusPx(const Item& it) const {
    const double s = MicMarkerScaleOf(pr_, it.id);
    return (int)std::lround(kMicMarkerR * s * dpi_ * uiScale_);
}
// the DRAWN radius as an exact float: the cable terminal radius is this length in
// metres (MicRadiusM × the mic's marker factor), so the two can never disagree by
// a rounding pixel and the line really stops ON the circle
double App::MicMarkerRadiusF(const Item& it) const {
    const double s = MicMarkerScaleOf(pr_, it.id);
    return kMicMarkerR * s * dpi_ * uiScale_;
}
double App::MicRadiusM() const {
    if (ppm_ <= 1e-6) return kCableMicRadius;
    // unrounded: the router multiplies by the mic's own marker factor, and the
    // drawn circle is that same product, so the terminal sits on the edge
    return (kMicMarkerR * dpi_ * uiScale_) / ppm_;
}

// ---------------------------------------------------------------------------
// stage painting (only devices on this layer)
// ---------------------------------------------------------------------------
void App::PaintDevices(Gdiplus::Graphics& g) {
    // the box itself is a WORLD size (DeviceRects), but the text it hosts — the
    // kind badge, the overload badge, the name under it — is drawn at the export
    // text boost, so its slots and paddings scale with the same factor
    const float gb = CablingBoost();
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_ * gb); };

    for (const auto& dv : pr_.devices) {
        RECT box{}, lbl{};
        if (!DeviceRects(dv, ppm_, ox_, oy_, d(8), box, lbl)) continue;
        if (box.right < L_.canvas.left || box.left > L_.canvas.right) continue;
        if (box.bottom < L_.canvas.top || box.top > L_.canvas.bottom) continue;

        const Gdiplus::Color ink = DevInk(dv.kind);
        const bool sel = dv.selected;
        const bool hover = (ui_.hoverCbDevice == dv.id);
        {
            // diagnostics: the tag layout must never cover a device box or its
            // name (the same text band the layout uses as a hard obstacle)
            extern std::vector<std::wstring> g_micStackDump;
            const float ntw = dr::TextW(g, DevLabelText(dv), Font(8.5f));
            const LONG bcx = (box.left + box.right) / 2;
            const LONG nh = (LONG)(ntw * 0.5f) + d(2);
            g_micStackDump.push_back(dr::Fmt(
                L"devbox <%s> %ld %ld %ld %ld %ld %ld %ld %ld",
                DevLabelText(dv).c_str(), box.left, box.top, box.right, box.bottom,
                bcx - nh, box.bottom + d(2), bcx + nh, box.bottom + d(17)));
        }

        dr::FillRound(g, dr::RF(box), (float)d(3), dr::C(0xFFFFFF));
        dr::StrokeRound(g, dr::RF(box), (float)d(3),
                        sel ? dr::C(theme::kAccent) : ink, sel ? 2.f : 1.4f);

        if (sel || hover) {
            RECT h = box;
            h.left -= d(3);
            h.top -= d(3);
            h.right += d(3);
            h.bottom += d(3);
            dr::StrokeRound(g, dr::RF(h), (float)d(5),
                            dr::C(theme::kAccent, sel ? 110 : 60), 1.f);
        }

        // interface face tick: the side the cables will enter in phase 2
        {
            const int t = d(5);
            float x1 = (float)((box.left + box.right) / 2);
            float y1 = (float)((box.top + box.bottom) / 2);
            float x2 = x1, y2 = y1;
            switch (dv.faceDir) {
            case 0: y1 = (float)box.top; y2 = y1 - t; break;
            case 1: x1 = (float)box.right; x2 = x1 + t; break;
            case 2: y1 = (float)box.bottom; y2 = y1 + t; break;
            default: x1 = (float)box.left; x2 = x1 - t; break;
            }
            dr::Line(g, x1, y1, x2, y2, ink, 2.f);
        }

        // kind badge inside the box (≤5 chars, never the ambiguous "DI");
        // only painted when it really fits, the label below names the device
        const wchar_t* code = DeviceCode(dv.kind);
        Gdiplus::Font* cf = Font(7.5f, true);
        if (dr::TextW(g, code, cf) + 4.f <= (float)(box.right - box.left)) {
            RECT cr = {box.left + 1, box.top + 1, box.right - 1, box.bottom - 1};
            dr::Text(g, code, cf, ink, cr, 1, true);
        }

        // overload marker: declared channels above the device capacity, or more
        // channels arriving from the wiring than the box has inputs
        const bool over = pr_.DeviceOverCapacity(dv) ||
                          DeviceOverloaded(pr_, pr_.links, dv.id);
        if (over) {
            const int bw = d(11);
            RECT br = {box.right - bw, box.top - d(2), box.right + d(1), box.top + bw - d(2)};
            dr::FillRound(g, dr::RF(br), (float)d(5), dr::C(theme::kDanger));
            dr::Text(g, L"!", Font(7.5f, true), dr::C(0xFFFFFF), br, 1, true);
        }

        // (the `locked` device flag is kept for file compatibility but nothing
        // sets it any more: no auto-placed box, so no padlock is painted)
        // label under the box: the full device name (capacity is in the
        // device list and in the inspector, where there is room for it)
        dr::TextHalo(g, DevLabelText(dv), Font(8.5f),
                     dr::C(over ? theme::kDanger : theme::kText),
                     dr::C(0xFFFFFF, 235),
                     RECT{lbl.left, box.bottom + d(2), lbl.right, box.bottom + d(17)}, 1);
    }
}

// ---------------------------------------------------------------------------
// hit test / placement / drag
// ---------------------------------------------------------------------------
int App::DeviceHitTest(int x, int y) const {
    POINT p = {x, y};
    const int pad = (int)std::lround(6.0 * dpi_ * uiScale_);
    for (auto it = pr_.devices.rbegin(); it != pr_.devices.rend(); ++it) {
        RECT box{}, lbl{};
        if (!DeviceRects(*it, ppm_, ox_, oy_, pad, box, lbl)) continue;
        RECT all = box;
        all.left = (std::min)(all.left, lbl.left);
        all.right = (std::max)(all.right, lbl.right);
        all.bottom = (std::max)(all.bottom, lbl.bottom);
        if (PtInRect(&all, p)) return it->id;
    }
    return -1;
}

// Rubber-band multi-select on layer 03: the device's DRAWN box (label excluded,
// exactly the rectangle the painter fills) against the band rectangle.
bool App::DeviceHitsBand(const Device& dv, const RECT& band) const {
    RECT box{}, lbl{};
    if (!DeviceRects(dv, ppm_, ox_, oy_, 0, box, lbl)) return false;
    return band.left <= box.right && band.right >= box.left &&
           band.top <= box.bottom && band.bottom >= box.top;
}

Device* App::AddDevice(DevKind kind) {
    if (kind == DevKind::Mic) return nullptr;
    Device dv;
    dv.id = pr_.nextDeviceId++;
    // both box enum values are the ONE 「接口箱 / Stagebox」: created as a plain
    // analogue box, no protocol, no ports, no per-port count
    if (DeviceIsStagebox(kind)) kind = DevKind::AnalogBox;
    dv.kind = kind;
    int n = 1;
    for (const auto& o : pr_.devices)
        if (o.kind == kind) ++n;
    dv.label = DeviceKindTitle(kind) + L" " + std::to_wstring(n);
    switch (kind) {
    case DevKind::AnalogBox:
        dv.channels = 8;     // 输入通道数, editable in the inspector
        break;
    case DevKind::SoundCard:
        dv.channels = 8;     // a hub: its inputs never run out
        break;
    case DevKind::Receiver:
        // wireless receiver: analogue outputs only, the channels are the limit
        dv.channels = 4;
        break;
    default:
        return nullptr;
    }

    // free spot around the centre of the stage (never stack two boxes)
    const double cx = pr_.stage.w * 0.5, cy = pr_.stage.d * 0.5;
    auto freeAt = [&](double px, double py) {
        for (const auto& o : pr_.devices)
            if (std::fabs(o.x - px) < 1.20 && std::fabs(o.y - py) < 0.90) return false;
        return true;
    };
    double bx = cx, by = cy;
    bool found = false;
    for (int r = 0; r < 16 && !found; ++r) {
        for (int ix = -r; ix <= r && !found; ++ix) {
            for (int iy = -r; iy <= r && !found; ++iy) {
                if ((std::max)(std::abs(ix), std::abs(iy)) != r) continue;
                const double px = cx + ix * 1.25;
                const double py = cy + iy * 0.95;
                if (px < 0.5 || px > pr_.stage.w - 0.5) continue;
                if (py < 0.5 || py > pr_.stage.d - 0.5) continue;
                if (!freeAt(px, py)) continue;
                bx = px;
                by = py;
                found = true;
            }
        }
    }
    dv.x = std::round(bx / 0.05) * 0.05;
    dv.y = std::round(by / 0.05) * 0.05;

    pr_.devices.push_back(dv);
    PushUndoSnapshot();   // creating a box is undoable
    pr_.ClearDeviceSelection();
    pr_.SelectDevice(dv.id);
    pr_.Deselect();            // the cabling inspector replaces the mic inspector
    ui_.hoverCbRow = (int)pr_.devices.size() - 1;
    RefreshAfterProjectChange();
    return pr_.FindDevice(dv.id);
}

void App::DeleteSelectedDevice() {
    if (RefuseLocked()) return;
    // EVERY selected device is deleted (Ctrl+A / Ctrl+click build a multi-selection)
    std::vector<int> ids;
    for (const Device& dv : pr_.devices)
        if (dv.selected) ids.push_back(dv.id);
    if (ids.empty()) {
        if (const Device* dv = pr_.SelectedDevice()) ids.push_back(dv->id);
    }
    if (ids.empty()) return;
    PushUndoSnapshot();   // deleting a box (and its cables) is undoable
    for (int id : ids) pr_.RemoveLinksOf(0, id);   // no dangling cables are left
    pr_.devices.erase(std::remove_if(pr_.devices.begin(), pr_.devices.end(),
                                     [&ids](const Device& x) {
                                         return std::find(ids.begin(), ids.end(),
                                                          x.id) != ids.end();
                                     }),
                      pr_.devices.end());
    ui_.hoverCbRow = -1;
    ui_.hoverCbDevice = -1;
    ui_.hoverCable = -1;
    RefreshAfterProjectChange();
}

void App::DeviceDragTo(int x, int y) {
    if (!devDrag_) return;
    Device* dv = pr_.FindDevice(devDragId_);
    if (!dv) return;
    if (dv->locked) return;   // locked beside the sound card: no drag, no hint
    double wx = 0, wy = 0;
    if (!PointToWorld(x, y, wx, wy)) return;
    const double step = 0.05;
    double nx = std::round((wx - devDragDx_) / step) * step;
    double ny = std::round((wy - devDragDy_) / step) * step;
    nx = std::clamp(nx, -200.0, 200.0);
    ny = std::clamp(ny, -200.0, 200.0);
    const double dx = nx - dv->x;
    const double dy = ny - dv->y;
    if (std::fabs(dx) < 1e-9 && std::fabs(dy) < 1e-9) return;
    // the grabbed box is the anchor: when it belongs to the selection every other
    // selected box travels by the same delta (MouseDown collapses a click on an
    // unselected box to that box alone)
    std::vector<int> ids;
    if (dv->selected) {
        for (const Device& d : pr_.devices)
            if (d.selected) ids.push_back(d.id);
    }
    if (ids.empty()) ids.push_back(dv->id);
    GestureUndo();   // one snapshot for the whole device drag
    for (int id : ids) {
        Device* m = pr_.FindDevice(id);
        if (!m || m->locked) continue;   // a locked box is never moved, ever
        m->x = std::clamp(m->x + dx, -200.0, 200.0);
        m->y = std::clamp(m->y + dy, -200.0, 200.0);
    }
    // objects are always draggable now: the cables follow the box live
    RebuildAllCablePaths(pr_, MicRadiusM());
    devDragMoved_ = true;
    InvalidateAll();
}

// ---------------------------------------------------------------------------
// inspector (right panel) layout
// ---------------------------------------------------------------------------
void App::LayoutCabling() {
    auto S = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    const int topH = S(58);
    const int rx = L_.right.left + S(22);
    const int rw = (L_.right.right - L_.right.left) - S(44);
    int y = topH + S(18) - (int)ui_.rightScroll;

    // the cabling panel replaces every stage / item control
    HWND hide[] = {ePiece_, eVenue_, eConductor_, eNotes_, eSysEng_, eRecEng_,
                   eAsstEng_, eWidth_, eDepth_, eOff_, eCeiling_, eSelName_, eX_, eY_,
                   eAngle_, eMicH_, eSpace_, eCenter_, eModelL_, eModelR_,
                   eModelC_, eMicModel_, ePianoLen_};
    for (HWND h : hide)
        if (h) ShowWindow(h, SW_HIDE);

    // stale hit rects from the previous layer must not survive
    L_.secProject = L_.secStage = L_.secSel = L_.secAppearance = L_.secLayers = {};
    L_.p1Panel = L_.p1Title = L_.p1Rule = {};
    for (int i = 0; i < 3; ++i) L_.presets[i] = {};
    for (int i = 0; i < 4; ++i) {
        L_.colorRow[i] = {};
        L_.colorSwatch[i] = {};
        L_.colorHex[i] = {};
        L_.divider[i] = {};
    }
    // the per-item 颜色 row belongs to the 02/03/04 inspectors, never to this page
    L_.itemColorRow = L_.itemColorLabel = L_.itemColorSwatch = {};
    L_.itemColorHex = L_.itemColorReset = {};
    for (int i = 0; i < 2; ++i) {
        L_.layerRow[i] = {};
        L_.layerCheck[i] = {};
    }
    for (int i = 0; i < 2; ++i) L_.textRow[i] = L_.textCheck[i] = {};
    L_.delBtn = L_.bindBtn = L_.treeBtn = L_.spotBtn = L_.spotList = {};
    for (int i = 0; i < 16; ++i) L_.spotListRow[i] = {};
    L_.selInfo = {};
    L_.lenField = L_.micModel = L_.micHeight = L_.spaceField = L_.centerField = {};
    L_.modelL = L_.modelR = L_.modelC = {};
    L_.flownRow = L_.flownCheck = {};
    L_.flyField = {};
    for (int i = 0; i < 4; ++i) L_.techBtn[i] = L_.patternBtn[i] = {};
    for (int i = 0; i < 3; ++i) L_.techPreset[i] = {};

    // same vertical rhythm as the main inspector and the 项目信息 page: label
    // line, gap, box; the EDIT is placed at exactly the box rect
    auto field = [&](int fx, int fw, int fh, int fy) {
        return FieldRect(fx, fw, fh, fy);
    };

    // section header
    L_.cbSec = {rx, y, rx + rw, y + S(16)};
    y += S(22);

    // the three add buttons (接口箱 / 声卡 / 无线接收器), two per row
    {
        const int gap = S(8);
        const int bw = (rw - gap) / 2;
        const int bh = S(28);
        const int rows = (kAddDeviceCount + 1) / 2;
        for (int i = 0; i < kAddDeviceCount; ++i) {
            const int col = i % 2, row = i / 2;
            const int x0 = rx + col * (bw + gap);
            const int y0 = y + row * (bh + gap);
            L_.cbAdd[i] = {x0, y0, x0 + bw, y0 + bh};
        }
        y += rows * bh + (rows - 1) * gap + S(10);
    }
    L_.divider[0] = {rx, y - S(5), rx + rw, y - S(5)};

    // device list
    {
        const int rowH = S(28);
        const int n = (int)pr_.devices.size();
        const int boxH = S(8) + (std::max)(n, 1) * rowH;
        L_.cbList = {rx, y, rx + rw, y + boxH};
        L_.cbRow.clear();
        for (int i = 0; i < n; ++i) {
            L_.cbRow.push_back({rx + S(4), y + S(4) + i * rowH, rx + rw - S(4),
                                y + S(4) + (i + 1) * rowH});
        }
        y += boxH + S(10);
    }

    // delete device
    L_.cbDel = {rx, y, rx + rw, y + S(28)};
    y += S(28) + S(10);

    // per-device cable totals + the grand total
    {
        int lines = 1;   // the grand total always has a line of its own
        for (const auto& d2 : pr_.devices)
            if (DeviceCableStats(pr_, pr_.links, d2.id, false).links > 0) ++lines;
        L_.cbTotals = {rx, y, rx + rw, y + lines * S(17) + S(2)};
        y = L_.cbTotals.bottom + S(8);
    }
    L_.divider[1] = {rx, y - S(5), rx + rw, y - S(5)};

    const Device* dv = pr_.SelectedDevice();
    L_.cbName = L_.cbChannels = {};
    L_.cbX = L_.cbY = {};
    L_.cbCap = L_.cbInfo = L_.cbKind = {};
    if (dv) {
        L_.cbKind = {rx, y, rx + rw, y + S(17)};
        y += S(22);
        // every device offers exactly 名称 / 输入通道数 / X / Y now: no protocol,
        // no 端口数, no 每口通道数, no 型号
        L_.cbName = field(rx, rw, 28, y);
        y = L_.cbName.box.bottom + S(6);
        {
            // X / Y of the device
            const int half = (rw - S(10)) / 2;
            L_.cbX = field(rx, half, 26, y);
            L_.cbY = field(rx + half + S(10), rw - half - S(10), 26, y);
            y = L_.cbX.box.bottom + S(6);
        }
        L_.cbChannels = field(rx, rw, 28, y);
        y = L_.cbChannels.box.bottom + S(6);
        L_.cbCap = {rx, y, rx + rw, y + S(17)};
        y += S(20);
        L_.cbInfo = {rx, y, rx + rw, y + S(17)};
        y += S(20);
    }

    // ---- selected microphone: its information and its route list -----------
    L_.cbMiTitle = L_.cbMiInfo = L_.cbRouteSec = L_.cbRouteEmpty = {};
    L_.cbConnSec = L_.cbConnEmpty = {};
    L_.cbConnRow.clear();
    L_.cbConnDev.clear();
    L_.cbRouteRow.clear();
    L_.cbRouteDel.clear();
    L_.cbRouteLink.clear();
    const Item* micSel = (!dv) ? pr_.Selected() : nullptr;
    if (micSel && micSel->kind == ItemKind::Mic) {
        L_.cbMiTitle = {rx, y, rx + rw, y + S(20)};
        y += S(26);
        // 名称 / 型号 / 通道数 / 绑定的乐器 / 位置 — one line each.  The 吊装
        // toggle sits ON the 名称 line, at its right end, so the checkbox is right
        // next to the microphone name at the very top of the panel (the info block
        // is five lines instead of six: the old 吊装 text line is the checkbox now).
        const int rowH = S(18);
        const int cbW = S(92);
        // a wireless (lavalier) microphone can never be flown: no checkbox
        if (micSel->defId == L"lavalier") {
            L_.flownRow = L_.flownCheck = {};
        } else {
            L_.flownRow = {rx + rw - cbW, y - S(1), rx + rw, y + rowH - S(1)};
            L_.flownCheck = {L_.flownRow.left, L_.flownRow.top + S(3),
                             L_.flownRow.left + S(14), L_.flownRow.top + S(17)};
        }
        L_.cbMiInfo = {rx, y, rx + rw, y + 5 * rowH};
        y = L_.cbMiInfo.bottom + S(6);
        // 吊装高度: editable and right under the toggle, only while it is on
        if (micSel->flown) {
            L_.flyField = {};
        }
        L_.cbRouteSec = {rx, y, rx + rw, y + S(16)};
        y += S(24);
        const std::vector<int> cabIdx = MicCableIndices(pr_, *micSel);
        if (cabIdx.empty()) {
            L_.cbRouteEmpty = {rx, y, rx + rw, y + S(20)};
            y += S(28);
        } else {
            const int rowH = S(46);
            const int bw = S(72);
            for (size_t k = 0; k < cabIdx.size(); ++k) {
                RECT row = {rx, y, rx + rw, y + rowH};
                L_.cbRouteRow.push_back(row);
                L_.cbRouteLink.push_back(cabIdx[k]);
                L_.cbRouteDel.push_back({row.right - bw - S(8), row.bottom - S(26),
                                          row.right - S(8), row.bottom - S(6)});
                y += rowH + S(8);
            }
        }
        // ---- 「连接」: click a device row to wire this microphone to it ------
        // (the very same CreateCable path the right-click pair uses, so a bound
        // group is wired as ONE cable and a re-connect replaces the old route)
        L_.cbConnSec = {rx, y, rx + rw, y + S(16)};
        y += S(24);
        if (pr_.devices.empty()) {
            L_.cbConnEmpty = {rx, y, rx + rw, y + S(20)};
            y += S(26);
        } else {
            const int cRowH = S(28);
            for (size_t i = 0; i < pr_.devices.size(); ++i) {
                L_.cbConnRow.push_back({rx, y, rx + rw, y + cRowH});
                L_.cbConnDev.push_back(pr_.devices[i].id);
                y += cRowH + S(4);
            }
        }
        y += S(4);
    }

    // A native EDIT ignores the GDI+ clip, so every inspector field goes through
    // the one placement path (the same one the 01/02 panel uses): positioned at the
    // exact rect the painter draws and clipped to the panel.
    auto place = [&](HWND h, const RECT& r, bool show) {
        PlaceInspectorEdit(h, r, show, L_.right);
    };
    const bool hasDev = dv != nullptr;
    place(eDevName_, L_.cbName.edit, hasDev);
    place(eDevX_, L_.cbX.edit, hasDev);
    place(eDevY_, L_.cbY.edit, hasDev);
    if (eDevX_) SendMessageW(eDevX_, EM_SETREADONLY, (dv && dv->locked) ? TRUE : FALSE, 0);
    if (eDevY_) SendMessageW(eDevY_, EM_SETREADONLY, (dv && dv->locked) ? TRUE : FALSE, 0);
    place(eDevCh_, L_.cbChannels.edit, hasDev);
    // 05: the 吊装高度 of the selected microphone, right under its checkbox
    place(eFlyH_, {},
          micSel && micSel->kind == ItemKind::Mic && micSel->flown);

    L_.rightContent = y + (int)ui_.rightScroll + S(14);
    const int viewH = (L_.right.bottom - L_.right.top) - S(20);
    if (L_.rightContent - viewH < (int)ui_.rightScroll) {
        const int ns = (std::max)(0, L_.rightContent - viewH);
        if (ns != (int)ui_.rightScroll) {
            ui_.rightScroll = (double)ns;
            if (!relaying_) {   // clamp, then rebuild the rects from the new value
                relaying_ = true;
                LayoutCabling();
                relaying_ = false;
            }
            return;
        }
    }

    UpdateStageXf();
}

// ---------------------------------------------------------------------------
// inspector (right panel) painting
// ---------------------------------------------------------------------------
void App::PaintCablingRight(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    dr::Fill(g, L_.right, dr::C(theme::kPanel));
    dr::Line(g, (float)L_.right.left, (float)L_.right.top,
             (float)L_.right.left, (float)L_.right.bottom,
             dr::C(theme::kBorder), 1.f);

    for (int i = 0; i < 4; ++i) {
        if (L_.divider[i].right <= L_.divider[i].left) continue;
        dr::Line(g, (float)L_.divider[i].left, (float)L_.divider[i].top,
                 (float)L_.divider[i].right, (float)L_.divider[i].top,
                 dr::C(theme::kBorder), 1.f);
    }

    auto fieldBox = [&](const FieldBox& f, int ctrlId, bool enabled) {
        if (f.box.right <= f.box.left) return;
        const bool focus = ui_.focusEdit == ctrlId;
        dr::FillRound(g, dr::RF(f.box), (float)d(4),
                      dr::C(enabled ? theme::kField : theme::kPanelAlt));
        dr::StrokeRound(g, dr::RF(f.box), (float)d(4),
                        dr::C(focus ? theme::kAccent
                             : enabled ? theme::kBorderStrong : theme::kBorder),
                        focus ? 1.4f : 1.f);
    };
    auto labelOf = [&](const FieldBox& f, S id) {
        if (f.label.right <= f.label.left) return;
        dr::Text(g, L10n::T(id), Font(9.f), dr::C(theme::kTextMuted), f.label, 0, true);
    };

    // --- section header ------------------------------------------------------
    dr::FillRound(g, Gdiplus::RectF((float)L_.cbSec.left, (float)(L_.cbSec.top + d(3)),
                                   (float)d(3), (float)d(9)),
                  d(1.5f), dr::C(theme::kAccent, 200));
    dr::Text(g, L10n::T(S::CbTitle), Font(9.f), dr::C(theme::kTextMuted),
             RECT{L_.cbSec.left + d(10), L_.cbSec.top, L_.cbSec.right, L_.cbSec.bottom},
             0, true);
    dr::Text(g, dr::Fmt(L"%d", (int)pr_.devices.size()), Font(9.f),
             dr::C(theme::kTextFaint), L_.cbSec, 2, true);

    // --- 层高 (indoor height) ------------------------------------------------
    fieldBox(L_.cbCeilField, ID_CEILING, true);
    labelOf(L_.cbCeilField, S::FieldCeiling);

    // --- add buttons (接口箱 / 声卡 / 无线接收器) -------------------------------
    const S addIds[kAddDeviceCount] = {S::CbAddBox, S::CbAddSoundCard,
                                       S::CbAddReceiver};
    for (int i = 0; i < kAddDeviceCount; ++i) {
        if (L_.cbAdd[i].right <= L_.cbAdd[i].left) continue;
        const bool hover = ui_.hoverCbAdd == i;
        dr::FillRound(g, dr::RF(L_.cbAdd[i]), d(5),
                      dr::C(hover ? theme::kHover : theme::kPanelAlt));
        dr::StrokeRound(g, dr::RF(L_.cbAdd[i]), d(5),
                        dr::C(hover ? theme::kBorderStrong : theme::kBorder), 1.f);
        dr::Text(g, L10n::T(addIds[i]), Font(8.5f),
                 dr::C(hover ? theme::kText : theme::kTextMuted), L_.cbAdd[i], 1, true);
    }

    // --- device list ---------------------------------------------------------
    if (L_.cbList.right > L_.cbList.left) {
        dr::FillRound(g, dr::RF(L_.cbList), d(6), dr::C(theme::kField));
        dr::StrokeRound(g, dr::RF(L_.cbList), d(6), dr::C(theme::kBorder), 1.f);
        if (pr_.devices.empty()) {
            dr::Text(g, L10n::T(S::CbNoDevice), Font(9.f), dr::C(theme::kTextFaint),
                     RECT{L_.cbList.left + d(10), L_.cbList.top, L_.cbList.right,
                          L_.cbList.bottom},
                     0, true);
        }
        for (size_t i = 0; i < pr_.devices.size() && i < L_.cbRow.size(); ++i) {
            const Device& dv = pr_.devices[i];
            const RECT& r = L_.cbRow[i];
            const bool sel = dv.selected;
            const bool hover = ui_.hoverCbRow == (int)i;
            if (sel || hover)
                dr::FillRound(g, dr::RF(r), d(4),
                              dr::C(sel ? theme::kAccentSoft : theme::kHover));
            dr::Text(g, DeviceCode(dv.kind), Font(8.f, true), DevInk(dv.kind),
                     RECT{r.left + d(6), r.top, r.left + d(42), r.bottom}, 0, true);
            std::wstring name = DevLabelText(dv);
            dr::Text(g, name, Font(9.f), dr::C(theme::kText),
                     RECT{r.left + d(44), r.top, r.right - d(74), r.bottom}, 0, true);
            // "over" covers a declared channel count above the device capacity
            // and the real input load from the wiring.  A sound card is a hub: it
            // has no capacity limit.
            const bool over = pr_.DeviceOverCapacity(dv) ||
                              DeviceOverloaded(pr_, pr_.links, dv.id);
            const int cap = pr_.DeviceCapacity(dv);
            const int load = DeviceLoad(pr_, pr_.links, dv.id);
            // compact: `SC hub`, `BOX 4ch`, `RX ! 5/4ch` — the row already carries
            // the name, so the right cell is the capacity only.  A sound card is a
            // hub: its capacity is INFINITE and must never be printed as a number.
            std::wstring right;
            if (Project::DeviceIsHub(dv)) right = L"hub";
            else if (over) right = dr::Fmt(L"! %d/%dch", load, cap);
            else right = dr::Fmt(L"%dch", cap);
            dr::Text(g, right, Font(8.5f),
                     dr::C(over ? theme::kDanger : theme::kTextFaint),
                     RECT{r.right - d(70), r.top, r.right - d(6), r.bottom}, 2, true);
        }
    }

    // --- delete button -------------------------------------------------------
    {
        const bool any = pr_.SelectedDevice() != nullptr;
        const bool hover = ui_.hoverCbDel != 0 && any;
        dr::FillRound(g, dr::RF(L_.cbDel), d(5),
                      dr::C(hover ? 0xF6EFEC : theme::kPanel));
        dr::StrokeRound(g, dr::RF(L_.cbDel), d(5),
                        dr::C(hover ? 0xD9C7BF : theme::kBorder), 1.f);
        dr::Text(g, L10n::T(S::CbDelDevice), Font(9.f),
                 dr::C(any ? (hover ? theme::kDanger : theme::kTextMuted)
                           : theme::kTextFaint,
                       any ? 255 : 120),
                 L_.cbDel, 1, true);
    }

    // --- per-device cable totals + grand total -------------------------------
    // 05 shows the REAL length (what has to be ordered), 03 / 04 the reserved one
    if (L_.cbTotals.right > L_.cbTotals.left) {
        int ty = L_.cbTotals.top;
        auto lineOf = [&](const std::wstring& s, const Gdiplus::Color& col, bool bold) {
            RECT r = {L_.cbTotals.left, ty, L_.cbTotals.right, ty + d(17)};
            dr::Text(g, s, Font(8.5f, bold), col, r, 2, true);
            ty += d(17);
        };
        for (const auto& dvv : pr_.devices) {
            const CableStats st = DeviceCableStats(pr_, pr_.links, dvv.id, false);
            if (st.links <= 0) continue;
            lineOf(dr::Fmt(L10n::T(S::CbDevTotalRealFmt), DeviceCode(dvv.kind),
                           st.channels, st.length),
                   dr::C(theme::kTextMuted), false);
        }
        const CableStats tot = TotalCableStats(pr_, pr_.links, false);
        lineOf(dr::Fmt(L10n::T(S::CbGrandTotalRealFmt), tot.channels, tot.length),
               dr::C(theme::kText), true);
    }

    // --- selected device / selected microphone -------------------------------
    const Device* dv = pr_.SelectedDevice();
    if (!dv) {
        PaintCablingMicInfo(g);   // a mic on the 05 canvas owns the panel too
        return;
    }

    // full kind name, e.g. 接口箱 1 / 声卡 1 / 无线接收器 1
    if (L_.cbKind.right > L_.cbKind.left) {
        dr::Text(g, DevLabelText(*dv), Font(10.f, true), dr::C(theme::kText),
                 L_.cbKind, 0, true);
    }

    // X / Y: editable; a locked device (kept for file compatibility) is read-only
    const bool posEditable = !dv->locked;
    fieldBox(L_.cbX, ID_DEVX, posEditable);
    labelOf(L_.cbX, S::XLabel);
    fieldBox(L_.cbY, ID_DEVY, posEditable);
    labelOf(L_.cbY, S::YLabel);

    // every device: 名称 / 输入通道数 / X / Y — nothing else
    fieldBox(L_.cbName, ID_DEVNAME, true);
    labelOf(L_.cbName, S::CbDevName);
    fieldBox(L_.cbChannels, ID_DEVCH, true);
    labelOf(L_.cbChannels, S::CbChannels);

    // --- capacity + validation seed ------------------------------------------
    const int load = DeviceLoad(pr_, pr_.links, dv->id);
    const bool hub = Project::DeviceIsHub(*dv);
    const bool overCap = pr_.DeviceOverCapacity(*dv);
    const bool overLoad = !hub && load > pr_.DeviceCapacity(*dv);
    const bool over = overCap || overLoad;
    if (L_.cbCap.right > L_.cbCap.left) {
        std::wstring cap;
        if (hub)
            // a sound card is a hub: its inputs never run out — never a number
            cap = dr::Fmt(L"%s · hub", L10n::T(S::ChNum));
        else
            // compact: `4 通道 · 已用 2` — the story is in the status bar hover
            cap = dr::Fmt(L10n::T(S::CbCapUsedFmt), dv->channels, load);
        if (overLoad) cap = L"! " + cap;
        dr::Text(g, cap, Font(9.f),
                 dr::C(over ? theme::kDanger : theme::kText), L_.cbCap, 0, true);
    }
    if (L_.cbInfo.right > L_.cbInfo.left) {
        std::wstring note;
        if (overLoad) {
            // 「输入超载：接口箱 1 已接 5 路，容量 4 路」
            note = dr::Fmt(L10n::T(S::CbOverloadFmt), DevLabelText(*dv).c_str(),
                           load, pr_.DeviceCapacity(*dv));
        }
        if (!note.empty())
            dr::Text(g, note, Font(8.5f),
                     dr::C(over ? theme::kDanger : theme::kTextMuted), L_.cbInfo, 0,
                     true);
    }
}

// ---------------------------------------------------------------------------
// 05: the SELECTED MICROPHONE.  Same panel as the device inspector: its data
// (名称 / 型号 / 通道数 / 绑定的乐器 / 吊装 / 位置) and — under 「路由」 — every
// cable drawn for it, with channels, REAL length, state and a 「删除路由」 button.
// Clicking any capsule / group member on the canvas selects the whole microphone,
// so a grouped mic shows the group's single cable.
// ---------------------------------------------------------------------------
namespace {

std::wstring InstNameOf(const std::wstring& id) {
    // the two extra 对应乐器 targets (侧展 / 环境) and any catalogue instrument; an
    // unknown binding of an old project simply prints as written
    return SpotTypeName(id);
}

// the localized one-word state of a cable: 无线 / 超容量 / 正常
std::wstring CableStateText(const Project& pr, const CableLink& ln, bool& bad) {
    bad = false;
    if (ln.radio) return L10n::T(S::CbStateRadio);
    if (CheckCable(pr, ln).overChannels) {
        bad = true;
        return L10n::T(S::CbStateOver);
    }
    return L10n::T(S::CbStateOk);
}

} // namespace

void App::PaintCablingMicInfo(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    const Item* mic = pr_.Selected();
    if (!mic || mic->kind != ItemKind::Mic) return;

    // ---- headline -----------------------------------------------------------
    std::wstring title = mic->label;
    if (mic->defId == L"mainmic") title = L10n::T(S::MainMic);
    if (title.empty()) title = L10n::T(S::TypeMic);
    if (L_.cbMiTitle.right > L_.cbMiTitle.left)
        dr::Text(g, title, Font(10.5f, true), dr::C(theme::kText), L_.cbMiTitle, 0,
                 true);

    // ---- 名称 (+ the 吊装 toggle beside it) -----------------------------------
    // Every row paints into L_.cbMiInfo; the toggle is the L_.flownRow rect of the
    // 名称 line, so a click in the same place toggles it (CablingInspectorDown).
    auto line = [&](int row, const wchar_t* label, const std::wstring& value,
                    bool nameRow) {
        if (L_.cbMiInfo.right <= L_.cbMiInfo.left) return;
        const int ty = L_.cbMiInfo.top + row * d(18);
        RECT lr = {L_.cbMiInfo.left, ty, L_.cbMiInfo.left + d(92), ty + d(18)};
        // the name must not run under the checkbox on its right
        const int right = (nameRow && L_.flownRow.right > L_.flownRow.left)
                              ? L_.flownRow.left - d(6) : L_.cbMiInfo.right;
        RECT vr = {L_.cbMiInfo.left + d(96), ty, right, ty + d(18)};
        dr::Text(g, label, Font(9.f), dr::C(theme::kTextMuted), lr, 0, true);
        dr::Text(g, value, Font(9.f), dr::C(theme::kText), vr, 0, true);
    };
    std::wstring model = mic->model;
    if (mic->defId == L"mainmic") {
        std::wstring m;
        auto add = [&](const wchar_t* tag, const std::wstring& s) {
            if (s.empty()) return;
            if (!m.empty()) m += L"  ";
            m += std::wstring(tag) + s;
        };
        add(L"L ", mic->modelL);
        add(L"C ", mic->modelC);
        add(L"R ", mic->modelR);
        if (!m.empty()) model = m;
    }
    if (model.empty()) model = L"—";
    const int gi = pr_.GroupOf(mic->id);
    const int ch = (gi >= 0 && pr_.micGroups[(size_t)gi].ids.size() >= 2)
                       ? (int)pr_.micGroups[(size_t)gi].ids.size()
                       : MicItemChannels(*mic);
    const std::wstring inst = InstNameOf(mic->spotType);
    line(0, L10n::T(S::CbDevName), mic->label.empty() ? title : mic->label, true);
    line(1, L10n::T(S::FieldModel), model, false);
    line(2, L10n::T(S::CbMiCh), dr::Fmt(L"%d", ch), false);
    line(3, L10n::T(S::CbMiInst), inst.empty() ? std::wstring(L"—") : inst, false);
    line(4, L10n::T(S::PosLabel), dr::Fmt(L"X %.2f   Y %.2f", mic->x, mic->y), false);

    // ---- 「连接」: the devices this microphone can be wired to ------------------
    // Clicking a row wires THAT microphone (or its whole bound group — CreateCable
    // builds one cable for it) to that device.  The row of the device it is
    // already wired to is marked 已连接.  The painter lambda is defined here so the
    // route block below can call it from both of its exits (the route list has an
    // EMPTY state that returns early).
    auto paintConn = [&]() {
        if (L_.cbConnSec.right > L_.cbConnSec.left) {
            dr::FillRound(g,
                          Gdiplus::RectF((float)L_.cbConnSec.left,
                                         (float)(L_.cbConnSec.top + d(3)), (float)d(3),
                                         (float)d(9)),
                          d(1.5f), dr::C(theme::kAccent, 200));
            dr::Text(g, L10n::T(S::CbConnSec), Font(9.f), dr::C(theme::kTextMuted),
                     RECT{L_.cbConnSec.left + d(10), L_.cbConnSec.top,
                          L_.cbConnSec.right, L_.cbConnSec.bottom},
                     0, true);
        }
        if (L_.cbConnEmpty.right > L_.cbConnEmpty.left)
            dr::Text(g, L10n::T(S::CbNoDevice), Font(9.f), dr::C(theme::kTextFaint),
                     L_.cbConnEmpty, 0, true);
        const int connDev = MicConnectedDevice(mic->id);
        for (size_t k = 0; k < L_.cbConnRow.size() && k < L_.cbConnDev.size(); ++k) {
            const Device* dd = pr_.FindDevice(L_.cbConnDev[k]);
            if (!dd) continue;
            const RECT& row = L_.cbConnRow[k];
            const bool wired = (L_.cbConnDev[k] == connDev);
            const bool hov = ui_.hoverCbConn == (int)k;
            dr::FillRound(g, dr::RF(row), d(6),
                          dr::C(wired ? theme::kAccentSoft
                                      : (hov ? theme::kHover : theme::kField)));
            dr::StrokeRound(g, dr::RF(row), d(6),
                            dr::C(wired ? theme::kAccent : theme::kBorder), 1.f);
            dr::Text(g, DeviceCode(dd->kind), Font(8.f, true), DevInk(dd->kind),
                     RECT{row.left + d(8), row.top, row.left + d(44), row.bottom}, 0,
                     true);
            dr::Text(g, DevLabelText(*dd), Font(9.f), dr::C(theme::kText),
                     RECT{row.left + d(46), row.top, row.right - d(66), row.bottom}, 0,
                     true);
            if (wired)
                dr::Text(g, L10n::T(S::CbConnMark), Font(8.5f), dr::C(theme::kAccent),
                         RECT{row.right - d(64), row.top, row.right - d(8),
                              row.bottom},
                         2, true);
        }
    };

    // ---- 吊装 on/off + 吊装高度 (same look and ID as the 01/02 checkbox) -------
    if (L_.flownRow.right > L_.flownRow.left) {
        const bool on = mic->flown;
        if (ui_.hoverRow == 10)
            dr::FillRound(g, dr::RF(L_.flownRow), d(5), dr::C(theme::kHover));
        dr::FillRound(g, dr::RF(L_.flownCheck), d(3.5f),
                      dr::C(on ? theme::kAccent : theme::kField));
        dr::StrokeRound(g, dr::RF(L_.flownCheck), d(3.5f),
                        dr::C(on ? theme::kAccent : theme::kBorderStrong), 1.f);
        if (on) {
            const RECT& cb = L_.flownCheck;
            const float x0 = (float)cb.left + d(3.5f);
            const float y0 = (float)cb.top + d(8.f);
            const float x1 = (float)cb.left + d(6.8f);
            const float y1 = (float)cb.top + d(11.2f);
            const float x2 = (float)cb.right - d(3.5f);
            const float y2 = (float)cb.top + d(4.5f);
            dr::Line(g, x0, y0, x1, y1, dr::C(0xFFFFFF), 1.4f);
            dr::Line(g, x1, y1, x2, y2, dr::C(0xFFFFFF), 1.4f);
        }
        dr::Text(g, L10n::T(S::FieldFlown), Font(9.f), dr::C(theme::kText),
                 RECT{L_.flownCheck.right + d(7), L_.flownRow.top,
                      L_.flownRow.right, L_.flownRow.bottom},
                 0, true);
        if (L_.flyField.box.right > L_.flyField.box.left) {
            const bool focus = ui_.focusEdit == ID_FLYH;
            dr::FillRound(g, dr::RF(L_.flyField.box), (float)d(4), dr::C(theme::kField));
            dr::StrokeRound(g, dr::RF(L_.flyField.box), (float)d(4),
                            dr::C(focus ? theme::kAccent : theme::kBorderStrong),
                            focus ? 1.4f : 1.f);
            dr::Text(g, dr::Fmt(L"%s (m)", L10n::T(S::FieldFlyHeight)), Font(9.f),
                     dr::C(theme::kTextMuted), L_.flyField.label, 0, true);
        }
    }

    // ---- 「路由」 --------------------------------------------------------------
    if (L_.cbRouteSec.right > L_.cbRouteSec.left) {
        dr::FillRound(g, Gdiplus::RectF((float)L_.cbRouteSec.left,
                                       (float)(L_.cbRouteSec.top + d(3)), (float)d(3),
                                       (float)d(9)),
                      d(1.5f), dr::C(theme::kAccent, 200));
        dr::Text(g, L10n::T(S::CbRouteSec), Font(9.f), dr::C(theme::kTextMuted),
                 RECT{L_.cbRouteSec.left + d(10), L_.cbRouteSec.top,
                      L_.cbRouteSec.right, L_.cbRouteSec.bottom},
                 0, true);
    }
    if (L_.cbRouteEmpty.right > L_.cbRouteEmpty.left) {
        dr::Text(g, L10n::T(S::CbNoRoute), Font(9.f), dr::C(theme::kTextFaint),
                 L_.cbRouteEmpty, 0, true);
        paintConn();
        return;
    }

    // ---- one row per cable ---------------------------------------------------
    for (size_t k = 0; k < L_.cbRouteRow.size(); ++k) {
        const RECT& row = L_.cbRouteRow[k];
        if (k >= L_.cbRouteLink.size()) break;
        const int li = L_.cbRouteLink[k];
        if (li < 0 || li >= (int)pr_.links.size()) continue;
        const CableLink& ln = pr_.links[li];
        const bool hov = ui_.hoverCbRoute == (int)k;
        dr::FillRound(g, dr::RF(row), d(6),
                      dr::C(hov ? theme::kHover : theme::kField));
        dr::StrokeRound(g, dr::RF(row), d(6), dr::C(theme::kBorder), 1.f);

        // the target end (the device this cable feeds)
        const CableEnd a = ResolveCableEnd(pr_, ln.aKind, ln.a);
        const CableEnd b = ResolveCableEnd(pr_, ln.bKind, ln.b);
        const CableEnd& tgt = a.mic ? b : a;
        bool bad = false;
        const std::wstring state = CableStateText(pr_, ln, bad);
        RECT tr = {row.left + d(10), row.top + d(3), row.right - d(80),
                   row.top + d(21)};
        dr::Text(g, tgt.name, Font(9.f, true), dr::C(theme::kText), tr, 0, true);
        dr::Text(g, state, Font(8.5f),
                 dr::C(bad ? theme::kDanger
                           : (ln.radio ? 0x6A5F7A : theme::kTextMuted)),
                 RECT{row.right - d(86), row.top + d(3), row.right - d(10),
                      row.top + d(21)},
                 2, true);

        // channels · REAL length (+ 自定义 when the user typed a length)
        const double len = CableRealLen(pr_, ln);
        std::wstring detail = dr::Fmt(L10n::T(S::CbCableTagFmt), ln.channels, len);
        if (CableOverrideLen(pr_, ln) > 0.0)
            detail += std::wstring(L"  ") + L10n::T(S::CbCustom);
        RECT dr2 = {row.left + d(10), row.bottom - d(24), row.right - d(84),
                    row.bottom - d(5)};
        dr::Text(g, detail, Font(8.5f), dr::C(theme::kTextMuted), dr2, 0, true);

        // 「删除路由」 button
        if (k < L_.cbRouteDel.size()) {
            const RECT& br = L_.cbRouteDel[k];
            const bool bh = ui_.hoverCbRouteDel == (int)k;
            dr::FillRound(g, dr::RF(br), d(4),
                          dr::C(bh ? 0xF6EFEC : theme::kPanel));
            dr::StrokeRound(g, dr::RF(br), d(4),
                            dr::C(bh ? 0xD9C7BF : theme::kBorder), 1.f);
            dr::Text(g, L10n::T(S::CbDelRoute), Font(8.5f),
                     dr::C(bh ? theme::kDanger : theme::kTextMuted), br, 1, true);
        }
    }

    paintConn();
}

// ---------------------------------------------------------------------------
// 04 通道表: the 「链路 / 路由」 cell's floating DEVICE picker.
//
// Clicking that cell selects the row (as before) and opens this small list — the
// SAME devices the 布线 page's 「连接」 block shows — anchored beside the cell.  The
// cell itself stays read-only: there is no text editor here, only the connect
// action (ConnectMicToDevice, the one code path both pages share).  The panel is
// clamped INSIDE the canvas, painted LAST (PaintOverlay) and hit-tested first in
// MouseDown; Esc / a click outside closes it.
// ---------------------------------------------------------------------------
void App::PaintChannelRoute(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    L_.chRoutePanel = {};
    L_.chRouteRow.clear();
    L_.chRouteDev.clear();
    if (ui_.chRouteOpen <= 0) return;
    const Item* mic = pr_.Find(ui_.chRouteOpen);
    if (!mic || mic->kind != ItemKind::Mic) return;

    const int rowH = d(24);
    const int w = d(228);
    int x = ui_.chRouteAnchorX;
    if (x + w > L_.canvas.right - d(8)) x = L_.canvas.right - d(8) - w;
    if (x < L_.canvas.left + d(8)) x = L_.canvas.left + d(8);
    int y = ui_.chRouteAnchorY + d(4);
    if (pr_.devices.empty()) {
        // no devices yet: a one-line note instead of an empty box
        RECT box = {x, y, x + w, y + d(26)};
        if (box.bottom > L_.canvas.bottom - d(8)) {
            const int dy = box.bottom - (L_.canvas.bottom - d(8));
            box.top -= dy;
            box.bottom -= dy;
        }
        L_.chRoutePanel = box;
        dr::FillRound(g, dr::RF(box), d(7), dr::C(theme::kPanel));
        dr::StrokeRound(g, dr::RF(box), d(7), dr::C(theme::kBorderStrong), 1.f);
        dr::Text(g, L10n::T(S::CbNoDevice), Font(9.f), dr::C(theme::kTextFaint),
                 RECT{box.left + d(10), box.top, box.right - d(8), box.bottom}, 0, true);
        return;
    }
    const int n = (int)pr_.devices.size();
    // the caption line says whose routing this is: 「连接 · <话筒名>」
    const std::wstring capName = mic->label.empty() ? L10n::T(S::TypeMic) : mic->label;
    const int boxH = d(22) + n * rowH + d(6);
    RECT box = {x, y, x + w, y + boxH};
    if (box.bottom > L_.canvas.bottom - d(8)) {   // keep it INSIDE the window
        const int dy = box.bottom - (L_.canvas.bottom - d(8));
        box.top -= dy;
        box.bottom -= dy;
    }
    if (box.top < L_.canvas.top + d(8)) {
        const int dy = (L_.canvas.top + d(8)) - box.top;
        box.top += dy;
        box.bottom += dy;
    }
    L_.chRoutePanel = box;
    dr::FillRound(g, dr::RF(box), d(7), dr::C(theme::kPanel));
    dr::StrokeRound(g, dr::RF(box), d(7), dr::C(theme::kBorderStrong), 1.f);
    RECT cap = {box.left + d(10), box.top + d(2), box.right - d(8), box.top + d(20)};
    dr::Text(g, L10n::T(S::CbConnSec), Font(8.5f), dr::C(theme::kTextMuted), cap, 0,
             true);
    dr::Text(g, capName, Font(8.5f), dr::C(theme::kTextFaint), cap, 2, true);

    const int connDev = MicConnectedDevice(ui_.chRouteOpen);
    for (int i = 0; i < n; ++i) {
        const Device& dv = pr_.devices[(size_t)i];
        const int top = box.top + d(22) + i * rowH;
        RECT r = {box.left + d(4), top, box.right - d(4), top + rowH};
        L_.chRouteRow.push_back(r);
        L_.chRouteDev.push_back(dv.id);
        const bool wired = (dv.id == connDev);
        const bool hover = (ui_.chRouteHover == i);
        if (wired || hover)
            dr::FillRound(g, dr::RF(r), d(4),
                          dr::C(wired ? theme::kAccentSoft : theme::kHover));
        dr::Text(g, DeviceCode(dv.kind), Font(8.f, true), DevInk(dv.kind),
                 RECT{r.left + d(8), r.top, r.left + d(44), r.bottom}, 0, true);
        dr::Text(g, DevLabelText(dv), Font(9.f), dr::C(theme::kText),
                 RECT{r.left + d(46), r.top, r.right - d(58), r.bottom}, 0, true);
        if (wired)
            dr::Text(g, L10n::T(S::CbConnMark), Font(8.f), dr::C(theme::kAccent),
                     RECT{r.right - d(56), r.top, r.right - d(6), r.bottom}, 2, true);
    }
}

// ---------------------------------------------------------------------------
// delete-route confirmation: a small modal in the centre of the window (scrim +
// panel + 确定 / 取消).  Nothing behind it is clickable while it is open and Esc
// cancels — 确定 is the only way to remove the link.
// ---------------------------------------------------------------------------
void App::PaintCableConfirm(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    if (ui_.confirmLink < 0 || ui_.confirmLink >= (int)pr_.links.size()) {
        L_.cfScrim = L_.cfPanel = L_.cfOk = L_.cfCancel = L_.cfText = {};
        return;
    }
    // scrim over the whole window: the modal owns the input
    L_.cfScrim = {0, 0, cw_, ch_};
    dr::Fill(g, L_.cfScrim, dr::C(0x2B2723, 110));

    const int w = d(420);
    const int h = d(168);
    const int cx = cw_ / 2, cy = ch_ / 2;
    L_.cfPanel = {cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2};
    dr::FillRound(g, dr::RF(L_.cfPanel), d(12), dr::C(theme::kPanel));
    dr::StrokeRound(g, dr::RF(L_.cfPanel), d(12), dr::C(theme::kBorderStrong), 1.2f);

    // title + the message (which cable is it?)
    const CableLink& ln = pr_.links[(size_t)ui_.confirmLink];
    const CableEnd a = ResolveCableEnd(pr_, ln.aKind, ln.a);
    const CableEnd b = ResolveCableEnd(pr_, ln.bKind, ln.b);
    RECT tr = {L_.cfPanel.left + d(20), L_.cfPanel.top + d(18),
               L_.cfPanel.right - d(20), L_.cfPanel.top + d(42)};
    dr::Text(g, L10n::T(S::CbDelRoute), Font(11.f, true), dr::C(theme::kText), tr, 0,
             true);
    RECT mr = {L_.cfPanel.left + d(20), tr.bottom + d(6), L_.cfPanel.right - d(20),
               tr.bottom + d(40)};
    std::wstring who = a.name + L"  →  " + b.name;
    dr::Text(g, std::wstring(L10n::T(S::CbDelRouteConfirm)) + L"   " + who, Font(9.f),
             dr::C(theme::kTextMuted), mr, 0, true);

    // 取消 / 确定, right aligned
    const int bw = d(96), bh = d(32);
    const int by = L_.cfPanel.bottom - d(20) - bh;
    L_.cfCancel = {L_.cfPanel.right - d(20) - bw, by, L_.cfPanel.right - d(20), by + bh};
    L_.cfOk = {L_.cfCancel.left - d(10) - bw, by, L_.cfCancel.left - d(10), by + bh};
    auto button = [&](const RECT& r, S id, bool primary, int hoverFlag) {
        const bool hov = ui_.confirmHover == hoverFlag;
        dr::FillRound(g, dr::RF(r), d(6),
                      dr::C(primary ? (hov ? 0x8A6252 : theme::kAccent)
                                    : (hov ? theme::kHover : theme::kPanelAlt)));
        dr::StrokeRound(g, dr::RF(r), d(6),
                        dr::C(primary ? theme::kAccent : theme::kBorderStrong), 1.f);
        dr::Text(g, L10n::T(id), Font(9.f),
                 dr::C(primary ? 0xFFFFFF : theme::kText), r, 1, true);
    };
    button(L_.cfCancel, S::SrCancel, false, 2);
    button(L_.cfOk, S::SrOk, true, 1);
}

// ---------------------------------------------------------------------------
// big-group confirmation: the same centred modal as the delete-route one.  A group
// with 3+ members, or whose members are more than 5 m apart, becomes ONE cable
// that converges (collector stubs) — so the user confirms it first.
// ---------------------------------------------------------------------------
void App::PaintBindConfirm(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    if (!ui_.bindConfirmOpen) {
        L_.bgScrim = L_.bgPanel = L_.bgOk = L_.bgCancel = L_.bgTitle = L_.bgMsg = {};
        return;
    }
    L_.bgScrim = {0, 0, cw_, ch_};
    dr::Fill(g, L_.bgScrim, dr::C(0x2B2723, 110));

    const int w = d(460), h = d(184);
    const int cx = cw_ / 2, cy = ch_ / 2;
    L_.bgPanel = {cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2};
    dr::FillRound(g, dr::RF(L_.bgPanel), d(12), dr::C(theme::kPanel));
    dr::StrokeRound(g, dr::RF(L_.bgPanel), d(12), dr::C(theme::kBorderStrong), 1.2f);

    double maxD = 0.0;
    for (size_t i = 0; i < ui_.bindIds.size(); ++i)
        for (size_t j = i + 1; j < ui_.bindIds.size(); ++j) {
            const Item* a = pr_.Find(ui_.bindIds[i]);
            const Item* b = pr_.Find(ui_.bindIds[j]);
            if (!a || !b) continue;
            maxD = (std::max)(maxD, std::hypot(a->x - b->x, a->y - b->y));
        }

    L_.bgTitle = {L_.bgPanel.left + d(20), L_.bgPanel.top + d(16),
                  L_.bgPanel.right - d(20), L_.bgPanel.top + d(40)};
    dr::Text(g, L10n::T(S::BindBigTitle), Font(11.f, true), dr::C(theme::kText),
             L_.bgTitle, 0, true);
    L_.bgMsg = {L_.bgPanel.left + d(20), L_.bgTitle.bottom + d(8),
                L_.bgPanel.right - d(20), L_.bgTitle.bottom + d(64)};
    dr::TextWrap(g, dr::Fmt(L10n::T(S::BindBigFmt), (int)ui_.bindIds.size(), maxD),
                 Font(9.f), dr::C(theme::kTextMuted), L_.bgMsg, 0);

    const int bw = d(96), bh = d(32);
    const int by = L_.bgPanel.bottom - d(20) - bh;
    L_.bgCancel = {L_.bgPanel.right - d(20) - bw, by, L_.bgPanel.right - d(20), by + bh};
    L_.bgOk = {L_.bgCancel.left - d(10) - bw, by, L_.bgCancel.left - d(10), by + bh};
    auto button = [&](const RECT& r, S id, bool primary, int hoverFlag) {
        const bool hov = ui_.bindHover == hoverFlag;
        dr::FillRound(g, dr::RF(r), d(6),
                      dr::C(primary ? (hov ? 0x8A6252 : theme::kAccent)
                                    : (hov ? theme::kHover : theme::kPanelAlt)));
        dr::StrokeRound(g, dr::RF(r), d(6),
                        dr::C(primary ? theme::kAccent : theme::kBorderStrong), 1.f);
        dr::Text(g, L10n::T(id), Font(9.f),
                 dr::C(primary ? 0xFFFFFF : theme::kText), r, 1, true);
    };
    button(L_.bgCancel, S::SrCancel, false, 2);
    button(L_.bgOk, S::SrOk, true, 1);
}
void App::RequestDeleteCable(int linkIndex) {
    if (RefuseLocked()) return;
    if (linkIndex < 0 || linkIndex >= (int)pr_.links.size()) return;
    ui_.confirmLink = linkIndex;
    ui_.confirmHover = 0;
    InvalidateAll();
}

void App::ConfirmDeleteCable() {
    if (RefuseLocked()) return;
    const int li = ui_.confirmLink;
    ui_.confirmLink = -1;
    ui_.confirmHover = 0;
    if (li < 0 || li >= (int)pr_.links.size()) {
        InvalidateAll();
        return;
    }
    const CableEnd a = ResolveCableEnd(pr_, pr_.links[(size_t)li].aKind,
                                       pr_.links[(size_t)li].a);
    const CableEnd b = ResolveCableEnd(pr_, pr_.links[(size_t)li].bKind,
                                       pr_.links[(size_t)li].b);
    StatusMsg(dr::Fmt(L10n::T(S::LinkDeletedFmt), a.name.c_str(), b.name.c_str()));
    PushUndoSnapshot();   // deleting a route is undoable
    pr_.links.erase(pr_.links.begin() + li);
    RebuildAllCablePaths(pr_, MicRadiusM());   // lengths + the derived topology
    ui_.hoverCable = -1;
    ui_.hoverCbRoute = -1;
    ui_.hoverCbRouteDel = -1;
    ui_.pendingKind = 0;
    ui_.pendingId = 0;
    Layout();   // the route list shrank: relayout the scrollable panel
    InvalidateAll();
}

// ---------------------------------------------------------------------------
// input in the cabling inspector
// ---------------------------------------------------------------------------
bool App::CablingInspectorDown(int x, int y) {    POINT p = {x, y};

    for (int i = 0; i < kAddDeviceCount; ++i) {
        if (L_.cbAdd[i].right <= L_.cbAdd[i].left || !PtInRect(&L_.cbAdd[i], p))
            continue;
        AddDevice(AddKindAt(i));
        return true;
    }

    for (size_t i = 0; i < L_.cbRow.size() && i < pr_.devices.size(); ++i) {
        if (!PtInRect(&L_.cbRow[i], p)) continue;
        const int id = pr_.devices[i].id;
        pr_.Deselect();
        pr_.ClearDeviceSelection();
        pr_.SelectDevice(id);
        SyncEdits();
        Layout();
        InvalidateAll();
        return true;
    }

    if (L_.cbDel.right > L_.cbDel.left && PtInRect(&L_.cbDel, p)) {
        DeleteSelectedDevice();
        return true;
    }

    // 05 microphone inspector: 吊装 on/off (the very same toggle as the 01/02
    // checkbox — one shared path, so the height default and the re-route match)
    if (L_.flownRow.right > L_.flownRow.left && PtInRect(&L_.flownRow, p)) {
        ToggleFlown();
        return true;
    }

    // 05 microphone inspector: 「删除路由」 (a confirmation modal follows — the
    // link is only removed on 确定)
    for (size_t k = 0; k < L_.cbRouteDel.size(); ++k) {
        if (L_.cbRouteDel[k].right <= L_.cbRouteDel[k].left ||
            !PtInRect(&L_.cbRouteDel[k], p))
            continue;
        if (k < L_.cbRouteLink.size()) RequestDeleteCable(L_.cbRouteLink[k]);
        return true;
    }

    // 05 「连接」: click a device row to wire the selected microphone to it.  This
    // is the SAME path the right-click pair uses (CreateCable): one cable per
    // microphone / per bound group, an existing route is REPLACED in the same
    // undoable step, and the exclusion matrix still applies.
    for (size_t k = 0; k < L_.cbConnRow.size() && k < L_.cbConnDev.size(); ++k) {
        if (L_.cbConnRow[k].right <= L_.cbConnRow[k].left ||
            !PtInRect(&L_.cbConnRow[k], p))
            continue;
        Item* mic = pr_.Selected();
        if (!mic || mic->kind != ItemKind::Mic) return true;
        ConnectMicToDevice(mic->id, L_.cbConnDev[k]);
        return true;
    }
    return false;   // click inside the cabling panel but not on a control: no-op
}

// The ONE click-to-connect action: the 布线 page's 「连接」 block and the 04 链路 /
// 路由 popup both call this, and it is the same model path the right-click pair
// takes (CreateCable → one cable per microphone or bound group, replaced in one
// undoable step, exclusion matrix respected).
bool App::ConnectMicToDevice(int micId, int devId) {
    const Item* mic = pr_.Find(micId);
    if (!mic || mic->kind != ItemKind::Mic) return false;
    if (!pr_.FindDevice(devId)) return false;
    if (RefuseLocked()) return false;
    if (!CanConnect(0, devId, 1, micId)) {
        std::wstring why;
        CanConnectRule(pr_, 0, devId, 1, micId, &why);
        StatusMsg(why.empty() ? std::wstring(L10n::T(S::LinkRejected)) : why, true);
        return false;
    }
    if (MicConnectedDevice(micId) == devId) {
        StatusMsg(L10n::T(S::LinkAlready));          // already wired to this device
        return false;
    }
    CreateCable(0, devId, 1, micId);
    // the status line names the DEVICE the microphone just landed on
    const Device* dd = pr_.FindDevice(devId);
    StatusMsg(dr::Fmt(L10n::T(S::CbConnDoneFmt),
                      dd ? DevLabelText(*dd).c_str() : L""));
    Layout();
    InvalidateAll();
    return true;
}

// the device endpoint of the cable feeding `micId` (or its bound group's cable).
// The cable stores its two ends by KIND (0 = device, 1 = microphone), so either
// orientation resolves here.
int App::MicConnectedDevice(int micId) const {
    if (micId <= 0) return -1;
    const CableLink* f = FindMicCableForItem(pr_, pr_.links, micId);
    if (!f) return -1;
    if (f->aKind == 0) return f->a;
    if (f->bKind == 0) return f->b;
    return -1;
}

// ---------------------------------------------------------------------------
// cables: painting, hit-testing and the right-click wiring interaction
// ---------------------------------------------------------------------------
namespace {

// midpoint of a cable polyline, in world metres (for the channel/length tag)
std::pair<double, double> PolyMid(const CableLink& ln) {
    if (ln.path.empty()) return {0.0, 0.0};
    const double total = CablePolylineLen(ln);
    if (total <= 1e-9) return ln.path.front();
    const double half = total * 0.5;
    double acc = 0.0;
    for (size_t i = 1; i < ln.path.size(); ++i) {
        const double dx = ln.path[i].first - ln.path[i - 1].first;
        const double dy = ln.path[i].second - ln.path[i - 1].second;
        const double seg = std::sqrt(dx * dx + dy * dy);
        if (seg > 1e-9 && acc + seg >= half) {
            const double t = (half - acc) / seg;
            return {ln.path[i - 1].first + t * dx, ln.path[i - 1].second + t * dy};
        }
        acc += seg;
    }
    return ln.path.back();
}

// point at fraction t (0..1) along a raw polyline, by arc length — used to place
// the per-capsule tags apart along their run
std::pair<double, double> PolyAt(const std::vector<std::pair<double, double>>& p,
                                 double t) {
    if (p.empty()) return {0.0, 0.0};
    if (p.size() == 1) return p.front();
    const double total = PolyLen(p);
    if (total <= 1e-9) return p.front();
    const double want = total * std::clamp(t, 0.0, 1.0);
    double acc = 0.0;
    for (size_t i = 1; i < p.size(); ++i) {
        const double dx = p[i].first - p[i - 1].first;
        const double dy = p[i].second - p[i - 1].second;
        const double seg = std::sqrt(dx * dx + dy * dy);
        if (seg > 1e-9 && acc + seg >= want) {
            const double f = (want - acc) / seg;
            return {p[i - 1].first + f * dx, p[i - 1].second + f * dy};
        }
        acc += seg;
    }
    return p.back();
}

// channels > capacity of one of the endpoints → the cable is red + "!"
bool CableOver(const Project& pr, const CableLink& ln, std::wstring* why) {
    return CableProblemText(pr, ln, why);
}

} // namespace

// label placement helpers (drawing only): the collision set is filled with the
// device boxes and the mic markers first, then every tag/label claims a free spot
namespace {
bool RectHit(const RECT& a, const RECT& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

// ---------------------------------------------------------------------------
// Channel numbers of the 03 通道表, keyed by (itemId, capsule).  ONE source of
// truth for the numbers the 05 microphone labels and the wiring tags of this page
// both show: the tag 「03 话筒 7」 has to name the very row the table numbers, so
// the map is built from BuildChannelRows — the table's own order.
// ---------------------------------------------------------------------------
struct ChanNoMap {
    std::map<std::pair<int, int>, int> of;   // (itemId, capsule) -> channel
    std::map<int, int> first;                // itemId -> its first channel
    int get(int itemId, int capsule) const {
        auto k = of.find(std::make_pair(itemId, capsule));
        if (k != of.end()) return k->second;
        auto f = first.find(itemId);
        return f == first.end() ? 0 : f->second;
    }
};

ChanNoMap BuildChanNoMap(const std::vector<App::ChRow>& rows) {
    ChanNoMap m;
    for (const auto& r : rows) {
        if (r.summary || r.separator || r.itemId <= 0 || r.ch <= 0) continue;
        m.of.emplace(std::make_pair(r.itemId, r.capsule), r.ch);
        m.first.emplace(r.itemId, r.ch);   // emplace keeps the FIRST row
    }
    return m;
}

// ---------------------------------------------------------------------------
// GLOBAL tag layout for the wiring page (screen AND PNG export use this one pass,
// so both pages look identical).
//
// The old code placed every tag greedily in link order against a growing "taken"
// list: an early tag grabbed the middle of the page and the later ones were pushed
// onto other cables, far from their own run.  Here every tag first collects its
// candidate slots on a HALF-PILL grid along / across its OWN cable, the most
// constrained tag is placed first, and two relaxation sweeps afterwards let a tag
// push its neighbour.  Device boxes (with their name slots) are HARD obstacles, the
// other cables and the placed tags are soft costs (overlap area), and distance from
// the tag's home point on its own cable is priced in pills, so a tag only travels
// when it buys a free slot — never further than ~1.5 pill lengths.
// ---------------------------------------------------------------------------
struct TagItem {
    std::wstring text;
    bool over = false;
    bool hover = false;
    float cx = 0.f, cy = 0.f;   // home point (tag anchor, on its own drawn run)
    float ux = 1.f, uy = 0.f;   // own run direction (unit)
    float halfLen = 0.f;        // half the drawn run length (px)
    int wT = 0, hT = 0;         // pill size
    Gdiplus::Color ink;         // pill text ink (= the cable's own run ink)
    size_t runIdx = 0;          // index into the run polyline list
    RECT r{};                   // chosen slot
    int nStrict = -1, nAllow = -1;   // diagnostic: candidate counts
    bool fellBack = false;
    float capMul = 1.f;              // distance cap used (pill lengths)
};

// length of segment a-b inside rect r (Liang-Barsky)
static float SegLenInRect(const Gdiplus::PointF& a, const Gdiplus::PointF& b,
                          const RECT& r) {
    float t0 = 0.f, t1 = 1.f;
    const float dx = b.X - a.X, dy = b.Y - a.Y;
    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {a.X - (float)r.left, (float)r.right - a.X,
                        a.Y - (float)r.top, (float)r.bottom - a.Y};
    for (int k = 0; k < 4; ++k) {
        if (std::fabs(p[k]) < 1e-9f) {
            if (q[k] < 0.f) return 0.f;
            continue;
        }
        const float t = q[k] / p[k];
        if (p[k] < 0.f) {
            if (t > t1) return 0.f;
            if (t > t0) t0 = t;
        } else {
            if (t < t0) return 0.f;
            if (t < t1) t1 = t;
        }
    }
    return (t1 - t0) * std::hypot(dx, dy);
}

static long long RectOverlapArea(const RECT& a, const RECT& b) {
    const int w = (std::min)(a.right, b.right) - (std::max)(a.left, b.left);
    const int h = (std::min)(a.bottom, b.bottom) - (std::max)(a.top, b.top);
    return (w > 0 && h > 0) ? (long long)w * h : 0;
}

// distance from point p to the polyline (px)
static float PolyDist(const std::vector<Gdiplus::PointF>& p, float px, float py) {
    float best = 1e9f;
    for (size_t i = 1; i < p.size(); ++i) {
        const float dx = p[i].X - p[i - 1].X, dy = p[i].Y - p[i - 1].Y;
        const float l2 = dx * dx + dy * dy;
        float t = 0.f;
        if (l2 > 1e-9f)
            t = std::clamp(((px - p[i - 1].X) * dx + (py - p[i - 1].Y) * dy) / l2, 0.f,
                           1.f);
        const float qx = p[i - 1].X + dx * t, qy = p[i - 1].Y + dy * t;
        const float d = std::hypot(px - qx, py - qy);
        if (d < best) best = d;
    }
    return best;
}

// place every tag; devBoxes are HARD (never covered), polys/tags are soft costs
static void LayoutTags(std::vector<TagItem>& tags,
                       const std::vector<RECT>& devBoxes,
                       const std::vector<std::vector<Gdiplus::PointF>>& polys,
                       const RECT& canvas, int inset) {
    const size_t n = tags.size();
    if (n == 0) return;

    auto gridFor = [&](const TagItem& t, std::vector<RECT>& out, bool allowDev,
                       float capMul) {
        // half-pill grid: up to one pill length along the own cable, and up to
        // four pill heights across it — every slot that survives the HARD cap below
        // is within ONE pill length of the tag's own cable polyline
        static const int kAlong[] = {0, 1, -1, 2, -2, 3, -3, 4, -4};   // × wT/2
        static const int kPerp[] = {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5,
                                    6, -6, 7, -7, 8, -8, 9, -9, 10, -10,
                                    11, -11, 12, -12};               // × hT/2
        out.clear();
        out.reserve(85);
        const auto& own = polys[t.runIdx];
        for (int pi : kPerp) {
            for (int ai : kAlong) {
                const float as = (float)ai * (float)t.wT * 0.5f;
                const float ps = (float)pi * (float)t.hT * 0.5f;
                const float mx = t.cx + t.ux * as - t.uy * ps;
                const float my = t.cy + t.uy * as + t.ux * ps;
                const LONG l = (LONG)std::lround(mx) - t.wT / 2;
                const LONG tp = (LONG)std::lround(my) - t.hT / 2;
                RECT r{l, tp, l + t.wT, tp + t.hT};
                if (r.left < canvas.left + inset || r.right > canvas.right - inset ||
                    r.top < canvas.top + inset || r.bottom > canvas.bottom - inset)
                    continue;
                // HARD: never further than ONE pill length from the own cable
                if (!own.empty() && PolyDist(own, mx, my) > (float)t.wT * capMul)
                    continue;
                if (!allowDev) {
                    bool hard = false;   // a device box or its name is never covered
                    for (const RECT& b : devBoxes)
                        if (RectHit(b, r)) {
                            hard = true;
                            break;
                        }
                    if (hard) continue;
                }
                out.push_back(r);
            }
        }
    };
    // a tag never touches another tag while any free slot exists: a tag-tag
    // overlap carries a huge fixed penalty on top of its area
    const double kTagOverlapPenalty = 1.0e7;

    // cost: overlap area (other cables + placed tags) + distance from the tag's
    // home point, priced so one pill length of travel costs four pills of overlap
    auto cost = [&](const TagItem& t, const RECT& r,
                    const std::vector<RECT>& placed) {
        const long long pill = (long long)t.wT * (long long)t.hT;
        long long ov = 0;
        long long ovTag = 0;
        for (size_t k = 0; k < polys.size(); ++k) {
            if (k == t.runIdx) continue;   // a tag on its OWN cable is fine
            const auto& p = polys[k];
            for (size_t s = 1; s < p.size(); ++s) {
                const float len = SegLenInRect(p[s - 1], p[s], r);
                if (len > 0.5f) ov += 2 * (long long)(len * (float)t.hT);
            }
        }
        for (const RECT& o : placed) ovTag += RectOverlapArea(r, o);
        // a device box / name / mic marker is a HARD obstacle in the normal grid;
        // the "no slot at all" grid still prices it, so it is only covered when
        // nothing else on the canvas is left
        long long ovDev = 0;
        for (const RECT& b : devBoxes) ovDev += RectOverlapArea(r, b);
        const float mx = (float)((r.left + r.right) / 2);
        const float my = (float)((r.top + r.bottom) / 2);
        const float ddx = mx - t.cx, ddy = my - t.cy;
        const float along = ddx * t.ux + ddy * t.uy;
        const float perp = -ddx * t.uy + ddy * t.ux;
        const float distPills = std::hypot(along / (float)t.wT, perp / (float)t.hT);
        const float beyond =
            (std::max)(0.f, std::fabs(along) - t.halfLen) / (float)t.wT;
        return (double)ov + (double)ovTag +
               (ovTag > 0 ? kTagOverlapPenalty : 0.0) +
               (double)ovDev * 40.0 + (ovDev > 0 ? 1.0e5 : 0.0) +
               4.0 * (double)pill * (double)distPills +
               10.0 * (double)pill * (double)beyond;
    };

    // 1) most constrained first: fewest cable/device-free slots
    std::vector<size_t> order(n);
    std::vector<int> freeN(n, 0);
    for (size_t i = 0; i < n; ++i) {
        order[i] = i;
        std::vector<RECT> c;
        gridFor(tags[i], c, false, 1.6f);
        for (const RECT& r : c)
            if (cost(tags[i], r, {}) == 0.0) ++freeN[i];
    }
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return freeN[a] < freeN[b]; });

    // 2) place
    std::vector<RECT> placed;
    placed.reserve(n);
    int fallbacks = 0;
    for (size_t oi = 0; oi < n; ++oi) {
        const size_t i = order[oi];
        std::vector<RECT> c;
        // The tags are small now (screen-size text on a scaleMul-times drawing), so
        // there is a lot of empty space: the search may travel up to TWO pill lengths
        // along/perpendicular to its own cable to reach a real gap instead of sitting
        // on top of another cable.  Still glued to its own run (cap is measured with
        // PolyDist to that run's polyline).
        float capMul = 1.0f;   // one pill length first, then the 2-pill budget
        for (float cm : {1.0f, 1.5f, 2.0f}) {
            gridFor(tags[i], c, false, cm);
            capMul = cm;
            if (!c.empty()) break;
        }
        tags[i].nStrict = (int)c.size();
        if (c.empty()) {   // only now is a device box allowed
            gridFor(tags[i], c, true, 2.0f);
            capMul = 2.0f;
            tags[i].fellBack = true;
            ++fallbacks;
        }
        tags[i].nAllow = (int)c.size();
        tags[i].capMul = capMul;
        double bestCost = -1.0;
        RECT best{};
        for (const RECT& r : c) {
            const double v = cost(tags[i], r, placed);
            if (bestCost < 0.0 || v < bestCost) {
                bestCost = v;
                best = r;
            }
        }
        if (bestCost < 0.0) {   // the grid is empty (tiny canvas): stay on the anchor
            const LONG l = (LONG)std::lround(tags[i].cx) - tags[i].wT / 2;
            const LONG tp = (LONG)std::lround(tags[i].cy) - tags[i].hT / 2;
            RECT r{l, tp, l + tags[i].wT, tp + tags[i].hT};
            if (r.left < canvas.left + inset) {
                r.right += canvas.left + inset - r.left;
                r.left = canvas.left + inset;
            }
            if (r.right > canvas.right - inset) {
                r.left -= r.right - (canvas.right - inset);
                r.right = canvas.right - inset;
            }
            if (r.top < canvas.top + inset) {
                r.bottom += canvas.top + inset - r.top;
                r.top = canvas.top + inset;
            }
            if (r.bottom > canvas.bottom - inset) {
                r.top -= r.bottom - (canvas.bottom - inset);
                r.bottom = canvas.bottom - inset;
            }
            best = r;
            ++fallbacks;
        }
        tags[i].r = best;
        placed.push_back(best);
    }
    g_micStackDump.push_back(
        dr::Fmt(L"taglayout %d tags fallback %d", (int)n, fallbacks));

    // 3) relaxation: deterministic sweeps; a tag may push its neighbour.  A sweep
    //    can trade one overlap for another, so only a state that is not worse than
    //    the best seen (fewest overlapping pairs first, then least area) is kept.
    auto evalState = [&]() {
        int pairs = 0;
        long long area = 0;
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                const long long a = RectOverlapArea(tags[i].r, tags[j].r);
                if (a > 0) {
                    ++pairs;
                    area += a;
                }
            }
        }
        int devPairs = 0;
        for (size_t i = 0; i < n; ++i)
            for (const RECT& b : devBoxes) {
                const long long a = RectOverlapArea(tags[i].r, b);
                if (a > 0) {
                    ++devPairs;
                    area += a;
                }
            }
        return std::make_pair(pairs + devPairs, area);
    };
    auto snapshot = [&]() {
        std::vector<RECT> v(n);
        for (size_t i = 0; i < n; ++i) v[i] = tags[i].r;
        return v;
    };
    auto restore = [&](const std::vector<RECT>& v) {
        for (size_t i = 0; i < n; ++i) tags[i].r = v[i];
    };
    std::pair<int, long long> bestEval = evalState();
    std::vector<RECT> bestState = snapshot();
    for (int sweep = 0; sweep < 5; ++sweep) {
        bool changed = false;
        for (size_t oi = 0; oi < n; ++oi) {
            const size_t i = order[oi];
            std::vector<RECT> others;
            others.reserve(n);
            for (size_t j = 0; j < n; ++j)
                if (j != i) others.push_back(tags[j].r);
            std::vector<RECT> c;
            gridFor(tags[i], c, false, tags[i].capMul);
            if (tags[i].capMul < 2.0f) {
                // a crowded tag may also use the full ~2-pill travel budget in the
                // sweeps: the distance cost keeps it close when a close slot is free
                std::vector<RECT> wider;
                gridFor(tags[i], wider, false, 2.0f);
                c.insert(c.end(), wider.begin(), wider.end());
            }
            if (c.empty()) gridFor(tags[i], c, false, 2.0f);
            if (c.empty()) gridFor(tags[i], c, true, 2.0f);
            if (c.empty()) continue;
            RECT best = tags[i].r;
            double bestCost = cost(tags[i], tags[i].r, others);
            for (const RECT& r : c) {
                const double v = cost(tags[i], r, others);
                if (v < bestCost - 1e-9) {
                    bestCost = v;
                    best = r;
                }
            }
            if (best.left != tags[i].r.left || best.top != tags[i].r.top) changed = true;
            tags[i].r = best;
        }
        const auto e = evalState();
        if (e < bestEval) {
            bestEval = e;
            bestState = snapshot();
        }
        if (!changed) break;
    }
    restore(bestState);
    g_micStackDump.push_back(dr::Fmt(L"taglayout %d tags fallback %d overlaps %d area %lld",
                                     (int)n, fallbacks, bestEval.first,
                                     bestEval.second));
}
} // namespace
void App::StatusMsg(const std::wstring& text, bool warn) {
    ui_.msg = text;
    ui_.msgWarn = warn ? 1 : 0;
    ui_.msgUntil = GetTickCount() + 2600;
    if (hwnd_) SetTimer(hwnd_, 7, 2800, nullptr);
    InvalidateAll();
}

void App::PaintCables(Gdiplus::Graphics& g) {
    // tag pills, their paddings and the nudge steps are text HOSTS: in a PNG export
    // they have to grow with the boosted font, otherwise the glyphs overflow the
    // white pill and two tags land on top of each other.  gb is 1 on screen, so the
    // on-screen layout is untouched.
    const float gb = CablingBoost();
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_ * gb); };

    // the 03 通道表 numbering, built ONCE per paint: the tags on this page name the
    // table's own rows (「03 话筒 7」), exactly like the 05 microphone labels
    const ChanNoMap chan = BuildChanNoMap(BuildChannelRows());

    // HARD obstacles for the tag layout: every device box with its name slot, and
    // every microphone marker (a tag may never cover one).  The cables and the
    // placed tags are soft costs, priced by the layout pass.
    std::vector<RECT> devBoxes;
    for (const auto& dv : pr_.devices) {
        RECT box{}, lbl{};
        if (DeviceRects(dv, ppm_, ox_, oy_, d(6), box, lbl)) {
            devBoxes.push_back(box);
            if (lbl.bottom > lbl.top) {
                // the name obstacle is the DRAWN text band under the box; the
                // padded label slot is far wider than the text and used to black
                // out a whole band of free positions
                const float ntw = dr::TextW(g, DevLabelText(dv), Font(8.5f));
                const LONG bcx = (box.left + box.right) / 2;
                const LONG nh = (LONG)(ntw * 0.5f) + d(2);
                RECT nm{bcx - nh, box.bottom + d(2), bcx + nh, box.bottom + d(17)};
                nm.left = (std::max)(nm.left, lbl.left);
                nm.right = (std::min)(nm.right, lbl.right);
                devBoxes.push_back(nm);
            }
        }
    }
    for (const auto& it : pr_.items) {
        if (it.kind != ItemKind::Mic) continue;
        std::vector<std::pair<double, double>> cs;
        if (MicCapsuleCount(it) >= 2) cs = MicCapsulePos(it);
        else cs.push_back({it.x, it.y});
        for (const auto& c : cs) {
            const int cx = (int)std::lround(ox_ + c.first * ppm_);
            const int cy = (int)std::lround(oy_ + c.second * ppm_);
            const int r = (int)MicMarkerRadiusPx() + d(6);
            devBoxes.push_back(RECT{cx - r, cy - r, cx + r, cy + r});
        }
    }

    // ---- pass 1: every drawn run in screen space ----------------------------
    struct ScrRun {
        size_t link = 0;
        std::vector<Gdiplus::PointF> pts;
        Gdiplus::PointF off{0.f, 0.f};   // cosmetic separation only, never geometry
    };
    std::vector<ScrRun> runs;
    std::vector<std::vector<size_t>> perLink(pr_.links.size());
    for (size_t i = 0; i < pr_.links.size(); ++i) {
        const CableLink& ln = pr_.links[i];
        if (ln.path.size() < 2 && ln.capsulePaths.empty()) continue;
        std::vector<std::vector<std::pair<double, double>>> rl;
        if (!ln.capsulePaths.empty()) rl = ln.capsulePaths;
        else rl.push_back(ln.path);
        for (const auto& run : rl) {
            if (run.size() < 2) { perLink[i].push_back(SIZE_MAX); continue; }
            ScrRun sr;
            sr.link = i;
            sr.pts.reserve(run.size());
            for (const auto& pt : run)
                sr.pts.emplace_back((float)(ox_ + pt.first * ppm_),
                                    (float)(oy_ + pt.second * ppm_));
            perLink[i].push_back(runs.size());
            runs.push_back(std::move(sr));
        }
    }

    // ---- pass 2: nudge near-parallel runs apart (DRAWING ONLY) --------------
    // Two runs that are parallel and closer than ~8 px on screen read as ONE
    // line, so the painter shifts them a few pixels perpendicular (±3 px, sign
    // from geometry, ties broken by link index).  Lengths, channel counts, hit
    // tests and the 03 / 04 data keep the TRUE path — this is purely cosmetic.
    // cosmetic lane separation is POSITIONING: express it in metres and multiply
    // by the live ppm, so it scales with the drawing (2x/4x/8x exports included).
    const float kRefPpm = 56.0f;
    const float amp = (float)((3.0 / kRefPpm) * ppm_);
    const float closeD = (float)d(7);
    auto domDir = [](const std::vector<Gdiplus::PointF>& p) {
        Gdiplus::PointF u{1.f, 0.f};
        float best = -1.f;
        for (size_t k = 1; k < p.size(); ++k) {
            const float dx = p[k].X - p[k - 1].X, dy = p[k].Y - p[k - 1].Y;
            const float l = dx * dx + dy * dy;
            if (l > best) {
                best = l;
                const float len = std::sqrt(l);
                if (len > 1e-6f) u = Gdiplus::PointF{dx / len, dy / len};
            }
        }
        return u;
    };
    auto segDist = [](Gdiplus::PointF p, Gdiplus::PointF a, Gdiplus::PointF b) {
        const float dx = b.X - a.X, dy = b.Y - a.Y;
        const float l2 = dx * dx + dy * dy;
        float t = 0.f;
        if (l2 > 1e-9f)
            t = std::clamp(((p.X - a.X) * dx + (p.Y - a.Y) * dy) / l2, 0.f, 1.f);
        return std::hypot(p.X - (a.X + dx * t), p.Y - (a.Y + dy * t));
    };
    for (size_t i = 0; ui_.noCableNudge ? false : i < runs.size(); ++i) {
        for (size_t j = i + 1; j < runs.size(); ++j) {
            if (runs[i].link == runs[j].link) continue;   // one cable's own runs
            const Gdiplus::PointF ui = domDir(runs[i].pts);
            const Gdiplus::PointF uj = domDir(runs[j].pts);
            const float crs = ui.X * uj.Y - ui.Y * uj.X;
            if (std::fabs(crs) > 0.25f) continue;         // not parallel (~15°)
            bool isNear = false;
            for (size_t a = 1; a < runs[i].pts.size() && !isNear; ++a) {
                for (size_t b = 1; b < runs[j].pts.size() && !isNear; ++b) {
                    const Gdiplus::PointF a0 = runs[i].pts[a - 1], a1 = runs[i].pts[a];
                    const Gdiplus::PointF b0 = runs[j].pts[b - 1], b1 = runs[j].pts[b];
                    const Gdiplus::PointF ma{(a0.X + a1.X) * .5f, (a0.Y + a1.Y) * .5f};
                    const Gdiplus::PointF mb{(b0.X + b1.X) * .5f, (b0.Y + b1.Y) * .5f};
                    const float dist = (std::min)(
                        (std::min)(segDist(mb, a0, a1), segDist(ma, b0, b1)),
                        (std::min)(segDist(b0, a0, a1), segDist(b1, a0, a1)));
                    if (dist <= closeD) isNear = true;
                }
            }
            if (!isNear) continue;
            // which side is run j on?  push the two apart along run i's normal
            const Gdiplus::PointF pi0 = runs[i].pts.front();
            const Gdiplus::PointF pj0 = runs[j].pts.front();
            float side = ui.X * (pj0.Y - pi0.Y) - ui.Y * (pj0.X - pi0.X);
            if (std::fabs(side) < 0.5f)   // exactly on top of each other
                side = ((runs[i].link + runs[j].link) % 2) ? 1.f : -1.f;
            const float sgn = (side >= 0.f) ? 1.f : -1.f;
            const Gdiplus::PointF perp{-ui.Y, ui.X};
            runs[i].off.X -= perp.X * amp * sgn;
            runs[i].off.Y -= perp.Y * amp * sgn;
            runs[j].off.X += perp.X * amp * sgn;
            runs[j].off.Y += perp.Y * amp * sgn;
        }
    }
    for (auto& r : runs) {
        const float l = std::hypot(r.off.X, r.off.Y);
        const float lim = amp * 1.6f;
        if (l > lim) {
            r.off.X *= lim / l;
            r.off.Y *= lim / l;
        }
    }
    // every drawn cable as a polyline (screen px, cosmetic lane offset included):
    // the global tag layout treats the OTHER cables as obstacles
    std::vector<std::vector<Gdiplus::PointF>> polys(runs.size());
    for (size_t k = 0; k < runs.size(); ++k) {
        polys[k].reserve(runs[k].pts.size());
        for (const auto& p : runs[k].pts)
            polys[k].emplace_back(p.X + runs[k].off.X, p.Y + runs[k].off.Y);
    }

    // ---- pass 3: draw ------------------------------------------------------
    std::vector<TagItem> tags;   // collected here, placed by the global pass below
    for (size_t i = 0; i < pr_.links.size(); ++i) {
        const CableLink& ln = pr_.links[i];
        if (ln.path.size() < 2 && ln.capsulePaths.empty()) continue;
        std::wstring why;
        const bool over = CableOver(pr_, ln, &why);
        const bool hover = ui_.hoverCable == (int)i;
        // ONE colour family per 声部: the run, its collector stubs and its tag wear
        // the ink of the microphone the cable carries — the same instrument / 声部
        // tint its marker wears (MicBoundInk).  An unbound microphone and a
        // device ↔ device trunk keep the neutral cable grey; an overloaded cable is
        // red regardless (over-capacity marking keeps its own colour).
        const Item* micEnd = nullptr;
        if (ln.aKind == 1) micEnd = pr_.Find(ln.a);
        if (!micEnd && ln.bKind == 1) micEnd = pr_.Find(ln.b);
        const Rgb runInk =
            MicInkLine((micEnd && micEnd->kind == ItemKind::Mic)
                           ? MicBoundInk(pr_, *micEnd)
                           : Rgb{0x6B, 0x63, 0x5A});
        const Gdiplus::Color col = over ? dr::C(theme::kDanger) : dr::C(runInk);

        // ONE cable can carry several parallel runs: the main microphone's capsules
        // (2 for AB, 3 for a Decca tree) or every member of a bound MicGroup.  Each
        // run gets its own line, its own 1-channel count and its own length tag,
        // offset along the run so they never stack.
        std::vector<std::vector<std::pair<double, double>>> rl;
        if (!ln.capsulePaths.empty()) rl = ln.capsulePaths;
        else rl.push_back(ln.path);
        const int nRuns = (int)rl.size();
        const bool manualLen = CableOverrideLen(pr_, ln) > 0.0;
        for (int ri = 0; ri < nRuns; ++ri) {
            const auto& run = rl[(size_t)ri];
            if (run.size() < 2) continue;
            if (ri >= (int)perLink[i].size() || perLink[i][(size_t)ri] == SIZE_MAX)
                continue;
            const ScrRun& srun = runs[perLink[i][(size_t)ri]];
            std::vector<Gdiplus::PointF> pts;
            pts.reserve(srun.pts.size());
            for (const auto& p : srun.pts)
                pts.emplace_back(p.X + srun.off.X, p.Y + srun.off.Y);
            auto stroke = [&](size_t first, size_t count, bool dashed) {
                if (count < 2) return;
                Gdiplus::Pen pen(col, hover ? 2.8f : 1.9f);
                pen.SetLineJoin(Gdiplus::LineJoinRound);
                pen.SetStartCap(Gdiplus::LineCapRound);
                pen.SetEndCap(Gdiplus::LineCapRound);
                if (dashed) pen.SetDashStyle(Gdiplus::DashStyleDash);
                g.DrawLines(&pen, pts.data() + first, (INT)count);
            };
            // a radio link (wireless microphone or receiver) is dashed over its
            // whole length — it is not a cable at all.  A flown microphone's
            // ceiling / wall legs are dashed too; the rest of its run is solid.
            const size_t ceilSegs = ln.flown
                ? (size_t)std::clamp(ln.ceilSegs, 0, (int)pts.size() - 1) : 0;
            if (ln.radio) {
                stroke(0, pts.size(), true);
            } else {
                if (ceilSegs > 0) stroke(0, ceilSegs + 1, true);
                stroke(ceilSegs, pts.size() - ceilSegs, false);
            }

            // channel / length tag along the run.  A radio link shows only its
            // channels ("无线 1 通道"); an ordinary cable shows channels and metres.
            // Capsule / group-member tags sit near the source end and are staggered
            // PERPENDICULAR to the run, so two or three length labels never stack
            // on each other.
            double ax = 0.0, ay = 0.0;
            {
                const double tfrac = nRuns > 1 ? 0.20 + 0.10 * (double)ri : 0.5;
                const auto a0 = PolyAt(run, tfrac);
                ax = a0.first;
                ay = a0.second;
                if (nRuns > 1) {
                    const double dx = run[1].first - run[0].first;
                    const double dy = run[1].second - run[0].second;
                    const double l = std::sqrt(dx * dx + dy * dy);
                    if (l > 1e-6) {
                        const double off =
                            ((double)ri - (nRuns - 1) * 0.5) * (18.0 / 56.0);   // metres, not px
                        ax += -dy / l * off;
                        ay += dx / l * off;
                    }
                }
            }
            const auto anchor = std::make_pair(ax, ay);
            // The connection tag is WIRING information, not a mic label: it is
            // NEVER gated by 显示话筒文字 / 显示乐器文字 (a regression had hidden
            // every cable length behind that switch).
            // The tag carries no metre value any more (the length lives in the 03
            // 通道表 and in the hover bar): it names the 03 ROW this run belongs to
            // — 「03 话筒 7」 — exactly like the 05 microphone label, so a tag can be
            // matched to the table at a glance.
            std::wstring tag;
            if (ln.radio) {
                // a radio link is not a cable: its channels only (no length, and no
                // 03 row of its own to name)
                tag = dr::Fmt(L10n::T(S::CbRadioTagFmt), ln.channels);
            } else {
                // the microphone this run carries (endpoint kind 1 = Project::items)
                const int micId = ln.aKind == 1 ? ln.a : (ln.bKind == 1 ? ln.b : -1);
                const Item* mic = nullptr;
                for (const auto& q : pr_.items)
                    if (q.id == micId && q.kind == ItemKind::Mic) { mic = &q; break; }
                if (mic) {
                    // a capsule run IS one capsule of the main microphone: its own
                    // 03 row (L = 0, C = 1, R = 2; an AB pair numbers L then R)
                    int cap = -1;
                    if (nRuns > 1 && MicCapsuleCount(*mic) >= 2)
                        cap = (mic->mainTech == 3) ? (int)ri : (int)(ri == 0 ? 0 : 2);
                    int c0 = chan.get(micId, cap);
                    int c1 = c0;
                    int nameId = micId;   // whichever 03 row the tag names
                    // ONE run for a whole bound GROUP covers every member's row, so
                    // the tag names the RANGE (「03–04 话筒 7」) of its FIRST member
                    if (nRuns == 1 && c0 > 0) {
                        const int gi = pr_.GroupOf(micId);
                        if (gi >= 0) {
                            const MicGroup& grp = pr_.micGroups[(size_t)gi];
                            if (grp.ids.size() >= 2) {
                                int f = 0, l = 0;
                                for (int id : grp.ids) {
                                    const int c = chan.get(id, -1);
                                    if (c <= 0) continue;
                                    if (f == 0) { f = c; nameId = id; }
                                    l = c;
                                }
                                if (f > 0) { c0 = f; c1 = (std::max)(l, f); }
                            }
                        }
                    }
                    // the NAME leads (「话筒 7」); the model belongs to the 03 table
                    const Item* nmSrc = mic;
                    if (nameId != micId)
                        if (const Item* q = pr_.Find(nameId)) nmSrc = q;
                    std::wstring nm = nmSrc->label;
                    if (nm.empty() && nmSrc->defId == L"mainmic")
                        nm = L10n::T(S::MainMic);
                    if (nm.empty()) nm = nmSrc->model;
                    if (nm.empty()) nm = nmSrc->defId;
                    if (cap >= 0) {
                        static const wchar_t* kCap[3] = {L"L", L"C", L"R"};
                        nm += std::wstring(L" ") + kCap[cap];
                    }
                    if (c0 > 0)
                        tag = c1 > c0 ? dr::Fmt(L"%02d–%02d %s", c0, c1, nm.c_str())
                                      : dr::Fmt(L"%02d %s", c0, nm.c_str());
                    else
                        tag = nm;   // no 03 row: the plain name still identifies it
                } else {
                    // a device ↔ device trunk has no microphone row: its channel
                    // count is all the wiring identity it has (no metres here)
                    tag = dr::Fmt(L"%d ", ln.channels) + L10n::T(S::ChNum);
                }
                // a manual length override is still marked (its VALUE stays in the
                // 03 table and in the hover bar)
                if (manualLen && nRuns == 1)
                    tag += std::wstring(L"  ") + L10n::T(S::CbCustom);
            }
            if (over) tag = L"! " + tag;
            Gdiplus::Font* f = Font(8.f, over || hover);
            const float tw = dr::TextW(g, tag, f);
            const float cx = (float)(ox_ + anchor.first * ppm_) + srun.off.X;
            const float cy = (float)(oy_ + anchor.second * ppm_) + srun.off.Y;
            float ux = (float)(run.back().first - run.front().first);
            float uy = (float)(run.back().second - run.front().second);
            float ul = std::sqrt(ux * ux + uy * uy);
            if (ul < 1e-3f) {
                ux = 1.f;
                uy = 0.f;
                ul = 1.f;
            }
            ux /= ul;
            uy /= ul;
            const int wT = (int)tw + d(10);
            const int hT = d(17);
            // the tag is NOT placed here any more: the GLOBAL layout pass at the end
            // of this function places every tag of the page together (most
            // constrained first + relaxation sweeps), so a tag is never pushed onto
            // another cable just because a neighbour took its slot first
            TagItem ti;
            ti.text = tag;
            ti.over = over;
            ti.hover = hover;
            ti.cx = cx;
            ti.cy = cy;
            ti.ux = ux;
            ti.uy = uy;
            {
                float runLen = 0.f;
                for (size_t k2 = 1; k2 < pts.size(); ++k2)
                    runLen += std::hypot(pts[k2].X - pts[k2 - 1].X,
                                         pts[k2].Y - pts[k2 - 1].Y);
                ti.halfLen = runLen * 0.5f;
            }
            ti.wT = wT;
            ti.hT = hT;
            ti.ink = over ? dr::C(theme::kDanger) : dr::C(runInk);
            ti.runIdx = perLink[i][(size_t)ri];
            tags.push_back(std::move(ti));
        }

        // A bound group CONVERGES: every member has a short collector stub to the
        // group's convergence point, drawn thin and dashed because it is the same
        // cable — not a second one — and it carries no length tag (the single main
        // run's tag reports the whole cable, stubs included).
        for (const auto& cp : ln.collectorPaths) {
            if (cp.size() < 2) continue;
            std::vector<Gdiplus::PointF> cpts;
            cpts.reserve(cp.size());
            for (const auto& pt : cp)
                cpts.emplace_back((float)(ox_ + pt.first * ppm_),
                                  (float)(oy_ + pt.second * ppm_));
            const Gdiplus::Color stubPen =
                over ? dr::C(theme::kDanger, 200) : dr::C(runInk, 200);
            Gdiplus::Pen pen(stubPen, hover ? 1.8f : 1.1f);
            pen.SetDashStyle(Gdiplus::DashStyleDot);
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLines(&pen, cpts.data(), (INT)cpts.size());
            // the convergence point itself: a small hollow dot, so the "one run
            // from here on" is visible even when the stubs are short
            Gdiplus::SolidBrush dot(over ? dr::C(theme::kDanger, 230)
                                         : dr::C(runInk, 230));
            const float rr = (float)d(2);
            g.FillEllipse(&dot, cpts.back().X - rr, cpts.back().Y - rr, rr * 2, rr * 2);
        }
    }

    // ---- GLOBAL tag layout + draw -------------------------------------------
    // One pass for the whole page (never one-by-one greedy): the most constrained
    // tag is placed first, then two relaxation sweeps let a tag push its neighbour.
    // Device boxes/names and microphone markers are never covered, the other cables
    // cost overlap area and the travel from the tag's home point is priced in pills.
    // The global layout is the only expensive part of this painter, and it runs on
    // EVERY repaint (hovering, dragging a device, redraws while zooming).  Memoise the
    // chosen slots keyed by the exact inputs: same geometry + same view ⇒ same result,
    // so a repaint just replays the cached rects instead of solving the packing again.
    {
        static std::vector<RECT> sRects;
        static size_t sKey = 0;
        static bool sValid = false;
        size_t key = 1469598103934665603ull;
        auto mix = [&key](double v) {
            key ^= std::hash<double>{}(v);
            key *= 1099511628211ull;
        };
        mix(L_.canvas.left); mix(L_.canvas.top);
        mix(L_.canvas.right); mix(L_.canvas.bottom);
        mix(d(2));
        mix(ppm_); mix(ox_); mix(oy_);
        mix((double)tags.size());
        mix((double)devBoxes.size());
        mix((double)polys.size());
        for (const RECT& b : devBoxes) { mix(b.left); mix(b.top); mix(b.right); mix(b.bottom); }
        for (const auto& poly : polys)
            for (const auto& q : poly) { mix(q.X); mix(q.Y); }
        for (const TagItem& t : tags) {
            mix(t.cx); mix(t.cy); mix(t.ux); mix(t.uy);
            mix(t.halfLen); mix(t.wT); mix(t.hT); mix((double)t.runIdx);
        }
        if (sValid && key == sKey && sRects.size() == tags.size()) {
            for (size_t i = 0; i < tags.size(); ++i) tags[i].r = sRects[i];
        } else {
            LayoutTags(tags, devBoxes, polys, L_.canvas, d(2));
            sRects.resize(tags.size());
            for (size_t i = 0; i < tags.size(); ++i) sRects[i] = tags[i].r;
            sKey = key;
            sValid = true;
        }
    }
    for (const TagItem& t : tags) {
        const float mx = (t.r.left + t.r.right) * 0.5f;
        const float my = (t.r.top + t.r.bottom) * 0.5f;
        const float dOwn =
            (t.runIdx < polys.size()) ? PolyDist(polys[t.runIdx], mx, my) : 0.f;
        {
            extern std::vector<std::wstring> g_micStackDump;
            g_micStackDump.push_back(dr::Fmt(
                L"tagpill <%s> %ld %ld %ld %ld own %d dist %.1f pill %d strict %d allow %d cap %.1f",
                t.text.c_str(), t.r.left, t.r.top, t.r.right, t.r.bottom,
                (int)t.runIdx, dOwn, t.wT, t.nStrict, t.nAllow,
                (double)t.capMul));
        }
        dr::FillRound(g, dr::RF(t.r), (float)d(4), dr::C(0xFFFFFF, 232));
        dr::StrokeRound(g, dr::RF(t.r), (float)d(4),
                        dr::C(t.over ? theme::kDanger : theme::kBorder, 190), 1.f);
        // the pill keeps its light fill; its TEXT wears the cable's own ink so the
        // label and the run read as one colour family (an overloaded tag stays red)
        dr::Text(g, t.text, Font(8.f, t.over || t.hover),
                 t.over ? dr::C(theme::kDanger) : t.ink, t.r, 1, true);
    }
}

// microphones are drawn on layer 03 too: a wiring plan needs its sources.
// Compact markers only (the design doc asks for "model only" on this layer).
void App::PaintCablingMics(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    // ---- channel numbers for the mic annotations ---------------------------
    // The 05 mic label leads with the CHANNEL NUMBER of the microphone's own row in
    // the 03 通道表 (「03 话筒 7」), so a wired label can be matched to the table at a
    // glance.  The map comes from the SAME helper the wiring tags use (ChanNoMap,
    // built from BuildChannelRows — the table's own order); a main microphone numbers
    // each CAPSULE row separately, a bound group numbers each member.  A microphone
    // that occupies two channels (an intensity technique object) shows the FIRST one.
    const ChanNoMap chan = BuildChanNoMap(BuildChannelRows());
    auto chanOf = [&](int itemId, int capsule) { return chan.get(itemId, capsule); };

    std::vector<RECT> micLabels;   // greedy: name labels never overlap each other
    for (const auto& it : pr_.items) {
        if (it.kind != ItemKind::Mic) continue;
        // A main microphone is drawn as ONE MARKER PER CAPSULE (L/R, or L/C/R for
        // a Decca Tree), symmetrically about its position along the side axis —
        // exactly like 01/02's link line, and each capsule has its own cable.
        std::vector<std::pair<double, double>> caps;
        std::vector<std::wstring> capNames;
        std::vector<int> capIds;      // 0 = L, 1 = C, 2 = R, -1 = plain microphone
        const int nCap = MicCapsuleCount(it);
        if (nCap >= 2) {
            caps = MicCapsulePos(it);
            static const wchar_t* kCap[3] = {L"L", L"C", L"R"};
            static const int kPair[2] = {0, 2};
            static const int kTree[3] = {0, 1, 2};
            const bool decca = (it.mainTech == 3);
            for (int c = 0; c < nCap; ++c) {
                const int cap = decca ? kTree[c] : kPair[c];
                capNames.push_back(std::wstring(kCap[cap]));
                capIds.push_back(cap);
            }
        } else {
            caps.push_back({it.x, it.y});
            capNames.push_back(std::wstring());
            capIds.push_back(-1);
        }
        const std::wstring base = it.label.empty() ? L10n::T(S::MainMic) : it.label;
        const bool pend = (ui_.pendingKind == 2 && ui_.pendingId == it.id);
        for (size_t ci = 0; ci < caps.size(); ++ci) {
            const float cx = (float)(ox_ + caps[ci].first * ppm_);
            const float cy = (float)(oy_ + caps[ci].second * ppm_);
            if (cx < L_.canvas.left - d(40) || cx > L_.canvas.right + d(40)) continue;
            if (cy < L_.canvas.top - d(20) || cy > L_.canvas.bottom + d(20)) continue;
            // the marker is 1.6× the old circle so it is easy to grab; the cable
            // terminal uses the very same (unrounded) radius × the mic's marker
            // factor (App::MicRadiusM / MicMarkerScaleOf), so the line really
            // touches the circle — to the pixel
            const double mradF = MicMarkerRadiusF(it);
            // the marker wears the ink of what the microphone is BOUND to — the
            // same ink its cable and its tag use (unbound: the neutral mic ink)
            const Rgb mink = MicInkLine(MicBoundInk(pr_, it));
            dr::Circle(g, cx, cy, (float)mradF, dr::C(mink), 1.8f);
            dr::FillRound(g,
                          Gdiplus::RectF(cx - d(3.2f), cy - d(3.2f), d(6.4f), d(6.4f)),
                          (float)d(3.2f), dr::C(mink));
            if (pend) dr::Circle(g, cx, cy, d(17), dr::C(theme::kAccent), 1.6f);
            if (it.flown && ci == 0) {   // 吊装 badge, once per microphone
                // the badge hosts boosted text in an export, so its slot scales with
                // the same factor (bd == d exactly at 1×)
                const float bfx =
                    (dr::g_textLayoutBoost > 1.001f) ? dr::g_textLayoutBoost : 1.f;
                auto bd = [&](float v) { return (int)std::lround(d(v) * bfx); };
                RECT br = {(LONG)cx + bd(7), (LONG)cy - bd(2), (LONG)cx + bd(25),
                           (LONG)cy + bd(15)};
                dr::FillRound(g, dr::RF(br), (float)bd(3), dr::C(0x6A5F7A, 235));
                dr::Text(g, L10n::T(S::CbFlyBadge), Font(7.f, true),
                         dr::C(0xFFFFFF), br, 1, true);
            }
            // ---- the annotation text: ONE line, 「03 话筒 7」 ---------------------
            // The channel number is the microphone's row number in the 03 通道表,
            // zero padded exactly like the table's 通道 column.  A microphone that
            // occupies two channels shows the FIRST one.  NO instrument / 声部 line
            // on this page (the 02 话筒摆位 page keeps its own two-line label).
            const int chan = chanOf(it.id, capIds[ci]);
            std::wstring nm;
            if (!capNames[ci].empty()) {
                nm = base + L" " + capNames[ci];   // 主话筒 L / 主话筒 R / 主话筒 C
            } else {
                // the NAME leads (「话筒 7」); the model belongs to the 03 table, the
                // number lets you find that row.  A microphone without a name falls
                // back to its model.
                nm = it.label;
                if (nm.empty()) nm = it.model;
                if (nm.empty()) nm = it.defId;
            }
            const std::wstring txt =
                chan > 0 ? dr::Fmt(L"%02d %s", chan, nm.c_str()) : nm;
            // capsule / model names are placed greedily: below, above, then with
            // increasing offsets, dodging labels already placed
            Gdiplus::Font* nf = Font(8.f);
            const float ntw = dr::TextW(g, txt, nf);
            // the PNG export draws this text scaleMul× larger (see
            // dr::g_textLayoutBoost), so the box that hosts it — and every step
            // measured against it — grows too; at 1× these are exactly d(6)/d(16)
            const float cbBoost =
                (dr::g_textLayoutBoost > 1.001f) ? dr::g_textLayoutBoost : 1.f;
            const int nW = (int)ntw + d(6.f * cbBoost);
            const int nH = d(16.f * cbBoost);
            const int nPad = d(4.f * cbBoost);
            // GLUED to the marker: 3-4 px under the drawn circle, centred
            const int glued = (int)std::lround(cy + mradF + d(3.f * cbBoost));
            RECT lr = {(LONG)cx - nW / 2, glued, (LONG)cx + nW / 2, glued + nH};
            auto clampCanvas = [&](RECT q) {
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
            auto hits = [&](const RECT& q) {
                for (const RECT& t : micLabels)
                    if (RectHit(t, q)) return true;
                return false;
            };
            lr = clampCanvas(lr);
            // SAME rule and the SAME budget as the 02 canvas mic labels: SIDEWAYS
            // first while the row has room (half a label width, then a whole one,
            // right before left), then DOWN one line, then sideways one line lower,
            // then two and three lines — never further than three lines / one label
            // width from the marker.  If nothing inside the budget is free, the
            // candidate with the least overlap wins (a slight overlap is better than
            // a label far from its source).
            const int lineStep = nH + nPad;   // one label line + a small gap
            std::vector<RECT> cands;
            std::vector<int> cDown, cSide;
            auto addCap = [&](RECT q, int down, int side) {
                if (q.bottom > L_.canvas.bottom - d(2)) {   // keep the block inside
                    q.top -= q.bottom - (L_.canvas.bottom - d(2));
                    q.bottom = L_.canvas.bottom - d(2);
                }
                if (q.top < L_.canvas.top + d(2)) return;
                if (q.bottom > L_.canvas.bottom - d(2)) return;
                cands.push_back(q);
                cDown.push_back(down);
                cSide.push_back(side);
            };
            for (int lvl = 0; lvl <= 3; ++lvl) {
                const int dy = lvl * lineStep;
                if (lvl == 0) {
                    addCap(lr, 0, 0);   // glued under the marker
                } else {
                    RECT dn = lr;
                    dn.top += dy;
                    dn.bottom += dy;
                    addCap(dn, lvl, 0);
                }
                for (int lvlW = 1; lvlW <= 2; ++lvlW) {   // sideways at this height
                    const int off = lvlW == 1 ? nW / 2 : nW;
                    if (off <= 0) continue;
                    for (int dir = 0; dir < 2; ++dir) {
                        RECT sd = lr;
                        const int dx = dir ? -off : off;
                        sd.left += dx;
                        sd.right += dx;
                        sd.top += dy;
                        sd.bottom += dy;
                        addCap(sd, lvl, dir ? 2 : 1);
                    }
                }
            }
            {   // the old "above the marker" slot, as the very last resort inside the
                // budget (it is still one line away from the source)
                RECT up = lr;
                up.top -= lineStep;
                up.bottom -= lineStep;
                addCap(up, 0, 3);
            }
            int stacked = -1, sideK = 0;
            bool found = false;
            for (size_t ci2 = 0; ci2 < cands.size() && !found; ++ci2) {
                if (hits(cands[ci2])) continue;
                lr = cands[ci2];
                stacked = cDown[ci2];
                sideK = cSide[ci2];
                found = true;
            }
            if (!found) {   // no free slot: keep the least overlap
                size_t best = 0;
                long long bestOv = -1;
                for (size_t ci2 = 0; ci2 < cands.size(); ++ci2) {
                    const RECT& q = cands[ci2];
                    long long ov = 0;
                    for (const RECT& t : micLabels) {
                        const int ow = (std::min)(t.right, q.right) -
                                       (std::max)(t.left, q.left);
                        const int oh = (std::min)(t.bottom, q.bottom) -
                                       (std::max)(t.top, q.top);
                        if (ow > 0 && oh > 0) ov += (long long)ow * oh;
                    }
                    if (bestOv < 0 || ov < bestOv) {
                        bestOv = ov;
                        best = ci2;
                    }
                }
                lr = cands[best];
                stacked = cDown[best];
                sideK = cSide[best];
            }
            // 显示话筒文字 OFF ⇒ no capsule / model labels on the cabling canvas
            if (!pr_.showMicText) continue;
            if (lr.left < L_.canvas.left + d(2)) {
                lr.right += L_.canvas.left + d(2) - lr.left;
                lr.left = L_.canvas.left + d(2);
            }
            if (lr.right > L_.canvas.right - d(2)) {
                lr.left -= lr.right - (L_.canvas.right - d(2));
                lr.right = L_.canvas.right - d(2);
            }
            micLabels.push_back(lr);
            {
                extern std::vector<std::wstring> g_micStackDump;
                g_micStackDump.push_back(dr::Fmt(
                    L"cbmic <%s> %ld %ld %ld %ld step %d side %d ch %d",
                    txt.c_str(), lr.left, lr.top, lr.right, lr.bottom, stacked,
                    sideK, chan));
            }
            // ONE line: the channel number + the microphone name
            dr::TextHalo(g, txt, nf, dr::C(theme::kText), dr::C(0xFFFFFF, 235), lr, 1);
        }
    }
}

// pending source: accent ring around the object plus a hint next to the cursor
void App::PaintCablingHint(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    if (ui_.pendingKind == 0) return;
    const CableEnd e = ResolveCableEnd(pr_, ModelEndpointKind(ui_.pendingKind),
                                      ui_.pendingId);
    if (e.ok) {
        const float cx = (float)(ox_ + e.x * ppm_);
        const float cy = (float)(oy_ + e.y * ppm_);
        RECT r = {(LONG)cx - d(22), (LONG)cy - d(18), (LONG)cx + d(22), (LONG)cy + d(18)};
        dr::StrokeRound(g, dr::RF(r), (float)d(9), dr::C(theme::kAccent, 200), 1.6f);
    }
    const std::wstring hint = L10n::T(S::LinkPickTarget);
    Gdiplus::Font* f = Font(9.f);
    const float tw = dr::TextW(g, hint, f);
    int hx = ui_.dragPt_.x + d(16);
    int hy = ui_.dragPt_.y + d(16);
    RECT hr = {hx, hy, hx + (int)tw + d(18), hy + d(26)};
    if (hr.right > cw_ - d(8)) {
        hr.left -= hr.right - (cw_ - d(8));
        hr.right = cw_ - d(8);
    }
    if (hr.bottom > ch_ - d(8)) {
        const int dy = hr.bottom - (ch_ - d(8));
        hr.top -= dy;
        hr.bottom -= dy;
    }
    dr::FillRound(g, dr::RF(hr), (float)d(6), dr::C(0xFFFFFF, 242));
    dr::StrokeRound(g, dr::RF(hr), (float)d(6), dr::C(theme::kAccent, 200), 1.2f);
    dr::Text(g, hint, f, dr::C(theme::kText), hr, 1, true);
}

void App::PaintCablingLayer(Gdiplus::Graphics& g) {
    PaintCables(g);        // cables run under the objects
    PaintDevices(g);
    PaintCablingMics(g);
    PaintCablingHint(g);
}

int App::MicMarkerHitTest(int x, int y) const {
    POINT p = {x, y};
    const int pad = (int)std::lround(4.0 * dpi_ * uiScale_);
    for (auto it = pr_.items.rbegin(); it != pr_.items.rend(); ++it) {
        if (it->kind != ItemKind::Mic) continue;
        // the grab area follows the DRAWN circle of this microphone (the same
        // MicMarkerScaleOf factor as the cable terminal), so a point mic and a
        // lavalier are as easy to grab as they are to see
        const int r = MicMarkerRadiusPx(*it) + pad;
        // a main microphone is grabbed by any of its capsule circles
        const auto caps = MicCapsulePos(*it);
        for (const auto& c : caps) {
            const int cx = (int)std::lround(ox_ + c.first * ppm_);
            const int cy = (int)std::lround(oy_ + c.second * ppm_);
            RECT box = {cx - r, cy - r, cx + r, cy + r};
            if (PtInRect(&box, p)) return it->id;
        }
    }
    return -1;
}

int App::ConnectableHitTest(int x, int y, int& kind) const {
    const int dev = DeviceHitTest(x, y);
    if (dev >= 0) {
        kind = 0;
        return dev;
    }
    const int mic = MicMarkerHitTest(x, y);
    if (mic >= 0) {
        kind = 1;
        return mic;
    }
    kind = 0;
    return -1;
}

int App::CableHitTest(int x, int y) const {
    const double lim = 6.0 * dpi_ * uiScale_;
    double best = 1e18;
    int bestIdx = -1;
    for (size_t i = 0; i < pr_.links.size(); ++i) {
        const auto& path = pr_.links[i].path;
        for (size_t k = 1; k < path.size(); ++k) {
            const double x1 = ox_ + path[k - 1].first * ppm_;
            const double y1 = oy_ + path[k - 1].second * ppm_;
            const double x2 = ox_ + path[k].first * ppm_;
            const double y2 = oy_ + path[k].second * ppm_;
            const double dx = x2 - x1, dy = y2 - y1;
            const double len2 = dx * dx + dy * dy;
            double t = 0.0;
            if (len2 > 1e-9) {
                t = (((double)x - x1) * dx + ((double)y - y1) * dy) / len2;
                t = std::clamp(t, 0.0, 1.0);
            }
            const double px = x1 + t * dx;
            const double py = y1 + t * dy;
            const double dist = std::sqrt(((double)x - px) * ((double)x - px) +
                                          ((double)y - py) * ((double)y - py));
            if (dist < best) {
                best = dist;
                bestIdx = (int)i;
            }
        }
    }
    return best <= lim ? bestIdx : -1;
}

bool App::CanConnect(int ka, int ia, int kb, int ib) const {
    // the exclusion matrix lives in ONE place (Model.cpp) and is shared by the
    // right-click path, the project loader and the fuzz hook
    return CanConnectRule(pr_, ka, ia, kb, ib, nullptr);
}

// Index of the cable that already feeds this microphone (or any member of its
// bound group), or -1.  A group is wired as ONE cable, so the partner's cable is
// the group's cable.
static int MicCableIndex(const Project& pr, std::vector<CableLink>& links, int id) {
    const CableLink* f = FindMicCableForItem(pr, links, id);
    return (f && !links.empty()) ? (int)(f - links.data()) : -1;
}

void App::CreateCable(int ka, int ia, int kb, int ib) {
    if (RefuseLocked()) return;
    // ONE cable per microphone / per bound group.  A RE-CONNECT replaces the
    // existing cable instead of being refused: after a microphone is unbound (or
    // bound to another instrument) the user may right-click it and a new device
    // without deleting the old route first — the old link is retired in the same
    // undoable step (user report 「不删掉第 4 路的线就不能给第 5 路重新接线」).
    const int oldIdx = (ka == 1) ? MicCableIndex(pr_, pr_.links, ia)
                                 : ((kb == 1) ? MicCableIndex(pr_, pr_.links, ib) : -1);
    if (oldIdx >= 0 && oldIdx < (int)pr_.links.size()) {
        const CableLink& old = pr_.links[(size_t)oldIdx];
        // is it literally the same pair?  Then there is nothing to create.
        auto same = [&](int k, int id, int k2, int id2) {
            if (k != k2) return false;
            if (id == id2) return true;
            if (k != 1) return false;
            const int g1 = pr_.GroupOf(id), g2 = pr_.GroupOf(id2);
            return g1 >= 0 && g1 == g2;
        };
        const bool samePair =
            (same(old.aKind, old.a, ka, ia) && same(old.bKind, old.b, kb, ib)) ||
            (same(old.aKind, old.a, kb, ib) && same(old.bKind, old.b, ka, ia));
        if (samePair) {
            StatusMsg(L10n::T(S::LinkAlready));
            InvalidateAll();
            return;
        }
        // replace: keep the cable on the SAME endpoint orientation (mic first) and
        // re-derive everything after the swap
        PushUndoSnapshot();
        pr_.links.erase(pr_.links.begin() + oldIdx);
    }

    CableLink ln;
    ln.aKind = ka;
    ln.a = ia;
    ln.bKind = kb;
    ln.b = ib;

    const CableEnd a = ResolveCableEnd(pr_, ka, ia);
    const CableEnd b = ResolveCableEnd(pr_, kb, ib);
    // the cable carries its own demand, never the device's full channel count:
    // a microphone cable = that microphone's capsules, a trunk = min(capacity(A),
    // capacity(B)) so an 8-channel sound card on a 4-channel box is a 4-channel link
    ln.channels = DerivedCableChannels(pr_, ln);
    ln.manual = true;                     // every cable is a user-drawn one now
    BuildCablePath(pr_, ln, 0, MicRadiusM());
    if (oldIdx < 0) PushUndoSnapshot();   // creating a cable is undoable
    pr_.links.push_back(ln);
    // lanes and the derived topology depend on the whole set, not just this cable
    RebuildAllCablePaths(pr_, MicRadiusM());
    const int ch = pr_.links.back().channels;
    std::wstring why;
    const bool over = CableOver(pr_, pr_.links.back(), &why);
    StatusMsg(dr::Fmt(L10n::T(oldIdx >= 0 ? S::LinkRerouted : S::LinkCreatedFmt),
                      a.name.c_str(), b.name.c_str(), ch),
              over);
    if (over) StatusMsg(why, true);
    Layout();
}

void App::MouseDownRight(int x, int y) {
    if (exitOpen_) return;   // the close prompt owns the window while it is open
    if (RefuseLocked()) return;
    if (ui_.layer != 2) return;
    if (ui_.confirmLink >= 0) return;   // the confirmation modal owns the input
    POINT p = {x, y};

    // 1) right-click on an existing cable asks for confirmation first — the link
    // is only removed on 确定, while 取消 / Esc keeps it
    const int cab = CableHitTest(x, y);
    if (cab >= 0 && cab < (int)pr_.links.size()) {
        RequestDeleteCable(cab);
        return;
    }

    // 2) a connectable object (device first, then microphone)
    // ui_.pendingKind: 0 = none, 1 = device, 2 = microphone (the model uses
    // 0 = device / 1 = mic, so the UI value is offset by one)
    int kind = 0;
    const int id = PtInRect(&L_.canvas, p) ? ConnectableHitTest(x, y, kind) : -1;
    if (id < 0) {                       // empty canvas: cancel the pending pick
        if (ui_.pendingKind != 0) {
            ui_.pendingKind = 0;
            ui_.pendingId = 0;
            StatusMsg(L10n::T(S::LinkCancel));
        }
        InvalidateAll();
        return;
    }

    ui_.dragPt_ = p;
    const int uiKind = (kind == 0) ? 1 : 2;
    if (ui_.pendingKind == 0) {         // first click marks the source
        ui_.pendingKind = uiKind;
        ui_.pendingId = id;
        StatusMsg(L10n::T(S::LinkPickTarget));
        InvalidateAll();
        return;
    }

    const int pk = ModelEndpointKind(ui_.pendingKind), pid = ui_.pendingId;
    if (ui_.pendingKind == uiKind && pid == id) {
        StatusMsg(L10n::T(S::LinkSame), true);   // keep the pending source
        InvalidateAll();
        return;
    }
    if (!CanConnect(pk, pid, kind, id)) {
        // the exclusion matrix names the rule that refused the pair
        std::wstring why;
        CanConnectRule(pr_, pk, pid, kind, id, &why);
        StatusMsg(why.empty() ? std::wstring(L10n::T(S::LinkRejected)) : why, true);
        InvalidateAll();
        return;
    }
    CreateCable(pk, pid, kind, id);
    ui_.pendingKind = 0;
    ui_.pendingId = 0;
    InvalidateAll();
}
