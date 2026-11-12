#include "App.h"
#include <shellapi.h>
#include "DebugLog.h"
#include "Assets.h"
#include "Catalog.h"
#include "Pdf.h"
#include "Draw.h"
#include "L10n.h"
#include "Presets.h"
#include "Theme.h"

#include <algorithm>
#include <map>
#include <cmath>
#include <fstream>
#include <cwchar>
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (ECM_FIRST + 1)
#endif

App& App::Get() {
    static App app;
    return app;
}

static double NormAngle(double a) {
    a = std::fmod(a, 360.0);
    if (a < 0.0) a += 360.0;
    return a;
}

static int PatternAtSlot(int tech, int slot);

LRESULT CALLBACK App::StaticProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    App& app = App::Get();
    app.hwnd_ = h;   // valid from WM_NCCREATE onwards
    return app.WndProc(m, w, l);
}

int App::Run(HINSTANCE hInst, int nCmdShow) {
    hinst_ = hInst;

    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    exeDir_ = path;
    size_t slash = exeDir_.find_last_of(L'\\');
    if (slash != std::wstring::npos) exeDir_.resize(slash);
    Assets::Get().Init(exeDir_);
    // app lock: a missing / corrupt applock.dat simply means 「no lock」
    LockLoad();
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i < argc; ++i) {
                // a BARE PATH is the shell's file association landing here
                // ("<exe>" "%1"): it behaves exactly like --open <path>, so a
                // double-clicked .smproj opens the project
                if (argv[i][0] != L'-' && argv[i][0] != L'/') {
                    if (startupOpen_.empty() && GetFileAttributesW(argv[i]) !=
                                                    INVALID_FILE_ATTRIBUTES)
                        startupOpen_ = argv[i];
                    continue;
                }
                if (!_wcsicmp(argv[i], L"--layer") && i + 1 < argc) {
                    // --layer takes the LAYER value, not the tab number:
                    // 5 = 项目信息, 0 = 乐器摆位, 1 = 话筒摆位, 3 = 通道表
                    // 4 = 拓扑图 2 = 布线  (unchanged, so --layer 3 still opens
                    // the channel table the PDF exporter depends on)
                    int L = _wtoi(argv[++i]);
                    // layer 4 (the retired 拓扑图) is rejected: only the five live
                    // pages are addressable from the command line
                    if (L == 4) L = -1;
                    if (L >= 0 && L <= 5) {
                        ui_.layer = L;
                        ui_.libTab = (L == 1 ? 1 : 0);
                    }
                } else if (!_wcsicmp(argv[i], L"--en")) {
                    L10n::Set(Lang::En);
                } else if (!_wcsicmp(argv[i], L"--zh")) {
                    L10n::Set(Lang::Zh);
                } else if (!_wcsicmp(argv[i], L"--de")) {
                    L10n::Set(Lang::De);
                } else if (!_wcsicmp(argv[i], L"--fr")) {
                    L10n::Set(Lang::Fr);
                } else if (!_wcsicmp(argv[i], L"--pngx") && i + 1 < argc) {
                    pngScaleMul_ = std::clamp(_wtoi(argv[++i]), 1, 16);
                } else if (!_wcsicmp(argv[i], L"--png") && i + 1 < argc) {
                    startupPng_ = argv[++i];   // export the drawing as .png and quit
                } else if (!_wcsicmp(argv[i], L"--pngpick") && i + 1 < argc) {
                    // test hook: pick one row of the 导出图片 scale chooser (2/4/8)
                    // through the same remembering code, then export with --png
                    startupPngPick_ = _wtoi(argv[++i]);
                } else if (!_wcsicmp(argv[i], L"--lenmode") && i + 1 < argc) {
                    // test hook: 通道表 话筒线长度 mode (0 = 推荐, 1 = 实际) — the same
                    // project field the column header toggles (applied after --open)
                    startupLenMode_ = _wtoi(argv[++i]) == 1 ? 1 : 0;
                } else if (!_wcsicmp(argv[i], L"--pdf") && i + 1 < argc) {
                    pdfExportPath_ = argv[++i];
                } else if (!_wcsicmp(argv[i], L"--auto")) {
                    // kept for CLI compatibility: the cable paths are derived from
                    // the links automatically now, there is no routing snapshot
                } else if (!_wcsicmp(argv[i], L"--open") && i + 1 < argc) {
                    startupOpen_ = argv[++i];
                } else if (!_wcsicmp(argv[i], L"--dump") && i + 1 < argc) {
                    dumpPath_ = argv[++i];   // test hook: write the client layout
                } else if (!_wcsicmp(argv[i], L"--dump-layout") && i + 1 < argc) {
                    // dev hook: write the current instrument placement as
                    // ready-to-paste preset rows and exit (see src/Presets.cpp)
                    dumpLayoutPath_ = argv[++i];
                } else if (!_wcsicmp(argv[i], L"--dump-colors") && i + 1 < argc) {
                    // dev hook: write the muted-palette table (defId -> tint ->
                    // drawn ink) and exit; fails when two ids share a colour
                    dumpColorsPath_ = argv[++i];
                } else if (!_wcsicmp(argv[i], L"--preset") && i + 1 < argc) {
                    startupPreset_ = _wtoi(argv[++i]);   // dev hook: load a preset
                } else if (!_wcsicmp(argv[i], L"--zoom") && i + 1 < argc) {
                    ui_.zoom = std::clamp(_wtof(argv[++i]), 0.25, 5.0);   // test hook
                } else if (!_wcsicmp(argv[i], L"--adddev") && i + 4 < argc) {
                    // test hook: place a cabling device at a world position
                    const int kind = _wtoi(argv[++i]);
                    const double dx = _wtof(argv[++i]);
                    const double dy = _wtof(argv[++i]);
                    const int face = _wtoi(argv[++i]);
                    startupAddDevs_.push_back({(double)kind, dx, dy, (double)face});
                } else if (!_wcsicmp(argv[i], L"--link") && i + 4 < argc) {
                    // test hook: wire two objects exactly like a right-click pair
                    const int ka = _wtoi(argv[++i]);
                    const int ia = _wtoi(argv[++i]);
                    const int kb = _wtoi(argv[++i]);
                    const int ib = _wtoi(argv[++i]);
                    startupLinks_.push_back({ka, ia, kb, ib});
                } else if (!_wcsicmp(argv[i], L"--turn") && i + 2 < argc) {
                    // test hook: rotate a mic (and its whole group) by an angle
                    const int id = _wtoi(argv[++i]);
                    const double deg = _wtof(argv[++i]);
                    startupTurns_.push_back({id, deg});
                } else if (!_wcsicmp(argv[i], L"--select") && i + 1 < argc) {
                    // test hook: select an object like a click does
                    //   --select <itemId> | --select device <deviceId>
                    if (!_wcsicmp(argv[i + 1], L"device") && i + 2 < argc) {
                        startupSelDevice_ = _wtoi(argv[i + 2]);
                        i += 2;
                    } else {
                        startupSelect_ = _wtoi(argv[i + 1]);
                        ++i;
                    }
                } else if (!_wcsicmp(argv[i], L"--linear")) {
                    g_corridorMode = CorridorMode::Linear;   // escape hatch
                } else if (!_wcsicmp(argv[i], L"--nonudge")) {
                    ui_.noCableNudge = true;   // A/B check: no cosmetic split
                } else if (!_wcsicmp(argv[i], L"--grid")) {
                    g_corridorMode = CorridorMode::Grid;     // no-op alias (default)
                } else if (!_wcsicmp(argv[i], L"--fuzz") && i + 1 < argc) {
                    // test hook: try every ordered pair of connectable objects and
                    // write the matrix result to a file, then quit
                    startupFuzz_ = argv[++i];
                } else if (!_wcsicmp(argv[i], L"--tech") && i + 1 < argc) {
                    // test hook: main microphone technique 0=AB 1=mixed 2=intensity
                    // 3=Decca (a Decca tree draws three capsule markers)
                    startupTech_ = _wtoi(argv[++i]);
                } else if (!_wcsicmp(argv[i], L"--scroll") && i + 1 < argc) {
                    // test hook: preset the right inspector scroll offset
                    startupScroll_ = _wtoi(argv[++i]);
                } else if (!_wcsicmp(argv[i], L"--register-assoc")) {
                    startupAssoc_ = 1;   // write the HKCU .smproj association
                } else if (!_wcsicmp(argv[i], L"--unregister-assoc")) {
                    startupAssoc_ = 2;   // delete it again
                } else if (!_wcsicmp(argv[i], L"--save") && i + 1 < argc) {
                    startupSave_ = argv[++i];
                } else if (!_wcsicmp(argv[i], L"--close")) {
                    // test hook: post WM_CLOSE after the startup hooks — exactly the
                    // message the title-bar ×, Alt+F4 and the system menu produce,
                    // so a script exercises the real close path
                    startupClose_ = true;
                } else if (!_wcsicmp(argv[i], L"--closechoice") && i + 1 < argc) {
                    // test hook: answer the close prompt through the same handler
                    // the painted buttons call (save / discard / cancel)
                    ++i;
                    if (!_wcsicmp(argv[i], L"save")) closeChoice_ = 0;
                    else if (!_wcsicmp(argv[i], L"discard")) closeChoice_ = 1;
                    else if (!_wcsicmp(argv[i], L"cancel")) closeChoice_ = 2;
                } else if (!_wcsicmp(argv[i], L"--click") && i + 2 < argc) {
                    // scripted left click (client coordinates, test hook)
                    startupInputs_.push_back({0, _wtoi(argv[++i]), _wtoi(argv[++i])});
                } else if (!_wcsicmp(argv[i], L"--rclick") && i + 2 < argc) {
                    startupInputs_.push_back({1, _wtoi(argv[++i]), _wtoi(argv[++i])});
                } else if (!_wcsicmp(argv[i], L"--move") && i + 2 < argc) {
                    startupInputs_.push_back({2, _wtoi(argv[++i]), _wtoi(argv[++i])});
                } else if (!_wcsicmp(argv[i], L"--up") && i + 2 < argc) {
                    // scripted left button release: closes a --click/--move drag so
                    // the state is settled before the window becomes interactive
                    startupInputs_.push_back({4, _wtoi(argv[++i]), _wtoi(argv[++i])});
                } else if (!_wcsicmp(argv[i], L"--key") && i + 1 < argc) {
                    startupInputs_.push_back({3, _wtoi(argv[++i]), 0});
                } else if (!_wcsicmp(argv[i], L"--ctrl")) {
                    // test hook: the NEXT scripted --key arrives with Ctrl held
                    // (GetKeyState is the real keyboard and cannot be faked)
                    startupInputs_.push_back({5, 0, 0});
                } else if (!_wcsicmp(argv[i], L"--ctrlband")) {
                    // test hook: the NEXT scripted --click (and its drag) arrives
                    // with Ctrl held, so a script can exercise the Ctrl rubber band
                    startupInputs_.push_back({6, 0, 0});
                }
            }
            LocalFree(argv);
        }
    }
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;   // 04 double-click = fit
    wc.lpfnWndProc = &App::StaticProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"StageMicingWnd";
    if (!RegisterClassExW(&wc)) return 1;

    {
        UINT dpiSys = 96;
        using GetDpiForSystemFn = UINT(WINAPI*)();
        HMODULE u32 = GetModuleHandleW(L"user32.dll");
        auto fn = u32 ? (GetDpiForSystemFn)GetProcAddress(u32, "GetDpiForSystem") : nullptr;
        if (fn) dpiSys = fn();
        if (!dpiSys) dpiSys = 96;
        dpi_ = (float)dpiSys / 96.f;
    }
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int cw = std::min((int)(1460 * dpi_), sw - 60);
    int ch = std::min((int)(900 * dpi_), sh - 70);
    if (cw < 1000) cw = 1000;
    if (ch < 700) ch = 700;

    RECT wr = {0, 0, cw, ch};
    AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);
    int ww = wr.right - wr.left;
    int wh = wr.bottom - wr.top;

    hwnd_ = CreateWindowExW(
        0, L"StageMicingWnd", L"Stage Micing",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        std::max(0, (sw - ww) / 2), std::max(0, (sh - wh) / 2), ww, wh,
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd_) return 1;

    dpi_ = (float)GetDpiForWindow(hwnd_) / 96.f;
    if (dpi_ < 0.5f) dpi_ = 1.f;

    family_ = new Gdiplus::FontFamily(L"Microsoft YaHei UI");
    if (family_->IsAvailable() != TRUE) {
        delete family_;
        family_ = new Gdiplus::FontFamily(L"Segoe UI");
    }

    OnCreate();
    ShowWindow(hwnd_, nCmdShow);
    // the controls are created hidden: place them right away so the first frame
    // is already correct (no flash of stacked EDITs)
    Layout();
    InvalidateAll();
    if (!startupOpen_.empty()) OpenProjectFrom(startupOpen_);
    // --lenmode N (test hook): applied AFTER the file load so it always wins over
    // the value stored in the project — the same project field the column header
    // toggles
    if (startupLenMode_ >= 0) pr_.lenMode = startupLenMode_ == 1 ? 1 : 0;
    for (const auto& a : startupAddDevs_) {
        // scripted device placement (regression tests): same record the
        // "+ 模拟接口箱" buttons create, then moved to the requested spot
        Device* dv = AddDevice((DevKind)std::clamp((int)a[0], 0, 5));
        if (!dv) continue;
        if (a[1] > -900.0 && a[2] > -900.0) {   // -999,-999 = keep the auto spot
            dv->x = a[1];
            dv->y = a[2];
        }
        dv->faceDir = (((int)a[3] % 4) + 4) % 4;
    }
    if (!startupAddDevs_.empty()) RefreshAfterProjectChange();   // re-lay the patch
    if (!startupLinks_.empty()) {
        // scripted wiring (regression tests): same rules as the right-click pair
        for (const auto& l : startupLinks_)
            if (CanConnect(l[0], l[1], l[2], l[3])) CreateCable(l[0], l[1], l[2], l[3]);
        RefreshAfterProjectChange();
    }
    if (!startupTurns_.empty()) {
        for (const auto& t : startupTurns_) RotateGroupBy(t.first, t.second);
        RefreshAfterProjectChange();
    }
    if (startupSelect_ > 0) {
        // scripted click-selection (inspector layout checks)
        pr_.Deselect();
        pr_.selected = startupSelect_;
        pr_.selIds = {startupSelect_};
        RefreshAfterProjectChange();
    }
    if (startupSelDevice_ > 0) {
        pr_.SelectDevice(startupSelDevice_);
        pr_.Deselect();
        RefreshAfterProjectChange();
    }
    if (startupTech_ >= 0) {
        for (auto& it : pr_.items)
            if (it.defId == L"mainmic") {
                it.mainTech = std::clamp(startupTech_, 0, 3);
                break;
            }
        RefreshAfterProjectChange();
    }
    if (startupScroll_ > 0) {
        // scripted inspector scroll (clip / out-of-panel editor checks)
        ui_.rightScroll = (double)startupScroll_;
        Layout();
        InvalidateAll();
    }
    if (startupPreset_ >= 0) {
        // dev hook (--preset <n>): instantiate a built-in orchestra preset at
        // launch, through the same path the 预设 modal uses
        LoadPresetAt(startupPreset_);
    }
    // the demo / opened / scripted project is the UNDO BASELINE: every snapshot
    // recorded while building it is dropped, so undo can never walk behind a load
    ResetUndoHistory();
    if (!startupInputs_.empty()) {
        // scripted input: the real handlers, so the state after launch is the one
        // an interactive session would be in
        Layout();
        // one paint BEFORE the first scripted event: the palette tiles_ and the
        // popover hit rects are published by the painters, so an interactive
        // session always has them and the script must too
        UpdateWindow(hwnd_);
        for (const auto& in : startupInputs_) {
            if (in[0] == 0) MouseDown(in[1], in[2]);
            else if (in[0] == 1) MouseDownRight(in[1], in[2]);
            else if (in[0] == 2) MouseMove(in[1], in[2]);
            else if (in[0] == 4) MouseUp(in[1], in[2]);
            else if (in[0] == 5) scriptCtrl_ = true;   // --ctrl: the next key's Ctrl
            else if (in[0] == 6) scriptCtrlNext_ = true;   // --ctrlband: next press
            else OnKeyDown(in[1]);
            // publish the hit rects of whatever the input opened (the modal, a
            // popup) before the next scripted event arrives
            UpdateWindow(hwnd_);
        }
        Layout();
        InvalidateAll();
    }
    if (!dumpColorsPath_.empty()) {
        // dev hook (--dump-colors <file>): the muted-palette table — every defId
        // that owns a slot, the tint the 01/02 icon takes, and the ink MicInkLine()
        // derives from it — plus the no-duplicate assertion.  Exits 2 when two ids
        // share a tint or an ink, or when the fixed order and the catalogue
        // disagree (a renamed / added / removed instrument).  Needs no layout: the
        // assignment is a pure function of the defId.
        std::wstring out;
        std::vector<std::wstring> bad;
        auto hexOf = [](const Rgb& c) {
            return dr::Fmt(L"#%02X%02X%02X", (int)c.r, (int)c.g, (int)c.b);
        };
        auto lumOf = [](const Rgb& c) {
            return 0.299 * (double)c.r + 0.587 * (double)c.g + 0.114 * (double)c.b;
        };
        const int slots = StageMutedOrderCount();
        const int palSize = StageMutedPaletteSize();
        out += dr::Fmt(L"palette %d entries, %d ordered ids, %d spare\n", palSize, slots,
                       palSize - slots);
        out += L"# icon = StageMutedColor(defId) (the 01/02 icon tint); ink = "
               L"MicInkLine(icon), the 1.9 px cable / 3 px marker / tag ink the 05 "
               L"page draws (Rec.601 Y capped at 104)\n";
        out += L"slot defId icon ink iconY inkY\n";
        std::vector<std::pair<std::wstring, std::wstring>> tintSeen, inkSeen;
        std::vector<std::wstring> ordered;
        for (int i = 0; i < slots; ++i) {
            const std::wstring id = StageMutedOrderAt(i);
            ordered.push_back(id);
            const Rgb tint = StageMutedColor(id);
            const Rgb ink = MicInkLine(tint);
            const std::wstring th = hexOf(tint), ih = hexOf(ink);
            out += dr::Fmt(L"%4d %-18s %s %s %5.1f %5.1f\n", i, id.c_str(), th.c_str(),
                           ih.c_str(), lumOf(tint), lumOf(ink));
            for (const auto& s : tintSeen)
                if (s.first == th) bad.push_back(L"tint " + th + L": " + s.second + L" == " + id);
            for (const auto& s : inkSeen)
                if (s.first == ih) bad.push_back(L"ink  " + ih + L": " + s.second + L" == " + id);
            if (!FindItem(id)) bad.push_back(L"ordered id not in the catalogue: " + id);
            tintSeen.push_back({th, id});
            inkSeen.push_back({ih, id});
        }
        if ((int)tintSeen.size() < palSize)
            out += dr::Fmt(L"# %d palette entries are unused spare\n",
                           palSize - (int)tintSeen.size());
        out += L"# grand_piano draws pr.instColor for its icon and for its microphones "
               L"(MicBoundInk), so its row above is only the slot it occupies — the "
               L"same-type hull caption still uses that tint\n";
        // the other direction: every catalogue target must own a slot here
        for (const auto& c : Instruments()) {
            const std::wstring id = c.id;
            bool found = false;
            for (const auto& s : ordered)
                if (s == id) { found = true; break; }
            if (!found) bad.push_back(L"catalogue instrument missing from the order: " + id);
        }
        for (const auto& c : SpotExtras()) {
            const std::wstring id = c.id;
            bool found = false;
            for (const auto& s : ordered)
                if (s == id) { found = true; break; }
            if (!found) bad.push_back(L"section target missing from the order: " + id);
        }
        out += dr::Fmt(L"distinct tints %d/%d, distinct inks %d/%d\n", (int)tintSeen.size(),
                       slots, (int)inkSeen.size(), slots);
        // ---- consecutive-slot separation (the max-min palette order) --------
        // kMutedPal is ordered so two NEIGHBOURING slots are as far apart as the
        // fixed 40-colour set allows; that is the property item 1 asked for, so
        // the check measures it in BOTH spaces (icon tint and drawn ink).
        constexpr double kMinConsecDE = 12.0;
        double minIcon = 1e9, minInk = 1e9, worstSep = 1e9;
        int worstSlot = -1;
        std::wstring deRows;
        for (int i = 0; i + 1 < palSize; ++i) {
            const double di = StageMutedPairDE(i, i + 1, false);
            const double dk = StageMutedPairDE(i, i + 1, true);
            const double sp = (std::min)(di, dk);
            minIcon = (std::min)(minIcon, di);
            minInk = (std::min)(minInk, dk);
            if (sp < worstSep) { worstSep = sp; worstSlot = i; }
            deRows += dr::Fmt(L"%4d %6.1f %6.1f %6.1f\n", i, di, dk, sp);
        }
        out += L"# consecutive slots: slot iconDE inkDE sepDE (target: min sep >= 12)\n";
        out += deRows;
        out += dr::Fmt(L"min consecutive dE: icon %.1f, ink %.1f, sep %.1f (slots %d/%d)\n",
                       minIcon, minInk, worstSep, worstSlot, worstSlot + 1);
        if (worstSep < kMinConsecDE) {
            bad.push_back(dr::Fmt(L"min consecutive dE %.1f < %.0f (slots %d/%d)", worstSep,
                                  kMinConsecDE, worstSlot, worstSlot + 1));
        }
        if (bad.empty()) {
            out += L"assert OK: no two ids share a tint or an ink, and neighbouring "
                   L"slots are >= 12 dE apart\n";
        } else {
            out += L"assert FAILED\n";
            for (const auto& b : bad) out += L"  " + b + L"\n";
        }
        std::string utf8;
        const int need = WideCharToMultiByte(CP_UTF8, 0, out.c_str(), (int)out.size(), nullptr,
                                             0, nullptr, nullptr);
        utf8.resize((size_t)need);
        WideCharToMultiByte(CP_UTF8, 0, out.c_str(), (int)out.size(), utf8.data(), need, nullptr,
                            nullptr);
        FILE* f = nullptr;
        if (_wfopen_s(&f, dumpColorsPath_.c_str(), L"wb") == 0 && f) {
            fwrite(utf8.data(), 1, utf8.size(), f);
            fclose(f);
        }
        DestroyWindow(hwnd_);
        return bad.empty() ? 0 : 2;
    }
    if (!dumpLayoutPath_.empty()) {
        // dev hook (--dump-layout <file>): save the instrument placement as
        // ready-to-paste preset rows, then exit.  Runs AFTER --open / --preset /
        // --click, so a script can build a placement and dump it in one launch.
        Layout();
        std::wstring err;
        if (!DumpLayoutAsPresetRows(pr_, dumpLayoutPath_, err)) {
            const std::wstring msg = L"dump-layout: " + err;
            MessageBoxW(hwnd_, msg.c_str(), L"Stage Micing", MB_OK | MB_ICONWARNING);
        }
        DestroyWindow(hwnd_);
        return 0;
    }
    if (!startupFuzz_.empty()) {
        // ---- 排异 fuzz: every ordered pair of objects in the demo scene ----
        std::wstring out;
        out += L"fuzz " + std::to_wstring(pr_.items.size()) + L" items " +
               std::to_wstring(pr_.devices.size()) + L" devices\n";
        struct Obj { int kind; int id; std::wstring name; };
        std::vector<Obj> objs;
        for (const auto& it : pr_.items)
            if (it.kind == ItemKind::Mic)
                objs.push_back({1, it.id, it.label.empty() ? it.defId : it.label});
        for (const auto& dv : pr_.devices)
            objs.push_back({0, dv.id, dv.label.empty() ? DeviceCode(dv.kind) : dv.label});
        int okCount = 0, noCount = 0, made = 0;
        for (const auto& a : objs) {
            for (const auto& b : objs) {
                if (a.kind == b.kind && a.id == b.id) continue;
                std::wstring why;
                const bool ok = CanConnectRule(pr_, a.kind, a.id, b.kind, b.id, &why);
                if (ok) {
                    ++okCount;
                    // creating the link must never hang or throw
                    CreateCable(a.kind, a.id, b.kind, b.id);
                    ++made;
                    if (!pr_.links.empty()) {
                        pr_.links.pop_back();
                        RebuildAllCablePaths(pr_, MicRadiusM());
                    }
                } else {
                    ++noCount;
                }
                out += dr::Fmt(L"%s %d -> %s %d : %s%s\n", a.name.c_str(), a.kind,
                               b.name.c_str(), b.kind, ok ? L"OK" : L"NO",
                               why.empty() ? L"" : (L"  (" + why + L")").c_str());
            }
        }
        out += dr::Fmt(L"summary ok=%d rejected=%d created=%d\n", okCount, noCount, made);
        DLog("fuzz done");
        std::string utf8;
        const int need = WideCharToMultiByte(CP_UTF8, 0, out.c_str(), (int)out.size(),
                                             nullptr, 0, nullptr, nullptr);
        utf8.resize((size_t)need);
        WideCharToMultiByte(CP_UTF8, 0, out.c_str(), (int)out.size(), utf8.data(), need,
                            nullptr, nullptr);
        FILE* f = nullptr;
        if (fopen_s(&f, std::string(startupFuzz_.begin(), startupFuzz_.end()).c_str(),
                    "wb") == 0 && f) {
            fwrite(utf8.data(), 1, utf8.size(), f);
            fclose(f);
        }
        DestroyWindow(hwnd_);
        return 0;
    }
    if (startupAssoc_ != 0) {
        // --register-assoc / --unregister-assoc: run the setup action and quit
        const bool ok = (startupAssoc_ == 1) ? RegisterSmproj() : UnregisterSmproj();
        DestroyWindow(hwnd_);
        return ok ? 0 : 1;
    }
    if (!startupSave_.empty()) {
        // scripted save (round-trip tests / exports): write and quit
        Layout();
        // a scripted chooser pick must ride into the written file
        if (startupPngPick_ > 0) ApplyPngScale(startupPngPick_);
        SaveProjectTo(startupSave_);
        DestroyWindow(hwnd_);
        return 0;
    }
    if (!startupPng_.empty()) {
        Layout();
        // --pngpick N (the 导出图片 chooser's row N) picks through the same
        // remembering code the popup uses, then exports at that multiplier
        if (startupPngPick_ > 0) ApplyPngScale(startupPngPick_);
        const bool ok = ExportPngTo(startupPng_, pngScaleMul_);
        DestroyWindow(hwnd_);
        return ok ? 0 : 1;
    }
    if (!pdfExportPath_.empty()) {
        Layout();
        ExportPdfTo(pdfExportPath_);
        DestroyWindow(hwnd_);
        return 0;
    }
    // layout / geometry test hook: report the client-space position of every
    // canvas object so a script can click it without guessing the view transform
    if (!dumpPath_.empty()) {
        Layout();
        // the 01 page publishes its chip rects while it PAINTS, so force one
        // paint before reading them back (this hook is a test aid)
        UpdateWindow(hwnd_);
        std::wstring out = dr::Fmt(L"dpi %.4f uiScale %.4f ppm %.4f ox %.3f oy %.3f layer %d zoom %.4f\n",
                                   dpi_, uiScale_, ppm_, ox_, oy_, ui_.layer, ui_.zoom);
        // the client area's offset inside the window, so a PrintWindow screenshot
        // can be mapped onto the client coordinates this dump prints
        {
            RECT wr{};
            GetWindowRect(hwnd_, &wr);
            POINT c0{0, 0};
            ClientToScreen(hwnd_, &c0);
            out += dr::Fmt(L"clientoff %ld %ld winsz %ld %ld client %d %d\n",
                           c0.x - wr.left, c0.y - wr.top, wr.right - wr.left,
                           wr.bottom - wr.top, cw_, ch_);
        }
        // the stage the project is planning right now (metres) + the three
        // 音乐厅 / 教堂 / 沙龙 chips of the 01 page, so a script can press them
        out += dr::Fmt(L"stage %.3f %.3f\n", pr_.stage.w, pr_.stage.d);
        for (int i = 0; i < 3; ++i)
            out += dr::Fmt(L"stagepreset %d %ld %ld %ld %ld\n", i,
                           L_.presets[i].left, L_.presets[i].top,
                           L_.presets[i].right, L_.presets[i].bottom);
        out += dr::Fmt(L"canvas %ld %ld %ld %ld\n", L_.canvas.left, L_.canvas.top,
                       L_.canvas.right, L_.canvas.bottom);
        out += dr::Fmt(L"crossings %d runlen %.3f\n", CountCrossings(pr_.links),
                       TotalRunLen(pr_.links));
        for (const auto& dv : pr_.devices) {
            out += dr::Fmt(L"dev %d %ld %ld\n", dv.id,
                           (long)std::lround(ox_ + dv.x * ppm_),
                           (long)std::lround(oy_ + dv.y * ppm_));
            out += dr::Fmt(L"devw %d %.3f %.3f\n", dv.id, dv.x, dv.y);
        }
        for (const auto& it : pr_.items) {
            if (it.kind != ItemKind::Mic) continue;
            out += dr::Fmt(L"mic %d %ld %ld\n", it.id,
                           (long)std::lround(ox_ + it.x * ppm_),
                           (long)std::lround(oy_ + it.y * ppm_));
            out += dr::Fmt(L"micw %d %.3f %.3f %.1f\n", it.id, it.x, it.y, it.angle);
            // 对应乐器 binding of the microphone (regression hook: a bound GROUP
            // must carry the same spotType on every member)
            out += dr::Fmt(L"micspot %d <%s>\n", it.id, it.spotType.c_str());
            // ---- 1:1 SCALE AUDIT (item 2) ----------------------------------
            // the geometry the painters use, in metres and in device px, plus the
            // drawn marker radius: a script can compare the drawn capsule centres
            // (screenshot) with these and prove distance == spacing × ppm_
            out += dr::Fmt(L"micdef %d <%s> models <%s|%s|%s> ch %d\n", it.id,
                           it.defId.c_str(), it.modelL.c_str(), it.modelR.c_str(),
                           it.modelC.c_str(), MicItemChannels(it));
            out += dr::Fmt(L"micshape %d tech %d spacing %.4f center %.4f symscale %.4f\n",
                           it.id, it.mainTech, it.spacing, it.center, MicSymScaleOf(it));
            out += dr::Fmt(L"micradius %d %.4f %.4f\n", it.id, MicRadiusM() *
                                                              MicMarkerScaleOf(pr_, it.id),
                           MicMarkerRadiusF(it));
            {
                const std::vector<std::pair<double, double>> caps = MicCapsulePos(it);
                for (size_t ci = 0; ci < caps.size(); ++ci)
                    out += dr::Fmt(L"cap %d %d %.4f %.4f %ld %ld\n", it.id, (int)ci,
                                   caps[ci].first, caps[ci].second,
                                   (long)std::lround(ox_ + caps[ci].first * ppm_),
                                   (long)std::lround(oy_ + caps[ci].second * ppm_));
            }
        }
        for (size_t gi = 0; gi < pr_.micGroups.size(); ++gi) {
            const MicGroup& g = pr_.micGroups[gi];
            out += dr::Fmt(L"micgroup %d kind %d tech <%s> ids", (int)gi, g.kind,
                           g.tech.c_str());
            for (int id : g.ids) out += dr::Fmt(L" %d", id);
            out += L"\n";
        }
        for (int i = 0; i < kAddDeviceCount; ++i)
            out += dr::Fmt(L"add %d %ld %ld %ld %ld\n", i, L_.cbAdd[i].left,
                           L_.cbAdd[i].top, L_.cbAdd[i].right, L_.cbAdd[i].bottom);
        for (size_t i = 0; i < L_.cbRow.size(); ++i)
            out += dr::Fmt(L"row %d %ld %ld %ld %ld\n", (int)i, L_.cbRow[i].left,
                           L_.cbRow[i].top, L_.cbRow[i].right, L_.cbRow[i].bottom);
        // 05 「连接」 block: the section, its device rows, and the device the
        // selected microphone is wired to right now (-1 = none)
        {
            const Item* mi = pr_.Selected();
            out += dr::Fmt(L"connsec %ld %ld %ld %ld conn %d\n", L_.cbConnSec.left,
                           L_.cbConnSec.top, L_.cbConnSec.right, L_.cbConnSec.bottom,
                           (mi && mi->kind == ItemKind::Mic)
                               ? MicConnectedDevice(mi->id) : -1);
            for (size_t i = 0; i < L_.cbConnRow.size() && i < L_.cbConnDev.size(); ++i)
                out += dr::Fmt(L"connrow %d dev %d %ld %ld %ld %ld\n", (int)i,
                               L_.cbConnDev[i], L_.cbConnRow[i].left,
                               L_.cbConnRow[i].top, L_.cbConnRow[i].right,
                               L_.cbConnRow[i].bottom);
        }
        // 05 microphone inspector + the delete-route modal hit rects
        for (size_t i = 0; i < L_.cbRouteRow.size(); ++i) {
            out += dr::Fmt(L"route %d %ld %ld %ld %ld\n", (int)i, L_.cbRouteRow[i].left,
                           L_.cbRouteRow[i].top, L_.cbRouteRow[i].right,
                           L_.cbRouteRow[i].bottom);
            if (i < L_.cbRouteDel.size())
                out += dr::Fmt(L"routedel %d %ld %ld %ld %ld\n", (int)i,
                               L_.cbRouteDel[i].left, L_.cbRouteDel[i].top,
                               L_.cbRouteDel[i].right, L_.cbRouteDel[i].bottom);
            if (i < L_.cbRouteLink.size())
                out += dr::Fmt(L"routelink %d %d\n", (int)i, L_.cbRouteLink[i]);
        }
        out += dr::Fmt(L"modal %d %ld %ld %ld %ld %ld %ld %ld %ld\n",
                       ui_.confirmLink, L_.cfOk.left, L_.cfOk.top, L_.cfOk.right,
                       L_.cfOk.bottom, L_.cfCancel.left, L_.cfCancel.top,
                       L_.cfCancel.right, L_.cfCancel.bottom);
        // 预设 chip + the preset modal (only published while it is open)
        out += dr::Fmt(L"presetchip %ld %ld %ld %ld open %d rows %d\n",
                       L_.fnPreset.left, L_.fnPreset.top, L_.fnPreset.right,
                       L_.fnPreset.bottom, ui_.presetOpen ? 1 : 0,
                       (int)L_.pfRow.size());
        out += dr::Fmt(L"msg <%s>\n", ui_.msg.c_str());
        out += dr::Fmt(L"popup %d %d drag %d %d\n", ui_.popupIndex, ui_.popupCat,
                       ui_.dragCat_, ui_.paletteDragging_ ? 1 : 0);
        // palette tiles of the ACTIVE library tab (index into that catalogue)
        for (const auto& t : tiles_)
            out += dr::Fmt(L"tile %d %ld %ld %ld %ld\n", t.index, t.r.left, t.r.top,
                           t.r.right, t.r.bottom);
        // 文件 dropdown: the chip + its five rows (open / save / save as / register
        // the .smproj association / remove it)
        out += dr::Fmt(L"fnfile %ld %ld %ld %ld open %d\n", L_.fnFile.left,
                       L_.fnFile.top, L_.fnFile.right, L_.fnFile.bottom,
                       ui_.fileOpen ? 1 : 0);
        // the top-bar function chips (top-right cluster): a script needs the export
        // chip's rect to open the 导出图片 scale chooser
        out += dr::Fmt(L"fnchips pdf %ld %ld %ld %ld preset %ld %ld %ld %ld "
                       L"simple %ld %ld %ld %ld sra %ld %ld %ld %ld\n",
                       L_.fnPdf.left, L_.fnPdf.top, L_.fnPdf.right, L_.fnPdf.bottom,
                       L_.fnPreset.left, L_.fnPreset.top, L_.fnPreset.right,
                       L_.fnPreset.bottom, L_.fnSimple.left, L_.fnSimple.top,
                       L_.fnSimple.right, L_.fnSimple.bottom, L_.fnSra.left,
                       L_.fnSra.top, L_.fnSra.right, L_.fnSra.bottom);
        for (int i = 0; i < 5; ++i)
            out += dr::Fmt(L"filerow %d %ld %ld %ld %ld\n", i, L_.fileRow[i].left,
                           L_.fileRow[i].top, L_.fileRow[i].right, L_.fileRow[i].bottom);
        // current selection (item ids) + how many devices are selected: Ctrl+A and
        // click tests read these back
        {
            out += dr::Fmt(L"sel %d", (int)pr_.selIds.size());
            for (int id : pr_.selIds) out += dr::Fmt(L" %d", id);
            out += L"\n";
            int devSel = 0;
            for (const auto& dv : pr_.devices)
                if (dv.selected) ++devSel;
            out += dr::Fmt(L"devsel %d\n", devSel);
            // undo / redo depth AFTER the scripted input (regression hook)
            out += dr::Fmt(L"undo %d redo %d\n", (int)undoStack_.size(),
                           (int)redoStack_.size());
        }
        // instrument placement (the mic lines above only cover microphones)
        for (const auto& it : pr_.items) {
            if (it.kind != ItemKind::Instrument) continue;
            out += dr::Fmt(L"inst %d %ld %ld\n", it.id,
                           (long)std::lround(ox_ + it.x * ppm_),
                           (long)std::lround(oy_ + it.y * ppm_));
            out += dr::Fmt(L"instw %d %.3f %.3f %.1f\n", it.id, it.x, it.y, it.angle);
            out += dr::Fmt(L"instlabel %d <%s>\n", it.id, it.label.c_str());
            // per-item 颜色 override (0 = the palette slot of defId) + the tint the
            // icon actually paints, so a script can prove the override took effect
            {
                const Rgb ic = ItemIconTint(pr_, it, true);
                out += dr::Fmt(L"instcolor %d 0x%06X tint #%02X%02X%02X\n", it.id,
                               it.colorOverride & 0xFFFFFFu, (int)ic.r, (int)ic.g,
                               (int)ic.b);
            }
        }
        // the rotation handle a script has to grab + the pivot it turns about
        {
            double hx = 0, hy = 0;
            bool ok = false;
            if (pr_.selIds.size() >= 2) ok = RotateHandlePosMulti(hx, hy);
            else if (pr_.Selected()) ok = RotateHandlePos(*pr_.Selected(), hx, hy);
            RECT sb{};
            if (SelectionBBox(sb))
                out += dr::Fmt(L"selbbox %.1f %.1f %.1f %.1f pivot %.4f %.4f\n",
                               (double)sb.left, (double)sb.top, (double)sb.right,
                               (double)sb.bottom,
                               ((sb.left + sb.right) * 0.5 - ox_) / ppm_,
                               ((sb.top + sb.bottom) * 0.5 - oy_) / ppm_);
            out += dr::Fmt(L"rothandle %d %.1f %.1f\n", ok ? 1 : 0, hx, hy);
        }
        {
            int ins = 0, mics = 0;
            for (const auto& it : pr_.items)
                (it.kind == ItemKind::Instrument ? ins : mics) += 1;
            out += dr::Fmt(L"items %d inst %d mic %d\n", (int)pr_.items.size(),
                           ins, mics);
        }
        // channel table: the row order (automatic or manually dragged) and the
        // stored manual order, so a script can read the 03 order back
        {
            const std::vector<ChRow> rows = BuildChannelRows();
            for (size_t i = 0; i < rows.size(); ++i)
                out += dr::Fmt(L"chrow %d item %d ch %d inst <%s> model <%s> cap %d toMain %.2f az %.2f len %.2f\n",
                               (int)i, rows[i].itemId, rows[i].ch,
                               rows[i].inst.c_str(), rows[i].model.c_str(),
                               rows[i].capsule, rows[i].toMain,
                               rows[i].hasAz ? rows[i].azimuth : -999.0,
                               rows[i].len);
            out += dr::Fmt(L"chrows %d\n", (int)rows.size());
            // timing of the last paint / export (freeze diagnostics)
            out += dr::Fmt(L"timing paint %.1f png %.1f pdf %.1f\n", lastPaintMs_,
                           lastPngMs_, lastPdfMs_);
            // 话筒线长度 mode + the header caption the table is showing
            {
                const ChTableGeom g2 = ChannelTableGeom();
                const std::wstring h = ChannelLenHeader();
                out += dr::Fmt(L"lenmode %d <%s> hdrl %ld hdrr %ld headY %d headH %d\n",
                               pr_.lenMode, h.c_str(),
                               (long)std::lround(g2.cols.l[5]),
                               (long)std::lround(g2.cols.r[5]), (int)g2.headerY,
                               (int)g2.headerH);
            }
            // the 布线 tail block (接口箱 → 声卡 summary rows) and the pager buttons,
            // so a script can verify the block and flip to its page
            {
                const std::vector<ChRow> all = BuildChannelDisplayRows();
                for (size_t i = rows.size(); i < all.size(); ++i)
                    out += dr::Fmt(
                        L"chsum idx %d ch <%s> link <%s> len %.2f custom %d sep %d\n",
                        (int)i, all[i].chText.c_str(), all[i].link.c_str(),
                        all[i].len, all[i].custom ? 1 : 0, all[i].separator ? 1 : 0);
                out += dr::Fmt(L"chdisp %d\n", (int)all.size());
            }
            // each visible row's rect + its 通道 cell centre, so a script can drag
            // a row without guessing the table geometry
            const ChTableGeom gt = ChannelTableGeom();
            const int nVis = (std::min)((int)rows.size(), gt.visibleRows);
            for (int i = 0; i < nVis; ++i) {
                const int ty = gt.rowsY + i * gt.rowH;
                out += dr::Fmt(L"chrowrect %d %ld %ld %ld %ld grab %ld %ld\n", i,
                               (long)gt.x0, (long)ty, (long)gt.x1, (long)(ty + gt.rowH),
                               (long)std::lround(gt.cols.l[0]),
                               (long)(ty + gt.rowH / 2));
            }
            out += L"chorder";
            for (int id : pr_.channelOrder) out += dr::Fmt(L" %d", id);
            out += L"\n";
            // the six column spans, so a script can click a specific cell
            for (int c = 0; c < kChCols; ++c)
                if (ui_.chColVis[c])
                    out += dr::Fmt(L"chcol %d %ld %ld\n", c,
                                   (long)std::lround(gt.cols.l[c]),
                                   (long)std::lround(gt.cols.r[c]));
            out += dr::Fmt(L"bindconfirm %d n %d maxdist %.3f ok %ld %ld %ld %ld\n",
                           ui_.bindConfirmOpen ? 1 : 0, (int)ui_.bindIds.size(),
                           BindGroupMaxDist(),
                           (long)L_.bgOk.left, (long)L_.bgOk.top,
                           (long)L_.bgOk.right, (long)L_.bgOk.bottom);
        {
            const int pr2 = (std::max)(1, gt.pageRows);
            const int np = (std::max)(1, ((int)rows.size() + pr2 - 1) / pr2);
            out += dr::Fmt(L"chchip font %.1f lastr %ld availx %ld\n", gt.chipFont,
                           (long)gt.chip[7].right, (long)(gt.x1 - (gt.x0 + D(68))));
            out += dr::Fmt(L"chpage %d %d %d\n", ui_.chPage, np, pr2);
            out += dr::Fmt(L"chpagebtn prev %ld %ld %ld %ld next %ld %ld %ld %ld\n",
                           (long)gt.pagePrev.left, (long)gt.pagePrev.top,
                           (long)gt.pagePrev.right, (long)gt.pagePrev.bottom,
                           (long)gt.pageNext.left, (long)gt.pageNext.top,
                           (long)gt.pageNext.right, (long)gt.pageNext.bottom);
            if (eChCell_ && IsWindowVisible(eChCell_)) {
                RECT er{};
                GetWindowRect(eChCell_, &er);
                POINT o0{0, 0}; ClientToScreen(hwnd_, &o0);
                out += dr::Fmt(L"chedit %ld %ld %ld %ld item %d col %d`n",
                               er.left - o0.x, er.top - o0.y, er.right - o0.x,
                               er.bottom - o0.y, chEditItem_, chEditCol_);
            }
        }        out += dr::Fmt(L"chautosort %ld %ld %ld %ld\n", gt.autoSort.left,
                           gt.autoSort.top, gt.autoSort.right, gt.autoSort.bottom);
            // the 对应乐器 picker's row hit rects (published while it is open)
            for (size_t i = 0; i < L_.chPickRow.size(); ++i)
                out += dr::Fmt(L"chpick %d %ld %ld %ld %ld\n", (int)i,
                               L_.chPickRow[i].left, L_.chPickRow[i].top,
                               L_.chPickRow[i].right, L_.chPickRow[i].bottom);
            // 04 链路 / 路由 device popup: the panel, its rows and the microphone it
            // was opened for, so a script can click a device without guessing
            out += dr::Fmt(L"chrouteitem %d panel %ld %ld %ld %ld rows %d\n",
                           ui_.chRouteOpen, L_.chRoutePanel.left, L_.chRoutePanel.top,
                           L_.chRoutePanel.right, L_.chRoutePanel.bottom,
                           (int)L_.chRouteRow.size());
            for (size_t i = 0; i < L_.chRouteRow.size() && i < L_.chRouteDev.size(); ++i)
                out += dr::Fmt(L"chrouterow %d dev %d %ld %ld %ld %ld\n", (int)i,
                               L_.chRouteDev[i], L_.chRouteRow[i].left,
                               L_.chRouteRow[i].top, L_.chRouteRow[i].right,
                               L_.chRouteRow[i].bottom);
        }
        for (size_t i = 0; i < L_.pfRow.size(); ++i)
            out += dr::Fmt(L"presetrow %d %ld %ld %ld %ld\n", (int)i,
                           L_.pfRow[i].left, L_.pfRow[i].top, L_.pfRow[i].right,
                           L_.pfRow[i].bottom);
        out += dr::Fmt(L"presetcancel %ld %ld %ld %ld\n", L_.pfCancel.left,
                       L_.pfCancel.top, L_.pfCancel.right, L_.pfCancel.bottom);
        out += dr::Fmt(L"micradius %.4f\n", MicRadiusM());
        // ---- app lock: state + every hit rect (so a script can drive the chip,
        // the popover and the passcode modal without guessing) ---------------
        out += dr::Fmt(L"lock state %d open %d modal %d input %d err %d\n", lockState_,
                       lockOpen ? 1 : 0, lockModal, (int)lockInput.size(),
                       lockErr ? 1 : 0);
        out += dr::Fmt(L"lockchip %ld %ld %ld %ld\n", L_.lockChip.left,
                       L_.lockChip.top, L_.lockChip.right, L_.lockChip.bottom);
        out += dr::Fmt(L"lockpop %ld %ld %ld %ld\n", L_.lockPop.left, L_.lockPop.top,
                       L_.lockPop.right, L_.lockPop.bottom);
        for (int i = 0; i < 3; ++i)
            out += dr::Fmt(L"lockrow %d %ld %ld %ld %ld\n", i, L_.lockRow[i].left,
                           L_.lockRow[i].top, L_.lockRow[i].right, L_.lockRow[i].bottom);
        for (int i = 0; i < 4; ++i)
            out += dr::Fmt(L"lockcell %d %ld %ld %ld %ld\n", i, L_.lockCells[i].left,
                           L_.lockCells[i].top, L_.lockCells[i].right,
                           L_.lockCells[i].bottom);
        out += dr::Fmt(L"lockok %ld %ld %ld %ld\n", L_.lockOk.left, L_.lockOk.top,
                       L_.lockOk.right, L_.lockOk.bottom);
        out += dr::Fmt(L"lmpanel %ld %ld %ld %ld\n", L_.lmPanel.left, L_.lmPanel.top,
                       L_.lmPanel.right, L_.lmPanel.bottom);
        for (int i = 0; i < 4; ++i)
            out += dr::Fmt(L"lmcell %d %ld %ld %ld %ld\n", i, L_.lmCells[i].left,
                           L_.lmCells[i].top, L_.lmCells[i].right,
                           L_.lmCells[i].bottom);
        out += dr::Fmt(L"lmok %ld %ld %ld %ld\n", L_.lmOk.left, L_.lmOk.top,
                       L_.lmOk.right, L_.lmOk.bottom);
        out += dr::Fmt(L"lmcancel %ld %ld %ld %ld\n", L_.lmCancel.left,
                       L_.lmCancel.top, L_.lmCancel.right, L_.lmCancel.bottom);
        // ---- same-type group hulls (item 4): every hull the stage paints ----
        for (const auto& hh : BuildTypeHulls()) {
            out += dr::Fmt(L"hull <%s> n %d clean %d pts %d label %.3f %.3f\n",
                           hh.defId.c_str(), hh.count, hh.clean ? 1 : 0,
                           (int)hh.pts.size(), hh.labelX, hh.labelY);
            for (const auto& pt : hh.pts)
                out += dr::Fmt(L"hullpt <%s> %.3f %.3f\n", hh.defId.c_str(), pt.first,
                               pt.second);
        }
        // ---- 对应乐器 picker state (item 3) ---------------------------------
        out += dr::Fmt(L"spotgroup %d spotlist %d chspot %d crops %d\n", ui_.spotGroup,
                       ui_.spotListOpen, ui_.chSpotOpen, SpotGroupCount());
        out += dr::Fmt(L"spotsub %ld %ld %ld %ld n %d\n", L_.spotSubPanel.left,
                       L_.spotSubPanel.top, L_.spotSubPanel.right,
                       L_.spotSubPanel.bottom, (int)L_.spotSubRow.size());
        for (size_t i = 0; i < L_.spotSubRow.size(); ++i)
            out += dr::Fmt(L"spotsubrow %d %ld %ld %ld %ld\n", (int)i,
                           L_.spotSubRow[i].left, L_.spotSubRow[i].top,
                           L_.spotSubRow[i].right, L_.spotSubRow[i].bottom);
        // ---- right inspector audit (regression hook) -------------------------
        // For every inspector field: the rects the PAINTER uses (label / box) and
        // the EDIT rect the layout derives from the same FieldBox, plus the real
        // child window rectangle, its window region and its visibility.  A script
        // can therefore prove that no EDIT drifts away from its painted box and
        // that nothing paints outside the panel.  `scroll` must be the offset the
        // rects were built from.
        out += dr::Fmt(L"insp panel %ld %ld %ld %ld scroll %d content %d\n",
                       L_.right.left, L_.right.top, L_.right.right, L_.right.bottom,
                       (int)ui_.rightScroll, L_.rightContent);
        // scripted-click rects of the selection actions and the 对应乐器 picker, so a
        // regression script can bind a group without guessing the inspector layout
        out += dr::Fmt(L"spotbtn %ld %ld %ld %ld open %d\n", L_.spotBtn.left,
                       L_.spotBtn.top, L_.spotBtn.right, L_.spotBtn.bottom,
                       ui_.spotListOpen);
        out += dr::Fmt(L"bindbtn %ld %ld %ld %ld\n", L_.bindBtn.left, L_.bindBtn.top,
                       L_.bindBtn.right, L_.bindBtn.bottom);
        out += dr::Fmt(L"treebtn %ld %ld %ld %ld\n", L_.treeBtn.left, L_.treeBtn.top,
                       L_.treeBtn.right, L_.treeBtn.bottom);
        out += dr::Fmt(L"delbtn %ld %ld %ld %ld\n", L_.delBtn.left, L_.delBtn.top,
                       L_.delBtn.right, L_.delBtn.bottom);
        // the 颜色 row of the item inspector + its 「恢复默认」 button
        out += dr::Fmt(L"itemcolor row %ld %ld %ld %ld swatch %ld %ld %ld %ld "
                       L"reset %ld %ld %ld %ld\n",
                       L_.itemColorRow.left, L_.itemColorRow.top,
                       L_.itemColorRow.right, L_.itemColorRow.bottom,
                       L_.itemColorSwatch.left, L_.itemColorSwatch.top,
                       L_.itemColorSwatch.right, L_.itemColorSwatch.bottom,
                       L_.itemColorReset.left, L_.itemColorReset.top,
                       L_.itemColorReset.right, L_.itemColorReset.bottom);
        for (int i = 0; i < 16; ++i) {
            if (L_.spotListRow[i].right <= L_.spotListRow[i].left) continue;
            out += dr::Fmt(L"spotrow %d %ld %ld %ld %ld\n", i,
                           L_.spotListRow[i].left, L_.spotListRow[i].top,
                           L_.spotListRow[i].right, L_.spotListRow[i].bottom);
        }
        // 01 项目信息: the centred page panel (the fields below are laid out inside
        // it while the page is active) and the six tab hit rects
        out += dr::Fmt(L"p1 panel %ld %ld %ld %ld\n", L_.p1Panel.left,
                       L_.p1Panel.top, L_.p1Panel.right, L_.p1Panel.bottom);
        for (int i = 0; i < kTabCount; ++i)
            out += dr::Fmt(L"tab %d %ld %ld %ld %ld\n", i, L_.layerTabs[i].left,
                           L_.layerTabs[i].top, L_.layerTabs[i].right,
                           L_.layerTabs[i].bottom);
        auto fldLine = [&](const wchar_t* nm, const FieldBox& f, HWND h) {
            out += dr::Fmt(L"fld %s lab %ld %ld %ld %ld box %ld %ld %ld %ld edt %ld %ld %ld %ld",
                           nm, f.label.left, f.label.top, f.label.right, f.label.bottom,
                           f.box.left, f.box.top, f.box.right, f.box.bottom,
                           f.edit.left, f.edit.top, f.edit.right, f.edit.bottom);
            RECT wr{};
            if (h && GetWindowRect(h, &wr)) {
                POINT pa{wr.left, wr.top};
                POINT pb{wr.right, wr.bottom};
                ScreenToClient(hwnd_, &pa);
                ScreenToClient(hwnd_, &pb);
                RECT rb{};
                HRGN rg = CreateRectRgn(0, 0, 0, 0);
                if (rg) {
                    if (GetWindowRgn(h, rg) != ERROR) GetRgnBox(rg, &rb);
                    DeleteObject(rg);
                }
                out += dr::Fmt(L" win %ld %ld %ld %ld rgn %ld %ld %ld %ld vis %d",
                               pa.x, pa.y, pb.x, pb.y, rb.left, rb.top, rb.right,
                               rb.bottom, IsWindowVisible(h) ? 1 : 0);
            } else {
                out += L" win -";
            }
            out += L"\n";
        };
        fldLine(L"piece", L_.piece, ePiece_);
        fldLine(L"venue", L_.venue, eVenue_);
        fldLine(L"conductor", L_.conductor, eConductor_);
        fldLine(L"syseng", L_.sysEng, eSysEng_);
        fldLine(L"receng", L_.recEng, eRecEng_);
        fldLine(L"assteng", L_.asstEng, eAsstEng_);
        fldLine(L"notes", L_.notes, eNotes_);
        fldLine(L"width", L_.wField, eWidth_);
        fldLine(L"depth", L_.dField, eDepth_);
        fldLine(L"ceiling", L_.ceilingField, eCeiling_);
        fldLine(L"selname", L_.selName, eSelName_);
        fldLine(L"x", L_.xField, eX_);
        fldLine(L"y", L_.yField, eY_);
        fldLine(L"angle", L_.angleField, eAngle_);
        fldLine(L"pianolen", L_.lenField, ePianoLen_);
        fldLine(L"mich", L_.micHeight, eMicH_);
        fldLine(L"micmodel", L_.micModel, eMicModel_);
        fldLine(L"space", L_.spaceField, eSpace_);
        fldLine(L"center", L_.centerField, eCenter_);
        fldLine(L"modell", L_.modelL, eModelL_);
        fldLine(L"modelr", L_.modelR, eModelR_);
        fldLine(L"modelc", L_.modelC, eModelC_);
        fldLine(L"flyh", L_.flyField, eFlyH_);
        fldLine(L"devname", L_.cbName, eDevName_);
        fldLine(L"devch", L_.cbChannels, eDevCh_);
        fldLine(L"devx", L_.cbX, eDevX_);
        fldLine(L"devy", L_.cbY, eDevY_);
        for (int ti = 0; ti < 2; ++ti)
            out += dr::Fmt(L"textrow %d %ld %ld %ld %ld check %ld %ld %ld %ld "
                           L"show %d\n", ti, L_.textRow[ti].left, L_.textRow[ti].top,
                           L_.textRow[ti].right, L_.textRow[ti].bottom,
                           L_.textCheck[ti].left, L_.textCheck[ti].top,
                           L_.textCheck[ti].right, L_.textCheck[ti].bottom,
                           ti == 0 ? (pr_.showInstrumentText ? 1 : 0)
                                   : (pr_.showMicText ? 1 : 0));
        // 导出图片 scale chooser: the remembered multiplier, its open state and the
        // rects the painter published (a closed chooser has empty rects)
        out += dr::Fmt(L"pngscale mul %d open %d hover %d list %ld %ld %ld %ld "
                       L"foot %ld %ld %ld %ld\n",
                       pngScaleMul_, ui_.pngScaleOpen ? 1 : 0, ui_.pngScaleHoverRow,
                       L_.pngScaleList.left,
                       L_.pngScaleList.top, L_.pngScaleList.right,
                       L_.pngScaleList.bottom, L_.pngScaleFoot.left,
                       L_.pngScaleFoot.top, L_.pngScaleFoot.right,
                       L_.pngScaleFoot.bottom);
        for (int pi = 0; pi < 3; ++pi) {
            int pw = 0, ph = 0;
            double pww = 0, pwh = 0;
            PngExportSize(kPngScales[pi], pw, ph, pww, pwh);
            out += dr::Fmt(L"pngscalerow %d scale %d px %d %d rect %ld %ld %ld %ld\n",
                           pi, kPngScales[pi], pw, ph, L_.pngScaleRow[pi].left,
                           L_.pngScaleRow[pi].top, L_.pngScaleRow[pi].right,
                           L_.pngScaleRow[pi].bottom);
        }
        out += dr::Fmt(L"pngscale pr %d\n", pr_.pngScale);
        extern std::vector<std::wstring> g_labelDump;
        for (const auto& lb : g_labelDump) out += lb + L"\n";
        extern std::vector<std::wstring> g_micStackDump;
        for (const auto& ms : g_micStackDump) out += ms + L"\n";
        for (size_t si = 0; si < L_.sameTypeBtn.size(); ++si)
            out += dr::Fmt(L"sametype %d <%s> %ld %ld %ld %ld\n", (int)si,
                           L_.sameTypeDef[si].c_str(), L_.sameTypeBtn[si].left,
                           L_.sameTypeBtn[si].top, L_.sameTypeBtn[si].right,
                           L_.sameTypeBtn[si].bottom);
        out += dr::Fmt(L"flown row %ld %ld %ld %ld check %ld %ld %ld %ld\n",
                       L_.flownRow.left, L_.flownRow.top, L_.flownRow.right,
                       L_.flownRow.bottom, L_.flownCheck.left, L_.flownCheck.top,
                       L_.flownCheck.right, L_.flownCheck.bottom);
        out += dr::Fmt(L"cbm i title %ld %ld %ld %ld info %ld %ld %ld %ld\n",
                       L_.cbMiTitle.left, L_.cbMiTitle.top, L_.cbMiTitle.right,
                       L_.cbMiTitle.bottom, L_.cbMiInfo.left, L_.cbMiInfo.top,
                       L_.cbMiInfo.right, L_.cbMiInfo.bottom);
        for (const auto& dv : pr_.devices) {
            const std::wstring cap =
                Project::DeviceIsHub(dv)
                    ? std::wstring(L"hub")
                    : dr::Fmt(L"%d/%d", DeviceLoad(pr_, pr_.links, dv.id),
                              pr_.DeviceCapacity(dv));
            out += dr::Fmt(L"devload %d %s %s %s%s\n", dv.id, DeviceCode(dv.kind),
                           cap.c_str(),
                           DeviceOverloaded(pr_, pr_.links, dv.id) ? L"OVER" : L"ok",
                           dv.locked ? L" locked" : L"");
        }
        for (const auto& ln : pr_.links) {
            const CableIssue is = CheckCable(pr_, ln);
            std::wstring s = dr::Fmt(L"link %d %d %d %d %d %.3f %d", ln.aKind, ln.a,
                                     ln.bKind, ln.b, ln.channels, is.length,
                                     (int)ln.path.size());
            for (const auto& pt : ln.path)
                s += dr::Fmt(L" %.3f %.3f", pt.first, pt.second);
            for (const auto& cp : ln.capsulePaths) {
                // the run's own size + its terminal (metres): a script can check
                // that the terminal radius lands exactly on the drawn circle
                s += dr::Fmt(L" | run %d", (int)cp.size());
                if (cp.size() >= 2)
                    s += dr::Fmt(L" %.3f %.3f -> %.3f %.3f", cp.front().first,
                                 cp.front().second, cp.back().first, cp.back().second);
            }
            // converged group: the collector stubs + the convergence point, so a
            // script can prove that the members converge instead of running in
            // parallel
            if (!ln.collectorPaths.empty()) {
                s += dr::Fmt(L" | collectors %d len %.3f",
                             (int)ln.collectorPaths.size(), CollectorLen(ln));
                for (const auto& cp : ln.collectorPaths)
                    if (cp.size() >= 2)
                        s += dr::Fmt(L" [ %.3f %.3f -> %.3f %.3f ]", cp.front().first,
                                     cp.front().second, cp.back().first,
                                     cp.back().second);
            }
            // the two length conventions, so a script can check both: REAL (05)
            // and RESERVED (03 / 04), plus a manual override when there is one
            s += dr::Fmt(L" | real %.3f reserved %.3f", CableRealLen(pr_, ln),
                         CableReservedLen(pr_, ln));
            if (const double ov = CableOverrideLen(pr_, ln); ov > 0.0)
                s += dr::Fmt(L" custom %.3f", ov);
            if (ln.flown)
                s += dr::Fmt(L" | flown up %.3f ceil %.3f wall %.3f floor %.3f sum %.3f",
                             ln.legUp, ln.legCeil, ln.legWall, ln.legFloor,
                             FlownRunLength(ln.legUp, ln.legCeil, ln.legWall,
                                            ln.legFloor));
            s += dr::Fmt(L" | oc %d", is.overChannels ? 1 : 0);
            std::wstring why;
            if (CableProblemText(pr_, ln, &why)) s += L" why <" + why + L">";
            out += s + L"\n";
        }
        { std::ofstream f(dumpPath_, std::ios::binary | std::ios::trunc);
          const std::string b((const char*)out.data(), out.size() * sizeof(wchar_t));
          if (f) f.write(b.data(), (std::streamsize)b.size()); }
        DestroyWindow(hwnd_);
        return 0;
    }
    if (startupClose_) {
        // exercise the real close path (title-bar × / Alt+F4 / 系统菜单)
        DLog("close hook: posting WM_CLOSE");
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
    UpdateWindow(hwnd_);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

// ---------------------------------------------------------------------------
// WndProc: the message entry point (StaticProc thunks here).  It only routes:
// painting, the mouse/keyboard handlers, the EDIT notifications and the layout
// triggers.  Every hit test and every page lives in its own painter / handler.
// ---------------------------------------------------------------------------
LRESULT App::WndProc(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd_, &ps);
        Paint(dc);
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_PRINTCLIENT:   // the dump hooks render through WM_PAINT / this one
        Paint(reinterpret_cast<HDC>(wp));
        return 0;
    case WM_ERASEBKGND:
        return 1;   // every pixel is painted by Paint()
    case WM_SIZE:
        cw_ = (int)LOWORD(lp);
        ch_ = (int)HIWORD(lp);
        if (cw_ > 0 && ch_ > 0) {
            Layout();
            InvalidateAll();
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize.x = (LONG)(1100 * dpi_);
        mmi->ptMinTrackSize.y = (LONG)(800 * dpi_);
        return 0;
    }
    case WM_MOUSEMOVE:
        MouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSELEAVE:
        if (ui_.dragCat_ >= 0 && GetCapture() != hwnd_) CancelPaletteDrag();
        ClearHover();
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd_);
        MouseDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONUP:
        MouseUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONDBLCLK:
        // CS_DBLCLKS: the second press of a double click arrives HERE instead of
        // as another WM_LBUTTONDOWN.  No page has a double-click gesture any
        // more (the retired 04 diagram was the last one), so it is swallowed and
        // the stage / table keep their click-only behaviour.
        return 0;
    case WM_RBUTTONDOWN:
        SetFocus(hwnd_);
        MouseDownRight(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MBUTTONDOWN: {   // middle-drag pans anywhere on the canvas
        SetFocus(hwnd_);
        const POINT mp = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (PtInRect(&L_.canvas, mp)) PanBegin(mp.x, mp.y, false);
        return 0;
    }
    case WM_MBUTTONUP:
        if (panning_) PanEnd();
        return 0;
    case WM_MOUSEWHEEL:
        MouseWheel(GET_WHEEL_DELTA_WPARAM(wp), GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_KEYDOWN:
        OnKeyDown((int)wp);
        return 0;
    case WM_KEYUP:
        if (wp == VK_SPACE) {
            panSpace_ = false;
            if (!panning_) SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        }
        return 0;
    case WM_SETCURSOR:
        // a synchronous export owns the UI thread: a WAIT cursor stays pinned even
        // while the mouse moves over the window (see App::BeginBusy)
        if (busy_) {
            SetCursor(LoadCursorW(nullptr, IDC_WAIT));
            return TRUE;
        }
        if (LOWORD(lp) == HTCLIENT) {
            POINT cp;
            GetCursorPos(&cp);
            ScreenToClient(hwnd_, &cp);
            // a hand cursor while panning — or while Space arms a pan on the canvas
            if (panning_ || (panSpace_ && PtInRect(&L_.canvas, cp))) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            if (PtInRect(&L_.canvas, cp)) {
                if (ui_.layer == 2) {
                    if (DeviceHitTest(cp.x, cp.y) >= 0) {
                        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                        return TRUE;
                    }
                } else if (RotateHandleHit(cp.x, cp.y)) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                } else {
                    bool locked = false;
                    if (StageHitTest(cp.x, cp.y, locked) >= 0) {
                        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                        return TRUE;
                    }
                }
            }
        }
        break;
    case WM_TIMER:
        if (wp == 7) {   // transient status-bar message expired
            KillTimer(hwnd_, 7);
            ui_.msg.clear();
            ui_.msgWarn = 0;
            InvalidateAll();
        }
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        const int code = HIWORD(wp);
        if (code == EN_SETFOCUS) {
            ui_.focusEdit = id;
            InvalidateAll();
        } else if (code == EN_KILLFOCUS) {
            const bool wasFocused = (ui_.focusEdit == id);
            if (ui_.focusEdit == id) ui_.focusEdit = 0;
            // commit on focus loss: only now is an out-of-range number clamped
            // and written back into the field
            if (wasFocused && !syncing_) {
                commitEdit_ = true;
                ParseEdit(id);
                commitEdit_ = false;
                SyncEdits();
                RefreshAfterProjectChange();
            }
            InvalidateAll();
        } else if (code == EN_CHANGE) {
            ParseEdit(id);
        }
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetTextColor(dc, RGB(46, 44, 40));
        SetBkColor(dc, RGB(255, 255, 255));
        return reinterpret_cast<LRESULT>(fieldBrush_);
    }
    case WM_CAPTURECHANGED:
        if (ui_.dragCat_ >= 0) CancelPaletteDrag();
        panning_ = false;
        panMoved_ = false;
        panDeselect_ = false;
        if (devDrag_) {
            devDrag_ = false;
            devDragId_ = -1;
            devDragMoved_ = false;
        }
        if (drag_) {
            drag_ = false;
            dragId_ = -1;
        }
        if (rotating_) {
            rotating_ = false;
            rotId_ = -1;
        }
        return 0;
    case WM_DPICHANGED: {
        const RECT* r = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left,
                     r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        ApplyDpi();
        return 0;
    }
    case WM_CLOSE:
        // the title-bar ×, Alt+F4 and the system menu all arrive here.  With
        // unsaved changes the modal 是否保存更改？ owns the decision; RequestClose
        // only destroys the window when the project is clean / locked or the user
        // answered 保存 (and it succeeded) / 不保存.
        RequestClose();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

