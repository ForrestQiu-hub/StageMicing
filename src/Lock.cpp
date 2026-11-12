// ---------------------------------------------------------------------------
// APP LOCK (4-digit passcode)
//
// The passcode is NEVER stored.  `applock.dat`, written next to the executable,
// holds only
//     SM-LOCK 1
//     salt <32 hex>     16 random bytes
//     hash <64 hex>     SHA256(salt ‖ utf8(passcode))
// so the file reveals nothing about the code (a 4-digit code is trivially brute
// forceable offline — the lock is a UI guard, not disk encryption — but nothing
// is stored in plain text, which is what the feature promises).  A missing or
// corrupt file means 「no lock」: the padlock chip paints muted and the app is
// fully editable.
//
// The ONLY entry point is the top-bar padlock chip at the far right (after the
// language selector) and its popover.  While locked the app is a READ-ONLY
// VIEWER: the pages, the selection, zoom / pan, 导出 PDF, 打开 and 另存为 keep
// working, every data mutation is refused with 「已锁定，只读」 through the ONE
// `App::RefuseLocked()` gate (see the call sites in App.cpp / StageView.cpp /
// Cabling.cpp / ProjectIO.cpp / RightPanel edits).
// ---------------------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <objidl.h>
#include <gdiplus.h>

#include "App.h"
#include "Draw.h"
#include "L10n.h"
#include "Theme.h"

#include <cmath>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace {

constexpr const wchar_t* kLockFile = L"applock.dat";

std::wstring LockPath() { return App::Get().exeDir_ + L"\\" + kLockFile; }

// ---- SHA-256 through CNG ---------------------------------------------------
bool Sha256(const std::vector<unsigned char>& in, unsigned char out[32]) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return false;
    BCRYPT_HASH_HANDLE h = nullptr;
    bool ok = BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) >= 0;
    if (ok) ok = BCryptHashData(h, (PUCHAR)in.data(), (ULONG)in.size(), 0) >= 0;
    if (ok) ok = BCryptFinishHash(h, out, 32, 0) >= 0;
    if (h) BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

bool RandomBytes(unsigned char* p, int n) {
    return BCryptGenRandom(nullptr, p, (ULONG)n,
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
}

std::string ToHex(const unsigned char* p, int n) {
    static const char* k = "0123456789abcdef";
    std::string s;
    for (int i = 0; i < n; ++i) {
        s.push_back(k[(p[i] >> 4) & 0xF]);
        s.push_back(k[p[i] & 0xF]);
    }
    return s;
}

bool FromHex(const std::string& s, std::vector<unsigned char>& out) {
    out.clear();
    if (s.size() % 2) return false;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        const int a = nib(s[i]), b = nib(s[i + 1]);
        if (a < 0 || b < 0) return false;
        out.push_back((unsigned char)((a << 4) | b));
    }
    return true;
}

std::string Utf8Of(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int need = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                         nullptr, 0, nullptr, nullptr);
    std::string s((size_t)need, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), need,
                        nullptr, nullptr);
    return s;
}

// normalise what the user types: four ASCII digits, everything else dropped
std::wstring DigitsOf(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s)
        if (c >= L'0' && c <= L'9' && out.size() < 4) out.push_back(c);
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// storage
// ---------------------------------------------------------------------------
void App::LockLoad() {
    lockState_ = 0;
    lockSalt_.clear();
    lockHash_.clear();
    FILE* f = nullptr;
    if (_wfopen_s(&f, LockPath().c_str(), L"rb") != 0 || !f) return;   // no file = no lock
    std::string text;
    char buf[512];
    size_t got = 0;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, got);
    fclose(f);
    // tolerate ANY junk: only a well-formed salt + hash pair enables the lock
    std::string salt, hash, lockedFlag;
    for (size_t i = 0; i < text.size();) {
        size_t e = text.find('\n', i);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(i, e - i);
        i = e + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.rfind("salt ", 0) == 0) salt = line.substr(5);
        else if (line.rfind("hash ", 0) == 0) hash = line.substr(5);
        else if (line.rfind("locked ", 0) == 0) lockedFlag = line.substr(7);
    }
    std::vector<unsigned char> s, h;
    if (!FromHex(salt, s) || !FromHex(hash, h)) return;   // corrupt → no lock
    if (s.size() != 16 || h.size() != 32) return;
    lockSalt_ = s;
    lockHash_ = h;
    // the LOCKED flag survives a restart: an app lock that a relaunch could
    // bypass would not be a lock at all
    lockState_ = (lockedFlag.rfind("1", 0) == 0) ? 2 : 1;
}

