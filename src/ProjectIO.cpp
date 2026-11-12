#include "App.h"
#include "Catalog.h"
#include "Draw.h"
#include "L10n.h"

#include <algorithm>
#include <commdlg.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

// ---------------------------------------------------------------------------
// .smproj — the project file.
//
// UTF-8, one record per line, "key value" style, text fields double quoted
// (with \\ \" \n \r \t escapes), no external libraries.  Example:
//
//   SMPROJ 1
//   piece "Brahms · Symphony No. 4"
//   stage 14.0 10.0 5.0
//   colors 3A3732 554C42 33302B FBF9F4
//   item 12 1 "violin" 4.900 8.200 0 1.500 0 0 1 0 0.600 0.450 "Violin I.1" ...
//   device 1 3 -5.000 -0.500 "Sound card" "8ch" 8 "" 1 8 1 1
//
// Unknown keys are ignored and short records fall back to defaults, so an old
// file still opens after the format grows.
// ---------------------------------------------------------------------------

namespace {

// ---- UTF-8 <-> UTF-16 -----------------------------------------------------
std::string ToU8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                      nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr,
                        nullptr);
    return s;
}

std::wstring FromU8(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// ---- field quoting --------------------------------------------------------
std::wstring Quote(const std::wstring& s) {
    std::wstring o = L"\"";
    for (wchar_t c : s) {
        switch (c) {
        case L'\\': o += L"\\\\"; break;
        case L'"': o += L"\\\""; break;
        case L'\n': o += L"\\n"; break;
        case L'\r': o += L"\\r"; break;
        case L'\t': o += L"\\t"; break;
        default: o += c; break;
        }
    }
    o += L"\"";
    return o;
}

// Split one line into fields: whitespace separated, double quotes group them.
std::vector<std::wstring> Fields(const std::wstring& line) {
    std::vector<std::wstring> out;
    std::wstring cur;
    bool inQuote = false;
    bool started = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const wchar_t c = line[i];
        if (inQuote) {
            if (c == L'\\' && i + 1 < line.size()) {
                const wchar_t n = line[++i];
                cur += (n == L'n') ? L'\n'
                     : (n == L'r') ? L'\r'
                     : (n == L't') ? L'\t'
                                   : n;
            } else if (c == L'"') {
                inQuote = false;
            } else {
                cur += c;
            }
            continue;
        }
        if (c == L'"') {
            inQuote = true;
            started = true;
        } else if (c == L' ' || c == L'\t') {
            if (started || !cur.empty()) {
                out.push_back(cur);
                cur.clear();
                started = false;
            }
        } else {
            cur += c;
        }
    }
    if (started || !cur.empty()) out.push_back(cur);
    return out;
}

double Num(const std::vector<std::wstring>& f, size_t i, double def) {
    if (i >= f.size() || f[i].empty()) return def;
    wchar_t* end = nullptr;
    const double v = std::wcstod(f[i].c_str(), &end);
    return (end == f[i].c_str()) ? def : v;
}
int Int(const std::vector<std::wstring>& f, size_t i, int def) {
    return (int)std::lround(Num(f, i, (double)def));
}
std::wstring Str(const std::vector<std::wstring>& f, size_t i) {
    return i < f.size() ? f[i] : std::wstring();
}