// The DPI of the monitor the window is on.  Everything measured through D()
// depends on it, so a change rebuilds the fonts and re-runs the layout.
void App::ApplyDpi() {
    float d = (float)GetDpiForWindow(hwnd_) / 96.f;
    if (d < 0.5f) d = 1.f;
    if (std::fabs(d - dpi_) < 0.001f) return;
    dpi_ = d;
    RebuildFonts();
    Layout();
    InvalidateAll();
}

void App::OnCreate() {
    fieldBrush_ = CreateSolidBrush(RGB(255, 255, 255));

    HICON hBig = (HICON)LoadImageW(hinst_, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXICON),
                                   GetSystemMetrics(SM_CYICON), 0);
    HICON hSmall = (HICON)LoadImageW(hinst_, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                     GetSystemMetrics(SM_CXSMICON),
                                     GetSystemMetrics(SM_CYSMICON), 0);
    if (!hBig) hBig = LoadIconW(nullptr, IDI_APPLICATION);
    if (!hSmall) hSmall = hBig;
    SendMessageW(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)hBig);
    SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);

    RebuildFonts();
    BuildEmptyProject(pr_);
    CreateControls();
    SyncEdits();
    UpdateCueBanners();
    SetWindowTextW(hwnd_, L10n::IsZh() ? (std::wstring(L"Stage Micing — ") + L10n::T(S::AppName)).c_str() : L"Stage Micing");
    Layout();
}

void App::RebuildFonts() {
    if (uiFont_) DeleteObject(uiFont_);
    // the global +3 bump also applies to the native EDIT controls
    uiFont_ = CreateFontW(-D(17), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                          L"Microsoft YaHei UI");
    for (auto& kv : fonts_) delete kv.second;
    fonts_.clear();

    HWND all[] = {ePiece_, eVenue_, eConductor_, eNotes_, eSelName_,
                  eWidth_, eDepth_, eOff_, eCeiling_, eFlyH_, eSearch_, eX_, eY_, eAngle_, eMicH_, eSpace_, eCenter_,
        eModelL_, eModelR_, eModelC_, eSysEng_, eRecEng_, eAsstEng_, eMicModel_,
        eDevName_, eDevCh_};
    for (HWND h : all) {
        if (h) SendMessageW(h, WM_SETFONT, (WPARAM)uiFont_, TRUE);
    }
}

Gdiplus::Font* App::Font(float px, bool bold) {
    // Global type bump: every drawing helper asks for its size in "design" units,
    // and +3 is applied here once, so labels, table cells, node text, hints, the
    // status bar and the inspector all grow together (docs/CABLING.md P3).
    // type scale: the +3 bump was slightly too large and made the inspector look
    // crowded while scrolling; +2 keeps the larger, calmer look without clipping
    const float sz = (std::max)(7.f, px + 2.f) * exportFontScale_;
    int key = (int)std::lround(sz * 10.f) * 2 + (bold ? 1 : 0) +
              (int)std::lround(uiScale_ * 1000.f) * 1000;
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    auto* f = new Gdiplus::Font(family_, sz * dpi_ * uiScale_,
                               bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular,
                               Gdiplus::UnitPixel);
    fonts_[key] = f;
    return f;
}

void App::CreateControls() {
    auto mk = [&](int id, DWORD extra) -> HWND {
        // created HIDDEN: the first paint happens before Layout() has placed them, and
        // visible-at-creation controls flashed on top of each other for a frame
        HWND e = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_LEFT | extra,
                                 0, 0, 10, 10, hwnd_, (HMENU)(INT_PTR)id,
                                 hinst_, nullptr);
        // stay hidden until Layout() places it next to its painted box
        ShowWindow(e, SW_HIDE);
        SendMessageW(e, WM_SETFONT, (WPARAM)uiFont_, TRUE);
        SendMessageW(e, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(D(7), D(7)));
        return e;
    };

    ePiece_ = mk(ID_PIECE, ES_AUTOHSCROLL);
    eVenue_ = mk(ID_VENUE, ES_AUTOHSCROLL);
    eConductor_ = mk(ID_CONDUCTOR, ES_AUTOHSCROLL);
    eSysEng_ = mk(ID_SYSENG, ES_AUTOHSCROLL);
    eRecEng_ = mk(ID_RECENG, ES_AUTOHSCROLL);
    eAsstEng_ = mk(ID_ASSTENG, ES_AUTOHSCROLL);
    eNotes_ = mk(ID_NOTES, ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN);
    eSelName_ = mk(ID_SELNAME, ES_AUTOHSCROLL);
    ePianoLen_ = mk(ID_PIANOLEN, ES_AUTOHSCROLL);
    eWidth_ = mk(ID_WIDTH, ES_AUTOHSCROLL);
    eDepth_ = mk(ID_DEPTH, ES_AUTOHSCROLL);
    eOff_ = mk(ID_OFF, ES_AUTOHSCROLL);
    eCeiling_ = mk(ID_CEILING, ES_AUTOHSCROLL);
    eFlyH_ = mk(ID_FLYH, ES_AUTOHSCROLL);
    eSearch_ = mk(ID_SEARCH, ES_AUTOHSCROLL);
    eX_ = mk(ID_X, ES_AUTOHSCROLL);
    eY_ = mk(ID_Y, ES_AUTOHSCROLL);
    eAngle_ = mk(ID_ANGLE, ES_AUTOHSCROLL);
    eMicH_ = mk(ID_MICHEIGHT, ES_AUTOHSCROLL);
    eSpace_ = mk(ID_MAINSPACE, ES_AUTOHSCROLL);
    eCenter_ = mk(ID_MAINCENTER, ES_AUTOHSCROLL);
    eModelL_ = mk(ID_MODELL, ES_AUTOHSCROLL);
    eModelR_ = mk(ID_MODELR, ES_AUTOHSCROLL);
    eModelC_ = mk(ID_MODELC, ES_AUTOHSCROLL);
    eMicModel_ = mk(ID_MICMODEL, ES_AUTOHSCROLL);
    eDevName_ = mk(ID_DEVNAME, ES_AUTOHSCROLL);
    eDevCh_ = mk(ID_DEVCH, ES_AUTOHSCROLL | ES_NUMBER);
    eDevX_ = mk(ID_DEVX, ES_AUTOHSCROLL);
    eDevY_ = mk(ID_DEVY, ES_AUTOHSCROLL);

    SendMessageW(eSearch_, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(D(26), D(7)));
    SendMessageW(eNotes_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELPARAM(D(7), D(7)));
}

void App::UpdateCueBanners() {
    if (eSearch_)
        SendMessageW(eSearch_, EM_SETCUEBANNER, TRUE,
                     (LPARAM)L10n::T(S::SearchHint));
}

void App::SyncEdits() {
    if (!ePiece_) return;
    // NEVER rewrite the inspector while the user is typing in one of its fields:
    // a rewrite would reset the caret and replace a half-typed number.  The full
    // re-sync happens on commit (Enter / focus loss), where commitEdit_ is true.
    if (!commitEdit_ && !syncing_) {
        HWND f = GetFocus();
        if (f && f != hwnd_ && IsChild(hwnd_, f)) return;
    }
    syncing_ = true;
    SetWindowTextW(ePiece_, pr_.piece.c_str());
    SetWindowTextW(eVenue_, pr_.venue.c_str());
    SetWindowTextW(eConductor_, pr_.conductor.c_str());
    SetWindowTextW(eSysEng_, pr_.sysEngineer.c_str());
    SetWindowTextW(eRecEng_, pr_.recEngineer.c_str());
    SetWindowTextW(eAsstEng_, pr_.asstEngineer.c_str());
    SetWindowTextW(eNotes_, pr_.notes.c_str());
    SetWindowTextW(eWidth_, dr::Fmt(L"%.1f", pr_.stage.w).c_str());
    SetWindowTextW(eDepth_, dr::Fmt(L"%.1f", pr_.stage.d).c_str());
    SetWindowTextW(eOff_, dr::Fmt(L"%.1f", pr_.stage.off).c_str());
    SetWindowTextW(eCeiling_, dr::Fmt(L"%.1f", pr_.ceiling).c_str());
    const Item* it = pr_.Selected();
    SetWindowTextW(eSelName_, it ? it->label.c_str() : L"");
    SetWindowTextW(eX_, it ? dr::Fmt(L"%.2f", it->x).c_str() : L"");
    SetWindowTextW(eY_, it ? dr::Fmt(L"%.2f", it->y).c_str() : L"");
    SetWindowTextW(eAngle_, it ? dr::Fmt(L"%.0f", NormAngle(it->angle)).c_str() : L"");
    SetWindowTextW(eMicModel_, (it && it->kind == ItemKind::Mic &&
                                it->defId != L"mainmic" && !IsIntensityTech(it->defId))
                        ? it->model.c_str() : L"");
    SetWindowTextW(eMicH_, (it && it->kind == ItemKind::Mic)
                        ? dr::Fmt(L"%.2f", it->height).c_str() : L"");
    SetWindowTextW(eFlyH_, (it && it->kind == ItemKind::Mic)
                        ? dr::Fmt(L"%.2f", it->flyHeight).c_str() : L"");
    SetWindowTextW(eSpace_, (it && it->defId == L"mainmic")
                        ? (it->mainTech == 2 ? L"0.00"
                                             : dr::Fmt(L"%.2f", it->spacing).c_str())
                        : L"");
    SetWindowTextW(eCenter_, (it && it->defId == L"mainmic")
                        ? dr::Fmt(L"%.2f", it->center).c_str() : L"");
    SetWindowTextW(eModelL_, (it && (it->defId == L"mainmic" || IsIntensityTech(it->defId)))
                        ? it->modelL.c_str() : L"");
    SetWindowTextW(eModelR_, (it && (it->defId == L"mainmic" || IsIntensityTech(it->defId)))
                        ? it->modelR.c_str() : L"");
    SetWindowTextW(eModelC_, (it && it->defId == L"mainmic") ? it->modelC.c_str() : L"");
    SetWindowTextW(ePianoLen_,
                   (it && it->defId == L"grand_piano") ? dr::Fmt(L"%.2f", it->w >= it->h ? it->w : it->h).c_str() : L"");
    // layer 03 device properties: 名称 / 输入通道数 / X / Y only
    {
        const Device* dv = pr_.SelectedDevice();
        SetWindowTextW(eDevName_, dv ? dv->label.c_str() : L"");
        SetWindowTextW(eDevCh_, dv ? dr::Fmt(L"%d", dv->channels).c_str() : L"");
        SetWindowTextW(eDevX_, dv ? dr::Fmt(L"%.2f", dv->x).c_str() : L"");
        SetWindowTextW(eDevY_, dv ? dr::Fmt(L"%.2f", dv->y).c_str() : L"");
    }
    syncing_ = false;
}