// one writer for applock.dat (salt + hash + the locked flag, never the code)
bool App::LockWriteFile(const std::vector<unsigned char>& salt,
                        const std::vector<unsigned char>& hash, bool locked) {
    if (salt.size() != 16 || hash.size() != 32) return false;
    std::string text = "SM-LOCK 1\nsalt ";
    text += ToHex(salt.data(), 16);
    text += "\nhash ";
    text += ToHex(hash.data(), 32);
    text += locked ? "\nlocked 1\n" : "\nlocked 0\n";
    FILE* f = nullptr;
    if (_wfopen_s(&f, LockPath().c_str(), L"wb") != 0 || !f) return false;
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return true;
}

bool App::LockCheck(const std::wstring& code) const {
    if (lockState_ < 1 || lockHash_.size() != 32) return false;
    const std::string c = Utf8Of(DigitsOf(code));
    if (c.size() != 4) return false;
    std::vector<unsigned char> in = lockSalt_;
    in.insert(in.end(), c.begin(), c.end());
    unsigned char out[32] = {};
    if (!Sha256(in, out)) return false;
    for (int i = 0; i < 32; ++i)
        if (out[i] != lockHash_[(size_t)i]) return false;
    return true;
}

bool App::LockSaveCode(const std::wstring& code) {
    const std::string c = Utf8Of(DigitsOf(code));
    if (c.size() != 4) return false;
    std::vector<unsigned char> salt(16, 0);
    if (!RandomBytes(salt.data(), 16)) {
        // a deterministic fallback salt is still better than plaintext
        const DWORD t = GetTickCount();
        for (int i = 0; i < 16; ++i)
            salt[(size_t)i] = (unsigned char)((t >> ((i % 4) * 8)) ^ (i * 37 + 11));
    }
    std::vector<unsigned char> in = salt;
    in.insert(in.end(), c.begin(), c.end());
    unsigned char out[32] = {};
    if (!Sha256(in, out)) return false;
    const std::vector<unsigned char> hash(out, out + 32);
    // setting / changing the code LOCKS the app immediately: the file is written
    // with locked=1, the state becomes 2 (read-only) and the popover switches to
    // 「解锁」+ cells, so the user can unlock right away.  The code itself is never
    // stored — only salt + SHA256(salt‖code).
    if (!LockWriteFile(salt, hash, true)) return false;
    lockSalt_ = salt;
    lockHash_ = hash;
    lockState_ = 2;
    lockOpen = false;
    lockInput.clear();
    lockErr = false;
    lockModal = 0;
    StatusMsg(L10n::T(S::LockNowLocked));
    InvalidateAll();
    return true;
}

// ---------------------------------------------------------------------------
// the ONE mutation gate.  Every entry point of a data change starts with
//     if (RefuseLocked()) return;
// so no page can forget the rule and no mutation can slip through a new path.
// ---------------------------------------------------------------------------
bool App::RefuseLocked() {
    if (!Locked()) return false;
    StatusMsg(L10n::T(S::LockReadOnly), true);
    InvalidateAll();
    return true;
}

// ---------------------------------------------------------------------------
// state transitions
// ---------------------------------------------------------------------------
void App::LockCloseAll() {
    lockOpen = false;
    lockModal = 0;
    lockInput.clear();
    lockErr = false;
    lockHoverRow = -1;
    hoverLockCell = -1;
    hoverLockOk = hoverLockCancel = 0;
    hoverLmCell = -1;
    lockModalHoverOk = lockModalHoverCancel = 0;
}