std::vector<std::wstring> SplitLines(const std::wstring& text) {
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : text) {
        if (c == L'\n') {
            if (!cur.empty() && cur.back() == L'\r') cur.pop_back();
            out.push_back(cur);
            cur.clear();
        } else if (c != L'\r') {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// save
// ---------------------------------------------------------------------------
// The whole project as the .smproj TEXT.  The file writer below and the undo
// snapshots both use this, so a snapshot is exactly what a save would write.
std::wstring SerializeProject(const Project& pr) {
    std::wstring t;
    auto line = [&](const std::wstring& s) {
        t += s;
        t += L"\n";
    };
    auto num = [](double v) { return dr::Fmt(L"%.3f", v); };

    line(L"SMPROJ 1");
    line(L"# Stage Micing project — UTF-8, one record per line");
    line(L"piece " + Quote(pr.piece));
    line(L"venue " + Quote(pr.venue));
    line(L"conductor " + Quote(pr.conductor));
    line(L"sysengineer " + Quote(pr.sysEngineer));
    line(L"recengineer " + Quote(pr.recEngineer));
    line(L"asstengineer " + Quote(pr.asstEngineer));
    line(L"notes " + Quote(pr.notes));
    line(L"stage " + num(pr.stage.w) + L" " + num(pr.stage.d) + L" " +
         num(pr.stage.off));
    line(L"ceiling " + num(pr.ceiling));
    line(dr::Fmt(L"colors %02X%02X%02X %02X%02X%02X %02X%02X%02X %02X%02X%02X",
                 pr.instColor.r, pr.instColor.g, pr.instColor.b, pr.micColor.r,
                 pr.micColor.g, pr.micColor.b, pr.textColor.r, pr.textColor.g,
                 pr.textColor.b, pr.stageFill.r, pr.stageFill.g, pr.stageFill.b));
    line(dr::Fmt(L"samplerate %d", pr.sampleRate));
    line(std::wstring(L"showinstruments ") + (pr.showInstruments ? L"1" : L"0"));
    line(std::wstring(L"showmics ") + (pr.showMics ? L"1" : L"0"));
    // TEXT VISIBILITY (two independent switches).  Both default to ON, so a file
    // written before they existed loads with every label visible.
    line(std::wstring(L"showinstrtext ") + (pr.showInstrumentText ? L"1" : L"0"));
    line(std::wstring(L"showmictext ") + (pr.showMicText ? L"1" : L"0"));
    // 导出图片 resolution chosen in the chip's scale popup (2 / 4 / 8)
    line(dr::Fmt(L"pngscale %d", pr.pngScale));
    // 通道表 「话筒线长度」 mode: 0 = 推荐 (standard size), 1 = 实际 (the drawn run)
    line(dr::Fmt(L"lenmode %d", pr.lenMode == 1 ? 1 : 0));
    line(dr::Fmt(L"nextid %d", pr.nextId));
    line(dr::Fmt(L"nextdeviceid %d", pr.nextDeviceId));

    for (const auto& it : pr.items) {
        std::wstring s = L"item ";
        s += dr::Fmt(L"%d %d ", it.id, (int)it.kind);
        s += Quote(it.defId);
        s += L" " + num(it.x) + L" " + num(it.y) + L" " + num(it.angle) + L" " +
             num(it.height) + L" " + num(it.spacing) + L" " + num(it.center);
        s += dr::Fmt(L" %d %d", it.pattern, it.mainTech);
        s += L" " + num(it.w) + L" " + num(it.h);
        s += L" " + Quote(it.label) + L" " + Quote(it.model) + L" " +
             Quote(it.modelL) + L" " + Quote(it.modelR) + L" " + Quote(it.modelC);
        s += L" " + Quote(it.spotType) + L" " + Quote(it.remark);
        // phase 3: flown flag + height ride at the end of the record
        s += dr::Fmt(L" %d ", it.flown ? 1 : 0) + num(it.flyHeight);
    // P5: the manual cable length override (0 = automatic)
    s += L" " + num(it.cableLenOverride);
        // per-item colour override: a trailing `color 0xRRGGBB` pair (0 = the
        // muted palette slot that defId owns), so the record stays self-describing
        // and an older file simply has no pair
        s += dr::Fmt(L" color 0x%06X", it.colorOverride & 0xFFFFFFu);
        line(s);
    }

    for (const auto& g : pr.micGroups) {
        std::wstring s = dr::Fmt(L"micgroup %d ", g.kind) + Quote(g.tech);
        s += dr::Fmt(L" %d", (int)g.ids.size());
        for (int id : g.ids) s += dr::Fmt(L" %d", id);
        line(s);
    }

    {
        std::wstring s = dr::Fmt(L"mainmics %d", (int)pr.mainMics.size());
        for (int id : pr.mainMics) s += dr::Fmt(L" %d", id);
        line(s);
    }

    // chorder <count> <itemId> ... — the MANUAL channel-table row order the user
    // dragged into place.  Written even when empty, so a saved file states it.
    {
        std::wstring s = dr::Fmt(L"chorder %d", (int)pr.channelOrder.size());
        for (int id : pr.channelOrder) s += dr::Fmt(L" %d", id);
        line(s);
    }

    for (const auto& d : pr.devices) {
        std::wstring s = L"device ";
        s += dr::Fmt(L"%d %d ", d.id, (int)d.kind);
        s += num(d.x) + L" " + num(d.y) + L" " + num(d.angle) + L" ";
        s += Quote(d.label) + L" " + Quote(d.model);
        s += dr::Fmt(L" %d ", d.channels);
        // the protocol field is written empty so the record layout (and therefore
        // every old .smproj) stays readable: there are no protocols any more
        s += Quote(L"");
        s += dr::Fmt(L" %d %d %d %d", d.ports, d.channelsPerPort, d.faceDir,
                     d.selected ? 1 : 0);
        // phase 4: a locked device (file compatibility only: nothing sets it)
        s += dr::Fmt(L" %d", d.locked ? 1 : 0);
        line(s);
    }

    // link <aKind> <aId> <bKind> <bId> <channels> "" <npoints> x y ... <manual>
    for (const auto& l : pr.links) {
        std::wstring s = L"link ";
        s += dr::Fmt(L"%d %d %d %d %d ", l.aKind, l.a, l.bKind, l.b, l.channels);
        s += Quote(L"");   // protocol: always empty (kept for the record layout)
        s += dr::Fmt(L" %d", (int)l.path.size());
        for (const auto& pt : l.path) s += L" " + num(pt.first) + L" " + num(pt.second);
        // phase 2: the manual flag rides at the END of the record, so a file
        // written by phase 1 still loads (missing field = automatic = false)
        s += dr::Fmt(L" %d", l.manual ? 1 : 0);
        // phase 3: flown model (ceiling / wall legs cannot live in the polyline)
        s += dr::Fmt(L" %d %d ", l.flown ? 1 : 0, l.ceilSegs);
        s += num(l.legUp) + L" " + num(l.legCeil) + L" " + num(l.legWall) + L" " +
             num(l.legFloor);
        // phase 3b: explicit channel override (absent = derived from the endpoints)
        s += dr::Fmt(L" %d", l.channelsUser ? 1 : 0);
        line(s);
    }


    return t;
}

bool SaveProjectFile(const Project& pr, const std::wstring& path,
                     std::wstring& err) {
    const std::wstring t = SerializeProject(pr);
    std::ofstream f(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!f) {
        err = L"cannot write " + path;
        return false;
    }
    const std::string bytes = ToU8(t);
    f.write(bytes.data(), (std::streamsize)bytes.size());
    if (!f.good()) {
        err = L"write error " + path;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------
// Parse a .smproj TEXT into `pr` (built into a scratch project: nothing is
// committed on error).  Shared by the file loader and the undo snapshots.
bool ParseProject(const std::wstring& text, Project& pr, std::wstring& err,
                  int* skippedUnknown) {
    if (skippedUnknown) *skippedUnknown = 0;
    const std::vector<std::wstring> lines = SplitLines(text);

    Project np;                       // build into a scratch project, commit at the end
    np.items.clear();
    np.micGroups.clear();
    np.mainMics.clear();
    np.devices.clear();
    np.nextId = 1;
    np.nextDeviceId = 1;
    np.selected = -1;

    bool header = false;
    int lineNo = 0;
    for (const std::wstring& raw : lines) {
        ++lineNo;
        // trim
        size_t a = raw.find_first_not_of(L" \t");
        if (a == std::wstring::npos) continue;
        size_t b = raw.find_last_not_of(L" \t");
        std::wstring text2 = raw.substr(a, b - a + 1);
        if (text2.empty() || text2[0] == L'#') continue;

        const std::vector<std::wstring> f2 = Fields(text2);
        if (f2.empty()) continue;
        const std::wstring key = f2[0];

        if (!header) {
            if (key != L"SMPROJ") {
                err = L"line 1: not a Stage Micing project (missing SMPROJ header)";
                return false;
            }
            header = true;
            continue;
        }

        if (key == L"piece") np.piece = Str(f2, 1);
        else if (key == L"venue") np.venue = Str(f2, 1);
        else if (key == L"conductor") np.conductor = Str(f2, 1);
        else if (key == L"sysengineer") np.sysEngineer = Str(f2, 1);
        else if (key == L"recengineer") np.recEngineer = Str(f2, 1);
        else if (key == L"asstengineer") np.asstEngineer = Str(f2, 1);
        else if (key == L"notes") np.notes = Str(f2, 1);
        else if (key == L"stage") {
            np.stage.w = std::clamp(Num(f2, 1, np.stage.w), 4.0, 60.0);
            np.stage.d = std::clamp(Num(f2, 2, np.stage.d), 4.0, 60.0);
            np.stage.off = std::clamp(Num(f2, 3, np.stage.off), 0.0, 30.0);
        } else if (key == L"ceiling") {
            np.ceiling = std::clamp(Num(f2, 1, np.ceiling), 3.0, 20.0);
        } else if (key == L"colors") {
            auto hex = [&](size_t i, std::uint8_t def) -> std::uint8_t {
                if (i >= f2.size() || f2[i].size() < 6) return def;
                return (std::uint8_t)wcstoul(f2[i].c_str(), nullptr, 16);
            };
            // each record is RRGGBB, read the three channels back out
            auto col = [&](size_t i, Rgb def) {
                if (i >= f2.size() || f2[i].size() < 6) return def;
                const unsigned v = (unsigned)wcstoul(f2[i].c_str(), nullptr, 16);
                Rgb c;
                c.r = (std::uint8_t)((v >> 16) & 0xFF);
                c.g = (std::uint8_t)((v >> 8) & 0xFF);
                c.b = (std::uint8_t)(v & 0xFF);
                return c;
            };
            np.instColor = col(1, np.instColor);
            np.micColor = col(2, np.micColor);
            np.textColor = col(3, np.textColor);
            np.stageFill = col(4, np.stageFill);
            (void)hex;
        } else if (key == L"samplerate") {
            const int sr = Int(f2, 1, np.sampleRate);
            if (SampleRateIndex(sr) >= 0) np.sampleRate = sr;
        } else if (key == L"showinstruments") {
            np.showInstruments = Int(f2, 1, 1) != 0;
        } else if (key == L"showmics") {
            np.showMics = Int(f2, 1, 1) != 0;
        } else if (key == L"showinstrtext") {
            np.showInstrumentText = Int(f2, 1, 1) != 0;
        } else if (key == L"showmictext") {
            np.showMicText = Int(f2, 1, 1) != 0;
        } else if (key == L"pngscale") {
            // only the three offered resolutions are accepted; anything else keeps
            // the default so a hand-edited file can never ask for 1000x
            const int ps = Int(f2, 1, np.pngScale);
            if (ps == 2 || ps == 4 || ps == 8) np.pngScale = ps;
        } else if (key == L"lenmode") {
            // 0 = 推荐 (standard cable size), 1 = 实际 (the drawn run); an old file
            // without the record loads as 推荐, i.e. exactly the previous numbers
            np.lenMode = Int(f2, 1, 0) == 1 ? 1 : 0;
        } else if (key == L"nextid") {
            np.nextId = (std::max)(1, Int(f2, 1, 1));
        } else if (key == L"nextdeviceid") {
            np.nextDeviceId = (std::max)(1, Int(f2, 1, 1));
        } else if (key == L"item") {
            Item it;
            it.id = Int(f2, 1, 0);
            it.kind = Int(f2, 2, 0) == 0 ? ItemKind::Instrument : ItemKind::Mic;
            it.defId = Str(f2, 3);
            it.x = Num(f2, 4, 0.0);
            it.y = Num(f2, 5, 0.0);
            it.angle = Num(f2, 6, 0.0);
            it.height = Num(f2, 7, 1.5);
            it.spacing = Num(f2, 8, 0.0);
            it.center = Num(f2, 9, 0.0);
            it.pattern = Int(f2, 10, 1);
            it.mainTech = Int(f2, 11, 0);
            it.w = Num(f2, 12, 0.0);
            it.h = Num(f2, 13, 0.0);
            it.label = Str(f2, 14);
            it.model = Str(f2, 15);
            it.modelL = Str(f2, 16);
            it.modelR = Str(f2, 17);
            it.modelC = Str(f2, 18);
            it.spotType = Str(f2, 19);
            it.remark = Str(f2, 20);
            // phase 3: flown flag + height (absent in an older file)
            it.flown = Int(f2, 21, 0) != 0;
            it.flyHeight = std::clamp(Num(f2, 22, it.flyHeight), 0.30, 20.0);
        // P5: manual cable length (absent in an older file = automatic)
        it.cableLenOverride = std::clamp(Num(f2, 23, 0.0), 0.0, 500.0);
            // per-item colour override: a trailing `color 0xRRGGBB` pair, searched
            // by key so the fixed positional fields above never shift (an older
            // file has no pair → 0 → the palette slot)
            for (size_t ci = 24; ci + 1 < f2.size(); ++ci) {
                if (f2[ci] != L"color") continue;
                it.colorOverride = (unsigned)wcstoul(f2[ci + 1].c_str(), nullptr, 16) &
                                   0xFFFFFFu;
                break;
            }
            if (it.id <= 0) it.id = np.nextId;
            // An item whose type the catalogue no longer knows is SKIPPED on
            // load (the app retired the 管风琴 / organ): nothing could draw it,
            // click it or delete it, so keeping it would leave an invisible
            // ghost on the stage.  `skippedUnknown` carries the count back to
            // the caller, which reports it in the status bar.
            if (!it.defId.empty() && !FindItem(it.defId)) {
                if (skippedUnknown) ++*skippedUnknown;
                continue;
            }
            if (!it.defId.empty()) np.items.push_back(it);
        } else if (key == L"micgroup") {
            MicGroup g;
            g.kind = Int(f2, 1, 0);
            g.tech = Str(f2, 2);
            const int n = (std::max)(0, Int(f2, 3, 0));
            for (int i = 0; i < n && (size_t)(4 + i) < f2.size(); ++i) {
                const int id = Int(f2, (size_t)(4 + i), 0);
                if (id > 0) g.ids.push_back(id);
            }
            if (g.ids.size() >= 2) np.micGroups.push_back(g);
        } else if (key == L"mainmics") {
            const int n = (std::max)(0, Int(f2, 1, 0));
            np.mainMics.clear();
            for (int i = 0; i < n && (size_t)(2 + i) < f2.size(); ++i) {
                const int id = Int(f2, (size_t)(2 + i), 0);
                if (id > 0) np.mainMics.push_back(id);
            }
        } else if (key == L"chorder") {
            // manual channel-table row order (absent in an older file = automatic)
            const int n = (std::max)(0, Int(f2, 1, 0));
            np.channelOrder.clear();
            for (int i = 0; i < n && (size_t)(2 + i) < f2.size(); ++i) {
                const int id = Int(f2, (size_t)(2 + i), 0);
                if (id > 0) np.channelOrder.push_back(id);
            }
        } else if (key == L"device") {
            Device d;
            d.id = Int(f2, 1, 0);
            const int k = Int(f2, 2, 1);
            d.kind = (DevKind)std::clamp(k, 0, 5);
            d.x = std::clamp(Num(f2, 3, 0.0), -200.0, 200.0);
            d.y = std::clamp(Num(f2, 4, 0.0), -200.0, 200.0);
            d.angle = Num(f2, 5, 0.0);
            d.label = Str(f2, 6);
            d.model = Str(f2, 7);
            d.channels = std::clamp(Int(f2, 8, 8), 1, 64);
            d.ports = std::clamp(Int(f2, 10, 0), 0, 8);
            d.channelsPerPort = std::clamp(Int(f2, 11, 0), 0, 64);
            d.faceDir = ((Int(f2, 12, 0) % 4) + 4) % 4;
            d.selected = Int(f2, 13, 0) != 0;
            // phase 4: a locked device (absent in an older file = false)
            d.locked = Int(f2, 14, 0) != 0;
            if (d.id <= 0) d.id = np.nextDeviceId;
            np.devices.push_back(d);
        } else if (key == L"link") {
            CableLink l;
            l.aKind = std::clamp(Int(f2, 1, 0), 0, 1);
            l.a = Int(f2, 2, 0);
            l.bKind = std::clamp(Int(f2, 3, 0), 0, 1);
            l.b = Int(f2, 4, 0);
            l.channels = std::clamp(Int(f2, 5, 1), 1, 64);
            const int n = std::clamp(Int(f2, 7, 0), 0, 16);
            for (int i = 0; i < n; ++i) {
                const size_t ix = (size_t)(8 + 2 * i);
                if (ix + 1 >= f2.size()) break;
                l.path.emplace_back(Num(f2, ix, 0.0), Num(f2, ix + 1, 0.0));
            }
            // trailing manual flag (absent in a phase-1 file = automatic)
            l.manual = Int(f2, (size_t)(8 + 2 * n), 0) != 0;
            // phase 3: the flown model's legs
            l.flown = Int(f2, (size_t)(9 + 2 * n), 0) != 0;
            l.ceilSegs = std::clamp(Int(f2, (size_t)(10 + 2 * n), 0), 0, 16);
            l.legUp = Num(f2, (size_t)(11 + 2 * n), 0.0);
            l.legCeil = Num(f2, (size_t)(12 + 2 * n), 0.0);
            l.legWall = Num(f2, (size_t)(13 + 2 * n), 0.0);
            l.legFloor = Num(f2, (size_t)(14 + 2 * n), 0.0);
            // phase 3b: explicit channel override (absent in an older file: the
            // channels are derived from the endpoints)
            l.channelsUser = Int(f2, (size_t)(15 + 2 * n), 0) != 0;
            np.links.push_back(l);
        } else if (key == L"topology") {
            // field 1 = valid; the old files also carried a "stale" flag which is
            // gone now (the topology is always derived from the links)
            np.topo.valid = Int(f2, 1, 0) != 0;
        } else if (key == L"topolink") {
            CableLink l;
            l.aKind = std::clamp(Int(f2, 1, 0), 0, 1);
            l.a = Int(f2, 2, 0);
            l.bKind = std::clamp(Int(f2, 3, 0), 0, 1);
            l.b = Int(f2, 4, 0);
            l.channels = std::clamp(Int(f2, 5, 1), 1, 64);
            const int n = std::clamp(Int(f2, 7, 0), 0, 16);
            for (int i = 0; i < n; ++i) {
                const size_t ix = (size_t)(8 + 2 * i);
                if (ix + 1 >= f2.size()) break;
                l.path.emplace_back(Num(f2, ix, 0.0), Num(f2, ix + 1, 0.0));
            }
            l.manual = Int(f2, (size_t)(8 + 2 * n), 0) != 0;
            l.flown = Int(f2, (size_t)(9 + 2 * n), 0) != 0;
            l.ceilSegs = std::clamp(Int(f2, (size_t)(10 + 2 * n), 0), 0, 16);
            l.legUp = Num(f2, (size_t)(11 + 2 * n), 0.0);
            l.legCeil = Num(f2, (size_t)(12 + 2 * n), 0.0);
            l.legWall = Num(f2, (size_t)(13 + 2 * n), 0.0);
            l.legFloor = Num(f2, (size_t)(14 + 2 * n), 0.0);
            l.channelsUser = Int(f2, (size_t)(15 + 2 * n), 0) != 0;
            np.topo.links.push_back(l);
        }
        // everything else is ignored on purpose
    }

    if (!header) {
        err = L"empty file or missing SMPROJ header";
        return false;
    }

    // ---- the merged cabling model -------------------------------------------
    // A digital stage box IS a stagebox now: keep its channels, folding the old
    // ports × per-port budget into the input count.  An ADDA is gone from the
    // model entirely, so the device is dropped — and every cable that referenced
    // it disappears with it (the dangling-link filter below sees a missing end).
    {
        std::vector<Device> kept;
        kept.reserve(np.devices.size());
        for (Device& d : np.devices) {
            if (DeviceIsStagebox(d.kind)) {
                d.kind = DevKind::AnalogBox;
                const int portCh = (std::max)(0, d.ports) * (std::max)(0, d.channelsPerPort);
                d.channels = std::clamp((std::max)(d.channels, portCh), 1, 64);
                d.ports = 0;
                d.channelsPerPort = 0;
            } else if (d.kind == DevKind::Adda) {
                continue;   // ADDA: dropped, with all of its links
            }
            kept.push_back(d);
        }
        np.devices.swap(kept);
    }

    // never let a stale id collide with a new object
    int maxItem = 0;
    for (const auto& it : np.items) maxItem = (std::max)(maxItem, it.id);
    int maxDev = 0;
    for (const auto& d : np.devices) maxDev = (std::max)(maxDev, d.id);
    np.nextId = (std::max)(np.nextId, maxItem + 1);
    np.nextDeviceId = (std::max)(np.nextDeviceId, maxDev + 1);

    // groups / main mics may only reference items that exist
    auto exists = [&](int id) {
        for (const auto& it : np.items)
            if (it.id == id) return true;
        return false;
    };
    for (auto& g : np.micGroups)
        g.ids.erase(std::remove_if(g.ids.begin(), g.ids.end(),
                                   [&](int id) { return !exists(id); }),
                    g.ids.end());
    np.micGroups.erase(std::remove_if(np.micGroups.begin(), np.micGroups.end(),
                                      [](const MicGroup& g) {
                                          return g.ids.size() < 2;
                                      }),
                       np.micGroups.end());
    np.mainMics.erase(std::remove_if(np.mainMics.begin(), np.mainMics.end(),
                                     [&](int id) { return !exists(id); }),
                      np.mainMics.end());
    // the manual channel-table order may only name objects that still exist, and
    // an id may appear once
    {
        std::vector<int> kept;
        for (int id : np.channelOrder)
            if (exists(id) && std::find(kept.begin(), kept.end(), id) == kept.end())
                kept.push_back(id);
        np.channelOrder.swap(kept);
    }

    // cables may only reference objects that still exist
    np.links.erase(std::remove_if(np.links.begin(), np.links.end(),
                                  [&](const CableLink& l) {
                                      return !ResolveCableEnd(np, l.aKind, l.a).ok ||
                                             !ResolveCableEnd(np, l.bKind, l.b).ok;
                                  }),
                   np.links.end());
    // every cable path and the derived topology are rebuilt from the endpoints
    RebuildAllCablePaths(np);

    pr = np;
    err.clear();
    return true;
}

bool LoadProjectFile(Project& pr, const std::wstring& path, std::wstring& err,
                     int* skippedUnknown) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f) {
        err = path;
        return false;
    }
    std::string bytes((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    // optional UTF-8 BOM
    if (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF &&
        (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF) {
        bytes.erase(0, 3);
    }
    return ParseProject(FromU8(bytes), pr, err, skippedUnknown);
}

// ---------------------------------------------------------------------------
// App side: dialogs
// ---------------------------------------------------------------------------
void App::UpdateTitle() {
    std::wstring t = L10n::IsZh()
                         ? (std::wstring(L"Stage Micing — ") + L10n::T(S::AppName))
                         : L"Stage Micing";
    if (!projectPath_.empty()) {
        const size_t slash = projectPath_.find_last_of(L"\\/");
        t += L" · ";
        t += (slash == std::wstring::npos) ? projectPath_ : projectPath_.substr(slash + 1);
    }
    SetWindowTextW(hwnd_, t.c_str());
}

bool App::SaveProjectTo(const std::wstring& path) {
    std::wstring err;
    if (!SaveProjectFile(pr_, path, err)) {
        const std::wstring msg = std::wstring(L10n::T(S::FileSaveErr)) + err;
        MessageBoxW(hwnd_, msg.c_str(), L"Stage Micing", MB_OK | MB_ICONWARNING);
        return false;
    }
    projectPath_ = path;
    dirty_ = false;   // the file on disk now contains exactly this project
    UpdateTitle();
    return true;
}

bool App::SaveProjectDialog(bool saveAs) {
    // a locked app may 另存为 (a NEW file is not a mutation of the open project),
    // but it can never overwrite the current one
    if (!saveAs && RefuseLocked()) return false;
    std::wstring path = projectPath_;
    if (saveAs || path.empty()) {
        std::wstring def = exeDir_ + L"\\show.smproj";
        if (!path.empty()) {
            const size_t slash = path.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
                def = exeDir_ + L"\\" + path.substr(slash + 1);
        }
        wchar_t buf[MAX_PATH] = L"";
        wcsncpy_s(buf, def.c_str(), _TRUNCATE);
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"Stage Micing (*.smproj)\0*.smproj\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = buf;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"smproj";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&ofn)) return false;   // cancelled: nothing written
        path = buf;
    }
    return SaveProjectTo(path);
}

void App::OpenProjectDialog() {
    wchar_t buf[MAX_PATH] = L"";
    if (!projectPath_.empty())
        wcsncpy_s(buf, projectPath_.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"Stage Micing (*.smproj)\0*.smproj\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"smproj";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return;
    OpenProjectFrom(buf);
}

bool App::OpenProjectFrom(const std::wstring& path) {
    Project np;
    std::wstring err;
    int skipped = 0;
    if (!LoadProjectFile(np, path, err, &skipped)) {
        const std::wstring msg = std::wstring(L10n::T(S::FileOpenErr)) + err;
        MessageBoxW(hwnd_, msg.c_str(), L"Stage Micing", MB_OK | MB_ICONWARNING);
        return false;
    }
    pr_ = np;
    projectPath_ = path;
    dirty_ = false;   // a freshly opened file is the saved state
    AfterProjectLoaded();
    // report what the loader dropped: AfterProjectLoaded clears the status
    // line, so the note has to come after it
    if (skipped > 0) StatusMsg(dr::Fmt(L10n::T(S::IgnoredFmt), skipped), true);
    ResetUndoHistory();   // a freshly opened file / the demo is the UNDO baseline
    return true;
}

void App::AfterProjectLoaded() {
    // the 导出图片 chooser is remembered per project: take the file's value as the
    // active multiplier (the popup shows that row as selected)
    pngScaleMul_ = (pr_.pngScale == 2 || pr_.pngScale == 8) ? pr_.pngScale : 4;
    pr_.pngScale = pngScaleMul_;
    // an OLD file may still store an intensity-difference technique as TWO items
    // in a kind-2 group: merge them into the ONE stage object of the current model
    MergeIntensityPairs(pr_);
    if (!pr_.Selected()) pr_.selected = -1;
    if (pr_.selIds.empty() && pr_.selected >= 0) pr_.selIds.assign(1, pr_.selected);

    // the loaded project's cable paths were derived with the default marker
    // radius: force the next UpdateStageXf to re-derive them at this zoom
    lastMicRadiusM_ = -1.0;
    // A saved file may hold links the exclusion matrix no longer allows (an old
    // radio link into a wired box, a protocol mismatch, a duplicate): drop them
    // here so a bad project can never reach the painters.
    {
        size_t keep = 0;
        for (size_t i = 0; i < pr_.links.size(); ++i) {
            const CableLink& l = pr_.links[i];
            // `&l` is the stored link itself: it must not count as "already wired"
            if (!CanConnectRule(pr_, l.aKind, l.a, l.bKind, l.b, nullptr, &l)) continue;
            pr_.links[keep++] = l;
        }
        pr_.links.resize(keep);
    }
    RebuildAllCablePaths(pr_, MicRadiusM());

    // every cached or transient view state now refers to the old project
    tiles_.clear();
    ui_.search.clear();
    ui_.scroll = 0;
    ui_.rightScroll = 0;
    ui_.hoverTile = -1;
    ui_.hoverCbRow = -1;
    ui_.hoverCbDevice = -1;
    ui_.hoverCbAdd = -1;
    ui_.popupIndex = -1;
    ui_.popupCat = -1;
    ui_.spotListOpen = -1;
    ui_.spotScroll = 0;
    ui_.chSpotOpen = -1;
    ui_.chSpotScroll = 0;
    ui_.langOpen = false;
    ui_.langHoverRow = -1;
    ui_.fileOpen = false;
    ui_.fileHoverRow = -1;
    ui_.pendingKind = 0;
    ui_.pendingId = 0;
    ui_.hoverCable = -1;
    ui_.msg.clear();
    ui_.msgWarn = 0;
    ui_.msgUntil = 0;
    ui_.dragCat_ = -1;
    ui_.dragTab_ = -1;
    ui_.paletteDragging_ = false;
    L_.chPickRow.clear();
    clip_.clear();
    if (drag_) {
        drag_ = false;
        dragId_ = -1;
    }
    if (devDrag_) {
        devDrag_ = false;
        devDragId_ = -1;
    }
    CloseChannelCell();

    UpdateTitle();
    SyncEdits();
    UpdateCueBanners();
    Layout();
    InvalidateAll();
}