void App::SyncXYEdits() {
    const Item* it = pr_.Selected();
    syncing_ = true;
    SetWindowTextW(eX_, it ? dr::Fmt(L"%.2f", it->x).c_str() : L"");
    SetWindowTextW(eY_, it ? dr::Fmt(L"%.2f", it->y).c_str() : L"");
    SetWindowTextW(eAngle_, it ? dr::Fmt(L"%.0f", NormAngle(it->angle)).c_str() : L"");
    SetWindowTextW(eMicModel_, (it && it->kind == ItemKind::Mic &&
                                it->defId != L"mainmic" && !IsIntensityTech(it->defId))
                        ? it->model.c_str() : L"");
    SetWindowTextW(eMicH_, (it && it->kind == ItemKind::Mic)
                        ? dr::Fmt(L"%.2f", it->height).c_str() : L"");
    SetWindowTextW(eFlyH_, (it && it->kind == ItemKind::Mic)
                        ? dr::Fmt(L"%.2f", it->flyHeight).c_str() : L"");
    syncing_ = false;
}

void App::SyncAngleEdit() {
    const Item* it = pr_.Selected();
    syncing_ = true;
    SetWindowTextW(eAngle_, it ? dr::Fmt(L"%.0f", NormAngle(it->angle)).c_str() : L"");
    syncing_ = false;
}

std::wstring App::ControlText(HWND h) {
    if (!h) return L"";
    int n = GetWindowTextLengthW(h);
    if (n <= 0) return L"";
    std::wstring s((size_t)n + 1, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    s.resize((size_t)n);
    return s;
}

void App::InvalidateAll() {
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// Single refresh path for every project / selection mutation.  It drops the
// transient caches that could hold stale rows, rects or pending endpoints, then
// rewrites the editor text, re-runs the layout and repaints — so switching pages
// can never show data older than the Project itself.
void App::RefreshAfterProjectChange() {
    L_.chPickRow.clear();

    if (ui_.pendingKind != 0) {
        const CableEnd e =
            ResolveCableEnd(pr_, ui_.pendingKind == 2 ? 1 : 0, ui_.pendingId);
        if (!e.ok) {
            ui_.pendingKind = 0;
            ui_.pendingId = 0;
        }
    }
    // every cable follows its endpoints live — no auto-layout snapshot, no lock
    RebuildAllCablePaths(pr_, MicRadiusM());
    SyncEdits();
    Layout();
    InvalidateAll();
}

// ---------------------------------------------------------------------------
// undo / redo: a snapshot is the project serialised by the .smproj writer, so a
// restore is exactly the state a saved file would load back.  The loaded or
// scripted project is the BASELINE (ResetUndoHistory), which is why an undo with
// an empty stack cannot walk past it.
// ---------------------------------------------------------------------------
void App::ResetUndoHistory() {
    undoStack_.clear();
    redoStack_.clear();
    restoring_ = false;
    gestureUndoPushed_ = false;
}

void App::PushUndoSnapshot() {
    if (restoring_) return;   // a restore must never record itself
    // this is the app's one "a mutation is about to happen" hook, so it marks the
    // project dirty BEFORE the no-op check below: after a save the top of the undo
    // stack can still be the pre-save state, and a real change must never be
    // swallowed there
    dirty_ = true;
    std::wstring snap = SerializeProject(pr_);
    // an action that changed nothing must not fill the stack with no-op entries
    if (!undoStack_.empty() && undoStack_.back() == snap) return;
    undoStack_.push_back(std::move(snap));
    if (undoStack_.size() > kUndoDepth) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();   // a new action invalidates the redo branch
}

void App::GestureUndo() {
    if (gestureUndoPushed_) return;
    gestureUndoPushed_ = true;
    PushUndoSnapshot();
}

void App::Undo() {
    if (RefuseLocked()) return;
    if (restoring_) return;
    if (undoStack_.empty()) {
        StatusMsg(L10n::T(S::NoUndo));
        return;
    }
    redoStack_.push_back(SerializeProject(pr_));
    if (redoStack_.size() > kUndoDepth) redoStack_.erase(redoStack_.begin());
    const std::wstring snap = undoStack_.back();
    undoStack_.pop_back();
    RestoreSnapshot(snap);
    StatusMsg(L10n::T(S::Undone));
}

void App::Redo() {
    if (RefuseLocked()) return;
    if (restoring_) return;
    if (redoStack_.empty()) {
        StatusMsg(L10n::T(S::NoRedo));
        return;
    }
    undoStack_.push_back(SerializeProject(pr_));
    if (undoStack_.size() > kUndoDepth) undoStack_.erase(undoStack_.begin());
    const std::wstring snap = redoStack_.back();
    redoStack_.pop_back();
    RestoreSnapshot(snap);
    StatusMsg(L10n::T(S::Redone));
}

void App::RestoreSnapshot(const std::wstring& text) {
    Project restored;
    std::wstring err;
    if (!ParseProject(text, restored, err)) {
        StatusMsg(L10n::T(S::UndoFailed), true);
        return;
    }
    const std::vector<int> oldSel = pr_.selIds;
    const int oldSelected = pr_.selected;
    restoring_ = true;
    // an undo / redo puts the project in a state the file on disk does not have
    dirty_ = true;
    pr_ = restored;
    // the selection is kept BY ID when the restored project still has the object
    pr_.selIds.clear();
    for (int id : oldSel) {
        if (const Item* it = pr_.Find(id)) pr_.selIds.push_back(it->id);
    }
    pr_.selected = (oldSelected >= 0 && pr_.Find(oldSelected)) ? oldSelected : -1;
    if (pr_.selected < 0 && !pr_.selIds.empty()) pr_.selected = pr_.selIds.back();
    // every transient / open UI state dies with the project it referred to
    ui_.pendingKind = 0;
    ui_.pendingId = 0;
    ui_.confirmLink = -1;
    ui_.bindConfirmOpen = false;
    ui_.bindHover = 0;
    ui_.bindIds.clear();
    ui_.bindConfirmOpen = false;
    ui_.bindHover = 0;
    ui_.bindIds.clear();
    ui_.confirmHover = 0;
    ui_.presetOpen = false;
    ui_.presetHover = -1;
    ui_.popupIndex = -1;
    ui_.popupCat = -1;
    ui_.spotListOpen = -1;
    ui_.spotScroll = 0;
    ui_.spotHover = -1;
    ui_.chSpotOpen = -1;
    ui_.chSpotScroll = 0;
    ui_.chSpotHover = -1;
    ui_.chDragRow = -1;
    ui_.chDragActive = false;
    ui_.chDropRow = -1;
    ui_.langOpen = false;
    ui_.langHoverRow = -1;
    ui_.fileOpen = false;
    ui_.fileHoverRow = -1;
    ui_.dragCat_ = -1;
    ui_.dragTab_ = -1;
    ui_.paletteDragging_ = false;
    ui_.dragStart_ = POINT{};
    ui_.dragPt_ = POINT{};
    ui_.hoverCbRow = -1;
    ui_.hoverCbDevice = -1;
    ui_.hoverCable = -1;
    ui_.hoverCbRoute = -1;
    ui_.hoverCbRouteDel = -1;
    ui_.hoverCbConn = -1;
    ui_.focusEdit = 0;
    drag_ = false;
    dragId_ = -1;
    dragMoved_ = false;
    rotating_ = false;
    rotId_ = -1;
    rotBase_ = 0;
    rotApplied_ = 0;
    devDrag_ = false;
    devDragId_ = -1;
    devDragMoved_ = false;
    band_ = false;
    bandMoved_ = false;
    bandAdd_ = false;
    gestureUndoPushed_ = false;
    CloseChannelCell();       // the channel-cell editor must close
    restoring_ = false;
    lastMicRadiusM_ = -1.0;   // cable paths must be re-derived at the current zoom
    RefreshAfterProjectChange();   // RebuildAllCablePaths + SyncEdits + Layout + repaint
    SyncEdits();
}

// ---------------------------------------------------------------------------
// The ONE place the stage size changes: the 音乐厅 / 教堂 / 沙龙 buttons and a
// manual 宽度 / 深度 edit both come through here.
//
// A resize must not slide the ensemble into a corner: the layout stays CENTRED
// on the new stage, so every item and every cabling device is translated by
// half the size delta.  Icons are never scaled (they are metres, like the
// stage), and the cables are rebuilt from the moved endpoints by
// RefreshAfterProjectChange().  A change under 1 mm is not a change.
// ---------------------------------------------------------------------------
void App::SetStageSize(double w, double d) {
    if (RefuseLocked()) return;
    w = std::clamp(w, 4.0, 60.0);
    d = std::clamp(d, 4.0, 60.0);
    const double dw = w - pr_.stage.w;
    const double dd = d - pr_.stage.d;
    if (std::fabs(dw) <= 0.001 && std::fabs(dd) <= 0.001) return;
    PushUndoSnapshot();   // a stage resize is one undoable action
    const double dx = dw * 0.5;   // keep the layout's centre at the stage centre
    const double dy = dd * 0.5;
    pr_.stage.w = w;
    pr_.stage.d = d;
    for (Item& it : pr_.items) {
        it.x += dx;
        it.y += dy;
    }
    for (Device& dv : pr_.devices) {
        dv.x += dx;
        dv.y += dy;
    }
    RefreshAfterProjectChange();
    StatusMsg(L10n::T(S::StageRecentred));
}

// ---------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------
// One placement path for every native inspector EDIT.  `r` is the FieldBox::edit
// rect the painter fills for the same field — never a second derivation of the y
// rhythm — so label, painted box and EDIT can never drift apart.
//
// A child window is clipped to the PARENT client area, not to an arbitrary rect,
// so a scrolled field would still paint over the top bar / status bar even though
// the GDI+ painter clipped it away.  The window region below makes the control's
// visible pixels identical to the painted (clipped) box: no leak outside `panel`,
// and no need to hide a field that is only half scrolled in.
void App::PlaceInspectorEdit(HWND h, const RECT& r, bool show, const RECT& panel) {
    if (!h) return;
    RECT vis{};
    IntersectRect(&vis, &r, &panel);
    // a 2 px sliver at the panel edge reads as a broken box: below ~8 px visible
    // the field is hidden instead of half-painted
    const int minVis = D(8);
    const bool sliver = vis.right - vis.left < minVis || vis.bottom - vis.top < minVis;
    if (!show || r.right <= r.left || r.bottom <= r.top || sliver) {
        ShowWindow(h, SW_HIDE);
        SetWindowRgn(h, nullptr, TRUE);   // a hidden control must not keep a clip
        return;
    }
    SetWindowPos(h, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (vis.left == r.left && vis.top == r.top && vis.right == r.right &&
        vis.bottom == r.bottom) {
        SetWindowRgn(h, nullptr, TRUE);   // fully inside the panel: no clip needed
        return;
    }
    HRGN rg = CreateRectRgn(vis.left - r.left, vis.top - r.top, vis.right - r.left,
                            vis.bottom - r.top);
    if (rg) {
        if (!SetWindowRgn(h, rg, TRUE)) DeleteObject(rg);   // SetWindowRgn took it
    }
}

// 吊装 (flown) on/off for the selected microphone — exactly the behaviour of the
// 01/02 checkbox, shared with the 05 inspector.
void App::ToggleFlown() {
    if (RefuseLocked()) return;
    Item* m = pr_.Selected();
    if (!m || m->kind != ItemKind::Mic) return;
    // a wireless (lavalier) microphone hangs on the performer, never on a ceiling
    if (m->defId == L"lavalier") return;
    PushUndoSnapshot();   // 吊装 changes the route shape AND the lengths
    const bool want = !m->flown;
    // a microphone group is flown as a whole
    const int g = pr_.GroupOf(m->id);
    if (g >= 0 && !pr_.micGroups[(size_t)g].ids.empty()) {
        for (int id : pr_.micGroups[(size_t)g].ids)
            if (Item* q = pr_.Find(id))
                if (q->kind == ItemKind::Mic) q->flown = want;
    } else {
        m->flown = want;
    }
    // re-route the cables of this mic: the ceiling model changes both the path
    // shape (dashed legs) and the length
    RebuildAllCablePaths(pr_, MicRadiusM());
    SyncEdits();
    Layout();
    InvalidateAll();
}

// The one vertical rhythm for every labelled field box.  The right inspector and
// the 项目信息 page both call this, so a label can never sit on top of a box and an
// EDIT can never be placed from a second, hand-added offset.
FieldBox App::FieldRect(int x, int w, int h, int y) const {
    auto S = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    FieldBox f;
    f.label = {x, y, x + w, y + S(18)};
    f.box = {x, y + S(22), x + w, y + S(22) + S(h)};
    int in = S(3);
    f.edit = {f.box.left + in, f.box.top + in, f.box.right - in, f.box.bottom - in};
    return f;
}

void App::Layout() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    cw_ = rc.right;
    ch_ = rc.bottom;
    if (cw_ <= 0 || ch_ <= 0) return;

    // the close prompt owns the window while it is open: every native EDIT (a real
    // HWND, so it would paint OVER the painted modal) is hidden and the layout
    // behind the prompt is frozen until it closes (取消 re-runs the full Layout)
    if (exitOpen_) {
        HideAllChildEdits();
        return;
    }

    float bodyLogical = (float)ch_ / dpi_ - 86.f;
    float sc = std::clamp(bodyLogical / 800.f, 0.80f, 1.0f);
    if (std::fabs(sc - uiScale_) > 0.002f) {
        uiScale_ = sc;
        RebuildFonts();
    }
    auto S = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };

    const int topH = S(64), statusH = S(30);
    const int leftW = S(300), rightW = S(352);

    L_.top = {0, 0, cw_, topH};
    L_.status = {0, ch_ - statusH, cw_, ch_};
    L_.left = {0, topH, leftW, ch_ - statusH};
    L_.right = {cw_ - rightW, topH, cw_, ch_ - statusH};
    L_.canvas = {leftW, topH, cw_ - rightW, ch_ - statusH};
    // 项目信息 (layer 5) is a full-width document page: there is no inspector, so
    // the canvas takes the whole right-hand side and the page panel stays centred
    // in it.
    if (ui_.layer == 5) {
        L_.right = {};
        L_.canvas = {leftW, topH, cw_, ch_ - statusH};
    }

    // --- top bar -----------------------------------------------------------
    L_.logo = {S(20), S(17), S(50), S(47)};
    L_.brand = {S(60), S(9), S(280), S(37)};
    L_.brandSub = {S(61), S(35), S(286), S(53)};
    LayoutTopBar();   // measured right-aligned chip cluster + tab strip

    // --- left panel ---------------------------------------------------------
    int lx = S(20), lr = leftW - S(18);
    L_.libTitle = {lx, topH + S(16), lr, topH + S(40)};
    L_.libCount = {lx, topH + S(20), lr, topH + S(40)};
    L_.libTabs[0] = {lx, topH + S(50), lx + (lr - lx) / 2, topH + S(86)};
    L_.libTabs[1] = {L_.libTabs[0].right, L_.libTabs[0].top, lr, L_.libTabs[0].bottom};
    L_.searchBox = {lx, topH + S(96), lr, topH + S(132)};
    L_.searchEdit = {lx + S(32), topH + S(102), lr - S(10), topH + S(126)};
    // layer 03 reserves a strip at the bottom of the library for its hint; the
    // palette itself stays usable on every layer
    const bool cablingLayer = (ui_.layer == 2);
    L_.libView = {lx, topH + S(148), lr + S(4),
                  ch_ - statusH - S(10) - (cablingLayer ? S(50) : 0)};
    L_.cbLeftHint = cablingLayer
                        ? RECT{lx, ch_ - statusH - S(52), lr, ch_ - statusH - S(12)}
                        : RECT{};

    // --- right inspector -----------------------------------------------------
    int rx = L_.right.left + S(22);
    int rw = rightW - S(44);
    int y = topH + S(18) - (int)ui_.rightScroll;

    // Every labelled field box comes from App::FieldRect(): one label line, one
    // real gap, then the box the EDIT is placed at.  The 项目信息 page uses the
    // very same helper, so the two panels cannot drift apart.
    auto field = [&](int x, int w, int h, int yy) { return FieldRect(x, w, h, yy); };
    auto section = [&](RECT& r) {
        r = {rx, y, rx + rw, y + S(16)};
        y += S(24);
    };

    // layer 03 (话筒拉线) owns the whole inspector: it replaces the selection /
    // layers sections below.
    if (ui_.layer == 2) {
        LayoutCabling();
        return;
    }

    // layer 5 (项目信息): 项目名称 / 场地 / 指挥 / 系统 / 录音 / 助理 / 备注,
    // 舞台尺寸 and 外观颜色 live in ONE panel centred on the canvas — they left
    // this panel, which is why the inspector below fits without scrolling.
    if (ui_.layer == 5) {
        LayoutProjectPage();
        return;
    }

    // the 05-only rects must not survive a page switch: they double as hit-test
    // areas (CablingInspectorDown) and as the painter's source on layer 05 only
    L_.cbMiTitle = L_.cbMiInfo = L_.cbRouteSec = L_.cbRouteEmpty = {};
    L_.cbRouteRow.clear();
    L_.cbRouteDel.clear();
    L_.cbRouteLink.clear();
    L_.cbDel = L_.cbList = L_.cbTotals = L_.cbKind = L_.cbCap = L_.cbInfo = {};
    for (int i = 0; i < kAddDeviceCount; ++i) L_.cbAdd[i] = {};
    L_.cbRow.clear();
    L_.cbSec = {};
    L_.cbCeilField = {};

    // 项目信息 / 舞台尺寸 / 外观颜色 moved to the 项目信息 page: their rects stay
    // EMPTY on the stage pages, so neither the painter nor an EDIT can leak here.
    L_.secProject = L_.secStage = L_.secAppearance = {};
    L_.p1Panel = L_.p1Title = L_.p1Rule = {};
    L_.piece = L_.venue = L_.conductor = {};
    L_.sysEng = L_.recEng = L_.asstEng = L_.notes = {};
    L_.wField = L_.dField = L_.ceilingField = {};
    for (int i = 0; i < 3; ++i) L_.presets[i] = {};
    for (int i = 0; i < 4; ++i)
        L_.colorRow[i] = L_.colorSwatch[i] = L_.colorHex[i] = {};
    for (int i = 0; i < 4; ++i) L_.divider[i] = {};

    section(L_.secSel);
    L_.selName = field(rx, rw, 30, y);
    y = L_.selName.box.bottom + S(3);
    int cw3 = (rw - S(16)) / 3;
    L_.xField = field(rx, cw3, 30, y);
    L_.yField = field(rx + cw3 + S(8), cw3, 30, y);
    L_.angleField = field(rx + 2 * (cw3 + S(8)), rw - 2 * (cw3 + S(8)), 30, y);
    y = L_.xField.box.bottom + S(3);
    L_.selInfo = {rx, y, rx + rw, y + S(18)};
    y += S(20);
    bool spotSel = SpotMicInSel() != nullptr;
    if (spotSel) {
        L_.spotBtn = {rx, y, rx + rw, y + S(30)};
        y += S(30) + S(4);
    } else {
        L_.spotBtn = {};
    }
    const Item* selIt = pr_.Selected();
    bool pianoSel = selIt && selIt->defId == L"grand_piano";
    bool micSel = selIt && selIt->kind == ItemKind::Mic;
    // 颜色: the per-item tint override.  Instruments AND the 声部 / section targets
    // (both are ItemKind::Instrument) own a colour; a microphone never does — it
    // wears the ink of the instrument it is bound to.
    if (selIt && selIt->kind == ItemKind::Instrument) {
        L_.itemColorRow = {rx, y, rx + rw, y + S(30)};
        L_.itemColorLabel = {rx, y, rx + S(34), y + S(30)};
        L_.itemColorSwatch = {rx + S(38), y + S(6), rx + S(62), y + S(24)};
        L_.itemColorHex = {rx + S(68), y, rx + S(154), y + S(30)};
        L_.itemColorReset = selIt->colorOverride
                                ? RECT{rx + rw - S(76), y + S(3), rx + rw, y + S(27)}
                                : RECT{};
        y += S(30) + S(4);
    } else {
        L_.itemColorRow = L_.itemColorLabel = L_.itemColorSwatch = {};
        L_.itemColorHex = L_.itemColorReset = {};
    }
    if (pianoSel) {
        L_.lenField = field(rx, rw, 28, y);
        y = L_.lenField.box.bottom + S(4);
    } else {
        L_.lenField = {};
    }
    bool mainSel = selIt && selIt->defId == L"mainmic";
    // ONE stage object for an intensity-difference technique (XY / MS / Blumlein):
    // no 间距 row (its spacing is fixed by the technique), 型号 L / R instead
    const bool techObj = micSel && selIt && IsIntensityTech(selIt->defId);
    if (micSel && !mainSel && !techObj) {
        // 型号 + 高度: two DIFFERENT FieldBoxes, each from the shared rhythm, each
        // EDIT placed at its own box — the old overlap came from a second
        // derivation of the y position and is impossible now.
        L_.micModel = field(rx, rw, 28, y);
        y = L_.micModel.box.bottom + S(4);
    } else {
        L_.micModel = {};
    }
    // 高度 (m) is a property of EVERY microphone — the main microphone hangs from
    // the ceiling exactly like a spot mic, and a flown one's height IS the hang
    // height the cable model measures its 立管 from (Item::height), so the field
    // must be here for the main mic too.
    if (micSel) {
        L_.micHeight = field(rx, rw, 28, y);
        y = L_.micHeight.box.bottom + S(4);
    } else {
        L_.micHeight = {};
    }
    if (mainSel) {
        const Item* m = selIt;
        int tw = (rw - S(24)) / 4;
        for (int i = 0; i < 4; ++i) {
            int px = rx + i * (tw + S(8));
            L_.techBtn[i] = {px, y, px + tw, y + S(28)};
        }
        y += S(28) + S(4);
        int pw3 = (rw - S(16)) / 3;
        for (int i = 0; i < 3; ++i) {
            int px = rx + i * (pw3 + S(8));
            L_.techPreset[i] = (m->mainTech == 1)
                                   ? RECT{px, y, px + pw3, y + S(28)} : RECT{};
        }
        if (m->mainTech == 1) y += S(28) + S(4);
        int np = m->mainTech == 0 ? 1 : m->mainTech == 1 ? 2
                 : m->mainTech == 2 ? 3 : 0;
        int pw = (rw - S(18)) / 4;
        for (int i = 0; i < 4; ++i) {
            int px = rx + i * (pw + S(6));
            L_.patternBtn[i] = (i < np) ? RECT{px, y, px + pw, y + S(28)} : RECT{};
        }
        if (np > 0) y += S(28) + S(4);
        L_.spaceField = field(rx, rw, 28, y);
        y = L_.spaceField.box.bottom + S(4);
        if (m->mainTech == 3) {
            L_.centerField = field(rx, rw, 28, y);
            y = L_.centerField.box.bottom + S(4);
        } else {
            L_.centerField = {};
        }
        int nModel = m->mainTech == 3 ? 3 : 2;
        int mw = nModel == 3 ? (rw - S(16)) / 3 : (rw - S(8)) / 2;
        L_.modelL = field(rx, mw, 28, y);
        L_.modelR = field(rx + rw - mw, mw, 28, y);
        L_.modelC = (nModel == 3) ? field(rx + mw + S(8), mw, 28, y) : FieldBox{};
        y = L_.modelL.box.bottom + S(4);
    } else {
        for (int i = 0; i < 4; ++i) { L_.techBtn[i] = {}; L_.patternBtn[i] = {}; }
        for (int i = 0; i < 3; ++i) L_.techPreset[i] = {};
        L_.spaceField = {};   // no 间距 row for a technique object
        L_.centerField = {};
        L_.modelL = L_.modelR = L_.modelC = {};
        if (techObj) {
            const int mw = (rw - S(8)) / 2;
            L_.modelL = field(rx, mw, 28, y);
            L_.modelR = field(rx + rw - mw, mw, 28, y);
            y = L_.modelL.box.bottom + S(4);
        }
    }
    // 吊装 (flown) microphone: only a checkbox — the cable length comes from the
    // indoor height (层高) and the microphone's own height above the floor
    if (micSel && selIt && selIt->defId != L"lavalier") {
        L_.flownRow = {rx, y, rx + rw, y + S(30)};
        L_.flownCheck = {rx, y + S(6), rx + S(18), y + S(24)};
        y += S(30) + S(2);
    } else {
        L_.flownRow = L_.flownCheck = {};
    }
    L_.flyField = {};
    int third = (rw - S(16)) / 3;
    L_.delBtn = {rx, y, rx + third, y + S(32)};
    L_.bindBtn = {rx + third + S(8), y, rx + third * 2 + S(8), y + S(32)};
    L_.treeBtn = {rx + (third + S(8)) * 2, y, rx + rw, y + S(32)};
    y += S(32) + S(6);

    y += S(8);
    L_.divider[1] = {rx, y, rx + rw, y + 1};
    y += S(12);

    section(L_.secLayers);
    for (int i = 0; i < 2; ++i) {
        L_.layerRow[i] = {rx, y, rx + rw, y + S(30)};
        L_.layerCheck[i] = {rx, y + S(6), rx + S(18), y + S(24)};
        y += S(30);
    }
    // 显示文字 is TWO switches: 显示乐器文字 (instrument names + the 「型 ×N」 group
    // captions) and 显示话筒文字 (mic name / model / capsule L·R·C + cable tags).
    // Both live in the PROJECT, so a file remembers them.
    for (int ti = 0; ti < 2; ++ti) {
        L_.textRow[ti] = {rx, y, rx + rw, y + S(30)};
        L_.textCheck[ti] = {rx, y + S(6), rx + S(18), y + S(24)};
        y += S(30);
    }
    // ---- 同类乐器: a select-all button per instrument type with >= 3 instances --
    L_.sameTypeBtn.clear();
    L_.sameTypeDef.clear();
    if (ui_.layer != 2 && ui_.layer != 3 && ui_.layer != 5) {
        const std::vector<std::pair<std::wstring, int>> same = SameTypeGroups();
        if (!same.empty()) {
            y += S(8);
            L_.divider[3] = {rx, y, rx + rw, y + 1};
            y += S(12);
            L_.secSameType = {rx, y, rx + rw, y + S(18)};
            y += S(20);
            for (const auto& g : same) {
                L_.sameTypeBtn.push_back({rx, y, rx + rw, y + S(30)});
                L_.sameTypeDef.push_back(g.first);
                y += S(30) + S(2);
            }
            y += S(6);
        } else {
            L_.secSameType = {};
        }
    } else {
        L_.secSameType = {};
    }
    L_.rightContent = y + (int)ui_.rightScroll + S(16);
    int viewH = (L_.right.bottom - L_.right.top) - S(20);
    if (L_.rightContent - viewH < (int)ui_.rightScroll) {
        // the content shrank under the stored scroll (a selection change removes
        // rows): clamp FIRST and re-lay out, so the rects below are never built
        // from a scroll offset that no longer exists
        const int ns = (std::max)(0, L_.rightContent - viewH);
        if (ns != (int)ui_.rightScroll) {
            ui_.rightScroll = (double)ns;
            if (!relaying_) {
                relaying_ = true;
                Layout();
                relaying_ = false;
            }
            return;
        }
    }

    // --- child controls -------------------------------------------------------
    // A native EDIT is a window and ignores the paint clip, so it is positioned by
    // PlaceInspectorEdit() from exactly the rects painted above (FieldBox::edit)
    // and clipped to the panel — otherwise a scrolled field floats over the top
    // bar / canvas / status bar and the panel looks scrambled.
    // while the lock popover / passcode modal is open it owns the window: no
    // child EDIT may poke through it (they are real HWNDs and would paint on top)
    const bool lockOverlay = (lockOpen || lockModal != 0);
    auto place = [&](HWND h, const RECT& r, bool show) {
        PlaceInspectorEdit(h, r, show && !lockOverlay, L_.right);
    };
    // 项目信息 / 舞台尺寸 / 外观颜色 belong to the 项目信息 page now: on a stage
    // page their rects are empty and the controls are hidden.  They are placed
    // against that page's panel (never this one), so a latched EDIT cannot wander
    // over the inspector.
    place(ePiece_, L_.piece.edit, false);
    place(eVenue_, L_.venue.edit, false);
    place(eConductor_, L_.conductor.edit, false);
    place(eSysEng_, L_.sysEng.edit, false);
    place(eRecEng_, L_.recEng.edit, false);
    place(eAsstEng_, L_.asstEng.edit, false);
    place(eNotes_, L_.notes.edit, false);
    place(eWidth_, L_.wField.edit, false);
    place(eDepth_, L_.dField.edit, false);
    place(eOff_, {}, false);
    place(eCeiling_, L_.ceilingField.edit, false);
    place(eFlyH_, {}, false);   // no separate flown-height field any more
    // the layer-03 device fields must never leak onto the other layers
    place(eDevName_, {}, false);
    place(eDevCh_, {}, false);
    place(eDevX_, {}, false);
    place(eDevY_, {}, false);
    // the search box lives in the left panel, clipped to it
    PlaceInspectorEdit(eSearch_, L_.searchEdit, true, L_.left);
    bool hasSel = pr_.Selected() != nullptr;
    bool pianoSelP = pr_.Selected() && pr_.Selected()->defId == L"grand_piano";
    place(eSelName_, L_.selName.edit, hasSel);
    place(eX_, L_.xField.edit, hasSel);
    place(eY_, L_.yField.edit, hasSel);
    place(eAngle_, L_.angleField.edit, hasSel);
    place(ePianoLen_, L_.lenField.edit, hasSel && pianoSelP);
    place(eMicH_, L_.micHeight.edit, hasSel && pr_.Selected() && pr_.Selected()->kind == ItemKind::Mic);
    {
        const Item* mi = pr_.Selected();
        bool showModel = hasSel && mi && mi->kind == ItemKind::Mic &&
                         mi->defId != L"mainmic" && !IsIntensityTech(mi->defId);
        place(eMicModel_, L_.micModel.edit, showModel);
    }
    place(eSpace_, L_.spaceField.edit, hasSel && pr_.Selected() && pr_.Selected()->defId == L"mainmic" && pr_.Selected()->mainTech != 2);
    place(eCenter_, L_.centerField.edit, hasSel && pr_.Selected() && pr_.Selected()->defId == L"mainmic" && pr_.Selected()->mainTech == 3);
    bool modelSel = hasSel && pr_.Selected() &&
                    (pr_.Selected()->defId == L"mainmic" ||
                     IsIntensityTech(pr_.Selected()->defId));
    place(eModelL_, L_.modelL.edit, modelSel);
    place(eModelR_, L_.modelR.edit, modelSel);
    place(eModelC_, L_.modelC.edit,
          modelSel && pr_.Selected()->defId == L"mainmic" &&
              pr_.Selected()->mainTech == 3);

    UpdateStageXf();
}

void App::UpdateStageXf() {
    const RECT& c = L_.canvas;
    double padX = 46.0 * dpi_;
    double padTop = 54.0 * dpi_;
    double padBottom = 64.0 * dpi_;
    double avW = std::max(50.0, (double)(c.right - c.left) - 2 * padX);
    double avH = std::max(50.0, (double)(c.bottom - c.top) - padTop - padBottom);

    // the off-stage area is unbounded: the fit is based on the stage alone,
    // except on the cabling layer where off-stage devices must stay in view
    double sw = std::max(1.0, pr_.stage.w);
    double sd = std::max(1.0, pr_.stage.d);
    double bx0 = 0.0, by0 = 0.0, bx1 = sw, by1 = sd;
    if (ui_.layer == 2) {
        for (const auto& dv : pr_.devices) {
            bx0 = std::min(bx0, dv.x - 0.70);
            bx1 = std::max(bx1, dv.x + 0.70);
            by0 = std::min(by0, dv.y - 0.50);
            by1 = std::max(by1, dv.y + 0.50);
        }
    }
    const double wm = std::max(1.0, bx1 - bx0);
    const double hm = std::max(1.0, by1 - by0);
    double base = std::min(avW / wm, avH / hm) * 0.88;
    ppm_ = std::max(1.5, base * ui_.zoom);

    fieldW_ = wm * ppm_;
    fieldH_ = hm * ppm_;

    double cx = c.left + (c.right - c.left) * 0.5;
    double cy = c.top + padTop + (avH - fieldH_) * 0.5;
    ox_ = cx - fieldW_ * 0.5 + ui_.panX - bx0 * ppm_;
    oy_ = cy + ui_.panY - by0 * ppm_;
    fieldL_ = ox_;
    fieldT_ = oy_;

    // The drawn microphone marker has a fixed pixel radius, so its size in metres
    // changes with the zoom — and the cable terminal sits exactly on that edge.
    // Keep every cable path in sync with the current scale.
    const double micM = MicRadiusM();
    if (std::fabs(micM - lastMicRadiusM_) > 1e-6) {
        lastMicRadiusM_ = micM;
        RebuildAllCablePaths(pr_, micM);
    }
}
// ---------------------------------------------------------------------------
// painting
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Freeze protection / progress: a PNG or PDF export blocks the UI thread (the
// window cannot repaint while it runs), so before the heavy call the app pins a
// WAIT cursor and writes a status line; EndBusy() restores both even on failure.
// ---------------------------------------------------------------------------
namespace {
double NowMs() {
    static LARGE_INTEGER f = []() {
        LARGE_INTEGER q{};
        QueryPerformanceFrequency(&q);
        return q;
    }();
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return (double)(c.QuadPart) * 1000.0 / (double)(f.QuadPart > 0 ? f.QuadPart : 1);
}
} // namespace

void App::BeginBusy(const std::wstring& msg) {
    busy_ = true;
    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    StatusMsg(msg);
    // let the status line reach the screen before the thread blocks
    UpdateWindow(hwnd_);
}

void App::EndBusy() {
    if (!busy_) return;
    busy_ = false;
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    InvalidateAll();
}

void App::TimingLog(const std::wstring& line) {
    // SM_TIMING=<file>: UTF-16LE, one record per line — the measurement hook the
    // acceptance runs read (paint / png / pdf timings)
    wchar_t path[MAX_PATH] = L"";
    if (GetEnvironmentVariableW(L"SM_TIMING", path, MAX_PATH) == 0) return;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"ab") == 0 && f) {
        const std::wstring s = line + L"\r\n";
        fwrite(s.c_str(), sizeof(wchar_t), s.size(), f);
        fclose(f);
    }
}