void App::LockBeginSet() {
    lockInput.clear();
    lockErr = false;
    lockModal = 1;
    lockOpen = false;
    SetFocus(hwnd_);
    InvalidateAll();
}

void App::LockBeginChange() {
    lockInput.clear();
    lockErr = false;
    lockModal = 2;
    lockOpen = false;
    SetFocus(hwnd_);
    InvalidateAll();
}

void App::LockNow() {
    LockWriteFile(lockSalt_, lockHash_, true);   // survives a restart
    lockState_ = 2;
    LockCloseAll();
    StatusMsg(L10n::T(S::LockNowLocked));
    RefreshAfterProjectChange();
}

// 确定 of the passcode modal / of the locked popover
void App::LockApplyCode() {
    if (lockModal == 1 || lockModal == 3) {
        const int stage = lockModal;
        if (lockInput.size() != 4) {
            lockErr = true;
            InvalidateAll();
            return;
        }
        if (LockSaveCode(lockInput)) {
            LockCloseAll();
            StatusMsg(L10n::T(stage == 1 ? S::LockSetDone : S::LockChangedDone));
            SyncEdits();
            Layout();
            InvalidateAll();
        } else {
            lockErr = true;
            InvalidateAll();
        }
        return;
    }
    if (lockModal == 2) {
        if (LockCheck(lockInput)) {
            lockModal = 3;   // second stage: the new code
            lockInput.clear();
            lockErr = false;
        } else {
            lockErr = true;
            lockInput.clear();
        }
        InvalidateAll();
        return;
    }
    // the popover's 解锁 (only reachable while locked)
    if (lockState_ == 2) {
        if (LockCheck(lockInput)) {
            LockWriteFile(lockSalt_, lockHash_, false);
            lockState_ = 1;
            LockCloseAll();
            StatusMsg(L10n::T(S::LockUnlockedDone));
            RefreshAfterProjectChange();
        } else {
            lockErr = true;
            lockInput.clear();
            InvalidateAll();
        }
    }
}

// ---------------------------------------------------------------------------
// keyboard: digits, backspace, Enter (确定), Esc (cancel)
// ---------------------------------------------------------------------------
void App::LockKey(int vk) {
    auto push = [&](wchar_t d) {
        if (lockInput.size() < 4) lockInput.push_back(d);
        lockErr = false;
        InvalidateAll();
    };
    if (vk >= '0' && vk <= '9') {
        push((wchar_t)vk);
        return;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        push((wchar_t)(L'0' + (vk - VK_NUMPAD0)));
        return;
    }
    if (vk == VK_BACK) {
        if (!lockInput.empty()) lockInput.pop_back();
        lockErr = false;
        InvalidateAll();
        return;
    }
    if (vk == VK_RETURN) {
        LockApplyCode();
        return;
    }
    if (vk == VK_ESCAPE) {
        LockCloseAll();
        InvalidateAll();
        return;
    }
}

// ---------------------------------------------------------------------------
// geometry + painting
// ---------------------------------------------------------------------------
namespace {
// GDI+ objects are not copyable: the pen is configured in place
void SetPen(Gdiplus::Pen& p, const Gdiplus::Color& c, float w) {
    p.SetColor(c);
    p.SetWidth(w);
    p.SetLineJoin(Gdiplus::LineJoinRound);
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
}
} // namespace