void App::Paint(HDC target) {
    if (cw_ <= 0 || ch_ <= 0) return;
    const double t0 = NowMs();
    HDC mem = CreateCompatibleDC(target);
    HBITMAP bmp = CreateCompatibleBitmap(target, cw_, ch_);
    HGDIOBJ old = SelectObject(mem, bmp);

    {
        Gdiplus::Graphics g(mem);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        dr::Fill(g, {0, 0, cw_, ch_}, dr::C(theme::kBg));
        PaintTopBar(g);
        PaintLeft(g);
        // hard clip: nothing the stage paints (grid included) may escape the
        // canvas rect into the left/right panels
        {
            Gdiplus::GraphicsState gsStage = g.Save();
            Gdiplus::Region canvasClip(Gdiplus::Rect(
                L_.canvas.left, L_.canvas.top, L_.canvas.right - L_.canvas.left,
                L_.canvas.bottom - L_.canvas.top));
            g.SetClip(&canvasClip);
            PaintStage(g);
            g.Restore(gsStage);
        }
        // the right inspector scrolls: clip it so scrolled content can never
        // paint over the top bar / canvas / status bar
        {
            Gdiplus::Region clip(Gdiplus::Rect(L_.right.left, L_.right.top,
                                               L_.right.right - L_.right.left,
                                               L_.right.bottom - L_.right.top));
            clip.Intersect(Gdiplus::Rect(0, 0, cw_, ch_));
            g.SetClip(&clip);
            PaintRight(g);
            Gdiplus::Region all(Gdiplus::Rect(0, 0, cw_, ch_));
            g.SetClip(&all);
        }
        PaintStatus(g);
        PaintOverlay(g);
    }

    BitBlt(target, 0, 0, cw_, ch_, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    // paint timing: the status bar only shows it when it is slow (> 60 ms), the
    // SM_TIMING log always records it
    lastPaintMs_ = NowMs() - t0;
    if (lastPaintMs_ >= 60.0)
        TimingLog(dr::Fmt(L"paint %.1f layer %d", lastPaintMs_, ui_.layer));
}

void App::PaintOverlay(Gdiplus::Graphics& g) {
    // the unsaved-changes prompt on close is the TOPMOST thing: while it is open
    // it is the only overlay painted, and its scrim dims the whole page behind it
    if (exitOpen_) {
        PaintExitPrompt(g);
        return;
    }
    // the rubber band sits ABOVE the stage objects (it is a live gesture) and
    // UNDER every modal / popup painted below
    PaintRubberBand(g);
    // the delete-route modal: painted last, and while it is open it is the only
    // thing painted / clickable (the scrim hides the page behind it)
    PaintCableConfirm(g);
    if (ui_.confirmLink >= 0) return;
    // big-group binding confirmation: also the only clickable thing while open
    PaintBindConfirm(g);
    if (ui_.bindConfirmOpen) return;
    // the 预设 modal: same contract — while it is open it is the only thing
    // painted and the only thing clickable
    PaintPresetModal(g);
    if (ui_.presetOpen) return;
    // The APP LOCK is modal-ish and painted LAST: while its popover (or passcode
    // modal) is open nothing else paints over it — no 文件 / language menu, no
    // picker, no inspector child control (see Layout, which hides the EDITs).
    if (lockOpen || lockModal != 0) {
        PaintLockPopover(g);
        return;
    }
    if (ui_.spotListOpen >= 0) {
        const std::vector<SpotRow> rows = SpotTopRows(ui_.spotListOpen);
        int rowH = D(22);
        int w = D(212);
        int x = L_.right.left - w - D(10);
        int y = L_.spotBtn.top;
        int avail = (ch_ - D(30)) - y - D(8);
        int maxRows = (std::max)(1, (std::min)(16, avail / rowH));
        int total = (int)rows.size();
        if (ui_.spotScroll > (std::max)(0, total - maxRows))
            ui_.spotScroll = (std::max)(0, total - maxRows);
        int n = (std::min)(total - ui_.spotScroll, maxRows);
        n = (std::min)((std::max)(n, 0), 16);
        L_.spotSubRow.clear();
        L_.spotSubPanel = {};
        if (n > 0) {
            RECT box = {x, y, x + w, y + n * rowH + D(6)};
            L_.spotList = box;
            dr::FillRound(g, dr::RF(box), (float)D(6), dr::C(0xFFFFFF));
            dr::StrokeRound(g, dr::RF(box), (float)D(6), dr::C(theme::kBorderStrong), 1.f);
            for (int i = 0; i < n; ++i) {
                RECT r = {x + D(4), y + D(3) + i * rowH, x + w - D(4),
                          y + D(3) + (i + 1) * rowH};
                L_.spotListRow[i] = r;
                const int idx = ui_.spotScroll + i;
                const SpotRow& srow = rows[(size_t)idx];
                bool hover = ui_.spotHover == i;
                if (hover) dr::FillRound(g, dr::RF(r), (float)D(4), dr::C(theme::kHover));
                const bool dim = (srow.kind == 0);
                RECT tr = {r.left + D(10), r.top, r.right - D(58), r.bottom};
                dr::Text(g, srow.label, Font(9.f),
                         dr::C(dim ? theme::kTextMuted : theme::kText), tr, 0, true);
                if (!srow.right.empty())
                    dr::Text(g, srow.right, Font(8.f), dr::C(theme::kTextFaint),
                             {r.right - D(56), r.top, r.right - D(8), r.bottom}, 2, true);
            }
            memset(L_.spotListRow + n, 0, sizeof(RECT) * (16 - n));
            // second level, on the side with room (the inspector is on the right,
            // so the submenu opens to the LEFT of the list)
            if (ui_.spotGroup >= 0 && ui_.spotGroup < SpotGroupCount()) {
                const std::vector<SpotRow> sub = SpotSubRows(ui_.spotGroup);
                if (!sub.empty()) {
                    const int sw = D(188);
                    const int sh = (int)sub.size() * rowH + D(6);
                    int sx = x - D(6) - sw;
                    if (sx < L_.canvas.left + D(8)) sx = box.right + D(6);
                    int sy = y;
                    if (sy + sh > ch_ - D(30)) sy = ch_ - D(30) - sh;
                    if (sy < D(8)) sy = D(8);
                    L_.spotSubPanel = {sx, sy, sx + sw, sy + sh};
                    dr::FillRound(g, dr::RF(L_.spotSubPanel), (float)D(6),
                                  dr::C(0xFFFFFF));
                    dr::StrokeRound(g, dr::RF(L_.spotSubPanel), (float)D(6),
                                    dr::C(theme::kBorderStrong), 1.f);
                    L_.spotSubRow.assign(sub.size(), RECT{});
                    for (size_t k = 0; k < sub.size(); ++k) {
                        RECT r = {sx + D(4), sy + D(3) + (int)k * rowH, sx + sw - D(4),
                                  sy + D(3) + (int)(k + 1) * rowH};
                        L_.spotSubRow[k] = r;
                        if (ui_.hoverSpotSub == (int)k)
                            dr::FillRound(g, dr::RF(r), (float)D(4), dr::C(theme::kHover));
                        dr::Text(g, sub[k].label, Font(9.f), dr::C(theme::kText),
                                 {r.left + D(10), r.top, r.right - D(8), r.bottom}, 0,
                                 true);
                    }
                }
            }
        } else {
            L_.spotList = {};
            for (int i = 0; i < 16; ++i) L_.spotListRow[i] = {};
        }
    } else {
        L_.spotList = {};
        for (int i = 0; i < 16; ++i) L_.spotListRow[i] = {};
        L_.spotSubRow.clear();
        L_.spotSubPanel = {};
    }
    // 03 channel table: the floating 对应乐器 picker.  It owns L_.chPickRow and,
    // while it is open, the shared second-level L_.spotSubRow / L_.spotSubPanel
    // (the two pickers are never open together, see MouseDown)
    if (ui_.chSpotOpen >= 0) PaintChannelPicker(g);
    // 04 链路 / 路由 popup: painted AFTER the table and its picker, i.e. last
    if (ui_.chRouteOpen >= 0) {
        PaintChannelRoute(g);
    } else {
        // closed: drop the hit rects so no stale row can answer a press
        L_.chRoutePanel = {};
        L_.chRouteRow.clear();
        L_.chRouteDev.clear();
    }

    // palette popup: the plain click on a tile offers 「添加乐器 / 添加话筒」 right
    // underneath it (a drag instead drops the item on the stage)
    if (ui_.popupIndex >= 0) {
        const auto& list = ui_.libTab == 0 ? Instruments() : Mics();
        std::wstring label = L10n::T(ui_.libTab == 0 ? S::AddInstrument : S::AddMic);
        if (ui_.popupIndex >= 0 && ui_.popupIndex < (int)list.size())
            label = std::wstring(L"＋ ") + ItemName(list[(size_t)ui_.popupIndex]) +
                    L"  ·  " + label;
        RECT box = L_.popup;
        if (box.right > box.left) {
            dr::FillRound(g, dr::RF(box), (float)D(6), dr::C(0xFFFFFF));
            dr::StrokeRound(g, dr::RF(box), (float)D(6),
                            dr::C(theme::kBorderStrong), 1.f);
            dr::Text(g, label, Font(9.f), dr::C(theme::kText), box, 1, true);
        }
    }

    // ---- 文件 dropdown: the floating list under 文件 ▾ -----------------------
    // ONLY 打开 / 保存 / 另存为.  The .smproj association is a SETUP action, not a
    // document action: it moved behind --register-assoc / --unregister-assoc and
    // tools\associate-smproj.reg so it cannot be clicked by accident.
    if (ui_.fileOpen) {
        static const S kFileRows[3] = {S::FnOpen, S::FnSave, S::FnSaveAs};
        const int kFileRowCount = 3;
        const int rowH = D(26);
        const int w = D(228);
        int x = L_.fnFile.left;
        if (x + w > cw_ - D(8)) x = cw_ - D(8) - w;
        if (x < D(8)) x = D(8);
        const int y = L_.fnFile.bottom + D(4);
        RECT box = {x, y, x + w, y + kFileRowCount * rowH + D(8)};
        L_.fileList = box;
        dr::FillRound(g, dr::RF(box), (float)D(7), dr::C(theme::kPanel));
        dr::StrokeRound(g, dr::RF(box), (float)D(7), dr::C(theme::kBorderStrong), 1.f);
        // rows past the three document actions do not exist
        L_.fileRow[3] = L_.fileRow[4] = {};
        for (int i = 0; i < kFileRowCount; ++i) {
            RECT r = {x + D(4), y + D(4) + i * rowH, x + w - D(4),
                      y + D(4) + (i + 1) * rowH};
            L_.fileRow[i] = r;
            if (ui_.fileHoverRow == i)
                dr::FillRound(g, dr::RF(r), (float)D(4), dr::C(theme::kHover));
            dr::Text(g, L10n::T(kFileRows[i]), Font(9.f), dr::C(theme::kText),
                     RECT{r.left + D(12), r.top, r.right - D(8), r.bottom}, 0, true);
        }
    } else {
        L_.fileList = {};
        for (int i = 0; i < 5; ++i) L_.fileRow[i] = {};
    }

    // ---- language dropdown: the floating list under the 中 / EN badge ---------
    if (ui_.langOpen) {
        const int rowH = D(26);
        const int w = D(132);
        int x = L_.langBox.left;
        if (x + w > cw_ - D(8)) x = cw_ - D(8) - w;
        if (x < D(8)) x = D(8);
        const int y = L_.langBox.bottom + D(4);
        RECT box = {x, y, x + w, y + 4 * rowH + D(8)};
        L_.langList = box;
        dr::FillRound(g, dr::RF(box), (float)D(7), dr::C(theme::kPanel));
        dr::StrokeRound(g, dr::RF(box), (float)D(7), dr::C(theme::kBorderStrong), 1.f);
        for (int i = 0; i < 4; ++i) {
            RECT r = {x + D(4), y + D(4) + i * rowH, x + w - D(4),
                      y + D(4) + (i + 1) * rowH};
            L_.langRow[i] = r;
            const bool active = (int)L10n::Get() == i;
            if (ui_.langHoverRow == i)
                dr::FillRound(g, dr::RF(r), (float)D(4), dr::C(theme::kHover));
            dr::Text(g, L10n::Label((Lang)i), Font(9.f, active),
                     dr::C(active ? theme::kText : theme::kTextMuted),
                     RECT{r.left + D(12), r.top, r.right - D(30), r.bottom}, 0, true);
            if (active)
                dr::Text(g, L10n::Code((Lang)i), Font(8.f),
                         dr::C(theme::kAccent),
                         RECT{r.right - D(34), r.top, r.right - D(8), r.bottom}, 2,
                         true);
        }
    } else {
        L_.langList = {};
        for (int i = 0; i < 4; ++i) L_.langRow[i] = {};
    }

    // ---- 导出图片 resolution chooser -------------------------------------
    // Hangs under the export chip on the three drawing pages.  THREE rows
    // (普通 2× / 高清 4× / 超清 8×) with the current one marked, plus one muted
    // line showing the pixel size the hovered (or current) row produces — the
    // same number the exporter will really write, straight from PngExportSize.
    if (ui_.pngScaleOpen) {
        const int rowH = D(26);
        const int w = D(210);
        const int footH = D(22);
        const int boxH = D(8) + 3 * rowH + D(6) + 1 + footH;
        int x = L_.fnPdf.left;
        if (x + w > cw_ - D(8)) x = cw_ - D(8) - w;
        if (x < D(8)) x = D(8);
        int y = L_.fnPdf.bottom + D(4);
        // never leave the window: flip above the chip when there is no room below
        if (y + boxH > ch_ - D(8))
            y = (std::max)((int)D(4), (int)(L_.fnPdf.top - boxH - D(4)));
        RECT box = {x, y, x + w, y + boxH};
        L_.pngScaleList = box;
        dr::FillRound(g, dr::RF(box), (float)D(7), dr::C(theme::kPanel));
        dr::StrokeRound(g, dr::RF(box), (float)D(7), dr::C(theme::kBorderStrong), 1.f);
        for (int i = 0; i < 3; ++i) {
            RECT r = {x + D(4), y + D(4) + i * rowH, x + w - D(4),
                      y + D(4) + (i + 1) * rowH};
            L_.pngScaleRow[i] = r;
            const bool active = pngScaleMul_ == kPngScales[i];
            if (ui_.pngScaleHoverRow == i || active)
                dr::FillRound(g, dr::RF(r), (float)D(4),
                              dr::C(active ? theme::kAccentSoft : theme::kHover));
            // the selected row gets a dot, like the language list marks its entry
            if (active)
                dr::FillRound(g, Gdiplus::RectF((float)(r.left + D(8)),
                                               (float)((r.top + r.bottom) / 2 - D(2)),
                                               (float)D(4), (float)D(4)),
                              (float)D(2), dr::C(theme::kAccent));
            // 「普通」 + the numeric multiplier, so the row never needs a translated
            // string that could disagree with the actual scale
            const std::wstring label =
                std::wstring(L10n::T(i == 0 ? S::PngScaleNormal
                                          : i == 1 ? S::PngScaleHigh : S::PngScaleUltra)) +
                dr::Fmt(L" %d×", kPngScales[i]);
            dr::Text(g, label, Font(9.f, active),
                     dr::C(active ? theme::kText : theme::kTextMuted),
                     RECT{r.left + D(20), r.top, r.right - D(8), r.bottom}, 0, true);
        }
        const int ruleY = y + D(4) + 3 * rowH + D(3);
        dr::Line(g, (float)(x + D(10)), (float)ruleY, (float)(x + w - D(10)),
                 (float)ruleY, dr::C(theme::kBorder), 1.f);
        // the size preview: hovered row wins, otherwise the row already selected
        const int previewIdx = (ui_.pngScaleHoverRow >= 0 && ui_.pngScaleHoverRow < 3)
                                   ? ui_.pngScaleHoverRow
                                   : (pngScaleMul_ == 2 ? 0 : pngScaleMul_ == 8 ? 2 : 1);
        int pw = 0, ph = 0;
        double pww = 0, pwh = 0;
        PngExportSize(kPngScales[previewIdx], pw, ph, pww, pwh);
        L_.pngScaleFoot = {x + D(4), ruleY, x + w - D(4), y + boxH - D(4)};
        dr::Text(g, dr::Fmt(L10n::T(S::PngScaleSizeFmt), pw, ph), Font(8.f),
                 dr::C(theme::kTextMuted), L_.pngScaleFoot, 1, true);
    } else {
        L_.pngScaleList = {};
        for (int i = 0; i < 3; ++i) L_.pngScaleRow[i] = {};
        L_.pngScaleFoot = {};
    }
}

// ---------------------------------------------------------------------------
// MouseDown: the single left-press handler.  The order follows the stacking:
// the three exclusive modals first, then the panels that float OVER a page (the
// 03 对应乐器 picker, the 02 spot-mic picker, the palette popup), then the top bar
// (tabs, the 文件 / 语言 lists, the function buttons), the library, and finally
// the canvas of the open page, the inspector and the stage.
// ---------------------------------------------------------------------------
void App::MouseDown(int x, int y) {
    POINT p = {x, y};

    // the unsaved-changes prompt is exclusive: 保存 / 不保存 / 取消 and nothing else
    if (exitOpen_) {
        ExitPromptClick(x, y);
        return;
    }

    // ---- app lock ----------------------------------------------------------
    // the passcode modal is exclusive: it is the only thing that takes a press
    if (lockModal != 0) {
        LockModalClick(x, y);
        Layout();              // the inspector EDITs hide while the modal is open
        InvalidateAll();
        return;
    }
    // the padlock chip always answers; while the popover is open it is the only
    // thing that does — a press anywhere else just closes it
    if (PtInRect(&L_.lockChip, p) || lockOpen) {
        if (!LockClick(x, y) && lockOpen) LockCloseAll();
        Layout();              // EDIT controls hide / come back with the overlay
        InvalidateAll();
        return;
    }

    // ---- the three modals are exclusive: 确定 / 取消 and nothing else ---------
    if (ui_.confirmLink >= 0) {
        // delete-route confirmation: 确定 removes the cable, anything else cancels
        if (PtInRect(&L_.cfOk, p)) {
            ConfirmDeleteCable();
            return;
        }
        ui_.confirmLink = -1;   // 取消 / the scrim: keep the cable
        ui_.confirmHover = 0;
        InvalidateAll();
        return;
    }
    if (ui_.bindConfirmOpen) {
        // big-group binding: 确定 creates the converging group, anything else keeps
        // the microphones unbound
        if (PtInRect(&L_.bgOk, p)) {
            ConfirmBindGroup();
            return;
        }
        CancelBindGroup();
        return;
    }
    if (PresetModalClick(x, y)) return;

    // channel-table 对应乐器 picker (drawn over the table), two levels: a category
    // row opens its instruments; 当前/侧展/环境 bind directly
    if (ui_.chSpotOpen >= 0) {
        const int micOpen = ui_.chSpotOpen;
        // second level first: it is drawn on top
        for (size_t k = 0; k < L_.spotSubRow.size(); ++k) {
            if (L_.spotSubRow[k].right <= L_.spotSubRow[k].left ||
                !PtInRect(&L_.spotSubRow[k], p))
                continue;
            const std::vector<SpotRow> sub = SpotSubRows(ui_.spotGroup);
            if (k < sub.size()) ApplySpotRow(sub[k], micOpen);
            ui_.chSpotOpen = -1;
            ui_.chSpotHover = -1;
            ui_.spotGroup = -1;
            ui_.hoverSpotSub = -1;
            L_.chPickRow.clear();
            L_.spotSubRow.clear();
            SyncEdits();
            InvalidateAll();
            return;
        }
        {
            const std::vector<SpotRow> rows = SpotTopRows(micOpen);
            for (size_t i = 0; i < L_.chPickRow.size(); ++i) {
                if (L_.chPickRow[i].right <= L_.chPickRow[i].left ||
                    !PtInRect(&L_.chPickRow[i], p))
                    continue;
                const int idx = ui_.chSpotScroll + (int)i;
                if (idx < 0 || idx >= (int)rows.size()) break;
                const SpotRow row = rows[(size_t)idx];
                if (row.kind == 3) {
                    // a category row opens its own submenu instead of binding
                    ui_.spotGroup = SpotGroupIndexOfKey(row.id);
                    ui_.chSpotHover = (int)i;
                    ui_.spotGroup = (ui_.spotGroup < 0) ? -1 : ui_.spotGroup;
                    InvalidateAll();
                    return;
                }
                ApplySpotRow(row, micOpen);
                ui_.chSpotOpen = -1;
                ui_.chSpotHover = -1;
                ui_.spotGroup = -1;
                ui_.hoverSpotSub = -1;
                L_.chPickRow.clear();
                L_.spotSubRow.clear();
                SyncEdits();
                InvalidateAll();
                return;
            }
        }
        if (L_.chPickList.right > L_.chPickList.left && PtInRect(&L_.chPickList, p)) return;
        if (L_.spotSubPanel.right > L_.spotSubPanel.left && PtInRect(&L_.spotSubPanel, p))
            return;
        ui_.chSpotOpen = -1;
        ui_.chSpotHover = -1;
        ui_.spotGroup = -1;
        ui_.hoverSpotSub = -1;
        L_.chPickRow.clear();
        L_.spotSubRow.clear();
        InvalidateAll();
    }

    // 04 链路 / 路由: the floating DEVICE picker.  It is painted last, so its press
    // handling comes before anything under the panel; picking a row connects that
    // row's microphone through the very same action the 布线 page uses.
    if (ui_.chRouteOpen >= 0) {
        for (size_t k = 0; k < L_.chRouteRow.size(); ++k) {
            if (L_.chRouteRow[k].right <= L_.chRouteRow[k].left ||
                !PtInRect(&L_.chRouteRow[k], p))
                continue;
            if (k < L_.chRouteDev.size()) {
                const int micId = ui_.chRouteOpen;
                ui_.chRouteOpen = -1;
                ui_.chRouteHover = -1;
                ConnectMicToDevice(micId, L_.chRouteDev[k]);
            }
            SyncEdits();
            InvalidateAll();
            return;
        }
        if (L_.chRoutePanel.right > L_.chRoutePanel.left &&
            PtInRect(&L_.chRoutePanel, p))
            return;                  // a press inside the panel is not "outside"
        ui_.chRouteOpen = -1;        // click outside closes it
        ui_.chRouteHover = -1;
        InvalidateAll();
    }

    // spot-mic instrument picker (drawn left of the inspector), two levels
    if (ui_.spotListOpen >= 0) {
        const int micOpen = ui_.spotListOpen;
        for (size_t k = 0; k < L_.spotSubRow.size(); ++k) {
            if (L_.spotSubRow[k].right <= L_.spotSubRow[k].left ||
                !PtInRect(&L_.spotSubRow[k], p))
                continue;
            const std::vector<SpotRow> sub = SpotSubRows(ui_.spotGroup);
            if (k < sub.size()) ApplySpotRow(sub[k], micOpen);
            ui_.spotListOpen = -1;
            ui_.spotGroup = -1;
            ui_.hoverSpotSub = -1;
            L_.spotSubRow.clear();
            SyncEdits();
            InvalidateAll();
            return;
        }
        for (int i = 0; i < 16; ++i) {
            if (L_.spotListRow[i].right <= L_.spotListRow[i].left ||
                !PtInRect(&L_.spotListRow[i], p))
                continue;
            // the picker belongs to the microphone it was opened for (not to
            // whatever is selected now) and binds its WHOLE group
            const int idx = ui_.spotScroll + i;
            const std::vector<SpotRow> rows = SpotTopRows(micOpen);
            if (idx >= 0 && idx < (int)rows.size()) {
                const SpotRow row = rows[(size_t)idx];
                if (row.kind == 3) {
                    ui_.spotGroup = SpotGroupIndexOfKey(row.id);
                    InvalidateAll();
                    return;
                }
                ApplySpotRow(row, micOpen);
                ui_.spotListOpen = -1;
                ui_.spotGroup = -1;
                ui_.hoverSpotSub = -1;
                L_.spotSubRow.clear();
                SyncEdits();
                InvalidateAll();
                return;
            }
            return;
        }
        if (L_.spotList.right > L_.spotList.left && PtInRect(&L_.spotList, p)) return;
        if (L_.spotSubPanel.right > L_.spotSubPanel.left && PtInRect(&L_.spotSubPanel, p))
            return;
        ui_.spotListOpen = -1;
        ui_.spotGroup = -1;
        ui_.hoverSpotSub = -1;
    }

    // palette popup: "add instrument"
    if (ui_.popupIndex >= 0) {
        if (PtInRect(&L_.popup, p)) {
            AddItem(ui_.popupIndex);
            ui_.popupIndex = -1;
            ui_.popupCat = -1;
            InvalidateAll();
            return;
        }
        ui_.popupIndex = -1;
        ui_.popupCat = -1;
        InvalidateAll();
    }

    // ---------------------------------------------------------------------
    // inline channel-cell editor is dismissed by any click outside of it
    // ---------------------------------------------------------------------
    CloseChannelCell();

    // Space + left drag anywhere on the canvas pans the view
    if (panSpace_ && PtInRect(&L_.canvas, p)) {
        PanBegin(x, y, false);
        return;
    }

    // ---- top bar ---------------------------------------------------------
    for (int i = 0; i < kTabCount; ++i) {
        if (!PtInRect(&L_.layerTabs[i], p)) continue;
        ui_.layer = LayerOfTab(i);
        // the library follows the page: 话筒摆位 is the mic palette, everything
        // else defaults to instruments
        ui_.libTab = (LayerOfTab(i) == 1) ? 1 : 0;
        ui_.popupIndex = -1;
        ui_.popupCat = -1;
        ui_.spotListOpen = -1;
        ui_.langOpen = false;
        ui_.langHoverRow = -1;
        // the export chip's scale chooser belongs to the drawing pages: a page
        // switch always closes it (通道表 would not offer it at all)
        ui_.pngScaleOpen = false;
        ui_.pngScaleHoverRow = -1;
        ui_.chSpotOpen = -1;
        ui_.chSpotHover = -1;
        L_.chPickRow.clear();
        SyncEdits();
        Layout();
        InvalidateAll();
        return;
    }
    if (PtInRect(&L_.langBox, p)) {
        ui_.langOpen = !ui_.langOpen;
        ui_.langHoverRow = -1;
        InvalidateAll();
        return;
    }
    if (PtInRect(&L_.fnFile, p)) {
        ui_.fileOpen = !ui_.fileOpen;
        ui_.fileHoverRow = -1;
        ui_.langOpen = false;
        ui_.langHoverRow = -1;
        InvalidateAll();
        return;
    }
    // the 文件 / 语言 lists hang BELOW their button (PaintOverlay draws them): a
    // row takes the press, a press anywhere else only closes the list
    if (ui_.fileOpen) {
        const bool inside = PtInRect(&L_.fileList, p);
        for (int i = 0; i < 5; ++i) {
            if (L_.fileRow[i].right <= L_.fileRow[i].left ||
                !PtInRect(&L_.fileRow[i], p)) continue;
            ui_.fileOpen = false;
            ui_.fileHoverRow = -1;
            if (i == 0) OpenProjectDialog();
            else if (i == 1) SaveProjectDialog(false);
            else if (i == 2) SaveProjectDialog(true);      // 另存为
            // (导出图片 lives on the top-bar export chip)
            InvalidateAll();
            return;
        }
        ui_.fileOpen = false;
        ui_.fileHoverRow = -1;
        InvalidateAll();
        if (inside) return;
    }
    if (ui_.pngScaleOpen) {
        const bool inside = PtInRect(&L_.pngScaleList, p);
        for (int i = 0; i < 3; ++i) {
            if (L_.pngScaleRow[i].right <= L_.pngScaleRow[i].left ||
                !PtInRect(&L_.pngScaleRow[i], p)) continue;
            ui_.pngScaleOpen = false;
            ui_.pngScaleHoverRow = -1;
            PickPngScale(kPngScales[i]);
            return;
        }
        ui_.pngScaleOpen = false;
        ui_.pngScaleHoverRow = -1;
        InvalidateAll();
        // a click ON the chip itself only closes the popup (the chip's own handler
        // below would otherwise immediately reopen it)
        if (inside || PtInRect(&L_.fnPdf, p)) return;
    }
    if (ui_.langOpen) {
        const bool inside = PtInRect(&L_.langList, p);
        for (int i = 0; i < 4; ++i) {
            if (L_.langRow[i].right <= L_.langRow[i].left ||
                !PtInRect(&L_.langRow[i], p)) continue;
            ui_.langOpen = false;
            ui_.langHoverRow = -1;
            if ((int)L10n::Get() != i) SetLanguage((Lang)i);
            InvalidateAll();
            return;
        }
        ui_.langOpen = false;
        ui_.langHoverRow = -1;
        InvalidateAll();
        if (inside) return;
    }
    if (L_.fnSra.right > L_.fnSra.left && PtInRect(&L_.fnSra, p)) {
        ui_.showSra = !ui_.showSra;
        InvalidateAll();
        return;
    }
    if (PtInRect(&L_.fnSimple, p)) {
        ui_.simple = !ui_.simple;
        InvalidateAll();
        return;
    }
    if (PtInRect(&L_.fnPdf, p)) {
        // one button, two exports:
        //  · 通道表 (layer 3)  → the PDF table, straight to the save dialog
        //  · a drawing page    → the tiny scale chooser (普通 2× / 高清 4× / 超清 8×),
        //    which then opens the PNG save dialog at the picked scale
        if (ui_.layer == 3) {
            ui_.pngScaleOpen = false;
            ui_.pngScaleHoverRow = -1;
            Layout();
            ExportPdf();
        } else {
            ui_.pngScaleOpen = !ui_.pngScaleOpen;
            ui_.pngScaleHoverRow = -1;
            // the two other top-bar lists are exclusive with this one
            ui_.fileOpen = false;
            ui_.fileHoverRow = -1;
            ui_.langOpen = false;
            ui_.langHoverRow = -1;
            Layout();      // the popup rects exist before the first paint/hit test
            InvalidateAll();
        }
        return;
    }
    // 预设: open the built-in orchestra preset modal (src/Presets.cpp)
    if (PtInRect(&L_.fnPreset, p)) {
        OpenPresetModal();
        return;
    }

    // ---- library ---------------------------------------------------------
    for (int i = 0; i < 2; ++i) {
        if (!PtInRect(&L_.libTabs[i], p)) continue;
        ui_.libTab = i;
        ui_.scroll = 0;
        InvalidateAll();
        return;
    }
    if (PtInRect(&L_.searchBox, p)) {
        SetFocus(eSearch_);
        return;
    }
    for (const auto& t : tiles_) {
        if (t.index < 0 || !PtInRect(&t.r, p)) continue;
        const auto& list = ui_.libTab == 0 ? Instruments() : Mics();
        if (t.index >= (int)list.size()) return;
        // a locked app never arms a palette drag / add
        if (RefuseLocked()) return;
        const CatItem& c = list[t.index];
        if (c.id == std::wstring(L"mainmic") && pr_.HasMainMicItem()) {
            InvalidateAll();
            return;
        }
        // a press on a tile only ARMS a drag; a plain click (released without
        // moving) still highlights twins and opens the add popup in MouseUp, so
        // the click behaviour is unchanged while dragging drops the item
        ui_.dragCat_ = t.index;
        ui_.dragTab_ = ui_.libTab;
        ui_.dragStart_ = p;
        ui_.dragPt_ = p;
        ui_.paletteDragging_ = false;
        ui_.popupCat = -1;
        ui_.popupIndex = -1;
        SetCapture(hwnd_);
        InvalidateAll();
        return;
    }

    // ---- layer 03 (话筒拉线): the devices own the inspector and the canvas ----
    if (ui_.layer == 2) {
        if (PtInRect(&L_.right, p)) {
            CablingInspectorDown(x, y);
            return;
        }
        if (PtInRect(&L_.canvas, p)) {
            const int hitDev = DeviceHitTest(x, y);
            // a microphone marker is selectable too: it opens the mic inspector
            const int hitMic = (hitDev < 0) ? MicMarkerHitTest(x, y) : -1;
            const bool ctrlDev = (GetKeyState(VK_CONTROL) & 0x8000) != 0 || scriptCtrl_;
            pr_.Deselect();               // items never mix with the device panel
            const bool emptyCanvas = (hitDev < 0 && hitMic < 0);
            // on an EMPTY canvas the press no longer drops the device selection:
            // PanEnd does it when the press turns out to be a plain click, because a
            // pan must leave the selection alone
            if (hitDev < 0 && !ctrlDev && !emptyCanvas) pr_.ClearDeviceSelection();
            if (emptyCanvas) {
                // empty canvas: Ctrl + left-drag draws the rubber band and ADDS to
                // the current device selection; without Ctrl the drag pans the view
                // (the middle-drag / Space+drag path) and a plain click still just
                // drops the selection when the release arrives
                if (ctrlDev) {
                    BandBegin(x, y, true);
                } else {
                    PanBegin(x, y, true);
                }
                SyncEdits();
                Layout();
                InvalidateAll();
                return;
            }
            if (hitMic >= 0) {
                // a grouped microphone resolves to the whole group, so any member
                // shows the group's single cable in the inspector
                pr_.selected = hitMic;
                pr_.selIds.assign(1, hitMic);
                ui_.hoverCable = -1;
                ui_.hoverCbRoute = -1;
                ui_.hoverCbRouteDel = -1;
                SyncEdits();
                Layout();
                InvalidateAll();
                return;   // microphones are not dragged on the wiring page
            }
            // Ctrl+click toggles a device in / out of the multi-selection
            if (ctrlDev) {
                if (Device* dv = pr_.FindDevice(hitDev)) dv->selected = !dv->selected;
                SyncEdits();
                Layout();
                InvalidateAll();
                return;
            }
            // a click on a box that is NOT selected drags that box alone; a click on
            // one that IS selected keeps the multi-selection and drags all of it
            const Device* d0 = pr_.FindDevice(hitDev);
            const bool wasSel = d0 && d0->selected;
            if (!wasSel) pr_.SelectDevice(hitDev);
            downX_ = x;
            downY_ = y;
            devDragMoved_ = false;
            if (const Device* dv = pr_.FindDevice(hitDev)) {
                double wx = 0, wy = 0;
                // a locked device (file-compatibility flag) can be selected, not moved;
                // neither can anything while the APP LOCK is engaged (read-only view)
                if (!dv->locked && !Locked() && PointToWorld(x, y, wx, wy)) {
                    devDrag_ = true;
                    devDragId_ = hitDev;
                    devDragDx_ = wx - dv->x;
                    devDragDy_ = wy - dv->y;
                    SetCapture(hwnd_);
                } else if (Locked()) {
                    StatusMsg(L10n::T(S::LockReadOnly), true);
                }
            }
            SyncEdits();
            Layout();
            InvalidateAll();
            return;
        }
        return;   // nothing else responds on this layer
    }


    // ---- inspector -------------------------------------------------------
    const Item* sel = pr_.Selected();
    for (int i = 0; i < 3; ++i) {
        if (!PtInRect(&L_.presets[i], p)) continue;
        // 音乐厅 18 x 12 (the hall this app plans for) / 教堂 / 沙龙
        const double sz[3][2] = {{18.0, 12.0}, {12.0, 20.0}, {9.0, 7.0}};
        SetStageSize(sz[i][0], sz[i][1]);   // keeps the ensemble centred
        return;
    }
    if (L_.spotBtn.right > L_.spotBtn.left && PtInRect(&L_.spotBtn, p)) {
        Item* mic = SpotMicInSel();
        if (mic) {
            const std::wstring inst = SelectedInstType();
            // the binding belongs to the whole group, never to one member
            const std::vector<int> targets = SpotTargetsOf(mic->id);
            if (!inst.empty()) {
                SetSpotType(targets, inst);
                ui_.spotListOpen = -1;
            } else if (!mic->spotType.empty()) {
                SetSpotType(targets, std::wstring());
                ui_.spotListOpen = -1;
            } else {
                ui_.spotListOpen = mic->id;
                ui_.spotScroll = 0;
                ui_.spotHover = -1;
            }
            SyncEdits();
            InvalidateAll();
        }
        return;
    }
    // ---- 颜色: the per-item tint override (instruments / 声部 targets) ---------
    if (sel && sel->kind == ItemKind::Instrument) {
        // 「恢复默认」: drop the override, the item goes back to its palette slot
        if (L_.itemColorReset.right > L_.itemColorReset.left &&
            PtInRect(&L_.itemColorReset, p)) {
            if (RefuseLocked()) return;
            if (sel->colorOverride) {
                PushUndoSnapshot();
                if (Item* t = pr_.Selected()) t->colorOverride = 0;
                StatusMsg(L10n::T(S::ColorResetDone));
                SyncEdits();
                Layout();
                InvalidateAll();
            }
            return;
        }
        // the row (label / swatch / hex) opens the SAME colour picker the 01 page
        // uses; 取消 leaves the palette colour untouched
        if (L_.itemColorRow.right > L_.itemColorRow.left &&
            PtInRect(&L_.itemColorRow, p)) {
            if (RefuseLocked()) return;
            Rgb c = ItemIconTint(pr_, *sel, true);
            const Rgb before = c;
            PickColor(c);
            if (c.r == before.r && c.g == before.g && c.b == before.b) return;
            PushUndoSnapshot();
            if (Item* t = pr_.Selected()) {
                t->colorOverride = ((unsigned)c.r << 16) | ((unsigned)c.g << 8) |
                                   (unsigned)c.b;
                StatusMsg(dr::Fmt(L10n::T(S::ColorSetFmt), t->colorOverride));
            }
            SyncEdits();
            Layout();
            InvalidateAll();
            return;
        }
    }
    if (sel && sel->defId == L"mainmic") {
        // the 制式 / 指向性 buttons are data (spacing, pattern, technique)
        if (RefuseLocked()) return;
        for (int i = 0; i < 4; ++i) {
            if (L_.techBtn[i].right <= L_.techBtn[i].left ||
                !PtInRect(&L_.techBtn[i], p)) continue;
            Item* m = pr_.Selected();
            m->mainTech = i;
            if (i == 0) {
                m->pattern = 0;
                if (m->spacing < 0.05) m->spacing = 0.40;
            } else if (i == 1) {
                m->pattern = 4;
                if (m->spacing < 0.05 || m->spacing > 0.35) m->spacing = 0.17;
            } else if (i == 2) {
                m->pattern = 1;
                m->spacing = 0.0;
            } else {
                m->pattern = 0;
                if (m->spacing < 0.05) m->spacing = 0.40;
                if (m->center < 0.05) m->center = 0.60;
            }
            SyncEdits();
            Layout();
            InvalidateAll();
            return;
        }
        for (int i = 0; i < 3; ++i) {
            if (L_.techPreset[i].right <= L_.techPreset[i].left ||
                !PtInRect(&L_.techPreset[i], p)) continue;
            static const double sp[3] = {0.17, 0.30, 0.20};   // ORTF / NOS / DIN
            Item* m = pr_.Selected();
            m->mainTech = 1;
            m->pattern = 4;
            m->spacing = sp[i];
            SyncEdits();
            Layout();
            InvalidateAll();
            return;
        }
        for (int i = 0; i < 4; ++i) {
            if (L_.patternBtn[i].right <= L_.patternBtn[i].left ||
                !PtInRect(&L_.patternBtn[i], p)) continue;
            Item* m = pr_.Selected();
            m->pattern = PatternAtSlot(m->mainTech, i);
            SyncEdits();
            InvalidateAll();
            return;
        }
    }
    for (int i = 0; i < 2; ++i) {   // 文字 / 舞台底色 (乐器线条·话筒线条 rows removed)
        if (!PtInRect(&L_.colorRow[i], p)) continue;
        if (i == 0) PickColor(pr_.textColor);
        else PickColor(pr_.stageFill);
        InvalidateAll();
        return;
    }
    for (int i = 0; i < 2; ++i) {
        if (!PtInRect(&L_.layerRow[i], p) && !PtInRect(&L_.layerCheck[i], p)) continue;
        if (i == 0) pr_.showInstruments = !pr_.showInstruments;
        else pr_.showMics = !pr_.showMics;
        InvalidateAll();
        return;
    }
    // 显示乐器文字 / 显示话筒文字 (project flags, persisted)
    if (PtInRect(&L_.textRow[0], p)) {
        pr_.showInstrumentText = !pr_.showInstrumentText;
        InvalidateAll();
        return;
    }
    if (PtInRect(&L_.textRow[1], p)) {
        pr_.showMicText = !pr_.showMicText;
        InvalidateAll();
        return;
    }
    // 同类乐器: select every instance of one type
    for (size_t si = 0; si < L_.sameTypeBtn.size(); ++si) {
        if (!PtInRect(&L_.sameTypeBtn[si], p)) continue;
        if (RefuseLocked()) return;
        SelectSameType(L_.sameTypeDef[si]);
        return;
    }
    // 吊装 (flown) checkbox on a microphone
    if (L_.flownRow.right > L_.flownRow.left && PtInRect(&L_.flownRow, p)) {
        ToggleFlown();
        return;
    }
    if (PtInRect(&L_.delBtn, p)) {
        if (sel) DeleteSelected();
        return;
    }
    if (PtInRect(&L_.bindBtn, p)) {
        bool allMics = !pr_.selIds.empty();
        for (int id : pr_.selIds) {
            const Item* q = pr_.Find(id);
            if (!q || q->kind != ItemKind::Mic) allMics = false;
        }
        bool anyGroup = false;
        for (int id : pr_.selIds)
            if (pr_.GroupOf(id) >= 0) anyGroup = true;
        bool canBind = allMics && pr_.selIds.size() >= 2 && !anyGroup;
        if (canBind) BindSelected(0);
        else UnbindSelected();
        Layout();
        InvalidateAll();
        return;
    }
    if (PtInRect(&L_.treeBtn, p)) {
        bool allMics = pr_.selIds.size() >= 3;
        for (int id : pr_.selIds) {
            const Item* q = pr_.Find(id);
            if (!q || q->kind != ItemKind::Mic || q->defId == L"mainmic") allMics = false;
        }
        if (allMics && !pr_.HasDeccaTree()) BindSelected(1);
        else if (pr_.HasDeccaTree()) UnbindSelected();
        Layout();
        InvalidateAll();
        return;
    }

    // ---- stage / channel table -------------------------------------------
    if (!PtInRect(&L_.canvas, p)) return;
    // 项目信息 (layer 5) is a document page: its canvas holds no objects, so a
    // click there pans nothing and selects nothing
    if (ui_.layer == 5) return;
    if (ui_.layer == 3) {
        // 「自动排序」 drop the manual row order and go back to the section order
        {
            const ChTableGeom gt = ChannelTableGeom();
            if (gt.autoSort.right > gt.autoSort.left && PtInRect(&gt.autoSort, p)) {
                ResetChannelOrder();
                return;
            }
            // pager: ◀ / ▶ flip a whole page (no scrolling)
            if (gt.pagePrev.right > gt.pagePrev.left && PtInRect(&gt.pagePrev, p)) {
                ChPageMove(-1);
                return;
            }
            if (gt.pageNext.right > gt.pageNext.left && PtInRect(&gt.pageNext, p)) {
                ChPageMove(1);
                return;
            }
            // 「话筒线长度」 COLUMN HEADER = the 推荐 / 实际 switch.  The header cell
            // shows the active mode (「…(推荐)」 / 「…(实际)」) and a click flips it.
            if (ui_.chColVis[5] && y >= gt.headerY && y <= gt.headerY + gt.headerH &&
                x >= (LONG)std::lround(gt.cols.l[5]) &&
                x <= (LONG)std::lround(gt.cols.r[5])) {
                pr_.lenMode = (pr_.lenMode == 1) ? 0 : 1;
                StatusMsg(L10n::T(pr_.lenMode == 1 ? S::LenModeOnReal
                                                   : S::LenModeOnReserved));
                Layout();
                InvalidateAll();
                return;
            }
        }
        // Drag-reorder: a press on the 通道 or 链路 column of a data row only
        // selects the row and ARMS a row move; every editable cell (型号 / 对应
        // 乐器 / 备注 / 长度) keeps its inline editor through OpenChannelCell.
        const ChTableGeom gt = ChannelTableGeom();
        if (x >= gt.x0 && x <= gt.x1 && y >= gt.rowsY && gt.rowH > 0) {
            const int visIdx = (y - gt.rowsY) / gt.rowH;
            const std::vector<ChRow> rows = BuildChannelRows();
            const int pageRows = (std::max)(1, gt.pageRows);
            const int idx = ui_.chPage * pageRows + visIdx;
            if (visIdx >= 0 && visIdx < gt.visibleRows && idx >= 0 &&
                idx < (int)rows.size()) {
                int col = -1;
                for (int i = 0; i < kChCols; ++i) {
                    if (!ui_.chColVis[i]) continue;
                    if (x >= gt.cols.l[i] - D(4) && x <= gt.cols.r[i] + D(4)) col = i;
                }
                pr_.selected = rows[(size_t)idx].itemId;
                pr_.selIds.assign(1, rows[(size_t)idx].itemId);
                if (col == 0) {   // 通道: the row's handle (链路 opens the 路由 popup)
                    if (RefuseLocked()) return;   // row reorder is a data change
                    ui_.chDragRow = idx;
                    ui_.chDragActive = false;
                    ui_.chDropRow = idx;
                    ui_.chDragStart = POINT{x, y};
                    SetCapture(hwnd_);
                    SyncEdits();
                    Layout();
                    InvalidateAll();
                    return;
                }
            }
        }
        OpenChannelCell(x, y);
        return;
    }
    if (RotateHandleHit(x, y)) {
        // rotation changes the angle of every selected object: refused while locked
        if (RefuseLocked()) return;
        // a selection that mixes microphones with instruments never rotates as one
        // (RotateHandlePosMulti already hides the handle for it; this is the guard
        // on the gesture itself, so no snapshot and no status spam can slip in)
        if (SelectionMixedKinds()) {
            StatusMsg(L10n::T(S::MixedKindsMove), true);
            return;
        }
        rotating_ = true;
        rotId_ = pr_.selected;
        rotApplied_ = 0;
        rotBase_ = 0;
        // a MULTI selection rotates about its bounding-box centre, frozen for the
        // whole gesture; the grab angle becomes the zero of the rotation
        if (pr_.selIds.size() >= 2) {
            RECT sb{};
            if (SelectionBBox(sb)) {
                const double cx = (sb.left + sb.right) * 0.5;
                const double cy = (sb.top + sb.bottom) * 0.5;
                rotPivotX_ = (cx - ox_) / ppm_;
                rotPivotY_ = (cy - oy_) / ppm_;
                rotBase_ = std::atan2((double)x - cx, -((double)y - cy)) * 180.0 /
                           3.14159265358979323846;
            } else {
                rotating_ = false;
            }
        }
        SetCapture(hwnd_);
        return;
    }
    bool locked = false;
    int hit = StageHitTest(x, y, locked);
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0 || scriptCtrl_;
    if (hit < 0) {
        // empty stage space: Ctrl + left-drag draws the rubber band over the objects
        // of the CURRENT layer and ADDS them to the selection (Esc cancels it, and
        // it never records an undo entry).  WITHOUT Ctrl the empty canvas keeps its
        // old gesture: the drag PANS the view through the very same PanBegin /
        // PanTo / PanEnd path that middle-drag and Space+drag use.  A plain click
        // still only drops the selection — PanEnd does the deselect when the drag
        // never passed the 3 px threshold.
        if (ctrl) {
            BandBegin(x, y, true);
        } else {
            PanBegin(x, y, true);
        }
        return;
    }
    if (ctrl) {
        if (pr_.IsSel(hit)) {
            pr_.selIds.erase(std::remove(pr_.selIds.begin(), pr_.selIds.end(), hit),
                             pr_.selIds.end());
            if (pr_.selected == hit)
                pr_.selected = pr_.selIds.empty() ? -1 : pr_.selIds.back();
        } else {
            pr_.selIds.push_back(hit);
            pr_.selected = hit;
        }
    } else if (!pr_.IsSel(hit)) {
        pr_.selIds.assign(1, hit);
        pr_.selected = hit;
    } else {
        pr_.selected = hit;
    }
    // objects are always draggable now: their cables follow live — EXCEPT when the
    // selection mixes microphones with instruments.  Only ONE kind belongs to the
    // open page (the other one is merely visible and locked), so moving the set as
    // one would drag a locked object along with the editable one.  The gesture is
    // refused HERE, at the single point it is armed: that is why the message shows
    // once per press instead of once per mouse-move, and why DragTo never records
    // an undo snapshot (nothing moves, so nothing may become undoable).
    const bool mixedSel = SelectionMixedKinds(hit);
    // ... and the very same rule applies to the APP LOCK: the selection changes,
    // the object never moves.  The press is reported once, then nothing is armed.
    const bool appLocked = Locked();
    if (appLocked) StatusMsg(L10n::T(S::LockReadOnly), true);
    drag_ = !locked && !mixedSel && !appLocked;
    dragMoved_ = false;
    dragId_ = hit;
    downX_ = x;
    downY_ = y;
    if (mixedSel && !appLocked) StatusMsg(L10n::T(S::MixedKindsMove), true);
    if (drag_) {
        double wx = 0, wy = 0;
        if (PointToWorld(x, y, wx, wy)) {
            if (const Item* it = pr_.Find(hit)) {
                dragDx_ = wx - it->x;
                dragDy_ = wy - it->y;
            }
        }
        SetCapture(hwnd_);
    }
    RefreshAfterProjectChange();
}

void App::ExportPdf() {
    std::wstring base = (ui_.layer == 3) ? L"channel-list"
                          : (ui_.layer == 2) ? L"cabling"
                      : (ui_.layer == 5) ? L"project-info"
                      : (ui_.layer == 1) ? L"mic-plot" : L"stage-plot";
    std::wstring path = exeDir_ + L"\\" + base + L".pdf";
    {
        wchar_t buf[MAX_PATH] = L"";
        wcsncpy_s(buf, (base + L".pdf").c_str(), _TRUNCATE);
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"PDF (*.pdf)\0*.pdf\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = buf;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"pdf";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetSaveFileNameW(&ofn)) {
            path = buf;
        } else if (CommDlgExtendedError() == 0) {
            return;   // user cancelled
        }
        // on dialog error fall back to the default location
    }
    for (int n = 2; GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES && n < 100; ++n)
        path = exeDir_ + L"\\" + base + L"-" + std::to_wstring(n) + L".pdf";
    // the export blocks the UI thread: pin the wait cursor + say so first
    BeginBusy(L10n::T(S::BusyPdf));
    const bool ok = ExportPdfTo(path);
    EndBusy();
    if (ok) {
        std::wstring msg = L"PDF 已导出：\n" + path;
        MessageBoxW(hwnd_, msg.c_str(), L"Stage Micing", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(hwnd_, L"PDF 导出失败。", L"Stage Micing", MB_OK | MB_ICONWARNING);
    }
}
void App::MarkMainMics() {
    if (RefuseLocked()) return;
    std::vector<int> ids;
    for (int id : pr_.selIds) {
        int g = pr_.GroupOf(id);
        if (g >= 0) {
            for (int m : pr_.micGroups[g].ids) ids.push_back(m);
        } else {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    PushUndoSnapshot();   // 标记主话筒 is undoable
    pr_.mainMics = ids;
    RefreshAfterProjectChange();
}

void App::UnbindSelected() {
    if (RefuseLocked()) return;
    if (Item* sel = pr_.Selected()) {
        int g = pr_.GroupOf(sel->id);
        if (g >= 0 && pr_.micGroups[g].kind == 2) {
            const std::wstring& tech = pr_.micGroups[g].tech;
            if (tech == L"ms" || tech == L"xy" || tech == L"blumlein") return;
        }
    }
    if (!pr_.Selected()) return;
    int g = pr_.GroupOf(pr_.Selected()->id);
    if (g < 0) return;
    PushUndoSnapshot();   // unbinding a group is undoable
    pr_.micGroups.erase(pr_.micGroups.begin() + g);
    InvalidateAll();
}

void App::CopySelected() {
    clip_.clear();
    std::vector<int> ids = pr_.selIds;
    if (ids.empty() && pr_.selected >= 0) ids.push_back(pr_.selected);
    for (int id : ids)
        if (const Item* it = pr_.Find(id)) clip_.push_back(*it);
}

void App::PasteClipboard() {
    if (RefuseLocked()) return;
    if (clip_.empty()) return;
    PushUndoSnapshot();   // pasting is undoable
    pr_.selIds.clear();
    for (Item src : clip_) {
        src.id = pr_.nextId++;
        src.x += 0.4;
        src.y += 0.4;
        pr_.items.push_back(src);
        pr_.selIds.push_back(src.id);
    }
    pr_.selected = pr_.selIds.back();
    RefreshAfterProjectChange();
}

void App::SetLanguage(Lang lang) {
    L10n::Set(lang);
    UpdateTitle();
    UpdateCueBanners();
    Layout();          // label widths and the tab strip depend on the language
    RefreshAfterProjectChange();
}

void App::PickColor(Rgb& c) {
    if (RefuseLocked()) return;
    static COLORREF custom[16] = {};
    CHOOSECOLORW cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd_;
    cc.rgbResult = RGB(c.r, c.g, c.b);
    cc.lpCustColors = custom;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&cc)) {
        c.r = GetRValue(cc.rgbResult);
        c.g = GetGValue(cc.rgbResult);
        c.b = GetBValue(cc.rgbResult);
    }
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static int PatternAtSlot(int tech, int slot) {
    if (tech == 0) return 0;                                  // AB: one omni pair
    if (tech == 1) {                                          // mixed pair: wide card / card
        static const int p[2] = {4, 1};
        return (slot >= 0 && slot < 2) ? p[slot] : 0;
    }
    if (tech == 2) {                                          // intensity pair
        static const int p[3] = {1, 2, 3};
        return (slot >= 0 && slot < 3) ? p[slot] : 0;
    }
    return 0;
}

static double ItemFootW(const Item& it) {
    if (it.w > 0.01) return it.w;
    if (const CatItem* c = FindItem(it.defId)) return c->w;
    return 0.6;
}

static double ItemFootH(const Item& it) {
    if (it.h > 0.01) return it.h;
    if (const CatItem* c = FindItem(it.defId)) return c->h;
    return 0.6;
}

static bool FootprintOverlaps(const Project& pr, double x, double y, double w, double h,
                              int skipId) {
    const double hw = (std::max)(0.2, w) * 0.5 + 0.10;
    const double hh = (std::max)(0.2, h) * 0.5 + 0.10;
    for (const auto& q : pr.items) {
        if (q.id == skipId) continue;
        const double qw = ItemFootW(q) * 0.5 + 0.08;
        const double qh = ItemFootH(q) * 0.5 + 0.08;
        if (std::fabs(q.x - x) < hw + qw && std::fabs(q.y - y) < hh + qh) return true;
    }
    return false;
}

// deterministic free-spot search, spiral outwards from the stage centre
static bool FindFreeSpot(const Project& pr, double w, double h, double& ox, double& oy) {
    const double step = 0.4;
    const double cx = pr.stage.w * 0.5;
    const double cy = pr.stage.d * 0.5;
    const double hw = (std::max)(0.2, w) * 0.5 + 0.15;
    const double hh = (std::max)(0.2, h) * 0.5 + 0.15;
    for (int ring = 0; ring < 44; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if ((std::max)(std::abs(dx), std::abs(dy)) != ring) continue;
                const double x = cx + dx * step;
                const double y = cy + dy * step;
                if (x - hw < 0.15 || x + hw > pr.stage.w - 0.15) continue;
                if (y - hh < 0.15 || y + hh > pr.stage.d - 0.15) continue;
                if (!FootprintOverlaps(pr, x, y, w, h, -1)) {
                    ox = x;
                    oy = y;
                    return true;
                }
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// editing operations
// ---------------------------------------------------------------------------
// Shared item factory.  usePos == false reproduces the original behaviour (the
// deterministic free-spot scan); usePos == true stamps the item at the world
// position given by the drag & drop.
Item* App::CreateItem(int catIndex, int tab, bool usePos, double wx, double wy) {
    if (RefuseLocked()) return nullptr;
    const auto& list = tab == 0 ? Instruments() : Mics();
    if (catIndex < 0 || catIndex >= (int)list.size()) return nullptr;
    const CatItem& c = list[catIndex];
    const std::wstring def = c.id;
    if (def == L"mainmic" && pr_.HasMainMicItem()) return nullptr;


    Item it;
    it.kind = (tab == 1) ? ItemKind::Mic : ItemKind::Instrument;
    it.defId = def;
    it.w = c.w * CatScale(def);
    it.h = c.h * CatScale(def);

    int count = 0;
    const Item* twin = nullptr;
    for (const auto& q : pr_.items) {
        if (q.defId != def) continue;
        ++count;
        twin = &q;
    }
    // a main microphone is named after the localised main-mic term, every other
    // symbol keeps its catalogue name (numbering still applies for twins)
    const std::wstring base =
        (def == L"mainmic") ? std::wstring(L10n::T(S::MainMic)) : ItemName(c);
    it.label = count > 0 ? base + L" " + std::to_wstring(count + 1) : base;
    if (twin) {
        it.angle = twin->angle;
        it.height = twin->height;
        it.pattern = twin->pattern;
        it.model = twin->model;
        it.spotType = twin->spotType;
    } else if (it.kind == ItemKind::Mic) {
        it.height = 1.50;
        it.pattern = (def == L"blumlein") ? 3 : (def == L"ortf" || def == L"nos" ? 4 : 1);
    }
    if (def == L"mainmic") {
        it.mainTech = 0;
        it.pattern = 0;
        it.spacing = 0.40;
        it.center = 0.0;
        it.w = 0.48;
        it.h = 0.42;
    }

    double px = 0, py = 0;
    if (usePos) {
        // a drop lands exactly under the cursor, clamped to the working range
        px = std::clamp(wx, -200.0, 200.0);
        py = std::clamp(wy, -200.0, 200.0);
    } else if (!FindFreeSpot(pr_, it.w, it.h, px, py)) {
        px = pr_.stage.w * 0.5;
        py = pr_.stage.d * 0.5;
    }
    it.x = px;
    it.y = py;
    it.id = pr_.nextId++;
    pr_.items.push_back(it);
    pr_.selected = it.id;
    pr_.selIds.assign(1, it.id);

    // 混合差 (ORTF / NOS / DIN): 「先生成两个单独的话筒并摆好，再绑定」 — two ordinary
    // point microphones are created through the very same CreateItem path a lone
    // mic uses, placed at ±spacing/2 with the technique's opening angles, and THEN
    // bound with CreateMicGroup (the call the 绑定 button uses).  An INTENSITY
    // technique (XY / MS / Blumlein) stays ONE object (its capsules coincide).
    if (ItemSRA(def) > 0.0 && !IsIntensityTech(def))
        PlaceTechniquePair(def, it.x, it.y, it.angle);

    if (it.kind == ItemKind::Mic && ui_.layer != 3) ui_.layer = 1;
    else if (it.kind == ItemKind::Instrument && ui_.layer != 3) ui_.layer = 0;
    ui_.popupIndex = -1;
    ui_.popupCat = -1;
    RefreshAfterProjectChange();
    return pr_.Selected();
}

void App::AddItem(int catIndex) {
    const int tab = ui_.popupCat >= 0 ? ui_.popupCat : ui_.libTab;
    PushUndoSnapshot();   // adding an object is undoable
    CreateItem(catIndex, tab, false, 0.0, 0.0);
}

Item* App::AddItemAt(int catIndex, double wx, double wy) {
    PushUndoSnapshot();   // adding an object is undoable
    return CreateItem(catIndex, ui_.libTab, true, wx, wy);
}

// A palette tile that is clicked (pressed and released without moving) keeps the
// original behaviour: highlight an existing twin and offer the add popup.
void App::PaletteTileClick(int catIndex) {
    if (RefuseLocked()) return;
    const auto& list = ui_.libTab == 0 ? Instruments() : Mics();
    if (catIndex < 0 || catIndex >= (int)list.size()) return;
    const CatItem& c = list[catIndex];
    if (c.id == std::wstring(L"mainmic") && pr_.HasMainMicItem()) {
        InvalidateAll();
        return;
    }
    // a tile click selects an existing instance of that symbol ...
    int found = -1;
    for (auto rit = pr_.items.rbegin(); rit != pr_.items.rend(); ++rit) {
        if (rit->defId == c.id) { found = rit->id; break; }
    }
    if (found >= 0) {
        pr_.selected = found;
        pr_.selIds.assign(1, found);
        SyncEdits();
    }
    // ... and offers the "add" popup right underneath the tile
    ui_.popupCat = ui_.libTab;
    ui_.popupIndex = catIndex;
    for (const auto& t : tiles_) {
        if (t.index != catIndex) continue;
        L_.popup = {t.r.left, t.r.bottom + D(6), t.r.right, t.r.bottom + D(6) + D(30)};
        break;
    }
    RefreshAfterProjectChange();
}

void App::CancelPaletteDrag() {
    if (ui_.dragCat_ < 0) return;
    ui_.dragCat_ = -1;
    ui_.dragTab_ = -1;
    ui_.paletteDragging_ = false;
    ui_.dragStart_ = POINT{};
    ui_.dragPt_ = POINT{};
    if (GetCapture() == hwnd_) ReleaseCapture();
    InvalidateAll();
}

void App::DeleteSelected() {
    if (RefuseLocked()) return;
    if (pr_.selIds.empty() && pr_.selected < 0) return;
    std::vector<int> ids = pr_.selIds;
    if (ids.empty() && pr_.selected >= 0) ids.push_back(pr_.selected);
    if (ids.empty()) return;

    std::vector<int> all;
    for (int id : ids) {
        int g = pr_.GroupOf(id);
        if (g >= 0) {
            for (int m : pr_.micGroups[g].ids) all.push_back(m);
        } else {
            all.push_back(id);
        }
    }
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());

    PushUndoSnapshot();   // deleting objects is undoable
    for (int id : all) pr_.RemoveFromGroups(id);
    pr_.items.erase(std::remove_if(pr_.items.begin(), pr_.items.end(),
                                   [&](const Item& q) {
                                       return std::find(all.begin(), all.end(), q.id) !=
                                              all.end();
                                   }),
                    pr_.items.end());
    pr_.micGroups.erase(std::remove_if(pr_.micGroups.begin(), pr_.micGroups.end(),
                                       [](const MicGroup& g) { return g.ids.size() < 2; }),
                        pr_.micGroups.end());
    for (int id : all) pr_.RemoveLinksOf(1, id);   // cables to a deleted mic go too
    pr_.Deselect();
    CloseChannelCell();
    RefreshAfterProjectChange();
}

void App::BindSelected(int kind) {
    if (RefuseLocked()) return;
    std::vector<int> ids;
    for (int id : pr_.selIds) {
        const Item* it = pr_.Find(id);
        if (!it || it->kind != ItemKind::Mic) continue;
        if (it->defId == L"mainmic") continue;
        if (pr_.GroupOf(id) >= 0) return;   // already bound
        ids.push_back(id);
    }
    if (kind == 1) {
        if (ids.size() < 2 || pr_.HasDeccaTree()) return;
    } else if (ids.size() < 2) {
        return;
    }
    const std::wstring tech = (kind == 2 && pr_.Selected()) ? pr_.Selected()->defId
                                                            : std::wstring();
    RequestMicGroup(ids, kind, tech);
}

// ---------------------------------------------------------------------------
// binding a group of microphones
//
// A MicGroup is ONE cable that CONVERGES: every member runs a short collector
// stub to the group's convergence point and a single run leaves it.  That is a
// surprise when the group is big or widely spread, so such a selection asks first
// (≥ 3 members, or the farthest two members more than 5 m apart).  A small / tight
// pair binds straight away, exactly as before.
// ---------------------------------------------------------------------------
void App::RequestMicGroup(const std::vector<int>& ids, int kind,
                          const std::wstring& tech) {
    if (RefuseLocked()) return;
    if (ids.size() < 2) return;
    double maxD = 0.0;
    for (size_t i = 0; i < ids.size(); ++i)
        for (size_t j = i + 1; j < ids.size(); ++j) {
            const Item* a = pr_.Find(ids[i]);
            const Item* b = pr_.Find(ids[j]);
            if (!a || !b) continue;
            maxD = (std::max)(maxD, std::hypot(a->x - b->x, a->y - b->y));
        }
    if (ids.size() >= 3 || maxD > 5.0) {
        ui_.bindIds = ids;
        ui_.bindKind = kind;
        ui_.bindTech = tech;
        ui_.bindConfirmOpen = true;
        ui_.bindHover = 0;
        InvalidateAll();
        return;
    }
    CreateMicGroup(ids, kind, tech);
}

void App::CreateMicGroup(const std::vector<int>& ids, int kind,
                         const std::wstring& tech) {
    if (RefuseLocked()) return;
    if (ids.size() < 2) return;
    MicGroup g;
    g.ids = ids;
    g.kind = kind;
    if (kind == 2) g.tech = tech;
    PushUndoSnapshot();   // binding a group is undoable
    pr_.micGroups.push_back(g);
    RefreshAfterProjectChange();
}

void App::ConfirmBindGroup() {
    if (RefuseLocked()) return;
    const std::vector<int> ids = ui_.bindIds;
    const int kind = ui_.bindKind;
    const std::wstring tech = ui_.bindTech;
    CancelBindGroup();
    CreateMicGroup(ids, kind, tech);
}

double App::BindGroupMaxDist() const {
    double maxD = 0.0;
    for (size_t i = 0; i < ui_.bindIds.size(); ++i)
        for (size_t j = i + 1; j < ui_.bindIds.size(); ++j) {
            const Item* a = pr_.Find(ui_.bindIds[i]);
            const Item* b = pr_.Find(ui_.bindIds[j]);
            if (!a || !b) continue;
            maxD = (std::max)(maxD, std::hypot(a->x - b->x, a->y - b->y));
        }
    return maxD;
}
void App::CancelBindGroup() {
    ui_.bindConfirmOpen = false;
    ui_.bindHover = 0;
    ui_.bindIds.clear();
    ui_.bindKind = 0;
    ui_.bindTech.clear();
    InvalidateAll();
}

// 混合差 placement, literally: two ORDINARY point microphones (same CreateItem
// path, same symbol / size / inspector as any single mic), laid out at ±spacing/2
// about the drop point with the technique's opening angles, THEN bound into a
// MicGroup of kind 2 with that technique.
void App::PlaceTechniquePair(const std::wstring& tech, double x, double y,
                             double ang) {
    if (RefuseLocked()) return;
    if (tech.empty()) return;
    double spacing = 0.17, half = 55.0;
    if (tech == L"nos") {
        spacing = 0.30;
        half = 45.0;
    } else if (tech == L"din") {
        spacing = 0.20;
        half = 45.0;
    } else if (tech == L"ortf") {
        spacing = 0.17;
        half = 55.0;
    }
    const CatItem* techCat = FindItem(tech);
    const std::wstring name = techCat ? ItemName(*techCat) : tech;
    // the placeholder technique item CreateItem just made is removed again
    if (Item* sel = pr_.Selected()) {
        const int ph = sel->id;
        if (sel->defId == tech)
            pr_.items.erase(std::remove_if(pr_.items.begin(), pr_.items.end(),
                                           [&](const Item& q) { return q.id == ph; }),
                            pr_.items.end());
        pr_.selIds.erase(std::remove(pr_.selIds.begin(), pr_.selIds.end(), ph),
                        pr_.selIds.end());
        if (pr_.selected == ph) pr_.selected = -1;
    }
    // the ordinary 话筒 palette entry — creating through it is EXACTLY what
    // dropping a lone point microphone does
    int micIdx = -1;
    for (size_t i = 0; i < Mics().size(); ++i)
        if (Mics()[i].id == L"mic") micIdx = (int)i;
    if (micIdx < 0) return;
    const double rad = ang * 3.14159265358979323846 / 180.0;
    std::vector<int> ids;
    for (int i = 0; i < 2; ++i) {
        const double sgn = (i == 0) ? -1.0 : 1.0;
        Item* m = CreateItem(micIdx, 1, true,
                             x + sgn * spacing * 0.5 * std::cos(rad),
                             y + sgn * spacing * 0.5 * std::sin(rad));
        if (!m) continue;
        m->angle = NormAngle(ang + sgn * half);
        m->pattern = (tech == L"blumlein") ? 3 : ((tech == L"ortf" || tech == L"nos") ? 4 : 1);
        m->label = name + (i == 0 ? L" L" : L" R");
        ids.push_back(m->id);
    }
    if (ids.size() == 2) CreateMicGroup(ids, 2, tech);   // the 绑定 call
    RefreshAfterProjectChange();
}

void App::SplitTechnique() {
    if (RefuseLocked()) return;
    Item* sel = pr_.Selected();
    if (!sel) return;
    const std::wstring tech = sel->defId;
    // an intensity-difference technique is ONE object by design (XY / MS / Blumlein)
    if (IsIntensityTech(tech)) return;
    const double sra = ItemSRA(tech);
    if (sra <= 0.0) return;

    const int symId = sel->id;
    const double x = sel->x, y = sel->y, ang = sel->angle;

    double spacing = 0.17, half = 55.0;
    if (tech == L"nos") { spacing = 0.30; half = 45.0; }
    else if (tech == L"din") { spacing = 0.20; half = 45.0; }
    else if (tech == L"xy") { spacing = 0.12; half = 65.0; }
    else if (tech == L"ms") { spacing = 0.12; half = 0.0; }
    else if (tech == L"blumlein") { spacing = 0.12; half = 45.0; }

    const CatItem* cat = FindItem(L"mic");
    const double mw = cat ? cat->w * CatScale(L"mic") : 0.9;
    const double mh = cat ? cat->h * CatScale(L"mic") : 0.9;
    const CatItem* techCat = FindItem(tech);
    const std::wstring name = techCat ? ItemName(*techCat) : tech;

    PushUndoSnapshot();   // splitting a technique symbol into a pair is undoable
    pr_.items.erase(std::remove_if(pr_.items.begin(), pr_.items.end(),
                                   [&](const Item& q) { return q.id == symId; }),
                    pr_.items.end());

    MicGroup g;
    g.kind = 2;
    g.tech = tech;
    const double rad = ang * 3.14159265358979323846 / 180.0;
    for (int i = 0; i < 2; ++i) {
        const double sgn = (i == 0) ? -1.0 : 1.0;
        Item m;
        m.id = pr_.nextId++;
        m.kind = ItemKind::Mic;
        m.defId = L"mic";
        m.w = mw;
        m.h = mh;
        m.x = x + sgn * spacing * 0.5 * std::cos(rad);
        m.y = y + sgn * spacing * 0.5 * std::sin(rad);
        m.angle = NormAngle(ang + sgn * half);
        m.height = 1.50;
        m.pattern = (tech == L"blumlein") ? 3 : ((tech == L"ortf" || tech == L"nos") ? 4 : 1);
        m.label = name + (i == 0 ? L" L" : L" R");
        pr_.items.push_back(m);
        g.ids.push_back(m.id);
    }
    pr_.micGroups.push_back(g);
    pr_.selIds = g.ids;
    pr_.selected = g.ids.back();
    RefreshAfterProjectChange();
}

double App::MainMicSRA() const {
    double sum = 0.0;
    int n = 0;
    for (int id : pr_.mainMics) {
        const Item* it = nullptr;
        for (const auto& q : pr_.items)
            if (q.id == id) { it = &q; break; }
        if (!it) continue;
        double s = 0.0;
        if (it->defId == L"mainmic") {
            // SRA from the stereo-localisation model (Sengpiel / 李伟《立体声拾音技术》:
            // level difference (l2p) and time-of-arrival difference (t2p) each give a
            // localization percentage; the recording angle is where they reach 100 %
            auto l2p = [](double dL) {
                return 1.729350e-4 * std::pow(dL, 4) - 4.932668e-3 * std::pow(dL, 3) -
                       0.148525 * std::pow(dL, 2) + 8.818633 * dL;
            };
            auto t2p = [](double dt) {
                return 21.090084 * std::pow(dt, 4) - 61.293151 * std::pow(dt, 3) +
                       17.099029 * std::pow(dt, 2) + 107.748680 * dt;
            };
            auto sraOf = [&](double spacing, double alphaDeg, double g) {
                const double k = 1.0 - g;
                const double c = 0.343;                 // m per ms
                const double alpha = alphaDeg * 3.14159265358979323846 / 180.0;
                // θ is the half angle of the source; a source beyond ±90° is behind
                // the pair, so a pair that never reaches 100 % inside that range
                // covers the whole front → 180°
                for (double th = 0.0; th <= 1.5707963267948966; th += 0.0005) {
                    double l1 = g + k * std::cos(alpha * 0.5 + th);
                    double l2 = g + k * std::cos(alpha * 0.5 - th);
                    if (l1 < 0.01) l1 = 0.01;
                    if (l2 < 0.01) l2 = 0.01;
                    const double dL = std::fabs(20.0 * std::log10(l1 / l2));
                    const double dt = spacing * std::sin(th) / c;
                    if (l2p(dL) + t2p(dt) >= 100.0)
                        return 2.0 * th * 180.0 / 3.14159265358979323846;
                }
                return 180.0;
            };
            // capsule pattern → pressure (g) / pressure-gradient (k) split
            double g = 0.5;
            switch (it->pattern) {
            case 0: g = 1.00; break;   // omni
            case 1: g = 0.50; break;   // cardioid
            case 2: g = 0.37; break;   // supercardioid
            case 3: g = 0.00; break;   // figure-8
            default: g = 0.50; break;
            }
            double spacing = it->spacing;
            double alpha = 90.0;
            bool fromTable = false;
            switch (it->mainTech) {
            case 0: alpha = 0.0; break;                       // AB: pure time difference
            case 1:                                            // 混合差 (ORTF / NOS / DIN)
                if (spacing >= 0.27) alpha = 90.0;             // NOS 90° / 30 cm
                else if (spacing >= 0.185) alpha = 90.0;       // DIN 90° / 20 cm
                else alpha = 110.0;                            // ORTF 110° / 17 cm
                break;
            case 2:                                            // 强度差  book table
                s = (it->pattern == 0) ? 180.0
                  : (it->pattern == 1) ? 130.0                  // cardioid
                  : (it->pattern == 2) ? 100.0                  // supercardioid
                                        : 76.0;                 // figure-8
                fromTable = true;
                break;
            default: alpha = 0.0;                             // Decca tree
                spacing = (std::max)(0.30, it->spacing);
                break;
            }
            if (!fromTable) s = sraOf(spacing, alpha, g);
        } else {
            s = ItemSRA(it->defId);
        }        if (s > 0.0) {
            sum += s;
            ++n;
        }
    }
    return n > 0 ? sum / n : 0.0;
}

// ---------------------------------------------------------------------------
// two-level 对应乐器 picker
//
// Level 0 (top):
//   1. 「当前：…」 the microphone's existing binding (the current state);
//   2. 侧展;
//   3. the big categories 弦乐 / 木管 / 铜管 / 打击乐 / 键盘乐器 / 竖琴 / 声乐 —
//      hovering one opens its instruments in a second panel;
//   4. 环境 last.
// Level 1: the section row 「弦乐组」 (bind the whole section) followed by its
// instruments.
// 竖琴 is its own row, never inside 弦乐 (the user asked for it explicitly).
// ---------------------------------------------------------------------------
std::vector<App::SpotRow> App::SpotTopRows(int micId) const {
    std::vector<SpotRow> rows;
    const Item* mic = pr_.Find(micId);
    const std::wstring cur = mic ? mic->spotType : std::wstring();
    SpotRow c;
    c.kind = 0;
    c.id = cur;
    c.label = std::wstring(L10n::IsZh() ? L"当前：" : L"Current: ") +
              (cur.empty() ? std::wstring(L10n::IsZh() ? L"未绑定" : L"unbound")
                           : SpotTypeName(cur));
    if (!cur.empty()) c.right = L10n::IsZh() ? L"解除" : L"clear";
    rows.push_back(c);

    // 环绕声道 / 天空声道: the two special sections, bound exactly like 侧展 (a plain
    // spotType string, no stage object) and listed right after the current binding
    for (const wchar_t* id : {L"section_surround", L"section_height", L"section_side"}) {
        SpotRow sp;
        sp.kind = 2;
        sp.id = id;
        sp.label = SpotTypeName(id);
        rows.push_back(sp);
    }

    for (int g = 0; g < SpotGroupCount(); ++g) {
        SpotRow gr;
        gr.kind = 3;
        gr.id = SpotGroupKey(g);
        // the row reads the SECTION TARGET name: 弦乐组 / 木管组 / 铜管组 / 打击乐组
        gr.label = SpotTypeName(SpotGroupTarget(g));
        gr.right = L"▸";
        rows.push_back(gr);
    }
    // ---- FLAT entries: every catalogue instrument that is NOT inside one of the
    // four submenus (三角钢琴 / 大键琴 / 钢片琴 / 立式钢琴 / 竖琴 / 打击乐 / 指挥 /
    // 独唱 / 合唱 / 自定义 …), in catalogue order; 环境 stays LAST -------------
    for (const auto& c : Instruments()) {
        bool inGroup = false;
        for (int g = 0; g < SpotGroupCount() && !inGroup; ++g) {
            const std::vector<std::wstring> items = SpotGroupItems(g);
            if (std::find(items.begin(), items.end(), std::wstring(c.id)) !=
                items.end())
                inGroup = true;
        }
        if (inGroup) continue;
        SpotRow fr;
        fr.kind = 4;
        fr.id = c.id;
        fr.label = ItemName(c);
        rows.push_back(fr);
    }

    SpotRow amb;
    amb.kind = 2;
    amb.id = L"section_ambience";
    amb.label = SpotTypeName(L"section_ambience");
    rows.push_back(amb);
    return rows;
}

std::vector<App::SpotRow> App::SpotSubRows(int group) const {
    std::vector<SpotRow> rows;
    if (group < 0 || group >= SpotGroupCount()) return rows;
    SpotRow all;
    all.kind = 2;
    all.id = SpotGroupTarget(group);
    // the whole-section row is the section's own name (「弦乐组」 …): the earlier
    // 「整个…」 phrasing is gone everywhere
    all.label = SpotTypeName(SpotGroupTarget(group));
    rows.push_back(all);
    for (const auto& id : SpotGroupItems(group)) {
        SpotRow r;
        r.kind = 4;
        r.id = id;
        r.label = SpotTypeName(id);
        rows.push_back(r);
    }
    return rows;
}

int App::SpotGroupIndexOfKey(const std::wstring& key) const {
    for (int g = 0; g < SpotGroupCount(); ++g)
        if (key == SpotGroupKey(g)) return g;
    return -1;
}

void App::ApplySpotRow(const SpotRow& row, int micId) {
    const Item* mic = pr_.Find(micId);
    if (!mic) return;
    const std::vector<int> targets = SpotTargetsOf(micId);
    if (row.kind == 0) {
        // the 「当前」 row offers 「解除」 only when something is bound
        if (!mic->spotType.empty()) SetSpotType(targets, std::wstring());
        return;
    }
    if (row.kind == 3) return;   // a category row only opens its submenu
    SetSpotType(targets, row.id);
}
std::vector<std::wstring> App::SpotTypeList() const {
    std::vector<std::wstring> out;
    for (const auto& q : pr_.items) {
        if (q.kind != ItemKind::Instrument) continue;
        if (std::find(out.begin(), out.end(), q.defId) == out.end()) out.push_back(q.defId);
    }
    if (out.empty())
        for (const auto& c : Instruments()) out.push_back(c.id);
    // the two EXTRA targets (侧展 / 环境) are not instruments and never appear on the
    // stage: they always close the list, so a mic or a whole group can be bound to
    // either of them like any other instrument type
    for (const auto& c : SpotExtras()) out.push_back(c.id);
    return out;
}

Item* App::SpotMicInSel() {
    // a microphone can take a 对应乐器 binding: the main mic has its own fields and
    // a technique symbol is not a point mic
    auto bindable = [](const Item* q) {
        return q && q->kind == ItemKind::Mic && q->defId != L"mainmic" &&
               (q->defId == L"mic" || q->defId == L"lavalier" ||
                IsIntensityTech(q->defId));
    };
    Item* it = pr_.Selected();
    if (bindable(it)) return it;   // a bound GROUP binds as ONE unit, see SpotTargetsOf
    // The instrument may have been clicked LAST (instrument + microphone is the
    // 点话筒(multi-selection): the binding still applies to the microphone of that
    // selection, so the order of the two clicks no longer matters.
    if (it && it->kind == ItemKind::Instrument) {
        for (auto id = pr_.selIds.rbegin(); id != pr_.selIds.rend(); ++id)
            if (Item* q = pr_.Find(*id); bindable(q)) return q;
    }
    return nullptr;
}

// Every member a spot-mic binding applies to: all members of the bound group (a
// stereo pair / Decca tree / technique pair binds as one unit) or the mic itself.
std::vector<int> App::SpotTargetsOf(int micId) const {
    std::vector<int> out;
    const int g = pr_.GroupOf(micId);
    if (g >= 0 && pr_.micGroups[(size_t)g].ids.size() >= 2)
        out = pr_.micGroups[(size_t)g].ids;
    if (out.empty()) out.push_back(micId);
    return out;
}

// Write the 对应乐器 of a microphone GROUP: every member takes the same type, so
// the inspector, the 03 channel table and the stage colouring can never disagree
// about a bound pair.  `inst` empty clears the binding.
void App::SetSpotType(const std::vector<int>& ids, const std::wstring& inst) {
    if (RefuseLocked()) return;
    int n = 0;
    for (int id : ids) {
        if (Item* m = pr_.Find(id)) {
            m->spotType = inst;
            ++n;
        }
    }
    if (n <= 0) return;
    if (inst.empty()) {
        StatusMsg(dr::Fmt(L10n::T(S::SpotUnboundFmt), n));
    } else {
        const std::wstring nm = SpotTypeName(inst);
        StatusMsg(dr::Fmt(L10n::T(S::SpotBoundFmt), n, nm.c_str()));
    }
    SyncEdits();
    InvalidateAll();
}

std::wstring App::SelectedInstType() const {
    for (auto id = pr_.selIds.rbegin(); id != pr_.selIds.rend(); ++id) {
        for (const auto& q : pr_.items) {
            if (q.id != *id) continue;
            if (q.kind == ItemKind::Instrument) return q.defId;
            break;
        }
    }
    return std::wstring();
}

void App::ParseEdit(int id) {
    if (syncing_) return;
    // a locked app is read-only: every field commit (project info, stage size,
    // ceiling, the selected item's X/Y/angle/height/spacing, the main mic's
    // pattern, a device's name / channels / position, the 03 cell editor) is
    // refused HERE, before any value can reach the model
    if (RefuseLocked()) return;
    if (!ePiece_) return;
    Item* it = pr_.Selected();
    double v = 0.0;
    auto readNum = [&](HWND h) -> bool {
        std::wstring s = ControlText(h);
        if (s.empty()) return false;
        wchar_t* end = nullptr;
        double d = std::wcstod(s.c_str(), &end);
        if (end == s.c_str()) return false;
        v = d;
        return true;
    };

    // live typing is never clamped: while the user is still typing we accept
    // in-range values only and leave the text alone; clamping and the write-back
    // happen on commit (Enter or focus loss), so numbers can be typed digit by
    // digit without snapping to a limit
    // numeric fields are applied ONLY on commit (Enter or focus loss): while the
    // user is typing the model is untouched, so nothing can rewrite the text or
    // jump the caret mid-number
    auto num = [&](double raw, double lo, double hi, double* out) -> bool {
        if (!commitEdit_) return false;
        *out = std::clamp(raw, lo, hi);
        return true;
    };

    switch (id) {
    case ID_PIECE: pr_.piece = ControlText(ePiece_); break;
    case ID_VENUE: pr_.venue = ControlText(eVenue_); break;
    case ID_CONDUCTOR: pr_.conductor = ControlText(eConductor_); break;
    case ID_SYSENG: pr_.sysEngineer = ControlText(eSysEng_); break;
    case ID_RECENG: pr_.recEngineer = ControlText(eRecEng_); break;
    case ID_ASSTENG: pr_.asstEngineer = ControlText(eAsstEng_); break;
    case ID_NOTES: pr_.notes = ControlText(eNotes_); break;
    case ID_WIDTH: {
        double c = 0;
        if (readNum(eWidth_) && num(v, 4.0, 60.0, &c)) SetStageSize(c, pr_.stage.d);
        break;
    }
    case ID_DEPTH: {
        double c = 0;
        if (readNum(eDepth_) && num(v, 4.0, 60.0, &c)) SetStageSize(pr_.stage.w, c);
        break;
    }
    case ID_OFF: {
        double c = 0;
        if (readNum(eOff_) && num(v, 0.0, 30.0, &c)) pr_.stage.off = c;
        break;
    }
    case ID_CEILING: {
        // 层高 (indoor height): the flown-microphone model hangs from this height
        double c = 0;
        if (readNum(eCeiling_) && num(v, 3.0, 20.0, &c)) {
            pr_.ceiling = c;
            // cable lengths depend on it → re-route immediately on commit
            if (commitEdit_) { RebuildAllCablePaths(pr_, MicRadiusM()); SyncEdits(); }
        }
        break;
    }
    case ID_FLYH: {
        double c = 0;
        if (it && it->kind == ItemKind::Mic && readNum(eFlyH_) &&
            num(v, 0.30, pr_.ceiling, &c)) {
            it->flyHeight = c;
            if (commitEdit_) { RebuildAllCablePaths(pr_, MicRadiusM()); SyncEdits(); }
        }
        break;
    }
    case ID_SEARCH:
        ui_.search = ControlText(eSearch_);
        ui_.scroll = 0;
        break;
    case ID_SELNAME:
        if (it) it->label = ControlText(eSelName_);
        break;
    case ID_X: {
        double c = 0;
        if (it && readNum(eX_) && num(v, -200.0, 200.0, &c)) it->x = c;
        break;
    }
    case ID_Y: {
        double c = 0;
        if (it && readNum(eY_) && num(v, -200.0, 200.0, &c)) it->y = c;
        break;
    }
    case ID_ANGLE:
        if (it && readNum(eAngle_)) it->angle = NormAngle(v);
        break;
    case ID_MICHEIGHT: {
        double c = 0;
        if (it && it->kind == ItemKind::Mic && readNum(eMicH_) &&
            num(v, 0.0, 12.0, &c))
            it->height = c;
        break;
    }
    case ID_MAINSPACE: {
        double c = 0;
        if (it && it->defId == L"mainmic" && it->mainTech != 2 && readNum(eSpace_) &&
            num(v, 0.10, 3.00, &c)) {
            it->spacing = c;
            it->w = c;
            if (commitEdit_) {
                SyncEdits();
                RebuildAllCablePaths(pr_, MicRadiusM());
            }
        }
        break;
    }
    case ID_MAINCENTER: {
        double c = 0;
        if (it && it->defId == L"mainmic" && readNum(eCenter_) &&
            num(v, 0.0, 3.00, &c)) {
            it->center = c;
            if (commitEdit_) {
                SyncEdits();
                RebuildAllCablePaths(pr_, MicRadiusM());
            }
        }
        break;
    }
    // the three capsule model fields are independent: each edit owns exactly one
    // field and never touches the other two
    case ID_MODELL:
        if (it && (it->defId == L"mainmic" || IsIntensityTech(it->defId)))
            it->modelL = ControlText(eModelL_);
        break;
    case ID_MODELR:
        if (it && (it->defId == L"mainmic" || IsIntensityTech(it->defId)))
            it->modelR = ControlText(eModelR_);
        break;
    case ID_MODELC:
        if (it && it->defId == L"mainmic") it->modelC = ControlText(eModelC_);
        break;
    case ID_MICMODEL:
        if (it && it->kind == ItemKind::Mic && it->defId != L"mainmic" &&
            !IsIntensityTech(it->defId))
            it->model = ControlText(eMicModel_);
        break;
    case ID_PIANOLEN:
        if (it && it->defId == L"grand_piano" && readNum(ePianoLen_)) {
            const double len = std::clamp(v, 0.5, 8.0);
            const double ratio = (it->w > 0.01 && it->h > 0.01) ? it->h / it->w : 1.0;
            if (it->w >= it->h) {
                it->w = len;
                it->h = len * ratio;
            } else {
                it->h = len;
                it->w = ratio > 0.01 ? len / ratio : len;
            }
        }
        break;
    case ID_CHCELL:
        if (chEditItem_ >= 0) {
            if (Item* m = pr_.Find(chEditItem_)) {
                const std::wstring s = ControlText(eChCell_);
                if (chEditCol_ == 5) {
                    // 长度 manual override: empty or 0 returns to the automatic
                    // (standard-size rounded) value; anything else is used verbatim
                    double v2 = 0.0;
                    bool have = false;
                    {
                        std::wstring t = s;
                        while (!t.empty() && (t.front() == L' ' || t.front() == L'\t'))
                            t.erase(t.begin());
                        if (!t.empty()) {
                            wchar_t* end = nullptr;
                            const double d2 = std::wcstod(t.c_str(), &end);
                            if (end != t.c_str()) {
                                v2 = d2;
                                have = true;
                            }
                        }
                    }
                    m->cableLenOverride = (have && v2 > 0.0)
                                              ? std::clamp(std::round(v2 * 10.0) / 10.0,
                                                           0.5, 500.0)
                                              : 0.0;
                } else if (chEditCol_ == 3) {
                    m->remark = s;
                } else if (chEditCapsule_ == 0) {
                    m->modelL = s;
                } else if (chEditCapsule_ == 1) {
                    m->modelC = s;
                } else if (chEditCapsule_ == 2) {
                    m->modelR = s;
                } else {
                    m->model = s;
                }
            }
        }
        break;
    // ---- layer 03 device properties: 名称 / 输入通道数 / X / Y ---------------
    case ID_DEVNAME:
        if (Device* dv = pr_.SelectedDevice()) dv->label = ControlText(eDevName_);
        break;
    case ID_DEVCH:
        if (Device* dv = pr_.SelectedDevice(); dv && readNum(eDevCh_)) {
            const int nv = (int)std::lround(v);
            const int cl = std::clamp(nv, 1, 64);
            dv->channels = cl;
            if (cl != nv) SyncEdits();   // show the clamped value
            RebuildAllCablePaths(pr_, MicRadiusM());   // capacity warnings move
        }
        break;
    case ID_DEVX:
    case ID_DEVY: {
        Device* dv = pr_.SelectedDevice();
        if (!dv) break;
        if (dv->locked) {   // locked beside the sound card: put the value back
            SyncEdits();
            break;
        }
        const bool isX = (id == ID_DEVX);
        if (readNum(isX ? eDevX_ : eDevY_)) {
            double& vv = isX ? dv->x : dv->y;
            const double cl = std::clamp(v, -200.0, 200.0);
            vv = cl;
            if (cl != v) SyncEdits();
            RebuildAllCablePaths(pr_, MicRadiusM());
        }
    } break;
    default:
        break;
    }
    if (id == ID_WIDTH || id == ID_DEPTH) UpdateStageXf();
    RefreshAfterProjectChange();
}

// ---------------------------------------------------------------------------
// pointer / keyboard
// ---------------------------------------------------------------------------
bool App::PointToWorld(int x, int y, double& wx, double& wy) const {
    if (ppm_ <= 0.0) return false;
    wx = ((double)x - ox_) / ppm_;
    wy = ((double)y - oy_) / ppm_;
    return true;
}

// ---------------------------------------------------------------------------
// canvas panning
//
// Left-drag on empty canvas space, middle-drag anywhere on the canvas, or
// Space + left-drag move the view.  Nothing is deselected and no object moves
// while a pan is in progress: a press that never moves falls back to the old
// click behaviour (deselect) when PanEnd runs.
// ---------------------------------------------------------------------------
void App::PanBegin(int x, int y, bool deselectOnClick) {
    panning_ = true;
    panMoved_ = false;
    panDeselect_ = deselectOnClick;
    panStart_ = POINT{x, y};
    panStartX_ = ui_.panX;
    panStartY_ = ui_.panY;
    SetCapture(hwnd_);
}

void App::PanTo(int x, int y) {
    if (!panning_) return;
    const int dx = x - panStart_.x;
    const int dy = y - panStart_.y;
    if (!panMoved_ && (std::abs(dx) > 3 || std::abs(dy) > 3)) {
        panMoved_ = true;
        // 手型 cursor + a status hint while the view is being dragged
        StatusMsg(L10n::IsZh()
                      ? L"平移视图：拖动中（拖动空白处 / 空格 + 拖动 / 中键拖动）"
                      : L"Panning the view (drag empty canvas / Space+drag / middle-drag)");
    }
    if (!panMoved_) return;
    ui_.panX = panStartX_ + dx;
    ui_.panY = panStartY_ + dy;
    UpdateStageXf();
    InvalidateAll();
}

void App::PanEnd() {
    if (!panning_) return;
    const bool moved = panMoved_;
    const bool deselect = panDeselect_;
    panning_ = false;
    panMoved_ = false;
    panDeselect_ = false;
    if (GetCapture() == hwnd_) ReleaseCapture();
    if (moved) {
        InvalidateAll();
        return;
    }
    if (deselect) {   // a plain click on empty canvas behaves as before
        pr_.Deselect();
        pr_.ClearDeviceSelection();
        ui_.pendingKind = 0;
        ui_.pendingId = 0;
        SyncEdits();
        Layout();
        InvalidateAll();
    }
}

void App::DragTo(int x, int y) {
    if (!drag_) return;
    // a mixed microphone / instrument selection never moves: the press already
    // reported it (once) and no undo snapshot may be recorded for a no-op gesture
    if (SelectionMixedKinds()) return;
    Item* it = pr_.Find(dragId_);
    if (!it) return;
    double wx = 0, wy = 0;
    if (!PointToWorld(x, y, wx, wy)) return;

    const double step = 0.05;
    double nx = std::round((wx - dragDx_) / step) * step;
    double ny = std::round((wy - dragDy_) / step) * step;
    nx = std::clamp(nx, -200.0, 200.0);
    ny = std::clamp(ny, -200.0, 200.0);
    const double dx = nx - it->x;
    const double dy = ny - it->y;
    if (std::fabs(dx) < 1e-9 && std::fabs(dy) < 1e-9) return;

    std::vector<int> ids;
    // the clicked item is the ANCHOR: when it belongs to the selection the whole
    // selection travels by the same delta (MouseDown already collapsed a click on
    // an unselected item to that item alone)
    if (pr_.IsSel(it->id)) ids = SelectedMoveIds();
    if (ids.empty()) {
        const int g = pr_.GroupOf(it->id);
        if (g >= 0) ids = pr_.micGroups[(size_t)g].ids;
        else ids.push_back(it->id);
    }
    GestureUndo();   // one snapshot for the whole drag, never one per mouse move
    for (int id : ids) {
        if (Item* m = pr_.Find(id)) {
            m->x = std::clamp(m->x + dx, -200.0, 200.0);
            m->y = std::clamp(m->y + dy, -200.0, 200.0);
        }
    }
    dragMoved_ = true;
    // the cables follow the microphones live (cheap, no global search)
    RebuildAllCablePaths(pr_, MicRadiusM());
    SyncXYEdits();
    InvalidateAll();
}

void App::MouseUp(int x, int y) {
    if (exitOpen_) return;   // the close prompt never starts a drag

    // ---- channel-table row reorder ----------------------------------------
    if (ui_.chDragRow >= 0) {
        const int from = ui_.chDragRow;
        const int to = ui_.chDropRow;
        const bool active = ui_.chDragActive;
        ui_.chDragRow = -1;
        ui_.chDragActive = false;
        ui_.chDropRow = -1;
        if (GetCapture() == hwnd_) ReleaseCapture();
        if (active && to >= 0) ReorderChannelRow(from, to);
        Layout();
        InvalidateAll();
        return;
    }

    // ---- rubber-band multi-select -----------------------------------------
    if (band_) {
        BandEnd(x, y);
        return;
    }

    // ---- canvas panning ---------------------------------------------------
    if (panning_) {
        PanEnd();
        return;
    }

    // ---- palette drag & drop ---------------------------------------------
    if (ui_.dragCat_ >= 0) {
        const int cat = ui_.dragCat_;
        const int tab = ui_.dragTab_;
        const bool wasDragging = ui_.paletteDragging_;
        const POINT p = {x, y};
        // reset the state first: the WM_CAPTURECHANGED sent by ReleaseCapture
        // must not see an armed drag
        ui_.dragCat_ = -1;
        ui_.dragTab_ = -1;
        ui_.paletteDragging_ = false;
        ui_.dragStart_ = POINT{};
        ui_.dragPt_ = p;
        if (GetCapture() == hwnd_) ReleaseCapture();

        if (!wasDragging) {
            PaletteTileClick(cat);   // plain click: highlight + popup, no item
            return;
        }
        // only drop where the layer can accept the kind: instruments live on the
        // stage plot (layer 0), mics on the mic plot (layer 1); layer 3 cancels
        const bool mic = (tab == 1);
        const bool kindOk = mic ? (ui_.layer == 1) : (ui_.layer == 0);
        if (kindOk && PtInRect(&L_.canvas, p)) {
            double wx = 0, wy = 0;
            if (PointToWorld(x, y, wx, wy)) {
                AddItemAt(cat, wx, wy);
                return;
            }
        }
        // cancelled (wrong layer / outside the stage): add nothing
        SyncEdits();
        Layout();
        InvalidateAll();
        return;
    }


    // ---- device drag (layer 03) ------------------------------------------
    if (devDrag_) {
        devDrag_ = false;
        devDragId_ = -1;
        devDragMoved_ = false;
        if (GetCapture() == hwnd_) ReleaseCapture();
        Layout();
        InvalidateAll();
        return;
    }

    const bool wasDragging = drag_;
    drag_ = false;
    dragId_ = -1;
    dragMoved_ = false;
    rotating_ = false;
    rotId_ = -1;
    rotBase_ = 0;
    rotApplied_ = 0;
    if (GetCapture() == hwnd_) ReleaseCapture();
    if (wasDragging) {
        Layout();
        InvalidateAll();
    }
}

void App::MouseMove(int x, int y) {
    POINT p = {x, y};
    ui_.dragPt_ = p;   // the pending-cable hint follows the cursor
    TRACKMOUSEEVENT tme = {};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd_;
    TrackMouseEvent(&tme);

    // the unsaved-changes prompt owns the hover state while it is open
    if (exitOpen_) {
        int h = 0;
        if (PtInRect(&L_.xpSave, p)) h = 1;
        else if (PtInRect(&L_.xpDiscard, p)) h = 2;
        else if (PtInRect(&L_.xpCancel, p)) h = 3;
        if (exitHover_ != h) {
            exitHover_ = h;
            InvalidateAll();
        }
        return;
    }

    if (panning_) {   // panning wins over every hover / drag state
        PanTo(x, y);
        return;
    }

    // a live rubber band owns the drag: nothing moves, nothing is selected yet
    if (band_) {
        BandTo(x, y);
        return;
    }

    // the delete-route modal owns the hover state while it is open
    if (ui_.confirmLink >= 0) {
        int h = 0;
        if (PtInRect(&L_.cfOk, p)) h = 1;
        else if (PtInRect(&L_.cfCancel, p)) h = 2;
        if (ui_.confirmHover != h) {
            ui_.confirmHover = h;
            InvalidateAll();
        }
        return;
    }    if (ui_.bindConfirmOpen) {
        int h = 0;
        if (PtInRect(&L_.bgOk, p)) h = 1;
        else if (PtInRect(&L_.bgCancel, p)) h = 2;
        if (ui_.bindHover != h) {
            ui_.bindHover = h;
            InvalidateAll();
        }
        return;
    }

    // 预设 modal: hover one row (or 取消); everything else is frozen
    if (ui_.presetOpen) {
        int h = -1;
        for (size_t i = 0; i < L_.pfRow.size(); ++i)
            if (PtInRect(&L_.pfRow[i], p)) h = (int)i;
        if (PtInRect(&L_.pfCancel, p)) h = PresetCount();
        if (ui_.presetHover != h) {
            ui_.presetHover = h;
            InvalidateAll();
        }
        return;
    }

    // ---- palette drag & drop ---------------------------------------------
    if (ui_.dragCat_ >= 0) {
        RECT cr;
        GetClientRect(hwnd_, &cr);
        if (!PtInRect(&cr, p)) {
            CancelPaletteDrag();   // left the window: no drop
            return;
        }
        ui_.dragPt_ = p;
        if (!ui_.paletteDragging_) {
            const int dx = std::abs(x - ui_.dragStart_.x);
            const int dy = std::abs(y - ui_.dragStart_.y);
            if (dx > 4 || dy > 4) ui_.paletteDragging_ = true;
        }
        if (ui_.paletteDragging_) {
            InvalidateAll();   // repaint the ghost under the cursor
            return;
        }
    }

    // ---- channel-table row drag (reorder) ---------------------------------
    if (ui_.chDragRow >= 0) {
        if (!ui_.chDragActive) {
            const int dx = std::abs(x - ui_.chDragStart.x);
            const int dy = std::abs(y - ui_.chDragStart.y);
            if (dx <= 4 && dy <= 4) return;
            ui_.chDragActive = true;
        }
        const ChTableGeom gt = ChannelTableGeom();
        const std::vector<ChRow> rows = BuildChannelRows();
        int idx = (gt.rowH > 0 && y >= gt.rowsY) ? (y - gt.rowsY) / gt.rowH : 0;
        idx = std::clamp(idx, 0, (std::max)(0, (int)rows.size() - 1));
        if (ui_.chDropRow != idx) {
            ui_.chDropRow = idx;
            InvalidateAll();
        }
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        return;
    }


    if (devDrag_) {
        if (!devDragMoved_ && (std::abs(x - downX_) > 3 || std::abs(y - downY_) > 3))
            devDragMoved_ = true;
        if (devDragMoved_) {
            DeviceDragTo(x, y);
            return;
        }
    }
    if (rotating_) {
        RotateTo(x, y);
        return;
    }
    if (drag_) {
        if (!dragMoved_ && (std::abs(x - downX_) > 3 || std::abs(y - downY_) > 3))
            dragMoved_ = true;
        if (dragMoved_) {
            DragTo(x, y);
            return;
        }
    }

    bool changed = false;
    auto set = [&](int& v, int nv) {
        if (v != nv) {
            v = nv;
            changed = true;
        }
    };

    int hl = -1;
    for (int i = 0; i < kTabCount; ++i) {
        if (!PtInRect(&L_.layerTabs[i], p)) continue;
        hl = i;
    }
    set(ui_.hoverLayer, hl);
    set(ui_.hoverLang, PtInRect(&L_.langBox, p) ? 0 : -1);
    int lrow = -1;
    if (ui_.langOpen) {
        for (int i = 0; i < 4; ++i) {
            if (L_.langRow[i].right > L_.langRow[i].left && PtInRect(&L_.langRow[i], p))
                lrow = i;
        }
    }
    set(ui_.langHoverRow, lrow);
    // 导出图片 scale chooser rows
    {
        int prow = -1;
        if (ui_.pngScaleOpen) {
            for (int i = 0; i < 3; ++i)
                if (L_.pngScaleRow[i].right > L_.pngScaleRow[i].left &&
                    PtInRect(&L_.pngScaleRow[i], p))
                    prow = i;
        }
        set(ui_.pngScaleHoverRow, prow);
    }
    // ---- app lock: the chip, the popover and the passcode modal ------------
    set(hoverLock, (L_.lockChip.right > L_.lockChip.left &&
                        PtInRect(&L_.lockChip, p)) ? 1 : 0);
    {
        int lkRow = -1;
        if (lockOpen) {
            for (int i = 0; i < 3; ++i)
                if (L_.lockRow[i].right > L_.lockRow[i].left &&
                    PtInRect(&L_.lockRow[i], p))
                    lkRow = i;
        }
        set(lockHoverRow, lkRow);
        int lkCell = -1;
        if (lockOpen && lockState_ == 2) {
            for (int i = 0; i < 4; ++i)
                if (L_.lockCells[i].right > L_.lockCells[i].left &&
                    PtInRect(&L_.lockCells[i], p))
                    lkCell = i;
        }
        set(hoverLockCell, lkCell);
        set(hoverLockOk,
            (lockOpen && lockState_ == 2 && L_.lockOk.right > L_.lockOk.left &&
             PtInRect(&L_.lockOk, p)) ? 1 : 0);
        set(lockModalHoverOk,
            (lockModal != 0 && L_.lmOk.right > L_.lmOk.left && PtInRect(&L_.lmOk, p))
                ? 1 : 0);
        set(lockModalHoverCancel,
            (lockModal != 0 && L_.lmCancel.right > L_.lmCancel.left &&
             PtInRect(&L_.lmCancel, p)) ? 1 : 0);
        int lmCell = -1;
        if (lockModal != 0) {
            for (int i = 0; i < 4; ++i)
                if (L_.lmCells[i].right > L_.lmCells[i].left &&
                    PtInRect(&L_.lmCells[i], p))
                    lmCell = i;
        }
        set(hoverLmCell, lmCell);
    }
    set(ui_.hoverFn, PtInRect(&L_.fnFile, p) ? 5
                     : (L_.fnSra.right > L_.fnSra.left && PtInRect(&L_.fnSra, p)) ? 1
                     : PtInRect(&L_.fnSimple, p) ? 2
                     : PtInRect(&L_.fnPdf, p) ? 3
                     : PtInRect(&L_.fnPreset, p) ? 6 : -1);
    {
        int fr = -1;
        if (ui_.fileOpen) {
            for (int i = 0; i < 5; ++i)
                if (L_.fileRow[i].right > L_.fileRow[i].left && PtInRect(&L_.fileRow[i], p))
                    fr = i;
        }
        set(ui_.fileHoverRow, fr);
        int cba = -1;
        for (int i = 0; i < kAddDeviceCount; ++i)
            if (L_.cbAdd[i].right > L_.cbAdd[i].left && PtInRect(&L_.cbAdd[i], p))
                cba = i;
        set(ui_.hoverCbAdd, cba);
        int cbr = -1;
        for (size_t i = 0; i < L_.cbRow.size(); ++i)
            if (PtInRect(&L_.cbRow[i], p)) cbr = (int)i;
        set(ui_.hoverCbRow, cbr);
        set(ui_.hoverCbDel,
            (L_.cbDel.right > L_.cbDel.left && PtInRect(&L_.cbDel, p)) ? 1 : 0);
        set(ui_.hoverCbDevice,
            (ui_.layer == 2 && PtInRect(&L_.canvas, p)) ? DeviceHitTest(x, y) : -1);
        set(ui_.hoverCable,
            (ui_.layer == 2 && PtInRect(&L_.canvas, p)) ? CableHitTest(x, y) : -1);
        // 05 microphone inspector: the route rows and their 「删除路由」 buttons
        int rbRow = -1, rbDel = -1;
        for (size_t k = 0; k < L_.cbRouteRow.size(); ++k) {
            if (!PtInRect(&L_.cbRouteRow[k], p)) continue;
            rbRow = (int)k;
            if (k < L_.cbRouteDel.size() && PtInRect(&L_.cbRouteDel[k], p))
                rbDel = (int)k;
        }
        set(ui_.hoverCbRoute, rbRow);
        set(ui_.hoverCbRouteDel, rbDel);
        // 05 「连接」: the device row under the cursor (click = connect that mic)
        int connRow = -1;
        for (size_t k = 0; k < L_.cbConnRow.size(); ++k)
            if (L_.cbConnRow[k].right > L_.cbConnRow[k].left &&
                PtInRect(&L_.cbConnRow[k], p))
                connRow = (int)k;
        set(ui_.hoverCbConn, connRow);
    }
    set(ui_.hoverLibTab, PtInRect(&L_.libTabs[0], p) ? 0
                        : PtInRect(&L_.libTabs[1], p) ? 1 : -1);
    int tile = -1;
    for (const auto& t : tiles_) {
        if (t.index >= 0 && PtInRect(&t.r, p)) {
            tile = t.index;
            break;
        }
    }
    set(ui_.hoverTile, tile);
    set(ui_.hoverSearch, PtInRect(&L_.searchBox, p) ? 1 : 0);
    int preset = -1;
    for (int i = 0; i < 3; ++i)
        if (PtInRect(&L_.presets[i], p)) preset = i;
    set(ui_.hoverPreset, preset);
    int color = -1;
    for (int i = 0; i < 4; ++i)
        if (PtInRect(&L_.colorRow[i], p)) color = i;
    set(ui_.hoverColor, color);
    int row = -1;
    for (int i = 0; i < 2; ++i)
        if (PtInRect(&L_.layerRow[i], p)) row = i;
    if (PtInRect(&L_.textRow[0], p)) row = 9;
    if (PtInRect(&L_.textRow[1], p)) row = 10;
    if (L_.flownRow.right > L_.flownRow.left && PtInRect(&L_.flownRow, p)) row = 11;
    // the 颜色 row (12) and its 「恢复默认」 button (13)
    if (L_.itemColorRow.right > L_.itemColorRow.left && PtInRect(&L_.itemColorRow, p))
        row = 12;
    if (L_.itemColorReset.right > L_.itemColorReset.left &&
        PtInRect(&L_.itemColorReset, p))
        row = 13;
    set(ui_.hoverRow, row);
    set(ui_.hoverDel, PtInRect(&L_.delBtn, p) ? 1
                     : (L_.spotBtn.right > L_.spotBtn.left && PtInRect(&L_.spotBtn, p) &&
                        SpotMicInSel() != nullptr) ? 2 : -1);
    int tech = -1;
    for (int i = 0; i < 4; ++i) {
        if (L_.techBtn[i].right > L_.techBtn[i].left && PtInRect(&L_.techBtn[i], p))
            tech = i;
    }
    set(ui_.hoverTech, tech);
    int pat = -1;
    for (int i = 0; i < 4; ++i) {
        if (L_.patternBtn[i].right > L_.patternBtn[i].left && PtInRect(&L_.patternBtn[i], p))
            pat = i;
    }
    set(ui_.hoverPattern, pat);
    int spot = -1;
    if (ui_.spotListOpen >= 0) {
        for (int i = 0; i < 16; ++i) {
            if (L_.spotListRow[i].right > L_.spotListRow[i].left &&
                PtInRect(&L_.spotListRow[i], p))
                spot = i;
        }
    }
    set(ui_.spotHover, spot);
    // HOVERING a category row opens its second panel (the painter puts it on the
    // side with room); hovering any OTHER row closes it again, while moving the
    // cursor OFF the rows (towards the panel) leaves it alone
    if (ui_.spotListOpen >= 0) {
        const std::vector<SpotRow> rows = SpotTopRows(ui_.spotListOpen);
        const int idx = (spot >= 0) ? ui_.spotScroll + spot : -1;
        if (idx >= 0 && idx < (int)rows.size()) {
            if (rows[(size_t)idx].kind == 3) {
                const int g = SpotGroupIndexOfKey(rows[(size_t)idx].id);
                if (g >= 0) set(ui_.spotGroup, g);
            } else {
                set(ui_.spotGroup, -1);
            }
        }
    }

    // channel-table chips and 对应乐器 picker hover
    int chChip = -1;
    int chRow = -1;
    int chAutoSort = 0;
    int chPageBtn = 0;
    if (ui_.layer == 3) {
        const ChTableGeom gt = ChannelTableGeom();
        for (int i = 0; i < kChCols; ++i)
            if (PtInRect(&gt.chip[i], p)) chChip = i;
        if (gt.autoSort.right > gt.autoSort.left && PtInRect(&gt.autoSort, p))
            chAutoSort = 1;
        if (gt.pagePrev.right > gt.pagePrev.left && PtInRect(&gt.pagePrev, p))
            chPageBtn = 1;
        if (gt.pageNext.right > gt.pageNext.left && PtInRect(&gt.pageNext, p))
            chPageBtn = 2;
        if (p.x >= gt.x0 && p.x <= gt.x1 && p.y >= gt.rowsY && gt.rowH > 0) {
            const int visIdx = (p.y - gt.rowsY) / gt.rowH;
            const int idx = ui_.chPage * (std::max)(1, gt.pageRows) + visIdx;
            if (visIdx >= 0 && visIdx < gt.visibleRows && idx >= 0) chRow = idx;
        }
    }
    set(ui_.hoverChChip, chChip);
    set(ui_.hoverChAutoSort, chAutoSort);
    set(ui_.hoverChPageBtn, chPageBtn);
    set(ui_.hoverChRow, chRow);

    int chSpot = -1;
    if (ui_.chSpotOpen >= 0) {
        for (size_t i = 0; i < L_.chPickRow.size(); ++i) {
            if (L_.chPickRow[i].right > L_.chPickRow[i].left && PtInRect(&L_.chPickRow[i], p))
                chSpot = (int)i;
        }
    }
    set(ui_.chSpotHover, chSpot);
    // 04 链路 / 路由 popup: the device row under the cursor
    int chRoute = -1;
    if (ui_.chRouteOpen >= 0) {
        for (size_t i = 0; i < L_.chRouteRow.size(); ++i)
            if (L_.chRouteRow[i].right > L_.chRouteRow[i].left &&
                PtInRect(&L_.chRouteRow[i], p))
                chRoute = (int)i;
    }
    set(ui_.chRouteHover, chRoute);
    // the same hover-opens-a-category rule for the 03 table's picker
    if (ui_.chSpotOpen >= 0) {
        const std::vector<SpotRow> rows = SpotTopRows(ui_.chSpotOpen);
        const int idx = (chSpot >= 0) ? ui_.chSpotScroll + chSpot : -1;
        if (idx >= 0 && idx < (int)rows.size()) {
            if (rows[(size_t)idx].kind == 3) {
                const int g = SpotGroupIndexOfKey(rows[(size_t)idx].id);
                if (g >= 0) set(ui_.spotGroup, g);
            } else {
                set(ui_.spotGroup, -1);
            }
        }
    }

    if (changed) InvalidateAll();
}

void App::MouseWheel(int delta, int x, int y) {
    if (exitOpen_) return;   // the close prompt freezes the page behind it
    if (delta == 0) return;
    // WM_MOUSEWHEEL carries screen coordinates
    POINT p = {x, y};
    ScreenToClient(hwnd_, &p);
    const int notches = delta > 0 ? 1 : -1;

    if (ui_.chSpotOpen >= 0) {
        const int total = (int)SpotTypeList().size() + 1;
        const int maxRows = ChannelPickerMaxRows(ui_.chSpotAnchorY + D(4));
        const int maxScroll = (std::max)(0, total - maxRows);
        ui_.chSpotScroll = std::clamp(ui_.chSpotScroll - notches, 0, maxScroll);
        InvalidateAll();
        return;
    }
    if (ui_.spotListOpen >= 0) {
        auto types = SpotTypeList();
        const int rowH = D(22);
        const int avail = (ch_ - D(30)) - L_.spotBtn.top - D(8);
        const int maxRows = (std::max)(1, (std::min)(16, avail / rowH));
        const int maxScroll = (std::max)(0, (int)types.size() - maxRows);
        ui_.spotScroll = std::clamp(ui_.spotScroll - notches, 0, maxScroll);
        InvalidateAll();
        return;
    }
    if (PtInRect(&L_.right, p)) {
        const int viewH = (L_.right.bottom - L_.right.top) - D(20);
        const int maxScroll = (std::max)(0, L_.rightContent - viewH);
        ui_.rightScroll = std::clamp(ui_.rightScroll - notches * (double)D(64), 0.0,
                                     (double)maxScroll);
        Layout();
        // paint synchronously: otherwise the child EDIT controls you already see
        // linger at their previous positions for a frame and appear to overlap
        InvalidateAll();
        UpdateWindow(hwnd_);
        return;
    }

    if (PtInRect(&L_.canvas, p)) {
        double wx = 0, wy = 0;
        PointToWorld(p.x, p.y, wx, wy);
        const double f = delta > 0 ? 1.12 : 1.0 / 1.12;
        ui_.zoom = std::clamp(ui_.zoom * f, 0.25, 5.0);
        UpdateStageXf();
        const double nx = ox_ + wx * ppm_;
        const double ny = oy_ + wy * ppm_;
        ui_.panX += (double)p.x - nx;
        ui_.panY += (double)p.y - ny;
        UpdateStageXf();
        InvalidateAll();
        return;
    }
    if (PtInRect(&L_.libView, p)) {
        ui_.scroll = (std::max)(0.0, ui_.scroll - notches * (double)D(64));
        InvalidateAll();
        return;
    }
}

void App::OnKeyDown(int vk) {
    // the unsaved-changes prompt owns the keyboard: Esc = 取消 (stay), Enter =
    // 保存 (the default button)
    if (exitOpen_) {
        if (vk == VK_ESCAPE) ExitPromptAnswer(2);
        else if (vk == VK_RETURN) ExitPromptAnswer(0);
        return;
    }
    // the app-lock UI owns the keyboard while it is open: digits, Backspace,
    // Enter = 确定, Esc = cancel
    if (lockModal != 0 || (lockOpen && lockState_ == 2)) {
        const bool wasLockOverlay = (lockModal != 0 || lockOpen);
        (void)wasLockOverlay;
        LockKey(vk);
        return;
    }
    if (lockOpen && vk == VK_ESCAPE) {
        LockCloseAll();
        InvalidateAll();
        return;
    }
    const bool ctrl = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) || scriptCtrl_;
    scriptCtrl_ = false;
    // ---- 预设 modal owns the arrows and Enter while it is open --------------
    // The list is ONE column for up to six presets and TWO columns past that
    // (see PaintPresetModal); Up / Down walk a column, Left / Right switch
    // columns, Enter loads the hovered row.  Esc closes the modal in the block
    // further down, and the arrows must never fall through to the stage nudge.
    if (ui_.presetOpen) {
        const int n = PresetCount();
        const int cols = (n > 6) ? 2 : 1;
        const int rows = (n + cols - 1) / cols;   // presets per column
        if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT) {
            int cur = (ui_.presetHover >= 0 && ui_.presetHover < n)
                          ? ui_.presetHover
                          : 0;
            int col = cur / rows, row = cur % rows;
            if (vk == VK_UP)
                row = (row + rows - 1) % rows;
            else if (vk == VK_DOWN)
                row = (row + 1) % rows;
            else if (vk == VK_LEFT)
                col = (col + cols - 1) % cols;
            else
                col = (col + 1) % cols;
            int idx = col * rows + row;
            if (idx >= n) idx = n - 1;   // the shorter second column
            ui_.presetHover = idx;
            InvalidateAll();
            return;
        }
        if (vk == VK_RETURN) {
            // no hover yet (the modal was just opened) = load the first one
            const int idx = (ui_.presetHover >= 0 && ui_.presetHover < n)
                                ? ui_.presetHover
                                : 0;
            if (n > 0) LoadPresetAt(idx);
            return;
        }
    }
    // ---- 导出图片 scale chooser owns the arrow keys while it is open --------
    if (ui_.pngScaleOpen) {
        if (vk == VK_ESCAPE) {
            ui_.pngScaleOpen = false;
            ui_.pngScaleHoverRow = -1;
            InvalidateAll();
            return;
        }
        if (vk == VK_DOWN || vk == VK_UP) {
            int cur = ui_.pngScaleHoverRow;
            if (cur < 0)
                cur = (pngScaleMul_ == 2 ? 0 : pngScaleMul_ == 8 ? 2 : 1);
            else
                cur = (vk == VK_DOWN) ? (cur + 1) % 3 : (cur + 2) % 3;
            ui_.pngScaleHoverRow = cur;
            InvalidateAll();
            return;
        }
        if (vk == VK_RETURN) {
            const int cur = (ui_.pngScaleHoverRow >= 0 && ui_.pngScaleHoverRow < 3)
                                ? ui_.pngScaleHoverRow
                                : (pngScaleMul_ == 2 ? 0 : pngScaleMul_ == 8 ? 2 : 1);
            ui_.pngScaleOpen = false;
            ui_.pngScaleHoverRow = -1;
            PickPngScale(kPngScales[cur]);
            return;
        }
    }
    // Enter commits the focused numeric field (clamp + write-back) — typing a
    // value that is temporarily out of range is never rewritten while typing
    if (vk == VK_RETURN && ui_.focusEdit) {
        const int id = ui_.focusEdit;
        if (!syncing_) {
            commitEdit_ = true;
            ParseEdit(id);
            commitEdit_ = false;
            SyncEdits();
            RefreshAfterProjectChange();
            InvalidateAll();
        }
        return;
    }
    // 03 通道表 page flipping with PageUp / PageDown (and ← / →  — the table
    // PAGES, it never scrolls; ignored while a field or the modal owns the keys
    if (ui_.layer == 3 && (vk == VK_PRIOR || vk == VK_NEXT || vk == VK_LEFT ||
                           vk == VK_RIGHT)) {
        HWND f = GetFocus();
        wchar_t cls[16] = L"";
        if (f && f != hwnd_) GetClassNameW(f, cls, 16);
        const bool editFocus = (ui_.focusEdit != 0) || _wcsicmp(cls, L"EDIT") == 0;
        if (!editFocus) {
            ChPageMove((vk == VK_LEFT || vk == VK_PRIOR) ? -1 : 1);
            return;
        }
    }    // Ctrl+Z undo / Ctrl+Y (and Ctrl+Shift+Z) redo — never while a field is being
    // edited and never under a modal (the modal owns the keyboard)
    if (ctrl && (vk == 'Z' || vk == 'Y')) {
        HWND f = GetFocus();
        wchar_t cls[16] = L"";
        if (f && f != hwnd_) GetClassNameW(f, cls, 16);
        const bool editFocus = (ui_.focusEdit != 0) || _wcsicmp(cls, L"EDIT") == 0;
        const bool modalOpen = (ui_.confirmLink >= 0) || ui_.presetOpen;
        if (!editFocus && !modalOpen) {
            if (vk == 'Y' || (GetKeyState(VK_SHIFT) & 0x8000) != 0)
                Redo();
            else
                Undo();
            return;
        }
    }
    // Ctrl+A selects every object of the CURRENT layer.  It must never fire while
    // a field is being edited (there Ctrl+A belongs to the EDIT control itself).
    if (ctrl && vk == 'A') {
        HWND f = GetFocus();
        wchar_t cls[16] = L"";
        if (f && f != hwnd_) GetClassNameW(f, cls, 16);
        const bool editFocus = (ui_.focusEdit != 0) || _wcsicmp(cls, L"EDIT") == 0;
        if (!editFocus) SelectAllInLayer();
        return;
    }
    // arrow keys nudge the current selection: 0.05 m, Shift = 0.50 m, Ctrl = 0.01 m
    if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN) {
        // a nudge is a data change: refused while the app is locked
        if (RefuseLocked()) return;
        // a selection that mixes microphones with instruments is NOT nudged: only
        // one kind is editable on this page, so nothing moves, no undo snapshot is
        // recorded and the refusal is reported once per key press
        if (ui_.layer != 2 && SelectionMixedKinds()) {
            StatusMsg(L10n::T(S::MixedKindsMove), true);
            return;
        }
        double step = 0.05;
        if (GetKeyState(VK_SHIFT) & 0x8000) step = 0.50;
        if (ctrl) step = 0.01;
        const double dx = (vk == VK_LEFT) ? -step : (vk == VK_RIGHT) ? step : 0.0;
        const double dy = (vk == VK_UP) ? -step : (vk == VK_DOWN) ? step : 0.0;
        // ONE snapshot per key press; an empty selection records nothing
        bool hasSel = false;
        if (ui_.layer == 2) {
            for (const Device& dv : pr_.devices)
                if (dv.selected) {
                    hasSel = true;
                    break;
                }
        } else {
            hasSel = !SelectedMoveIds().empty();
        }
        if (hasSel) PushUndoSnapshot();
        bool moved = false;
        if (ui_.layer == 2) {
            // every selected device moves together (a single one when just clicked)
            for (Device& dv : pr_.devices) {
                if (!dv.selected) continue;
                dv.x = std::clamp(dv.x + dx, -200.0, 200.0);
                dv.y = std::clamp(dv.y + dy, -200.0, 200.0);
                moved = true;
            }
        } else {
            // the WHOLE selection (plus bound group partners) is nudged
            for (int id : SelectedMoveIds()) {
                Item* m = pr_.Find(id);
                if (!m) continue;
                m->x = std::clamp(m->x + dx, -200.0, 200.0);
                m->y = std::clamp(m->y + dy, -200.0, 200.0);
                moved = true;
            }
        }
        if (moved) {
            RebuildAllCablePaths(pr_, MicRadiusM());
            SyncEdits();
            InvalidateAll();
        }
        return;
    }
    if (vk == VK_SPACE) {
        // Space arms a canvas pan (Space + left drag); the cursor turns into a 手型
        panSpace_ = true;
        if (!panning_) {
            POINT cp;
            GetCursorPos(&cp);
            ScreenToClient(hwnd_, &cp);
            if (PtInRect(&L_.canvas, cp)) SetCursor(LoadCursorW(nullptr, IDC_HAND));
        }
        return;
    }
    if (vk == VK_DELETE || vk == VK_BACK) {
        if (ui_.layer == 2)
            DeleteSelectedDevice();
        else
            DeleteSelected();
    } else if (ctrl && (vk == 'C')) {
        CopySelected();
    } else if (ctrl && (vk == 'V')) {
        PasteClipboard();
    } else if (vk == VK_ESCAPE) {
        // a live rubber band is cancelled FIRST: the selection it would replace
        // (or add to) is left completely untouched
        if (band_) {
            BandCancel();
            return;
        }
        // the 预设 modal closes first (it changes nothing on its own)
        if (ui_.presetOpen) {
            ClosePresetModal();
            return;
        }
        // the delete-route modal is dismissed first (取消 = keep the cable)
        // the big-group confirmation is dismissed first (取消 = no group)
        if (ui_.bindConfirmOpen) {
            CancelBindGroup();
            return;
        }
        if (ui_.confirmLink >= 0) {
            ui_.confirmLink = -1;
            ui_.confirmHover = 0;
            InvalidateAll();
            return;
        }
        CancelPaletteDrag();
        ui_.popupIndex = -1;
        ui_.popupCat = -1;
        ui_.spotListOpen = -1;
        ui_.langOpen = false;
        ui_.langHoverRow = -1;
        ui_.pngScaleOpen = false;
        ui_.pngScaleHoverRow = -1;
        ui_.chSpotOpen = -1;
        ui_.chSpotHover = -1;
        ui_.chRouteOpen = -1;          // 04 链路 / 路由 device popup
        ui_.chRouteHover = -1;
        L_.chPickRow.clear();
        CloseChannelCell();
        pr_.Deselect();
        SyncEdits();
        Layout();
        InvalidateAll();
    }
}