// the padlock glyph, line art only, centred in `r`
static void PaintPadlockGlyph(Gdiplus::Graphics& g, const RECT& r,
                              const Gdiplus::Color& stroke, bool fillBody,
                              const Gdiplus::Color& bodyFill) {
    const float w = (float)(r.right - r.left);
    const float h = (float)(r.bottom - r.top);
    const float cx = (float)r.left + w * 0.5f;
    const float cy = (float)r.top + h * 0.5f;
    const float bw = w * 0.56f;              // body width
    const float bh = h * 0.46f;              // body height
    const float lw = (std::max)(1.f, h * 0.075f);
    Gdiplus::RectF body(cx - bw * 0.5f, cy - bh * 0.5f + h * 0.10f, bw, bh);
    const float rad = bw * 0.20f;
    // shackle: a half circle on top of the body
    const float sr = bw * 0.32f;             // shackle radius
    const float scy = cy - bh * 0.5f + h * 0.10f - h * 0.03f;
    {
        Gdiplus::Pen pen(stroke, lw);
        SetPen(pen, stroke, lw);
        Gdiplus::RectF arc(cx - sr, scy - sr, sr * 2.f, sr * 2.f);
        g.DrawArc(&pen, arc, 180.f, 180.f);
        // the two little legs that enter the body
        dr::Line(g, cx - sr, scy, cx - sr, scy + sr * 0.55f, stroke, lw);
        dr::Line(g, cx + sr, scy, cx + sr, scy + sr * 0.55f, stroke, lw);
    }
    if (fillBody) {
        dr::FillRound(g, body, rad, bodyFill);
    } else {
        dr::StrokeRound(g, body, rad, stroke, lw);
    }
    // keyhole
    const float kr = bw * 0.13f;
    const Gdiplus::Color keyCol = fillBody ? Gdiplus::Color(255, 255, 255, 255) : stroke;
    dr::Circle(g, cx, body.Y + body.Height * 0.42f, kr, keyCol, (std::max)(1.f, lw * 0.9f));
    dr::Line(g, cx, body.Y + body.Height * 0.42f + kr * 0.6f, cx,
             body.Y + body.Height * 0.78f, keyCol, (std::max)(1.f, lw * 0.9f));
}

void App::PaintLockChip(Gdiplus::Graphics& g) {
    if (L_.lockChip.right <= L_.lockChip.left) return;
    const int st = lockState_;
    const bool hover = (hoverLock == 1) || lockOpen;
    const Gdiplus::Color stroke =
        st == 2 ? dr::C(theme::kAccent)
                : (st == 1 ? dr::C(hover ? theme::kText : theme::kTextMuted)
                           : dr::C(hover ? theme::kTextMuted : theme::kTextFaint));
    if (st != 0 || hover)
        dr::FillRound(g, dr::RF(L_.lockChip), (float)D(7),
                      dr::C(lockOpen ? theme::kAccentSoft
                                     : (hover ? theme::kHover : theme::kPanelAlt)));
    if (st != 0 || hover)
        dr::StrokeRound(g, dr::RF(L_.lockChip), (float)D(7),
                        dr::C(hover ? theme::kBorderStrong : theme::kBorder), 1.f);
    RECT glyph = L_.lockChip;
    const int insetX = D(9), insetY = D(6);
    glyph.left += insetX;
    glyph.right -= insetX;
    glyph.top += insetY;
    glyph.bottom -= insetY;
    if (st == 2) {
        // locked: accent-filled body
        PaintPadlockGlyph(g, glyph, stroke, true, dr::C(theme::kAccent));
    } else {
        PaintPadlockGlyph(g, glyph, stroke, false, stroke);
    }
    if (st == 1) {
        // lock set and unlocked: the accent dot at the bottom-right corner
        const float r = (float)D(2.6);
        const float cx = (float)L_.lockChip.right - (float)D(7);
        const float cy = (float)L_.lockChip.bottom - (float)D(7);
        dr::FillRound(g, Gdiplus::RectF(cx - r, cy - r, r * 2.f, r * 2.f), r,
                      dr::C(theme::kAccent));
    }
}

void App::PaintLockPopover(Gdiplus::Graphics& g) {
    // the passcode modal comes first: while it is open it is the only thing the
    // padlock paints (and LockModalClick is the only input it accepts)
    L_.lockPop = L_.lockOk = L_.lockCancel = {};
    for (int i = 0; i < 3; ++i) L_.lockRow[i] = {};
    for (int i = 0; i < 4; ++i) L_.lockCells[i] = {};

    if (lockModal != 0) {
        const int w = D(340), h = D(190);
        const int x = (cw_ - w) / 2, y = (ch_ - h) / 2;
        L_.lmScrim = {0, 0, cw_, ch_};
        L_.lmPanel = {x, y, x + w, y + h};
        dr::Fill(g, L_.lmScrim, Gdiplus::Color(150, 240, 238, 233));
        dr::FillRound(g, dr::RF(L_.lmPanel), (float)D(10), dr::C(theme::kPanel));
        dr::StrokeRound(g, dr::RF(L_.lmPanel), (float)D(10),
                        dr::C(theme::kBorderStrong), 1.f);
        const S title = lockModal == 1 ? S::LockSetTitle
                        : lockModal == 2 ? S::LockChangeTitle
                                         : S::LockNewTitle;
        dr::Text(g, L10n::T(title), Font(11.f, true), dr::C(theme::kText),
                 RECT{x + D(20), y + D(14), x + w - D(20), y + D(44)}, 0, true);
        // four digit cells
        const int cellW = D(44), cellH = D(50), gap = D(12);
        const int totalW = cellW * 4 + gap * 3;
        int cx = x + (w - totalW) / 2;
        const int cy = y + D(62);
        for (int i = 0; i < 4; ++i) {
            L_.lmCells[i] = {cx, cy, cx + cellW, cy + cellH};
            const bool hoverCell = (hoverLmCell == i);
            dr::FillRound(g, dr::RF(L_.lmCells[i]), (float)D(7),
                          dr::C(hoverCell ? theme::kHover : theme::kField));
            dr::StrokeRound(g, dr::RF(L_.lmCells[i]), (float)D(7),
                            dr::C(lockErr ? theme::kDanger
                                          : (hoverCell ? theme::kBorderStrong
                                                       : theme::kBorder)),
                            1.f);
            if ((int)lockInput.size() > i) {
                const float r = (float)D(5);
                const float ccx = (float)(L_.lmCells[i].left + cellW / 2);
                const float ccy = (float)(L_.lmCells[i].top + cellH / 2);
                dr::FillRound(g, Gdiplus::RectF(ccx - r, ccy - r, r * 2, r * 2), r,
                              dr::C(theme::kText));
            }
            cx += cellW + gap;
        }
        if (lockErr)
            dr::Text(g, L10n::T(S::LockWrong), Font(9.f), dr::C(theme::kDanger),
                     RECT{x + D(20), cy + cellH + D(6), x + w - D(20),
                          cy + cellH + D(28)}, 1, true);
        // 确定 / 取消
        L_.lmOk = {x + w - D(196), y + h - D(52), x + w - D(108), y + h - D(18)};
        L_.lmCancel = {x + w - D(100), y + h - D(52), x + w - D(20), y + h - D(18)};
        dr::FillRound(g, dr::RF(L_.lmOk), (float)D(7),
                      dr::C(lockModalHoverOk ? theme::kAccentSoft
                                             : theme::kAccent));
        dr::Text(g, L10n::T(S::LockOk), Font(9.5f), dr::C(0xFFFFFF), L_.lmOk, 1, true);
        dr::FillRound(g, dr::RF(L_.lmCancel), (float)D(7),
                      dr::C(lockModalHoverCancel ? theme::kHover : theme::kPanelAlt));
        dr::StrokeRound(g, dr::RF(L_.lmCancel), (float)D(7),
                        dr::C(theme::kBorderStrong), 1.f);
        dr::Text(g, L10n::T(S::LockCancel), Font(9.5f), dr::C(theme::kTextMuted),
                 L_.lmCancel, 1, true);
        return;
    }
    L_.lmScrim = L_.lmPanel = {};
    for (int i = 0; i < 4; ++i) L_.lmCells[i] = {};
    L_.lmOk = L_.lmCancel = {};

    if (!lockOpen) return;

    // ---- the popover under the chip --------------------------------------
    const int rowH = D(28);
    const int w = D(212);
    // rows: 设置密码… (no lock) | 修改密码… + 锁定 (code set) | 解锁 + cells + 确定
    // (+ 打开… / 另存为… while locked)
    const bool lockedView = (lockState_ == 2);
    // the height is the sum of the very increments the painter below uses, so the
    // panel never carries an empty tail (or clips the 另存为… row)
    int h = 0;
    if (!lockedView) {
        const int rows = (lockState_ == 1) ? 2 : 1;
        h = D(6) + rows * rowH + D(6);
    } else {
        h = D(6) +                       // top padding
            D(20) +                      // 「解锁」 caption line
            D(34) +                      // the four digit cells
            D(4) +                       // gap under the cells
            (lockErr ? D(18) : 0) +      // the red hint
            D(30) +                      // 确定
            2 * rowH +                   // 打开… / 另存为…
            D(6);                        // bottom padding
    }
    int x = L_.lockChip.right - w;
    if (x < D(8)) x = D(8);
    if (x + w > cw_ - D(8)) x = cw_ - D(8) - w;
    int y = L_.lockChip.bottom + D(4);
    if (y + h > ch_ - D(30)) y = (std::max)(D(4), ch_ - D(30) - h);
    L_.lockPop = {x, y, x + w, y + h};
    dr::FillRound(g, dr::RF(L_.lockPop), (float)D(8), dr::C(theme::kPanel));
    dr::StrokeRound(g, dr::RF(L_.lockPop), (float)D(8),
                    dr::C(theme::kBorderStrong), 1.f);
    int cy = y + D(6);
    auto rowRect = [&](int i) {
        return RECT{x + D(5), cy + i * rowH, x + w - D(5), cy + (i + 1) * rowH};
    };
    auto drawRow = [&](int i, const wchar_t* label) {
        RECT r = rowRect(i);
        if (lockHoverRow == i)
            dr::FillRound(g, dr::RF(r), (float)D(5), dr::C(theme::kHover));
        dr::Text(g, label, Font(9.5f), dr::C(theme::kText),
                 RECT{r.left + D(12), r.top, r.right - D(8), r.bottom}, 0, true);
    };
    if (lockState_ == 0) {
        drawRow(0, L10n::T(S::LockSet));
        L_.lockRow[0] = rowRect(0);
        return;
    }
    if (lockState_ == 1) {
        drawRow(0, L10n::T(S::LockChange));
        drawRow(1, L10n::T(S::LockLockNow));
        L_.lockRow[0] = rowRect(0);
        L_.lockRow[1] = rowRect(1);
        return;
    }
    // ---- locked: 「解锁」 caption, cells, 确定, then 打开… / 另存为… ----
    dr::Text(g, L10n::T(S::LockUnlock), Font(9.5f, true), dr::C(theme::kText),
             RECT{x + D(12), cy, x + w - D(12), cy + D(18)}, 0, true);
    cy += D(20);
    {
        const int cellW = D(34), cellH = D(34), cgap = D(8);
        const int total = cellW * 4 + cgap * 3;
        int sx = x + (w - total) / 2;
        for (int i = 0; i < 4; ++i) {
            L_.lockCells[i] = {sx, cy, sx + cellW, cy + cellH};
            dr::FillRound(g, dr::RF(L_.lockCells[i]), (float)D(6),
                          dr::C(hoverLockCell == i ? theme::kHover
                                                       : theme::kField));
            dr::StrokeRound(g, dr::RF(L_.lockCells[i]), (float)D(6),
                            dr::C(lockErr ? theme::kDanger : theme::kBorder), 1.f);
            if ((int)lockInput.size() > i) {
                const float r = (float)D(4);
                const float ccx = (float)(L_.lockCells[i].left + cellW / 2);
                const float ccy = (float)(L_.lockCells[i].top + cellH / 2);
                dr::FillRound(g, Gdiplus::RectF(ccx - r, ccy - r, r * 2, r * 2), r,
                              dr::C(theme::kText));
            }
            sx += cellW + cgap;
        }
        cy += cellH + D(4);
        if (lockErr)
            dr::Text(g, L10n::T(S::LockWrong), Font(8.5f), dr::C(theme::kDanger),
                     RECT{x + D(8), cy, x + w - D(8), cy + D(16)}, 1, true);
        cy += lockErr ? D(18) : 0;
    }
    {
        RECT ok = {x + w - D(96), cy, x + w - D(8), cy + D(24)};
        L_.lockOk = ok;
        dr::FillRound(g, dr::RF(ok), (float)D(6),
                      dr::C(hoverLockOk ? theme::kAccentSoft : theme::kAccent));
        dr::Text(g, L10n::T(S::LockOk), Font(9.f), dr::C(0xFFFFFF), ok, 1, true);
        cy += D(30);
    }
    // 打开… / 另存为… stay available while locked (read-only viewer promise)
    {
        RECT r0 = {x + D(5), cy, x + w - D(5), cy + rowH};
        if (lockHoverRow == 0)
            dr::FillRound(g, dr::RF(r0), (float)D(5), dr::C(theme::kHover));
        dr::Text(g, std::wstring(L10n::T(S::FnOpen)) + L"…", Font(9.5f),
                 dr::C(theme::kText),
                 RECT{r0.left + D(12), r0.top, r0.right - D(8), r0.bottom}, 0, true);
        L_.lockRow[0] = r0;
        RECT r1 = {x + D(5), cy + rowH, x + w - D(5), cy + rowH * 2};
        if (lockHoverRow == 1)
            dr::FillRound(g, dr::RF(r1), (float)D(5), dr::C(theme::kHover));
        dr::Text(g, L10n::T(S::FnSaveAs), Font(9.5f), dr::C(theme::kText),
                 RECT{r1.left + D(12), r1.top, r1.right - D(8), r1.bottom}, 0, true);
        L_.lockRow[1] = r1;
    }
}