// ---------------------------------------------------------------------------
// Ctrl+A: select every object of the CURRENT layer.  The item selection uses the
// very same container Ctrl+click fills (Project::selIds + selected), so move,
// rotate, nudge, copy and delete already treat all of it as ONE selection; the
// cabling layer expresses a multi-selection through Device::selected (the flag
// every painter reads), so all devices simply get the flag.
// ---------------------------------------------------------------------------
// every instrument defId with >= 3 instances, in catalogue order (the 同类乐器
// section lists exactly these)
std::vector<std::pair<std::wstring, int>> App::SameTypeGroups() const {
    std::map<std::wstring, int> count;
    for (const auto& it : pr_.items)
        if (it.kind == ItemKind::Instrument) ++count[it.defId];
    // only a type that ALREADY HAS an instance selected offers 「全选 … (N)」: the
    // button EXTENDS the current selection to the whole type.  Nothing is listed
    // when the selection does not contain such a type.
    std::vector<std::pair<std::wstring, int>> out;
    for (const auto& c : Instruments()) {
        auto f = count.find(c.id);
        if (f == count.end() || f->second < 3) continue;
        // 独唱歌手 is never a "same type" group (it keeps per-instance labels;
        // see BuildTypeHulls in src/StageView.cpp), so no 全选 row is offered
        if (c.id == L"solo_singer") continue;
        bool inSel = false;
        for (const auto& it : pr_.items)
            if (it.kind == ItemKind::Instrument && it.defId == c.id &&
                pr_.IsSel(it.id)) {
                inSel = true;
                break;
            }
        if (inSel) out.push_back({c.id, f->second});
    }
    return out;
}

// 同类乐器 button: select every instance of one type — the same selection
// container Ctrl+A / the rubber band use, so drag / rotate / delete follow.
void App::SelectSameType(const std::wstring& defId) {
    if (RefuseLocked()) return;
    pr_.selIds.clear();
    for (const auto& it : pr_.items)
        if (it.kind == ItemKind::Instrument && it.defId == defId)
            pr_.selIds.push_back(it.id);
    if (pr_.selIds.empty()) return;
    pr_.selected = pr_.selIds.front();
    std::wstring nm = defId;
    if (const CatItem* c = FindItem(defId)) nm = ItemName(*c);
    StatusMsg(dr::Fmt(L10n::T(S::SelSameFmt), nm.c_str(),
                      (int)pr_.selIds.size()).c_str());
    RefreshAfterProjectChange();
}

void App::SelectAllInLayer() {
    if (ui_.layer == 2) {
        pr_.Deselect();   // items and devices never mix in one selection
        int n = 0;
        for (Device& dv : pr_.devices) {
            dv.selected = true;
            ++n;
        }
        ui_.hoverCbRow = -1;
        ui_.hoverCbDevice = -1;
        StatusMsg(dr::Fmt(L10n::T(S::SelectAllFmt), n));
        RefreshAfterProjectChange();
        return;
    }
    // 通道表 / 拓扑图 / 项目信息 own no canvas objects of their own
    if (ui_.layer != 0 && ui_.layer != 1) return;
    const ItemKind want = (ui_.layer == 0) ? ItemKind::Instrument : ItemKind::Mic;
    pr_.ClearDeviceSelection();
    pr_.selIds.clear();
    for (const Item& it : pr_.items)
        if (it.kind == want) pr_.selIds.push_back(it.id);
    pr_.selected = pr_.selIds.empty() ? -1 : pr_.selIds.back();
    StatusMsg(dr::Fmt(L10n::T(S::SelectAllFmt), (int)pr_.selIds.size()));
    RefreshAfterProjectChange();
}