// ---------------------------------------------------------------------------
// input
// ---------------------------------------------------------------------------
bool App::LockModalClick(int x, int y) {
    POINT p = {x, y};
    for (int i = 0; i < 4; ++i) {
        if (PtInRect(&L_.lmCells[i], p)) {
            SetFocus(hwnd_);
            return true;
        }
    }
    if (PtInRect(&L_.lmOk, p)) {
        LockApplyCode();
        return true;
    }
    if (PtInRect(&L_.lmCancel, p)) {
        LockCloseAll();
        InvalidateAll();
        return true;
    }
    // the scrim swallows everything (the modal is exclusive); a click outside the
    // panel cancels it, exactly like 取消
    if (!PtInRect(&L_.lmPanel, p)) {
        LockCloseAll();
        InvalidateAll();
    }
    return true;
}

bool App::LockClick(int x, int y) {
    POINT p = {x, y};
    if (lockModal != 0) {
        LockModalClick(x, y);
        return true;
    }
    if (PtInRect(&L_.lockChip, p)) {
        lockOpen = !lockOpen;
        lockInput.clear();
        lockErr = false;
        lockHoverRow = -1;
        if (lockModal != 0) lockModal = 0;
        if (lockOpen) SetFocus(hwnd_);
        InvalidateAll();
        return true;
    }
    if (!lockOpen) return false;
    if (!PtInRect(&L_.lockPop, p)) {
        // a press outside closes the popover and continues with the normal hit
        // tests, so the very same click can still reach what it was aimed at
        lockOpen = false;
        lockInput.clear();
        lockErr = false;
        InvalidateAll();
        return false;
    }
    if (lockState_ == 2) {
        for (int i = 0; i < 4; ++i)
            if (PtInRect(&L_.lockCells[i], p)) {
                SetFocus(hwnd_);
                return true;
            }
        if (PtInRect(&L_.lockOk, p)) {
            LockApplyCode();
            return true;
        }
        if (PtInRect(&L_.lockRow[0], p)) {
            lockOpen = false;
            OpenProjectDialog();
            InvalidateAll();
            return true;
        }
        if (PtInRect(&L_.lockRow[1], p)) {
            lockOpen = false;
            SaveProjectDialog(true);   // 另存为 is always allowed
            InvalidateAll();
            return true;
        }
        return true;   // the rest of the popover is chrome
    }
    if (lockState_ == 0) {
        if (PtInRect(&L_.lockRow[0], p)) {
            LockBeginSet();
            return true;
        }
        return true;
    }
    // code set, unlocked
    if (PtInRect(&L_.lockRow[0], p)) {
        LockBeginChange();
        return true;
    }
    if (PtInRect(&L_.lockRow[1], p)) {
        LockNow();
        return true;
    }
    return true;
}