// The ids a MOVE must carry: the selection itself plus the partners of every
// grouped microphone, so a bound array keeps its internal geometry even when only
// one of its members is selected (the drag behaviour the canvas always had).
std::vector<int> App::SelectedMoveIds() const {
    std::vector<int> ids = pr_.selIds;
    for (int id : pr_.selIds) {
        const int g = pr_.GroupOf(id);
        if (g < 0) continue;
        for (int m : pr_.micGroups[(size_t)g].ids) ids.push_back(m);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

// A selection that mixes the two object kinds cannot travel as ONE set.  Objects
// are always clickable, but only the kind that belongs to the open page is
// draggable: on 话筒摆位 the microphones move and the instruments are locked (and
// the other way round on 乐器摆位).  A mixed drag would therefore carry a locked
// object along with the editable one, which is exactly what the user must not get
// from a Ctrl+click — so every move / rotate / nudge path asks this first.
bool App::SelectionMixedKinds(int extraId) const {
    auto scan = [this](int id, bool& mic, bool& inst) {
        const Item* it = pr_.Find(id);
        if (!it) return;
        if (it->kind == ItemKind::Mic) mic = true;
        else inst = true;
    };
    bool anyMic = false, anyInst = false;
    for (int id : pr_.selIds) scan(id, anyMic, anyInst);
    if (extraId >= 0) scan(extraId, anyMic, anyInst);   // the item under the press
    // bound groups are microphones only, so selIds already decides the mix; the
    // group partners cannot introduce the second kind
    return anyMic && anyInst;
}

void App::ClearHover() {
    exitHover_ = -1;
    ui_.hoverLayer = -1;
    ui_.hoverLibTab = -1;
    ui_.hoverTile = -1;
    ui_.hoverPreset = -1;
    ui_.hoverColor = -1;
    ui_.hoverRow = -1;
    ui_.hoverLang = -1;
    ui_.langHoverRow = -1;
    ui_.hoverDel = -1;
    ui_.hoverFn = -1;
    ui_.hoverPattern = -1;
    ui_.hoverTech = -1;
    ui_.hoverSearch = 0;
    ui_.spotHover = -1;
    ui_.hoverChChip = -1;
    ui_.hoverChRow = -1;

    ui_.chSpotHover = -1;
    ui_.hoverCbAdd = -1;
    ui_.hoverCbRow = -1;
    ui_.hoverCbDel = 0;
    ui_.hoverCbDevice = -1;
    ui_.hoverCbConn = -1;
    ui_.hoverCable = -1;
    ui_.fileHoverRow = -1;
    // app lock hovers
    hoverLock = 0;
    lockHoverRow = -1;
    hoverLockCell = -1;
    hoverLockOk = 0;
    hoverLmCell = -1;
    lockModalHoverOk = 0;
    lockModalHoverCancel = 0;
    InvalidateAll();
}

// ---------------------------------------------------------------------------
// channel table cell editor
// ---------------------------------------------------------------------------
// Move the row block of one item to another row position and REMEMBER it: the
// project keeps the manual order (Project::channelOrder, a list of item ids), so a
// drag overrides the automatic section order and survives save / load / undo.  A
// block is every row of the dragged item (the main microphone owns one row per
// capsule, a group one row per member).
void App::ReorderChannelRow(int fromRow, int toRow) {
    if (RefuseLocked()) return;
    std::vector<ChRow> rows = BuildChannelRows();
    if (fromRow < 0 || fromRow >= (int)rows.size()) return;
    const int id = rows[(size_t)fromRow].itemId;
    int first = fromRow, last = fromRow;
    while (first > 0 && rows[(size_t)(first - 1)].itemId == id) --first;
    while (last + 1 < (int)rows.size() && rows[(size_t)(last + 1)].itemId == id) ++last;
    if (toRow >= first && toRow <= last) return;   // dropped inside its own block
    std::vector<ChRow> block(rows.begin() + first, rows.begin() + last + 1);
    rows.erase(rows.begin() + first, rows.begin() + last + 1);
    int insertAt = (toRow > last) ? toRow - (int)block.size() : toRow;
    insertAt = std::clamp(insertAt, 0, (int)rows.size());
    rows.insert(rows.begin() + insertAt, block.begin(), block.end());

    std::vector<int> order;
    for (const ChRow& r : rows)
        if (order.empty() || order.back() != r.itemId) order.push_back(r.itemId);
    if (order == pr_.channelOrder) return;
    PushUndoSnapshot();   // a manual reorder is one undoable action
    pr_.channelOrder = order;
    StatusMsg(dr::Fmt(L10n::T(S::ChOrderMoved), fromRow + 1, insertAt + 1));
    SyncEdits();
}

// 「自动排序」 forget the manual row order (the drag-reorder override) and let the
// section order rule again — 主话筒 → 侧展 → 乐器声部 → 自定义 → 未分配 → 独唱 →
// 合唱 → 环境.  Undoable, because a user who drags 20 rows wants one way back.
// flip the 03 table one page (no scrolling): the page index is clamped to the
// pages the current row count produces
void App::ChPageMove(int delta) {
    const ChTableGeom gt = ChannelTableGeom();
    const int pageRows = (std::max)(1, gt.pageRows);
    const int rows = (int)BuildChannelRows().size();
    const int pages = (std::max)(1, (rows + pageRows - 1) / pageRows);
    const int want = std::clamp(ui_.chPage + delta, 0, pages - 1);
    if (want == ui_.chPage) return;
    ui_.chPage = want;
    CloseChannelCell();
    InvalidateAll();
}
void App::ResetChannelOrder() {
    if (RefuseLocked()) return;
    if (pr_.channelOrder.empty()) {
        StatusMsg(L10n::T(S::ChAutoSorted));
        InvalidateAll();
        return;
    }
    PushUndoSnapshot();
    pr_.channelOrder.clear();
    SyncEdits();
    Layout();
    InvalidateAll();
    StatusMsg(L10n::T(S::ChAutoSorted));
}

void App::CloseChannelCell() {
    const bool wasOpen = eChCell_ && IsWindowVisible(eChCell_);
    if (eChCell_) ShowWindow(eChCell_, SW_HIDE);
    chEditItem_ = -1;
    chEditCol_ = 1;
    chEditCapsule_ = -1;
    if (wasOpen) {
        SyncEdits();
        InvalidateAll();
    }
}

void App::OpenChannelCell(int x, int y) {
    if (RefuseLocked()) return;
    CloseChannelCell();
    if (ui_.layer != 3) return;
    POINT p = {x, y};

    const ChTableGeom gt = ChannelTableGeom();
    if (x < gt.x0 || x > gt.x1) return;

    // --- column show / hide chips ------------------------------------------
    for (int i = 0; i < kChCols; ++i) {
        if (!PtInRect(&gt.chip[i], p)) continue;
        int visible = 0;
        for (int k = 0; k < kChCols; ++k)
            if (ui_.chColVis[k]) ++visible;
        if (ui_.chColVis[i] && visible <= 1) return;   // never hide the last column
        ui_.chColVis[i] = !ui_.chColVis[i];
        InvalidateAll();
        return;
    }

    // --- data rows ----------------------------------------------------------
    auto rows = BuildChannelRows();
    if (rows.empty()) return;
    if (y < gt.rowsY || y >= gt.rowsY + gt.rowH * gt.visibleRows) return;
    // the visible row maps into the CURRENT page of the table
    const int visIdx = (y - gt.rowsY) / gt.rowH;
    const int idx = ui_.chPage * (std::max)(1, gt.pageRows) + visIdx;
    if (visIdx < 0 || idx < 0 || idx >= (int)rows.size()) return;

    // clicking any row selects the underlying item, so the inspector follows
    pr_.selected = rows[idx].itemId;
    pr_.selIds.assign(1, rows[idx].itemId);

    int col = -1;
    for (int i = 0; i < kChCols; ++i) {
        if (!ui_.chColVis[i]) continue;
        if (x >= gt.cols.l[i] - D(4) && x <= gt.cols.r[i] + D(4)) col = i;
    }
    Item* it = pr_.Find(rows[idx].itemId);
    // 通道 / 长度 / 距主话筒 / 幅角 are read-only: a click there only selects the
    // row.  The 长度 cell is editable: a typed number is a manual override, empty
    // = automatic.  The 链路 / 路由 cell opens the floating DEVICE picker below —
    // never a text editor.
    if (!it || col < 0 || col == 0 || col == 6 || col == 7) {
        SyncEdits();
        Layout();
        InvalidateAll();
        return;
    }

    // --- 链路 / 路由: a floating device picker (the cell stays read-only) ------
    if (col == 4) {
        SyncEdits();
        if (!it || it->kind != ItemKind::Mic) {
            Layout();
            InvalidateAll();
            return;
        }
        ui_.chSpotOpen = -1;         // only one of the two popups can be open
        ui_.chSpotHover = -1;
        L_.chPickRow.clear();
        L_.spotSubRow.clear();
        ui_.chRouteOpen = it->id;
        ui_.chRouteAnchorX = x;
        ui_.chRouteAnchorY = y;
        ui_.chRouteHover = -1;
        Layout();
        InvalidateAll();
        return;
    }

    // --- 对应乐器: floating instrument-type picker --------------------------
    if (col == 2) {
        SyncEdits();
        Layout();
        if (it->defId == L"mainmic") {   // the main mic rows are not bindable
            InvalidateAll();
            return;
        }
        ui_.chRouteOpen = -1;            // the 路由 popup must never survive this
        ui_.chRouteHover = -1;
        ui_.chSpotOpen = it->id;
        ui_.chSpotAnchorX = x;
        ui_.chSpotAnchorY = y;
        ui_.chSpotHover = -1;
        ui_.chSpotScroll = 0;
        InvalidateAll();
        return;
    }

    // --- 型号 / 备注 / 长度: inline editor ----------------------------------
    Item* target = it;
    int capsule = rows[idx].capsule;
    std::wstring initial;
    if (col == 3) {
        capsule = -1;
        initial = target->remark;
    } else if (col == 5) {
        // the manual cable length (0 = automatic → empty cell)
        capsule = -1;
        if (target->cableLenOverride > 0.0)
            initial = dr::Fmt(L"%.1f", target->cableLenOverride);
    } else if (capsule >= 0) {
        // a main-mic capsule row edits only that capsule's own model field
        initial = (capsule == 0) ? target->modelL
                  : (capsule == 1) ? target->modelC
                                   : target->modelR;
    } else {
        initial = target->model;
    }

    if (!eChCell_) {
        eChCell_ = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_LEFT | ES_AUTOHSCROLL, 0, 0,
                                   10, 10, hwnd_, (HMENU)(INT_PTR)ID_CHCELL, hinst_, nullptr);
        SendMessageW(eChCell_, WM_SETFONT, (WPARAM)uiFont_, TRUE);
        SendMessageW(eChCell_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(D(3), D(3)));
    }
    chEditItem_ = target->id;
    chEditCol_ = col;
    chEditCapsule_ = col == 3 ? -1 : capsule;
    // the EDIT sits on the row the user CLICKED: the visible row index, not the
    // absolute one (page 2+ would otherwise position it pages below the table)
    RECT cell = {(LONG)std::lround(gt.cols.l[col]), gt.rowsY + visIdx * gt.rowH,
                 (LONG)std::lround(gt.cols.r[col] + D(4)),
                 gt.rowsY + (visIdx + 1) * gt.rowH};
    SetWindowPos(eChCell_, HWND_TOP, cell.left, cell.top, cell.right - cell.left,
                 cell.bottom - cell.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    syncing_ = true;
    SetWindowTextW(eChCell_, initial.c_str());
    syncing_ = false;
    SetFocus(eChCell_);
    SendMessageW(eChCell_, EM_SETSEL, 0, -1);
    RefreshAfterProjectChange();
}

// ---------------------------------------------------------------------------
// PDF export
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// 「导出图片…」: a REAL .png of the whole drawing of the current page (stage /
// mic plot or 布线).  The bitmap is sized to the CONTENT bounding box — every
// instrument, every microphone (however far outside the stage it sits), every
// device and every cable with its tags — and painted with the on-screen painters,
// never with the window's viewport.
// ---------------------------------------------------------------------------
static bool SavePngFile(Gdiplus::Bitmap& bmp, const std::wstring& path) {
    CLSID pngClsid{};
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    std::vector<BYTE> buf(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    if (Gdiplus::GetImageEncoders(num, size, codecs) != Gdiplus::Ok) return false;
    bool found = false;
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(codecs[i].MimeType, L"image/png") == 0) {
            pngClsid = codecs[i].Clsid;
            found = true;
            break;
        }
    }
    if (!found) return false;
    // write to a temp file first, verify it decodes, then replace — a failed or
    // interrupted save must never leave a half-written (un-openable) .png behind
    const std::wstring tmp = path + L".part";
    if (bmp.Save(tmp.c_str(), &pngClsid, nullptr) != Gdiplus::Ok) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    bool ok = false;
    {
        Gdiplus::Bitmap chk(tmp.c_str(), FALSE);
        ok = (chk.GetLastStatus() == Gdiplus::Ok && chk.GetWidth() == bmp.GetWidth() &&
              chk.GetHeight() == bmp.GetHeight());
    }
    if (!ok) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        // fall back to a plain copy when the target is locked by a viewer
        if (!CopyFileW(tmp.c_str(), path.c_str(), FALSE)) {
            DeleteFileW(tmp.c_str());
            return false;
        }
        DeleteFileW(tmp.c_str());
    }
    return true;
}

// the blank rim the PNG export keeps around the content bounding box, and the
// gap between the header band and the drawing underneath it
static constexpr int kPngPadPx = 20;
// the published PNG header is 8× the old header (see PngHeaderLines): every row
// and this gap are multiplied by 8, so the band scales with the text
static constexpr int kPngHeaderGapPx = 16 * 8;   // 128 design px (D() scales it)

// world bbox of everything the current page draws (shared by the size preview and
// the exporter) — implemented as a helper right above ExportPngTo
static void PngWorldBox(const App& app, const Project& pr, double ppm, bool wiring,
                        double& wl, double& wt, double& wr, double& wb) {
    wl = 0.0;
    wt = 0.0;
    wr = pr.stage.w;
    wb = pr.stage.d;
    auto grow = [&](double cx, double cy, double w, double h, double ang) {
        const double rad = ang * 3.14159265358979323846 / 180.0;
        const double ca = std::fabs(std::cos(rad)), sa = std::fabs(std::sin(rad));
        const double ex = (w * ca + h * sa) * 0.5;
        const double ey = (w * sa + h * ca) * 0.5;
        wl = (std::min)(wl, cx - ex);
        wr = (std::max)(wr, cx + ex);
        wt = (std::min)(wt, cy - ey);
        wb = (std::max)(wb, cy + ey);
    };
    for (const auto& it : pr.items) {
        if (it.kind == ItemKind::Mic) {
            // the drawn marker radius in metres (same formula the painters use)
            const double r = (std::max)(5.0 / (ppm > 1e-6 ? ppm : 1.0), 0.066) *
                                 MicMarkerScaleOf(pr, it.id) + 0.20;
            grow(it.x, it.y, 2 * r, 2 * r, 0.0);
        } else {
            grow(it.x, it.y, (std::max)(0.1, it.w), (std::max)(0.1, it.h), it.angle);
        }
    }
    // devices and cables are drawn on the 布线 page only, so they may only grow
    // the frame when that page is being exported
    if (wiring) {
        for (const auto& dv : pr.devices)
            grow(dv.x, dv.y, kCableDevW * 2.0, kCableDevH * 2.0, dv.angle);
        for (const auto& ln : pr.links) {
            auto eat = [&](const std::vector<std::pair<double, double>>& poly) {
                for (const auto& q : poly) grow(q.first, q.second, 0.0, 0.0, 0.0);
            };
            eat(ln.path);
            for (const auto& cp : ln.capsulePaths) eat(cp);
            for (const auto& cp : ln.collectorPaths) eat(cp);
        }
    }
    wt -= 1.60;   // the 18.0 m ruler + page extras sit above the stage
    const double pad = 0.45;
    wl -= pad;
    wr += pad;
    wt -= pad;
    wb += pad;
    (void)app;
}

// ---------------------------------------------------------------------------
// 「导出图片」 header band construction — see App.h.  The fields are exactly the
// ones the 01 项目信息 page edits (Project::piece / venue / conductor /
// sysEngineer / recEngineer / asstEngineer / notes).  The struct has no date or
// audience member, so line 2 carries 场地 + 备注; the role line carries the four
// people.  Empty fields are skipped, so a blank project yields ZERO lines and
// therefore NO band at all (the export stays byte-for-byte the old image).
// ---------------------------------------------------------------------------
static std::wstring PngHeaderOneLine(const std::wstring& s) {
    // 备注 is a multi-line edit and every header line is ONE line: line breaks
    // and runs of blanks collapse to a single space
    std::wstring o;
    bool gap = false;
    for (wchar_t ch : s) {
        const bool space = ch == L' ' || ch == L'\t' || ch == L'\r' ||
                           ch == L'\n' || ch == L'\u3000';
        if (space) {
            gap = true;
            continue;
        }
        if (gap && !o.empty()) o.push_back(L' ');
        gap = false;
        o.push_back(ch);
    }
    return o;
}

std::vector<App::PngHeadLine> App::PngHeaderLines() const {
    std::vector<PngHeadLine> lines;
    // line 1 — 曲目, biggest and bold: 8× the old size (14.f → 112.f; Font adds
    // +2 to the design size).  The header text is NOT enlarged by the export
    // multiplier — that rule is unchanged — this is a fixed 8× on the header only.
    const std::wstring piece = PngHeaderOneLine(pr_.piece);
    if (!piece.empty()) lines.push_back({piece, 112.f, true, D(23 * 8)});
    // line 2 — 场地 + 备注 in one muted line, 「 · 」 between them
    std::wstring sub;
    for (const std::wstring& s :
         {PngHeaderOneLine(pr_.venue), PngHeaderOneLine(pr_.notes)}) {
        if (s.empty()) continue;
        if (!sub.empty()) sub += L" · ";
        sub += s;
    }
    if (!sub.empty()) lines.push_back({sub, 72.f, false, D(18 * 8)});   // 8× of 9.f
    // line 3 — 指挥 / 系统 / 录音 / 助理, compact 「指挥：X · 录音：Y」, empties
    // skipped.  The labels are the localized ones the 01 page paints.
    const std::wstring colon = L10n::IsZh() ? L"：" : L": ";
    std::wstring team;
    auto add = [&](S id, const std::wstring& v) {
        const std::wstring t = PngHeaderOneLine(v);
        if (t.empty()) return;
        if (!team.empty()) team += L" · ";
        team += std::wstring(L10n::T(id)) + colon + t;
    };
    add(S::FieldConductor, pr_.conductor);
    add(S::SysEngineer, pr_.sysEngineer);
    add(S::RecEngineer, pr_.recEngineer);
    add(S::AsstEngineer, pr_.asstEngineer);
    if (!team.empty()) lines.push_back({team, 68.f, false, D(17 * 8)});   // 8× of 8.5f
    return lines;
}

int App::PngHeaderBandPx() const {
    const std::vector<PngHeadLine> lines = PngHeaderLines();
    if (lines.empty()) return 0;
    int h = D(kPngHeaderGapPx);   // one blank gap between header and drawing
    for (const auto& l : lines) h += l.rowPx;
    return h;
}

void App::PaintPngHeaderBand(Gdiplus::Graphics& g, int pageW, bool withText) {
    const int bandPx = PngHeaderBandPx();
    if (bandPx <= 0) return;
    // the band is the blank paper above the drawing, so it wears exactly the
    // colour PaintStage fills the export canvas with (the off-stage warm grey) —
    // no differently coloured strip, and the 1 m grid stops where the drawing
    // starts instead of running through the header text
    const int pad = kPngPadPx;
    const RECT band = {0, 0, pageW, pad + bandPx};
    dr::Fill(g, band, dr::C(theme::kOffstage));
    if (!withText) return;
    const std::vector<PngHeadLine> lines = PngHeaderLines();
    const Rgb& tc = pr_.textColor;
    // line 1 wears the project's text colour; the two sub-lines are muted (that
    // same colour alpha-blended onto the paper, so the header stays one ink)
    const Gdiplus::Color muted(150, tc.r, tc.g, tc.b);
    int y = pad;
    for (const auto& l : lines) {
        const RECT r = {pad, y, pageW - pad, y + l.rowPx};
        dr::Text(g, l.text, Font(l.fontPx, l.bold), l.bold ? dr::C(tc) : muted, r, 0,
                 true);
        y += l.rowPx;
    }
}

void App::PngExportSize(int scaleMul, int& w, int& h, double& worldW,
                        double& worldH) const {
    double wl, wt, wr, wb;
    PngWorldBox(*this, pr_, ppm_, ui_.layer == 2, wl, wt, wr, wb);
    worldW = (std::max)(0.5, wr - wl);
    worldH = (std::max)(0.5, wb - wt);
    const double exportPpm = (double)(std::max)(1, scaleMul) * ppm_;
    const int paddingPx = kPngPadPx;
    // the header band is part of the OUTPUT, not of the world box: it is added
    // on top of the padded content box and never scales with the multiplier
    const int headerPx = PngHeaderBandPx();
    w = (int)std::lround(worldW * exportPpm) + 2 * paddingPx;
    h = (int)std::lround(worldH * exportPpm) + 2 * paddingPx + headerPx;
    if (w < 64) w = 64;
    if (h < 64) h = 64;
}

bool App::ExportPngTo(const std::wstring& path, int scaleMul) {
    if (ppm_ <= 1e-6 || !family_) return false;
    const double tExp0 = NowMs();
    const bool wiring = (ui_.layer == 2);

    // ---- TEMPORARY DIAGNOSTIC (SM_PNG_DIAG=<file>) -----------------------------
    wchar_t diagPath[MAX_PATH] = L"";
    const bool wantDiag = GetEnvironmentVariableW(L"SM_PNG_DIAG", diagPath, MAX_PATH) > 0;
    std::wstring pngDiag;
    auto WDiag = [&](const std::wstring& s) { if (wantDiag) pngDiag += s + L"\n"; };

    // ---- content bounding box in world metres (EVERYTHING that is drawn) ------
    double wl = 0.0, wt = 0.0, wr = 0.0, wb = 0.0;
    PngWorldBox(*this, pr_, ppm_, ui_.layer == 2, wl, wt, wr, wb);
    if (wantDiag) {
        double sl = 0, st = 0, sr = pr_.stage.w, sb = pr_.stage.d;
        double il = sl, it_ = st, ir = sr, ib = sb;
        for (const auto& it : pr_.items) {
            const double r = (it.kind == ItemKind::Mic)
                ? (std::max)(5.0 / (ppm_ > 1e-6 ? ppm_ : 1.0), 0.066) *
                      MicMarkerScaleOf(pr_, it.id) + 0.20
                : 0.0;
            const double w = (it.kind == ItemKind::Mic) ? 2 * r
                                                        : (std::max)(0.1, it.w);
            const double h = (it.kind == ItemKind::Mic) ? 2 * r
                                                        : (std::max)(0.1, it.h);
            const double rad = it.angle * 3.14159265358979323846 / 180.0;
            const double ca = std::fabs(std::cos(rad)), sa = std::fabs(std::sin(rad));
            const double ex = (w * ca + h * sa) * 0.5;
            const double ey = (w * sa + h * ca) * 0.5;
            il = (std::min)(il, it.x - ex); ir = (std::max)(ir, it.x + ex);
            it_ = (std::min)(it_, it.y - ey); ib = (std::max)(ib, it.y + ey);
        }
        double dl = il, dt = it_, dr = ir, db = ib;
        for (const auto& dv : pr_.devices) {
            dl = (std::min)(dl, dv.x - kCableDevW); dr = (std::max)(dr, dv.x + kCableDevW);
            dt = (std::min)(dt, dv.y - kCableDevH); db = (std::max)(db, dv.y + kCableDevH);
        }
        WDiag(dr::Fmt(L"ppm %.4f layer %d stage %.2fx%.2f canvas %ld %ld %ld %ld",
                      ppm_, ui_.layer, pr_.stage.w, pr_.stage.d, L_.canvas.left,
                      L_.canvas.top, L_.canvas.right, L_.canvas.bottom));
        WDiag(dr::Fmt(L"box final wl %.4f wt %.4f wr %.4f wb %.4f", wl, wt, wr, wb));
        WDiag(dr::Fmt(L"box stage %.4f %.4f %.4f %.4f", sl, st, sr, sb));
        WDiag(dr::Fmt(L"box items %.4f %.4f %.4f %.4f", il, it_, ir, ib));
        WDiag(dr::Fmt(L"box devs  %.4f %.4f %.4f %.4f (n %d, kCableDev %.3f x %.3f)",
                      dl, dt, dr, db, (int)pr_.devices.size(), kCableDevW, kCableDevH));
        for (const auto& dv : pr_.devices)
            WDiag(dr::Fmt(L"device %d %.3f %.3f angle %.1f", dv.id, dv.x, dv.y, dv.angle));
        WDiag(dr::Fmt(L"links n %d", (int)pr_.links.size()));
    }

    const double worldW = (std::max)(0.5, wr - wl);
    const double worldH = (std::max)(0.5, wb - wt);

    // ---- supersampled output: scaleMul x the on-screen pixels-per-metre ------
    int bw = 0, bh = 0;
    double wW = 0.0, wH = 0.0;
    PngExportSize(scaleMul, bw, bh, wW, wH);
    const double exportPpm = (double)(std::max)(1, scaleMul) * ppm_;
    const int paddingPx = kPngPadPx;
    // blank band ABOVE the drawing for the 01 项目信息 header (0 = no band)
    const int headerPx = PngHeaderBandPx();
    // SM_PNG_DIAG evidence: the band height that PngHeaderBandPx() returned and
    // the page padding, so a script can prove the grid starts right below it
    WDiag(dr::Fmt(L"band headerPx %d pad %d out %d x %d", headerPx, paddingPx, bw, bh));
    const long long totalPx = (long long)bw * (long long)bh;
    const long long kMaxPx = 400LL * 1000LL * 1000LL;   // guard: ~1.6 GB of ARGB
    if (totalPx > kMaxPx) return false;
    if (bw > 60000 || bh > 60000) return false;

    // ---- compose tile by tile (bounded memory, no seams) ---------------------
    // Every tile is painted with an OVERDRAW border (8 px) and only its inner
    // region is blitted, so content that straddles a tile edge is drawn twice and
    // the stitched image is pixel-continuous.
    Gdiplus::Bitmap out(bw, bh, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics og(&out);
        const Rgb pf = pr_.stageFill;
        og.Clear(Gdiplus::Color(255, pf.r, pf.g, pf.b));
        og.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);

        const double savePpm = ppm_, saveOx = ox_, saveOy = oy_;
        const RECT saveCanvas = L_.canvas;
        const int saveLayer = ui_.layer;
        const std::vector<int> saveSel = pr_.selIds;
        const int saveSelected = pr_.selected;
        ppm_ = exportPpm;
        // THE PICTURE IS ENLARGED, THE TEXT IS NOT (user: 「把图片放得很大，再在空隙上
        // 加小字」).  Every label / tag / ruler text keeps its ON-SCREEN pixel size, so
        // a 4× / 8× export is a big drawing annotated with small labels riding in the
        // empty gaps.  Both knobs therefore stay at 1: exportFontScale_ (App::Font)
        // and dr::g_textLayoutBoost (the layout metrics of the painters, which are
        // screen-sized constants and now never need to grow).
        exportFontScale_ = 1.f;
        dr::g_textLayoutBoost = 1.f;
        L_.canvas = {0, 0, bw, bh};
        ui_.layer = wiring ? 2 : (ui_.layer == 1 ? 1 : 0);
        pr_.selIds.clear();
        pr_.selected = -1;

        const int kTile = 2048, kOver = 8;
        int tilesPainted = 0;
        // world (wl, wt) maps to output pixel (padding, padding + header band).
        // The mapping is GLOBAL for both phases: a tile is rendered by
        // TRANSLATING the graphics by its origin instead of reloading ox_/oy_
        // with a tile-local value.  That matters because the painters clamp
        // labels, tags and pills against
        // L_.canvas, which is a canvas in export pixels — with tile-local
        // coordinates those clamps pinned anything near a tile edge to that edge
        // (the duplicated-label ghost) and left clamped pills stranded at the tile
        // boundaries.  With one global coordinate system for the whole image the
        // clamps behave exactly as they do on the on-screen page.
        ox_ = (double)paddingPx - wl * exportPpm;
        // the world box starts BELOW the header band, so the drawing keeps its
        // own position and only the paper above it grew (the band is never part
        // of the drawing area — no clamp, tag or bbox reads it)
        oy_ = (double)(paddingPx + headerPx) - wt * exportPpm;
        // PHASE 1 — GEOMETRY ONLY.  The dr:: capture hook makes every text call a
        // no-op here, so the tiles carry the drawing but not a single glyph.  The
        // 8 px overdraw stays: it is what keeps the seams of the stitched image
        // invisible for the geometry that straddles a tile edge.
        dr::g_captureText = true;
        dr::g_textSink = nullptr;
        for (int ty = 0; ty < bh; ty += kTile) {
            for (int tx = 0; tx < bw; tx += kTile) {
                const int tw = (std::min)(kTile, bw - tx);
                const int th = (std::min)(kTile, bh - ty);
                const int mx0 = (std::max)(0, tx - kOver);
                const int my0 = (std::max)(0, ty - kOver);
                const int mx1 = (std::min)(bw, tx + tw + kOver);
                const int my1 = (std::min)(bh, ty + th + kOver);
                const int twAll = mx1 - mx0, thAll = my1 - my0;
                Gdiplus::Bitmap tile(twAll, thAll, PixelFormat32bppARGB);
                {
                    Gdiplus::Graphics g(&tile);
                    g.Clear(Gdiplus::Color(255, pf.r, pf.g, pf.b));
                    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
                    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                    // the tile shows the canvas rect [mx0,my0,mx1,my1) at 1:1
                    g.TranslateTransform((Gdiplus::REAL)(-mx0), (Gdiplus::REAL)(-my0));
                    g.SetClip(Gdiplus::Rect(mx0, my0, twAll, thAll));
                    PaintStage(g);
                    if (wantDiag && tilesPainted == 0) {
                        WDiag(dr::Fmt(L"tile0 ox %.3f oy %.3f clip %d %d %d %d", ox_, oy_,
                                      mx0, my0, twAll, thAll));
                        {
                            extern std::vector<std::wstring> g_labelReqDump;
                            extern std::vector<std::wstring> g_labelDump;
                            int n = 0;
                            for (const auto& s : g_labelReqDump) {
                                WDiag(s);
                                if (++n >= 12) break;
                            }
                            n = 0;
                            for (const auto& s : g_labelDump) {
                                WDiag(s);
                                if (++n >= 12) break;
                            }
                        }
                    }
                    ++tilesPainted;
                }
                og.DrawImage(&tile, Gdiplus::Rect(tx, ty, tw, th), tx - mx0,
                             ty - my0, tw, th, Gdiplus::UnitPixel);
            }
        }
        dr::g_captureText = false;

        // ---- PHASE 2 — TEXT, ONCE, OVER THE WHOLE IMAGE ---------------------
        // The same painters run ONE more time over the whole export canvas,
        // recording every text call instead of drawing it, and each recorded call
        // is then replayed exactly once.  This is what makes the text layout —
        // the two safety clamps, the group captions, the horizontal de-overlap —
        // identical to the on-screen page: computed in the same global coordinates
        // as phase 1, and only once for the whole image instead of once per tile.
        wchar_t textLogPath[MAX_PATH] = L"";
        const bool wantTextLog =
            GetEnvironmentVariableW(L"SM_PNG_TEXTLOG", textLogPath, MAX_PATH) > 0;
        wchar_t noText[8] = L"";
        const bool skipText =
            GetEnvironmentVariableW(L"SM_PNG_NOTEXT", noText, 8) > 0 && noText[0] == L'1';
        std::wstring textLog;
        {
            Gdiplus::Bitmap probe(8, 8, PixelFormat32bppARGB);
            Gdiplus::Graphics sg(&probe);
            sg.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            sg.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
            sg.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            sg.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            std::vector<dr::TextCall> calls;
            dr::g_textSink = &calls;
            dr::g_captureText = true;
            PaintStage(sg);
            dr::g_captureText = false;
            dr::g_textSink = nullptr;
            if (wantTextLog) {
                textLog += dr::Fmt(L"calls %d canvas %ld %ld %ld %ld\n", (int)calls.size(),
                                   L_.canvas.left, L_.canvas.top, L_.canvas.right,
                                   L_.canvas.bottom);
                for (const auto& t : calls)
                    textLog += dr::Fmt(
                        L"text <%.120s> %ld %ld %ld %ld halo %d wrap %d align %d\n",
                        t.s.c_str(), t.r.left, t.r.top, t.r.right, t.r.bottom,
                        t.useHalo ? 1 : 0, t.wrap ? 1 : 0, t.align);
                // the label layout of THIS pass (same vectors the on-screen dump
                // uses): lets a script check the mic stacking and the de-overlap
                {
                    extern std::vector<std::wstring> g_micStackDump;
                    for (const auto& s : g_micStackDump) textLog += s + L"\n";
                    extern std::vector<std::wstring> g_labelDump;
                    for (const auto& s : g_labelDump) textLog += s + L"\n";
                    extern std::vector<std::wstring> g_labelReqDump;
                    for (const auto& s : g_labelReqDump) textLog += s + L"\n";
                }
            }
            if (!skipText) {
                // SourceOver: the halo glyphs are alpha-blended, SourceCopy would
                // punch transparent holes through them
                og.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
                og.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                og.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
                og.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                og.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                for (const auto& t : calls) dr::PaintTextCall(og, t);
            }
            // the header band is painted LAST, directly in this phase-2 pass (it
            // never goes through the tag layout): blank paper above the drawing
            // plus the 01 项目信息 lines at the top-left.  SM_PNG_NOTEXT keeps
            // only the blank band, so geometry comparisons stay meaningful.
            PaintPngHeaderBand(og, bw, !skipText);
        }
        if (wantTextLog) {
            std::ofstream f(textLogPath, std::ios::binary | std::ios::trunc);
            if (f) {
                const std::string b((const char*)textLog.data(),
                                    textLog.size() * sizeof(wchar_t));
                f.write(b.data(), (std::streamsize)b.size());
            }
        }

        ppm_ = savePpm;
        exportFontScale_ = 1.f;
        dr::g_textLayoutBoost = 1.f;
        ox_ = saveOx;
        oy_ = saveOy;
        L_.canvas = saveCanvas;
        ui_.layer = saveLayer;
        pr_.selIds = saveSel;
        pr_.selected = saveSelected;
        (void)wW;
        (void)wH;
    }
    if (wantDiag) {
        std::ofstream f(diagPath, std::ios::binary | std::ios::trunc);
        if (f) {
            const std::string b((const char*)pngDiag.data(),
                                pngDiag.size() * sizeof(wchar_t));
            f.write(b.data(), (std::streamsize)b.size());
        }
    }
    const bool saved = SavePngFile(out, path);
    // export timing: the status bar shows it, SM_TIMING logs it, --dump prints it
    lastPngMs_ = NowMs() - tExp0;
    TimingLog(dr::Fmt(L"png %.1f scale %d %d x %d ok %d", lastPngMs_,
                      (std::max)(1, scaleMul), bw, bh, saved ? 1 : 0));
    return saved;
}

// the three resolutions the 导出图片 chip offers (output px per metre as a
// multiple of the on-screen ppm): 普通 2× / 高清 4× / 超清 8×
const int App::kPngScales[3] = {2, 4, 8};

// the state half of a chooser pick: remember the multiplier for this run and in
// the project (persisted as `pngscale N` in the .smproj)
void App::ApplyPngScale(int mul) {
    if (mul != 2 && mul != 4 && mul != 8) mul = 4;
    pngScaleMul_ = mul;
    pr_.pngScale = mul;
}

// one row of the chooser was picked: remember it and run the normal PNG save
// dialog, which exports at exactly this multiplier
void App::PickPngScale(int mul) {
    ApplyPngScale(mul);
    Layout();
    InvalidateAll();
    SavePngDialog();
}

void App::SavePngDialog() {
    if (RefuseLocked()) return;
    std::wstring base = (ui_.layer == 2) ? L"cabling" : L"stage-plot";
    wchar_t buf[MAX_PATH] = L"";
    wcsncpy_s(buf, (base + L".png").c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"PNG (*.png)\0*.png\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) return;
    std::wstring out = buf;
    if (out.size() > 4) {
        std::wstring ext = out.substr(out.size() - 4);
        for (auto& ch : ext) ch = (wchar_t)towlower(ch);
        if (ext != L".png") out += L".png";
    }
    // a big export blocks the UI thread: pin the wait cursor and say what is going
    // on BEFORE the heavy call, restore both afterwards (also on failure)
    BeginBusy(dr::Fmt(L10n::T(S::BusyPngFmt), pngScaleMul_));
    const bool ok = ExportPngTo(out, pngScaleMul_);
    EndBusy();
    if (ok) {
        const std::wstring note =
            lastPngMs_ > 0.0
                ? dr::Fmt(L10n::T(S::BusyDoneFmt), L10n::T(S::FnExportPngDone),
                          lastPngMs_ / 1000.0)
                : std::wstring(L10n::T(S::FnExportPngDone));
        StatusMsg(note);
    } else {
        StatusMsg(L10n::T(S::FnExportPngFail), true);
    }
}

bool App::ExportPdfTo(const std::wstring& path) {
    if (cw_ <= 0 || ch_ <= 0 || !family_) return false;
    const int cl = L_.canvas.left;
    const int ct = L_.canvas.top;
    const int cwid = L_.canvas.right - L_.canvas.left;
    const int chgt = L_.canvas.bottom - L_.canvas.top;
    if (cwid <= 0 || chgt <= 0) return false;
    const double tPdf0 = NowMs();

    const int headerPx = D(66);
    const int footerPx = D(84);
    auto P = [](int v) { return (double)v * 0.75; };   // px -> pt at 96 dpi

    // ---- plot page sizing (layer != 3) --------------------------------------
    // measure the plot's world bounding box and fit it to the page WIDTH; the page
    // height then follows the plot, so the drawing fills the sheet (header and
    // footer stay outside the content band).  Canvases whose aspect makes the plot
    // taller than a sane sheet are fitted by height instead.
    double plotW = 0.0, plotH = 0.0, plotScale = 0.0;   // px
    int plotBw = 0, plotBh = 0;
    double pageHpx = (double)(headerPx + chgt + footerPx);
    if (ui_.layer != 3 && cwid > 0 && ppm_ > 1e-6) {
        double wl = 0.0, wt = 0.0, wr = pr_.stage.w, wb = pr_.stage.d;
        auto grow = [&](double cx, double cy, double w, double h, double ang) {
            const double rad = ang * 3.14159265358979323846 / 180.0;
            const double ca = std::fabs(std::cos(rad)), sa = std::fabs(std::sin(rad));
            const double ex = (w * ca + h * sa) * 0.5;
            const double ey = (w * sa + h * ca) * 0.5;
            wl = (std::min)(wl, cx - ex);
            wr = (std::max)(wr, cx + ex);
            wt = (std::min)(wt, cy - ey);
            wb = (std::max)(wb, cy + ey);
        };
        for (const auto& it : pr_.items) {
            if (it.kind == ItemKind::Mic) {
                if (!pr_.showMics) continue;
                const double r = MicMarkerRadiusF(it) / ppm_;
                grow(it.x, it.y, r * 2.0 + 0.10, r * 2.0 + 0.10, 0.0);
            } else {
                if (!pr_.showInstruments) continue;
                grow(it.x, it.y, it.w > 0.01 ? it.w : 0.6, it.h > 0.01 ? it.h : 0.6,
                     it.angle);
            }
        }
        for (const auto& dv : pr_.devices)
            grow(dv.x, dv.y, kCableDevW * 2.0, kCableDevH * 2.0, dv.angle);
        wt -= 1.20;                    // room for the 18.0 m ruler above the stage
        const double pad = 0.35;
        wl -= pad;
        wr += pad;
        wt -= pad;
        wb += pad;
        const double cwl = (L_.canvas.left - ox_) / ppm_;
        const double cwt = (L_.canvas.top - oy_) / ppm_;
        const double cwr = (L_.canvas.right - ox_) / ppm_;
        const double cwb = (L_.canvas.bottom - oy_) / ppm_;
        wl = (std::max)(wl, cwl);
        wt = (std::max)(wt, cwt);
        wr = (std::min)(wr, cwr);
        wb = (std::min)(wb, cwb);
        const double worldW = (std::max)(0.5, wr - wl);
        const double worldH = (std::max)(0.5, wb - wt);
        const int marginPx0 = D(24);
        const int availW0 = (std::max)(64, cwid - 2 * marginPx0);
        const double spxW = (double)availW0 / worldW;
        const double bhW = worldH * spxW;
        // keep the sheet sane: if the plot would be very tall, fit the height too
        const int maxContentPx = (int)(1500.0 / 0.75);
        double spx = spxW;
        if (bhW > maxContentPx) spx = (double)maxContentPx / worldH;
        plotScale = spx;
        plotBw = (int)std::lround(worldW * spx);
        plotBh = (int)std::lround(worldH * spx);
        plotW = worldW;
        plotH = worldH;
        pageHpx = (double)(headerPx + marginPx0 * 2 + plotBh + footerPx);
    }

    PdfDoc doc(P(cwid), P((int)std::lround(pageHpx)));

    // The channel-list page prints on white paper (ink saving); the stage and
    // mic-plot pages keep the cream stage fill.
    const Rgb pageFill = (ui_.layer == 3) ? Rgb{255, 255, 255} : pr_.stageFill;
    const PdfColor pageFillC{(int)pageFill.r, (int)pageFill.g, (int)pageFill.b};

    const Rgb ink{(BYTE)((theme::kText >> 16) & 0xFF), (BYTE)((theme::kText >> 8) & 0xFF),
                  (BYTE)(theme::kText & 0xFF)};
    const Rgb muted{(BYTE)((theme::kTextMuted >> 16) & 0xFF),
                    (BYTE)((theme::kTextMuted >> 8) & 0xFF),
                    (BYTE)(theme::kTextMuted & 0xFF)};
    const Rgb accent{(BYTE)((theme::kAccent >> 16) & 0xFF),
                     (BYTE)((theme::kAccent >> 8) & 0xFF), (BYTE)(theme::kAccent & 0xFF)};
    const PdfColor inkC{(int)ink.r, (int)ink.g, (int)ink.b};
    const PdfColor mutedC{(int)muted.r, (int)muted.g, (int)muted.b};
    const PdfColor accentC{(int)accent.r, (int)accent.g, (int)accent.b};

    // Single line of real vector glyph outlines.  x/size are device px like the
    // rest of the layout; yTop is the top of the GDI+ line box, so the baseline
    // lands at the same pixel pos the old DrawString bitmaps used.
    auto text = [&](const std::wstring& s, int xPx, int yTopPx, int sizePx, bool bold,
                    PdfColor col) {
        if (s.empty()) return;
        const double sizePt = P(sizePx);
        doc.GlyphText(s, P(xPx), P(yTopPx) + sizePt * 0.88, sizePt, col, bold);
    };

    // --- header (project info) ---------------------------------------------
    // repeated on EVERY page of a multi-page table: the document stays readable
    // when the pages are printed / handed out separately
    auto header = [&]() {
        const std::wstring title = pr_.piece.empty() ? std::wstring(L10n::T(S::AppName))
                                                     : pr_.piece;
        text(title, D(36), D(12), D(17), true, inkC);

        std::wstring sub;
        if (!pr_.venue.empty()) sub += std::wstring(L10n::T(S::FieldVenue)) + L" " + pr_.venue;
        if (!pr_.conductor.empty()) {
            if (!sub.empty()) sub += L"   ·   ";
            sub += std::wstring(L10n::T(S::FieldConductor)) + L" " + pr_.conductor;
        }
        text(sub, D(36), D(38), D(9), false, mutedC);

        doc.Line(P(D(36)), P(D(56)), P(cwid - D(36)), P(D(56)), 0.7, mutedC);
    };

    // footer page number 「1 / 3」  bottom right, on every page
    auto pageNumber = [&](int idx, int total) {
        doc.SetOffset(0);
        wchar_t buf[32] = L"";
        swprintf_s(buf, L"%d / %d", idx + 1, total);
        const std::wstring s(buf);
        const double sizePt = P(D(9));
        const double wx = (double)s.size() * sizePt * 0.55;   // approximate advance
        doc.GlyphText(s, doc.Width() - P(D(36)) - wx,
                      P(headerPx + (int)std::lround(pageHpx) - headerPx - footerPx) +
                          P(D(24)),
                      sizePt, mutedC, false);
        doc.SetOffset(0);
    };

    // The DRAWING pages are not printed to PDF any more (a plot can sit far
    // outside the stage and would not fit a sheet): the PDF holds the 通道表.
    // 「导出图片…」 covers the stage and 布线 drawings as PNG.
    if (true) {
        // typeset channel table: rules, labels and one vector-text row per
        // channel, built from BuildChannelRows() - no raster of the screen.
        // Columns follow the same show/hide state as the on-screen table.
        // PAGINATION: when the rows do not fit one page the table continues on the
        // next page with the title + the column header row repeated and 「i / N」 in
        // the footer; everything that fits stays a single page (unchanged).
        // the same list the screen paints: numbered channel rows, then the
        // 布线 tail block (one row per 接口箱 → 声卡 cable)
        const std::vector<ChRow> rows = BuildChannelDisplayRows();
        wchar_t pdfDiagPath[MAX_PATH] = L"";
        const bool wantPdfDiag =
            GetEnvironmentVariableW(L"SM_PDF_DIAG", pdfDiagPath, MAX_PATH) > 0;
        std::wstring pdfLog;
        const double x0 = P(D(36));
        const double x1 = doc.Width() - P(D(36));
        const ChColSpan cs = ChannelColumns(x0, x1, P(D(64)), P(D(12)), P(D(6)));
        const S colNames[kChCols] = {S::ChNum,   S::ChModel, S::ChInst, S::ChRemark,
                                     S::CbLink, S::CbLen,   S::ChToMain, S::ChAzimuth};
        const double titleSize = P(D(16));
        const double bodySize = P(D(11));
        const double contentBottom = P(chgt);

        const double titleBase = P(D(26)) + titleSize * 0.92;
        const double rule1Y = titleBase + P(D(12));   // one blank line below title
        const double headerBase = rule1Y + 6.0 + bodySize * 0.73;   // 6pt gap above caps
        const double rule2Y = headerBase + bodySize * 0.30 + 6.0;
        const double rowH = P(D(24));
        const double bodyLimit = contentBottom - P(D(10));

        int perPage = (int)std::floor((bodyLimit - rule2Y) / rowH);
        if (perPage < 1) perPage = 1;
        const int total = (int)rows.size();
        const int pagesNeeded = total <= 0 ? 1 : (total + perPage - 1) / perPage;

        for (int pg = 0; pg < pagesNeeded; ++pg) {
            if (pg > 0) doc.NewPage();
            doc.FillRect(0.0, 0.0, doc.Width(), doc.Height(), pageFillC);
            header();
            doc.SetOffset(P(headerPx));

            doc.GlyphText(L10n::T(S::ChTitle), x0, titleBase, titleSize, inkC, true);
            doc.FillRect(x0, rule1Y, x1 - x0, 1.4, inkC);
            for (int c = 0; c < kChCols; ++c) {
                if (!ui_.chColVis[c]) continue;
                // a caption longer than its column (「话筒线长度」 / 「Mikrofonkabel」)
                // shrinks instead of running into the next column; the glyph run
                // has no clipping, so the size is fitted up front
                const std::wstring cap =
                    (c == 5) ? ChannelLenHeader() : L10n::T(colNames[c]);
                double capSize = bodySize;
                auto estW = [&](double sz) {
                    double wsum = 0.0;
                    for (wchar_t ch : cap)
                        wsum += (ch > 0x2E7F) ? sz * 1.0 : sz * 0.56;   // CJK / latin
                    return wsum;
                };
                const double capW = cs.r[c] - cs.l[c];
                // shrink the header caption until it fits, but ALWAYS by a positive
                // step and never more than a few dozen times (P() takes an int, so
                // P(0.5) is 0 — that used to make this loop spin forever)
                const double capMin = bodySize * 0.75;
                for (int guard = 0; guard < 40 && capSize > capMin &&
                                    estW(capSize) > capW; ++guard)
                    capSize = (std::max)(capMin, capSize - 0.5);
                doc.GlyphText(cap, cs.l[c], headerBase, capSize, mutedC, true);
                if (wantPdfDiag)
                    pdfLog += dr::Fmt(L"pdfhead %d <%s> x %.2f size %.2f\n", c, cap,
                                      cs.l[c], capSize);
            }
            doc.FillRect(x0, rule2Y, x1 - x0, 0.8, mutedC);

            double rowTop = rule2Y;
            const int from = pg * perPage;
            const int to = (std::min)(total, from + perPage);
            if (total == 0 && pg == 0) {
                doc.GlyphText(L"—", cs.l[0], rowTop + rowH * 0.5 + bodySize * 0.36,
                              bodySize, mutedC, false);
                rowTop += rowH;
            }
            for (int i = from; i < to; ++i) {
                const ChRow& r = rows[(size_t)i];
                const double base = rowTop + rowH * 0.5 + bodySize * 0.36;
                if (r.separator) {   // 「布线」 separator above the summary block
                    doc.FillRect(x0, rowTop, x1 - x0, 1.0, mutedC);
                    doc.GlyphText(L"布线", cs.l[0], base, bodySize, mutedC, true);
                    rowTop += rowH;
                    continue;
                }
                if (ui_.chColVis[0]) {
                    if (r.summary) {
                        doc.GlyphText(r.chText, cs.l[0], base, bodySize, mutedC,
                                      false);
                    } else {
                        wchar_t chbuf[16] = L"";
                        swprintf_s(chbuf, L"%02d", r.ch);
                        doc.GlyphText(chbuf, cs.l[0], base, bodySize, inkC, false);
                    }
                }
                if (ui_.chColVis[1])
                    doc.GlyphText(r.model, cs.l[1], base, bodySize, inkC, false);
                if (ui_.chColVis[2])
                    doc.GlyphText(r.inst, cs.l[2], base, bodySize, accentC, false);
                if (ui_.chColVis[3])
                    doc.GlyphText(r.remark, cs.l[3], base, bodySize, mutedC, false);
                if (ui_.chColVis[4])
                    doc.GlyphText(r.link.empty() ? std::wstring(L"—") : r.link, cs.l[4],
                                  base, bodySize, mutedC, false);
                if (ui_.chColVis[5]) {
                    wchar_t lenbuf[32] = L"";
                    if (r.len > 0.0) swprintf_s(lenbuf, L"%.1f m", r.len);
                    doc.GlyphText(r.len > 0.0 ? std::wstring(lenbuf) : std::wstring(L"—"),
                                  cs.l[5], base, bodySize, mutedC, false);
                }
                // 距主话筒: the plan distance to the L/R main capsule on this side
                if (ui_.chColVis[6]) {
                    wchar_t tmbuf[32] = L"";
                    if (r.toMain >= 0.0) swprintf_s(tmbuf, L"%.1f m", r.toMain);
                    doc.GlyphText(r.toMain >= 0.0 ? std::wstring(tmbuf)
                                                  : std::wstring(L"—"),
                                  cs.l[6], base, bodySize, mutedC, false);
                }
                // 幅角: azimuth, positive to the right, 「+12.5°」 / 「−30.0°」
                if (ui_.chColVis[7]) {
                    std::wstring az;
                    if (r.hasAz) {
                        const double a = r.azimuth;
                        wchar_t abuf[32] = L"";
                        if (a >= -0.049 && a <= 0.049)
                            wcscpy_s(abuf, L"0.0°");
                        else
                            swprintf_s(abuf, a > 0 ? L"+%.1f°" : L"−%.1f°",
                                       std::fabs(a));
                        az = abuf;
                    } else {
                        az = L"—";
                    }
                    doc.GlyphText(az, cs.l[7], base, bodySize, mutedC, false);
                }
                if (wantPdfDiag)
                    pdfLog += dr::Fmt(
                        L"pdfrow pg %d y %.2f ch <%s> link <%s> len %.2f custom %d "
                        L"summary %d sep %d\n",
                        pg, rowTop,
                        r.summary ? r.chText.c_str()
                                  : dr::Fmt(L"%02d", r.ch).c_str(),
                        r.link.c_str(), r.len, r.custom ? 1 : 0,
                        r.summary ? 1 : 0, r.separator ? 1 : 0);
                rowTop += rowH;
            }
            doc.FillRect(x0, rowTop, x1 - x0, 1.4, inkC);

            // notes belong to the last page (the footer strip has room there)
            if (pg + 1 == pagesNeeded && !pr_.notes.empty()) {
                std::wstring notes = pr_.notes;
                for (auto& chp : notes)
                    if (chp == L'\r' || chp == L'\n') chp = L' ';
                doc.SetOffset(0);
                const double sizePt = P(D(9));
                doc.GlyphText(std::wstring(L10n::T(S::FieldNotes)) + L"  " + notes,
                              x0, P(headerPx + chgt) + P(D(24)), sizePt, mutedC, false);
            }
            pageNumber(pg, pagesNeeded);
        }
        if (wantPdfDiag) {
            std::ofstream f(pdfDiagPath, std::ios::binary | std::ios::trunc);
            if (f) {
                const std::string b((const char*)pdfLog.data(),
                                    pdfLog.size() * sizeof(wchar_t));
                f.write(b.data(), (std::streamsize)b.size());
            }
        }
    } else {
        // stage / mic plot: FIT the PLOT to the printable area.  The raster is no
        // longer the whole window canvas (which carries the on-screen fit padding
        // and made the plot print small in the middle of the sheet): the world
        // bounding box of the plot is measured, the fit scale is
        //     s = min(printableW / plotW, printableH / plotH)
        // the raster is drawn at exactly that scale and centred — so a small stage
        // FILLS the page (upscaled when needed) and a large one is scaled down,
        // never cropped, always keeping the aspect ratio.
        doc.FillRect(0.0, 0.0, doc.Width(), doc.Height(), pageFillC);
        header();
        doc.SetOffset(P(headerPx));

        // ---- plot bounding box in world metres --------------------------------
        double wl = 0.0, wt = 0.0, wr = pr_.stage.w, wb = pr_.stage.d;
        auto growBox = [&](double cx, double cy, double w, double h, double ang) {
            const double rad = ang * 3.14159265358979323846 / 180.0;
            const double ca = std::fabs(std::cos(rad)), sa = std::fabs(std::sin(rad));
            const double ex = (w * ca + h * sa) * 0.5;
            const double ey = (w * sa + h * ca) * 0.5;
            wl = (std::min)(wl, cx - ex);
            wr = (std::max)(wr, cx + ex);
            wt = (std::min)(wt, cy - ey);
            wb = (std::max)(wb, cy + ey);
        };
        for (const auto& it : pr_.items) {
            if (it.kind == ItemKind::Mic) {
                if (!pr_.showMics) continue;
                const double r = MicMarkerRadiusF(it) / (ppm_ > 0.0 ? ppm_ : 1.0);
                growBox(it.x, it.y, r * 2.0 + 0.10, r * 2.0 + 0.10, 0.0);
            } else {
                if (!pr_.showInstruments) continue;
                growBox(it.x, it.y, it.w > 0.01 ? it.w : 0.6,
                        it.h > 0.01 ? it.h : 0.6, it.angle);
            }
        }
        for (const auto& dv : pr_.devices)
            growBox(dv.x, dv.y, kCableDevW * 2.0, kCableDevH * 2.0, dv.angle);
        // the 18.0 m ruler is drawn ABOVE the stage: keep room for it
        wt -= 1.20;
        const double pad = 0.35;   // small breathing room around the plot
        wl -= pad;
        wr += pad;
        wt -= pad;
        wb += pad;
        // the canvas is what PaintStage actually paints: clamp the bbox to it so
        // no unpainted (transparent -> black in JPEG) band can appear
        if (ppm_ > 1e-6) {
            const double cwl = (L_.canvas.left - ox_) / ppm_;
            const double cwt = (L_.canvas.top - oy_) / ppm_;
            const double cwr = (L_.canvas.right - ox_) / ppm_;
            const double cwb = (L_.canvas.bottom - oy_) / ppm_;
            wl = (std::max)(wl, cwl);
            wt = (std::max)(wt, cwt);
            wr = (std::min)(wr, cwr);
            wb = (std::min)(wb, cwb);
        }
        const double worldW = (std::max)(0.5, wr - wl);
        const double worldH = (std::max)(0.5, wb - wt);

        // ---- printable area: ~8 mm margin on every side -----------------------
        const int marginPx = D(24);                     // ~8 mm at 96 dpi
        const int contentPx = (int)std::lround(pageHpx) - headerPx - footerPx;
        const int availW = (std::max)(32, cwid - 2 * marginPx);
        const int availH = (std::max)(32, contentPx - 2 * marginPx);

        // ---- fit scale (px per metre) and the exact raster size ---------------
        double spx = (std::min)((double)availW / worldW, (double)availH / worldH);
        if (!(spx > 0.0)) spx = 1.0;
        int bw = (int)std::lround(worldW * spx);
        int bh = (int)std::lround(worldH * spx);
        if (bw < 8) bw = 8;
        if (bh < 8) bh = 8;
        // supersample for crisp lines, then embed at the fitted size
        const int SS = (bw * 2 < 6000 && bh * 2 < 6000) ? 2 : 1;
        Gdiplus::Bitmap bmp(bw * SS, bh * SS, PixelFormat32bppARGB);
        {
            Gdiplus::Graphics g(&bmp);
            // solid page fill first: a transparent pixel becomes BLACK in the JPEG
            g.Clear(Gdiplus::Color(255, pageFill.r, pageFill.g, pageFill.b));
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            // client px -> raster px: k = spx / ppm_ (world metres stay exact)
            const double ppmSafe = ppm_ > 1e-6 ? ppm_ : 1.0;
            const double k = (double)SS * spx / ppmSafe;
            g.ScaleTransform((float)k, (float)k);
            g.TranslateTransform((float)-(ox_ + wl * ppm_), (float)-(oy_ + wt * ppm_));
            PaintStage(g);
        }
        // centre it in the printable area
        const int px = marginPx + (availW - bw) / 2;
        const int py = marginPx + (availH - bh) / 2;
        std::vector<BYTE> jpg;
        if (EncodeJpeg(bmp, jpg) && !jpg.empty())
            doc.PlaceImage(doc.AddJpeg(bw * SS, bh * SS, jpg), P(px), P(py), P(bw),
                           P(bh), 0.0);
        doc.SetOffset(0);

        // footer (notes) + page number
        if (!pr_.notes.empty()) {
            std::wstring notes = pr_.notes;
            for (auto& chp : notes)
                if (chp == L'\r' || chp == L'\n') chp = L' ';
            const int fy = headerPx + contentPx + D(6);
            text(L10n::T(S::FieldNotes), D(36), fy, D(9), false, mutedC);
            text(notes, D(36), fy + D(14), D(9), false, inkC);
        }
        pageNumber(0, 1);
    }
    const bool saved = doc.Save(path);
    lastPdfMs_ = NowMs() - tPdf0;
    TimingLog(dr::Fmt(L"pdf %.1f layer %d ok %d", lastPdfMs_, ui_.layer,
                      saved ? 1 : 0));
    return saved;
}

// ---------------------------------------------------------------------------
// closing the window with unsaved changes
// ---------------------------------------------------------------------------
// WM_CLOSE (title-bar ×, Alt+F4 and 系统菜单 → 关闭 all arrive as WM_CLOSE) no
// longer destroys the window unconditionally: a dirty project opens the painted
// modal 是否保存更改？ with three buttons.  The prompt is an in-app overlay like the
// 预设 / delete-route modals — one paint path, one click path — so PrintWindow
// screenshots and the --close / --closechoice test hooks see exactly what the
// user sees, and the child EDITs are hidden while it is open (Layout).
void App::RequestClose() {
    if (exitOpen_) return;   // a second close request is ignored while asking
    if (Locked()) {
        // read-only: never ask (a locked app cannot save the open project)
        DLog("close: locked -> exit, no prompt");
        DestroyWindow(hwnd_);
        return;
    }
    if (!dirty_) {
        DLog("close: clean -> exit, no prompt");
        DestroyWindow(hwnd_);
        return;
    }
    SetFocus(hwnd_);
    exitOpen_ = true;
    exitHover_ = -1;
    DLog("close: dirty -> prompt open");
    Layout();          // hides every native EDIT under the painted prompt
    InvalidateAll();
    if (closeChoice_ >= 0) {
        // --closechoice test hook: answer through the very same handler the three
        // painted buttons call, so the scripted path and the GUI path cannot drift
        const int choice = closeChoice_;
        closeChoice_ = -1;
        ExitPromptAnswer(choice);
    }
}

void App::ExitPromptAnswer(int choice) {
    if (!exitOpen_) return;
    if (choice == 0) {
        // 保存: the normal save (a project with no path yet gets the 另存为 dialog).
        // A cancelled 另存为 is NOT an exit: the prompt stays open.
        if (!SaveProjectDialog(false)) {
            DLog("close prompt: save not written -> stay");
            exitHover_ = -1;
            InvalidateAll();
            return;
        }
        DLog("close prompt: saved -> exit");
        exitOpen_ = false;
        exitHover_ = -1;
        L_.xpScrim = L_.xpPanel = L_.xpTitle = L_.xpMsg = {};
        L_.xpSave = L_.xpDiscard = L_.xpCancel = {};
        DestroyWindow(hwnd_);
        return;
    }
    exitOpen_ = false;
    exitHover_ = -1;
    L_.xpScrim = L_.xpPanel = L_.xpTitle = L_.xpMsg = {};
    L_.xpSave = L_.xpDiscard = L_.xpCancel = {};
    if (choice == 1) {
        DLog("close prompt: discard -> exit without saving");
        DestroyWindow(hwnd_);
        return;
    }
    DLog("close prompt: cancel -> stay");
    Layout();          // the page (and its EDITs) come back
    InvalidateAll();
}

void App::HideAllChildEdits() {
    for (HWND h = GetWindow(hwnd_, GW_CHILD); h; h = GetWindow(h, GW_HWNDNEXT)) {
        wchar_t cls[32] = L"";
        if (!GetClassNameW(h, cls, 32)) continue;
        if (_wcsicmp(cls, L"EDIT") != 0) continue;
        ShowWindow(h, SW_HIDE);
        SetWindowRgn(h, nullptr, TRUE);
    }
}

bool App::ExitPromptClick(int x, int y) {
    if (!exitOpen_) return false;
    const POINT p = {x, y};
    if (PtInRect(&L_.xpSave, p)) {
        ExitPromptAnswer(0);
        return true;
    }
    if (PtInRect(&L_.xpDiscard, p)) {
        ExitPromptAnswer(1);
        return true;
    }
    // a click on the panel body (not on a button) changes nothing; 取消, the scrim
    // and Esc all answer 取消 and clear the prompt
    if (PtInRect(&L_.xpPanel, p) && !PtInRect(&L_.xpCancel, p)) return true;
    ExitPromptAnswer(2);
    return true;
}

void App::PaintExitPrompt(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    if (!exitOpen_) {
        L_.xpScrim = L_.xpPanel = L_.xpTitle = L_.xpMsg = {};
        L_.xpSave = L_.xpDiscard = L_.xpCancel = {};
        return;
    }
    L_.xpScrim = {0, 0, cw_, ch_};
    dr::Fill(g, L_.xpScrim, dr::C(0x2B2723, 120));

    const int w = d(620), h = d(214);
    const int ccx = cw_ / 2, ccy = ch_ / 2;
    L_.xpPanel = {ccx - w / 2, ccy - h / 2, ccx + w / 2, ccy + h / 2};
    dr::FillRound(g, dr::RF(L_.xpPanel), (float)d(12), dr::C(theme::kPanel));
    dr::StrokeRound(g, dr::RF(L_.xpPanel), (float)d(12),
                    dr::C(theme::kBorderStrong), 1.2f);

    L_.xpTitle = {L_.xpPanel.left + d(24), L_.xpPanel.top + d(18),
                  L_.xpPanel.right - d(24), L_.xpPanel.top + d(46)};
    dr::Text(g, L10n::T(S::AppName), Font(11.f, true), dr::C(theme::kText),
             L_.xpTitle, 0, true);
    L_.xpMsg = {L_.xpPanel.left + d(24), L_.xpPanel.top + d(58),
                L_.xpPanel.right - d(24), L_.xpPanel.top + d(108)};
    dr::Text(g, L10n::T(S::AskSaveMsg), Font(12.f), dr::C(theme::kText), L_.xpMsg,
             0, true);

    const int bw = d(128), bh = d(38), gap = d(10);
    const int by = L_.xpPanel.bottom - d(22) - bh;
    int bx = L_.xpPanel.right - d(24) - bw;   // 取消 (rightmost)
    L_.xpCancel = {bx, by, bx + bw, by + bh};
    bx -= bw + gap;                           // 不保存
    L_.xpDiscard = {bx, by, bx + bw, by + bh};
    bx -= bw + gap;                           // 保存 (primary, leftmost)
    L_.xpSave = {bx, by, bx + bw, by + bh};
    auto button = [&](const RECT& r, const wchar_t* label, bool primary, bool hot) {
        const Gdiplus::Color fill = primary
            ? dr::C(theme::kAccent)
            : dr::C(hot ? theme::kHover : theme::kPanelAlt);
        const Gdiplus::Color stroke = primary
            ? dr::C(hot ? theme::kText : theme::kAccent)
            : dr::C(hot ? theme::kBorderStrong : theme::kBorder);
        const Gdiplus::Color ink = primary ? dr::C(theme::kPanel) : dr::C(theme::kText);
        dr::FillRound(g, dr::RF(r), (float)d(7), fill);
        dr::StrokeRound(g, dr::RF(r), (float)d(7), stroke, 1.f);
        dr::Text(g, label, Font(10.f), ink, r, 1, true);
    };
    button(L_.xpSave, L10n::T(S::AskSaveSave), true, exitHover_ == 1);
    button(L_.xpDiscard, L10n::T(S::AskSaveDiscard), false, exitHover_ == 2);
    button(L_.xpCancel, L10n::T(S::AskSaveCancel), false, exitHover_ == 3);
}



