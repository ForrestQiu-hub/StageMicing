#include "Presets.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "App.h"
#include "Catalog.h"
#include "Draw.h"
#include "L10n.h"
#include "Theme.h"

// ---------------------------------------------------------------------------
// BUILT-IN ORCHESTRA PRESETS（预设）:
//     三管编制 · 欧式 / 三管编制 · 美式 / 欧式钢协 / 美式钢协
//     古乐团 / 合唱乐团 / 合唱+钢琴 / 弦乐四重奏 1 / 弦乐四重奏 2 /
//     钢琴三重奏 / 钢琴四重奏 / 钢琴独奏   (the demo2 projects)
// A preset is a table of players in NORMALISED stage space, so it can be
// instantiated on whatever stage the project has:
//     x = nx * stage.w            y = ny * stage.d
// nx = 0 is the left stage edge, nx = 1 the right one.  ny = 0 is the BACK edge
// and ny = 1 the FRONT (audience) edge, because the app's world y axis grows
// toward the audience: StageView draws cy = oy + y * ppm, so the front edge is
// at the bottom of the screen.
//
// ANGLE CONVENTION — read off dr::Icon and the icon art, and the convention the
// dumps below were written with:
//   * dr::Icon rotates the icon by "angle" degrees about its centre with GDI+
//     RotateTransform, i.e. a POSITIVE angle turns CLOCKWISE on screen;
//   * at angle 0 the icon's "front" (its up axis) points UP the screen, i.e.
//     toward the BACK of the stage: the main-microphone symbol is built with
//     forward = (sin a, -cos a), and the conductor icon's baton points up and
//     to the right at angle 0;
//   * the FRENCH HORN is held with its bell turned away from the conductor, so a
//     horn row carries the facing angle + 180 degrees and the bell faces the
//     audience.
//
// THE TWELVE TABLES ARE THE USER'S OWN PLACEMENTS.  Each one was dumped from one
// of their saved projects with the --dump-layout hook and pasted back VERBATIM,
// in the order the dump wrote it (defId, then y, then x):
//     三管编制 · 欧式   demo1 European.smproj            86 rows
//     三管编制 · 美式   demo1 American.smproj            85 rows
//     欧式钢协          demo1 European+piano.smproj      87 rows
//     美式钢协          demo1 American+piano.smproj      86 rows
//     古乐团            demo2 古乐团.smproj              37 rows
//     合唱乐团          demo2 合唱乐团.smproj           154 rows
//     合唱+钢琴         demo2 合唱钢琴.smproj            66 rows
//     弦乐四重奏 1      demo2 弦乐四重奏 1.smproj        4 rows
//     弦乐四重奏 2      demo2 弦乐四重奏 2.smproj        4 rows
//     钢琴三重奏        demo2 钢琴三重奏.smproj          3 rows
//     钢琴四重奏        demo2 钢琴四重奏.smproj          4 rows
//     钢琴独奏          demo2 钢琴独奏.smproj            1 row
// Nothing here is generated, re-spaced, re-centred or re-faced: the positions
// and the angles are THEIRS.  Do NOT "tidy" a table — a second --dump-layout
// has to reproduce the source dump byte for byte.
//
// Loading a preset REPLACES the PLACEMENT and starts from an EMPTY stage: every
// item is dropped (instruments AND microphones), together with the mic groups,
// the main pair and every cable that ends on a microphone.  Cabling devices and
// their device -> device trunks stay, as do the project fields.  The app never
// reads or writes preset files — these tables ARE the presets.
// ---------------------------------------------------------------------------

namespace {

std::string ToU8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr,
                                      0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr,
                        nullptr);
    return s;
}

// 三管编制 · 欧式 / Triple Wind · European (86 players incl. the conductor) —
// dumped from the user's own project "demo1 European.smproj" with the
// --dump-layout hook.  Positions and angles are THEIRS: do NOT "tidy" them.
static const PresetRow kEu[] = {
    { L"bass", 0.281, 0.396, 223.6 },   // 低音提琴 x4
    { L"bass", 0.175, 0.454, 206.9 },   // 低音提琴 x4
    { L"bass", 0.250, 0.454, 211.4 },   // 低音提琴 x4
    { L"bass", 0.231, 0.533, 214.9 },   // 低音提琴 x4
    { L"bassdrum", 0.689, 0.079, 0.0 },   // 大鼓 x1
    { L"bassoon", 0.653, 0.277, 272.7 },   // 大管 x3
    { L"bassoon", 0.696, 0.288, 272.7 },   // 大管 x3
    { L"bassoon", 0.735, 0.295, 271.6 },   // 大管 x3
    { L"cello", 0.411, 0.392, 252.6 },   // 大提琴 x9
    { L"cello", 0.342, 0.417, 242.0 },   // 大提琴 x9
    { L"cello", 0.450, 0.446, 257.9 },   // 大提琴 x9
    { L"cello", 0.403, 0.479, 238.2 },   // 大提琴 x9
    { L"cello", 0.339, 0.508, 214.1 },   // 大提琴 x9
    { L"cello", 0.450, 0.550, 255.5 },   // 大提琴 x9
    { L"cello", 0.317, 0.562, 212.7 },   // 大提琴 x9
    { L"cello", 0.397, 0.583, 213.4 },   // 大提琴 x9
    { L"cello", 0.356, 0.646, 196.7 },   // 大提琴 x9
    { L"clarinet", 0.428, 0.281, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.464, 0.281, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.503, 0.281, 89.2 },   // 单簧管 x3
    { L"conductor", 0.506, 0.750, 0.0 },   // 指挥 x1
    { L"cymbal", 0.292, 0.159, 0.0 },   // 钹 x1
    { L"flute", 0.396, 0.284, 84.2 },   // 长笛 x3
    { L"flute", 0.357, 0.291, 84.2 },   // 长笛 x3
    { L"flute", 0.325, 0.302, 74.4 },   // 长笛 x3
    { L"glockenspiel", 0.100, 0.337, 298.9 },   // 钟琴 x1
    { L"gong", 0.350, 0.125, 0.0 },   // 锣 x1
    { L"horn", 0.244, 0.288, 233.3 },   // 圆号 x3
    { L"horn", 0.200, 0.308, 217.3 },   // 圆号 x3
    { L"horn", 0.153, 0.350, 203.2 },   // 圆号 x3
    { L"oboe", 0.542, 0.284, 92.0 },   // 双簧管 x3
    { L"oboe", 0.582, 0.288, 92.0 },   // 双簧管 x3
    { L"oboe", 0.625, 0.288, 92.0 },   // 双簧管 x3
    { L"snare", 0.231, 0.204, 0.0 },   // 军鼓 x1
    { L"timpani", 0.528, 0.054, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.439, 0.058, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.464, 0.108, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.506, 0.108, 0.0 },   // 定音鼓 x4
    { L"trombone", 0.485, 0.181, 0.4 },   // 长号 x2
    { L"trombone", 0.442, 0.184, 359.0 },   // 长号 x2
    { L"trumpet", 0.389, 0.195, 345.3 },   // 小号 x3
    { L"trumpet", 0.346, 0.202, 345.3 },   // 小号 x3
    { L"trumpet", 0.302, 0.213, 345.3 },   // 小号 x3
    { L"tuba", 0.621, 0.190, 0.0 },   // 大号 x2
    { L"tuba", 0.557, 0.191, 0.0 },   // 大号 x2
    { L"viola", 0.558, 0.379, 262.5 },   // 中提琴 x11
    { L"viola", 0.517, 0.392, 260.5 },   // 中提琴 x11
    { L"viola", 0.611, 0.396, 283.0 },   // 中提琴 x11
    { L"viola", 0.669, 0.429, 298.2 },   // 中提琴 x11
    { L"viola", 0.517, 0.479, 264.9 },   // 中提琴 x11
    { L"viola", 0.564, 0.492, 280.8 },   // 中提琴 x11
    { L"viola", 0.617, 0.513, 291.3 },   // 中提琴 x11
    { L"viola", 0.664, 0.529, 301.5 },   // 中提琴 x11
    { L"viola", 0.528, 0.571, 280.9 },   // 中提琴 x11
    { L"viola", 0.567, 0.583, 295.2 },   // 中提琴 x11
    { L"viola", 0.608, 0.612, 304.3 },   // 中提琴 x11
    { L"violin", 0.147, 0.600, 23.5 },   // 第一小提琴 x15
    { L"violin", 0.217, 0.617, 19.4 },   // 第一小提琴 x15
    { L"violin", 0.117, 0.654, 14.8 },   // 第一小提琴 x15
    { L"violin", 0.175, 0.658, 18.4 },   // 第一小提琴 x15
    { L"violin", 0.231, 0.667, 17.4 },   // 第一小提琴 x15
    { L"violin", 0.297, 0.683, 14.9 },   // 第一小提琴 x15
    { L"violin", 0.131, 0.713, 11.4 },   // 第一小提琴 x15
    { L"violin", 0.206, 0.721, 9.5 },   // 第一小提琴 x15
    { L"violin", 0.356, 0.725, 9.3 },   // 第一小提琴 x15
    { L"violin", 0.275, 0.729, 9.2 },   // 第一小提琴 x15
    { L"violin", 0.100, 0.771, 3.3 },   // 第一小提琴 x15
    { L"violin", 0.164, 0.775, 2.1 },   // 第一小提琴 x15
    { L"violin", 0.231, 0.775, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.300, 0.775, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.369, 0.775, 0.0 },   // 第一小提琴 x15
    { L"violin2", 0.739, 0.475, 316.8 },   // 第二小提琴 x14
    { L"violin2", 0.775, 0.525, 316.8 },   // 第二小提琴 x14
    { L"violin2", 0.700, 0.554, 316.8 },   // 第二小提琴 x14
    { L"violin2", 0.800, 0.608, 321.9 },   // 第二小提琴 x14
    { L"violin2", 0.725, 0.617, 326.2 },   // 第二小提琴 x14
    { L"violin2", 0.656, 0.638, 323.0 },   // 第二小提琴 x14
    { L"violin2", 0.722, 0.688, 336.0 },   // 第二小提琴 x14
    { L"violin2", 0.786, 0.692, 340.4 },   // 第二小提琴 x14
    { L"violin2", 0.656, 0.700, 335.6 },   // 第二小提琴 x14
    { L"violin2", 0.853, 0.700, 340.4 },   // 第二小提琴 x14
    { L"violin2", 0.800, 0.758, 358.9 },   // 第二小提琴 x14
    { L"violin2", 0.725, 0.763, 356.5 },   // 第二小提琴 x14
    { L"violin2", 0.878, 0.763, 358.9 },   // 第二小提琴 x14
    { L"violin2", 0.653, 0.767, 1.0 },   // 第二小提琴 x14
    { L"xylophone", 0.142, 0.250, 320.9 },   // 木琴 x1
};

// 三管编制 · 美式 / Triple Wind · American (85 players incl. the conductor) —
// dumped from the user's own project "demo1 American.smproj" with the
// --dump-layout hook.  Positions and angles are THEIRS: do NOT "tidy" them.
// One caveat, kept visible on purpose: the two 第一小提琴 rows at ny 0.854
// (nx 0.339 / 0.125) are 10.248 m and 10.250 m in the user's project, so the
// dump writes 0.339 first; both normalise to 0.854, i.e. the 2 mm difference
// does not survive the 3-decimal table.  The rows are in the dumped order here;
// a second --dump-layout sorts them by world y again and prints 0.125 first
// (identical values, only the order of this one pair differs).
static const PresetRow kUs[] = {
    { L"bass", 0.803, 0.583, 320.7 },   // 低音提琴 x4
    { L"bass", 0.858, 0.629, 325.2 },   // 低音提琴 x4
    { L"bass", 0.889, 0.693, 328.7 },   // 低音提琴 x4
    { L"bass", 0.914, 0.767, 337.4 },   // 低音提琴 x4
    { L"bassdrum", 0.647, 0.321, 0.0 },   // 大鼓 x1
    { L"bassoon", 0.657, 0.500, 272.7 },   // 大管 x3
    { L"bassoon", 0.700, 0.511, 272.7 },   // 大管 x3
    { L"bassoon", 0.739, 0.518, 271.6 },   // 大管 x3
    { L"cello", 0.681, 0.692, 327.1 },   // 大提琴 x10
    { L"cello", 0.797, 0.717, 335.9 },   // 大提琴 x10
    { L"cello", 0.697, 0.750, 329.2 },   // 大提琴 x10
    { L"cello", 0.611, 0.775, 327.1 },   // 大提琴 x10
    { L"cello", 0.808, 0.808, 347.1 },   // 大提琴 x10
    { L"cello", 0.708, 0.825, 341.3 },   // 大提琴 x10
    { L"cello", 0.625, 0.829, 331.5 },   // 大提琴 x10
    { L"cello", 0.822, 0.888, 0.0 },   // 大提琴 x10
    { L"cello", 0.728, 0.892, 0.0 },   // 大提琴 x10
    { L"cello", 0.636, 0.896, 0.0 },   // 大提琴 x10
    { L"clarinet", 0.432, 0.504, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.468, 0.504, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.507, 0.504, 89.2 },   // 单簧管 x3
    { L"conductor", 0.492, 0.879, 0.0 },   // 指挥 x1
    { L"cymbal", 0.292, 0.342, 0.0 },   // 钹 x1
    { L"flute", 0.400, 0.507, 84.2 },   // 长笛 x3
    { L"flute", 0.361, 0.514, 84.2 },   // 长笛 x3
    { L"flute", 0.329, 0.525, 74.4 },   // 长笛 x3
    { L"glockenspiel", 0.111, 0.533, 298.9 },   // 钟琴 x1
    { L"gong", 0.350, 0.308, 0.0 },   // 锣 x1
    { L"horn", 0.261, 0.479, 233.3 },   // 圆号 x3
    { L"horn", 0.200, 0.514, 217.3 },   // 圆号 x3
    { L"horn", 0.157, 0.561, 203.2 },   // 圆号 x3
    { L"oboe", 0.546, 0.507, 92.0 },   // 双簧管 x3
    { L"oboe", 0.586, 0.511, 92.0 },   // 双簧管 x3
    { L"oboe", 0.629, 0.511, 92.0 },   // 双簧管 x3
    { L"snare", 0.214, 0.375, 0.0 },   // 军鼓 x1
    { L"timpani", 0.436, 0.258, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.547, 0.258, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.525, 0.325, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.458, 0.329, 0.0 },   // 定音鼓 x4
    { L"trombone", 0.489, 0.404, 0.4 },   // 长号 x2
    { L"trombone", 0.446, 0.407, 359.0 },   // 长号 x2
    { L"trumpet", 0.393, 0.418, 345.3 },   // 小号 x3
    { L"trumpet", 0.350, 0.425, 345.3 },   // 小号 x3
    { L"trumpet", 0.306, 0.436, 345.3 },   // 小号 x3
    { L"tuba", 0.625, 0.413, 0.0 },   // 大号 x2
    { L"tuba", 0.561, 0.414, 0.0 },   // 大号 x2
    { L"viola", 0.475, 0.600, 256.5 },   // 中提琴 x11
    { L"viola", 0.514, 0.600, 275.5 },   // 中提琴 x11
    { L"viola", 0.558, 0.608, 295.4 },   // 中提琴 x11
    { L"viola", 0.439, 0.617, 241.6 },   // 中提琴 x11
    { L"viola", 0.600, 0.621, 306.0 },   // 中提琴 x11
    { L"viola", 0.414, 0.629, 241.5 },   // 中提琴 x11
    { L"viola", 0.636, 0.650, 310.1 },   // 中提琴 x11
    { L"viola", 0.483, 0.696, 261.1 },   // 中提琴 x11
    { L"viola", 0.522, 0.696, 286.6 },   // 中提琴 x11
    { L"viola", 0.442, 0.708, 243.1 },   // 中提琴 x11
    { L"viola", 0.567, 0.717, 313.3 },   // 中提琴 x11
    { L"violin", 0.189, 0.717, 19.4 },   // 第一小提琴 x15
    { L"violin", 0.144, 0.754, 18.4 },   // 第一小提琴 x15
    { L"violin", 0.297, 0.763, 23.5 },   // 第一小提琴 x15
    { L"violin", 0.206, 0.771, 17.4 },   // 第一小提琴 x15
    { L"violin", 0.281, 0.796, 14.9 },   // 第一小提琴 x15
    { L"violin", 0.133, 0.808, 11.4 },   // 第一小提琴 x15
    { L"violin", 0.361, 0.811, 23.5 },   // 第一小提琴 x15
    { L"violin", 0.206, 0.825, 9.5 },   // 第一小提琴 x15
    { L"violin", 0.272, 0.842, 9.2 },   // 第一小提琴 x15
    { L"violin", 0.339, 0.854, 9.3 },   // 第一小提琴 x15
    { L"violin", 0.125, 0.854, 7.9 },   // 第一小提琴 x15
    { L"violin", 0.125, 0.896, 2.1 },   // 第一小提琴 x15
    { L"violin", 0.197, 0.896, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.267, 0.896, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.342, 0.896, 0.0 },   // 第一小提琴 x15
    { L"violin2", 0.293, 0.575, 214.3 },   // 第二小提琴 x12
    { L"violin2", 0.254, 0.607, 216.3 },   // 第二小提琴 x12
    { L"violin2", 0.368, 0.614, 237.8 },   // 第二小提琴 x12
    { L"violin2", 0.193, 0.618, 211.2 },   // 第二小提琴 x12
    { L"violin2", 0.311, 0.629, 230.4 },   // 第二小提琴 x12
    { L"violin2", 0.186, 0.661, 211.2 },   // 第二小提琴 x12
    { L"violin2", 0.275, 0.671, 220.6 },   // 第二小提琴 x12
    { L"violin2", 0.350, 0.679, 230.0 },   // 第二小提琴 x12
    { L"violin2", 0.236, 0.704, 211.2 },   // 第二小提琴 x12
    { L"violin2", 0.328, 0.725, 217.4 },   // 第二小提琴 x12
    { L"violin2", 0.408, 0.737, 234.8 },   // 第二小提琴 x12
    { L"violin2", 0.383, 0.779, 214.9 },   // 第二小提琴 x12
    { L"xylophone", 0.154, 0.443, 320.9 },   // 木琴 x1
};

// 欧式钢协 / European Piano Concerto (87 players: 欧式 + the grand piano) —
// dumped from the user's own project "demo1 European+piano.smproj" with the
// --dump-layout hook.  Positions and angles are THEIRS: do NOT "tidy" them.
static const PresetRow kEuPiano[] = {
    { L"bass", 0.281, 0.396, 223.6 },   // 低音提琴 x4
    { L"bass", 0.175, 0.454, 206.9 },   // 低音提琴 x4
    { L"bass", 0.250, 0.454, 211.4 },   // 低音提琴 x4
    { L"bass", 0.231, 0.533, 214.9 },   // 低音提琴 x4
    { L"bassdrum", 0.689, 0.079, 0.0 },   // 大鼓 x1
    { L"bassoon", 0.653, 0.277, 272.7 },   // 大管 x3
    { L"bassoon", 0.696, 0.288, 272.7 },   // 大管 x3
    { L"bassoon", 0.735, 0.295, 271.6 },   // 大管 x3
    { L"cello", 0.411, 0.392, 252.6 },   // 大提琴 x9
    { L"cello", 0.342, 0.417, 242.0 },   // 大提琴 x9
    { L"cello", 0.450, 0.446, 257.9 },   // 大提琴 x9
    { L"cello", 0.403, 0.479, 238.2 },   // 大提琴 x9
    { L"cello", 0.339, 0.508, 214.1 },   // 大提琴 x9
    { L"cello", 0.450, 0.550, 255.5 },   // 大提琴 x9
    { L"cello", 0.317, 0.562, 212.7 },   // 大提琴 x9
    { L"cello", 0.397, 0.583, 213.4 },   // 大提琴 x9
    { L"cello", 0.356, 0.646, 196.7 },   // 大提琴 x9
    { L"clarinet", 0.428, 0.281, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.464, 0.281, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.503, 0.281, 89.2 },   // 单簧管 x3
    { L"conductor", 0.506, 0.750, 0.0 },   // 指挥 x1
    { L"cymbal", 0.292, 0.159, 0.0 },   // 钹 x1
    { L"flute", 0.396, 0.284, 84.2 },   // 长笛 x3
    { L"flute", 0.357, 0.291, 84.2 },   // 长笛 x3
    { L"flute", 0.325, 0.302, 74.4 },   // 长笛 x3
    { L"glockenspiel", 0.100, 0.337, 298.9 },   // 钟琴 x1
    { L"gong", 0.350, 0.125, 0.0 },   // 锣 x1
    { L"grand_piano", 0.508, 0.879, 270.0 },   // 三角钢琴 x1
    { L"horn", 0.244, 0.288, 233.3 },   // 圆号 x3
    { L"horn", 0.200, 0.308, 217.3 },   // 圆号 x3
    { L"horn", 0.153, 0.350, 203.2 },   // 圆号 x3
    { L"oboe", 0.542, 0.284, 92.0 },   // 双簧管 x3
    { L"oboe", 0.582, 0.288, 92.0 },   // 双簧管 x3
    { L"oboe", 0.625, 0.288, 92.0 },   // 双簧管 x3
    { L"snare", 0.231, 0.204, 0.0 },   // 军鼓 x1
    { L"timpani", 0.528, 0.054, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.439, 0.058, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.464, 0.108, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.506, 0.108, 0.0 },   // 定音鼓 x4
    { L"trombone", 0.485, 0.181, 0.4 },   // 长号 x2
    { L"trombone", 0.442, 0.184, 359.0 },   // 长号 x2
    { L"trumpet", 0.389, 0.195, 345.3 },   // 小号 x3
    { L"trumpet", 0.346, 0.202, 345.3 },   // 小号 x3
    { L"trumpet", 0.302, 0.213, 345.3 },   // 小号 x3
    { L"tuba", 0.621, 0.190, 0.0 },   // 大号 x2
    { L"tuba", 0.557, 0.191, 0.0 },   // 大号 x2
    { L"viola", 0.558, 0.379, 262.5 },   // 中提琴 x11
    { L"viola", 0.517, 0.392, 260.5 },   // 中提琴 x11
    { L"viola", 0.611, 0.396, 283.0 },   // 中提琴 x11
    { L"viola", 0.669, 0.429, 298.2 },   // 中提琴 x11
    { L"viola", 0.517, 0.479, 264.9 },   // 中提琴 x11
    { L"viola", 0.564, 0.492, 280.8 },   // 中提琴 x11
    { L"viola", 0.617, 0.513, 291.3 },   // 中提琴 x11
    { L"viola", 0.664, 0.529, 301.5 },   // 中提琴 x11
    { L"viola", 0.528, 0.571, 280.9 },   // 中提琴 x11
    { L"viola", 0.567, 0.583, 295.2 },   // 中提琴 x11
    { L"viola", 0.608, 0.612, 304.3 },   // 中提琴 x11
    { L"violin", 0.147, 0.600, 23.5 },   // 第一小提琴 x15
    { L"violin", 0.217, 0.617, 19.4 },   // 第一小提琴 x15
    { L"violin", 0.117, 0.654, 14.8 },   // 第一小提琴 x15
    { L"violin", 0.175, 0.658, 18.4 },   // 第一小提琴 x15
    { L"violin", 0.231, 0.667, 17.4 },   // 第一小提琴 x15
    { L"violin", 0.297, 0.683, 14.9 },   // 第一小提琴 x15
    { L"violin", 0.131, 0.713, 11.4 },   // 第一小提琴 x15
    { L"violin", 0.206, 0.721, 9.5 },   // 第一小提琴 x15
    { L"violin", 0.356, 0.725, 9.3 },   // 第一小提琴 x15
    { L"violin", 0.275, 0.729, 9.2 },   // 第一小提琴 x15
    { L"violin", 0.100, 0.771, 3.3 },   // 第一小提琴 x15
    { L"violin", 0.164, 0.775, 2.1 },   // 第一小提琴 x15
    { L"violin", 0.231, 0.775, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.300, 0.775, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.369, 0.775, 0.0 },   // 第一小提琴 x15
    { L"violin2", 0.739, 0.475, 316.8 },   // 第二小提琴 x14
    { L"violin2", 0.775, 0.525, 316.8 },   // 第二小提琴 x14
    { L"violin2", 0.700, 0.554, 316.8 },   // 第二小提琴 x14
    { L"violin2", 0.800, 0.608, 321.9 },   // 第二小提琴 x14
    { L"violin2", 0.725, 0.617, 326.2 },   // 第二小提琴 x14
    { L"violin2", 0.656, 0.638, 323.0 },   // 第二小提琴 x14
    { L"violin2", 0.722, 0.688, 336.0 },   // 第二小提琴 x14
    { L"violin2", 0.786, 0.692, 340.4 },   // 第二小提琴 x14
    { L"violin2", 0.656, 0.700, 335.6 },   // 第二小提琴 x14
    { L"violin2", 0.853, 0.700, 340.4 },   // 第二小提琴 x14
    { L"violin2", 0.800, 0.758, 358.9 },   // 第二小提琴 x14
    { L"violin2", 0.725, 0.763, 356.5 },   // 第二小提琴 x14
    { L"violin2", 0.878, 0.763, 358.9 },   // 第二小提琴 x14
    { L"violin2", 0.653, 0.767, 1.0 },   // 第二小提琴 x14
    { L"xylophone", 0.142, 0.250, 320.9 },   // 木琴 x1
};

// 美式钢协 / American Piano Concerto (86 players: 美式 + the grand piano) —
// dumped from the user's own project "demo1 American+piano.smproj" with the
// --dump-layout hook.  Positions and angles are THEIRS: do NOT "tidy" them.
static const PresetRow kUsPiano[] = {
    { L"bass", 0.803, 0.458, 320.7 },   // 低音提琴 x4
    { L"bass", 0.858, 0.504, 325.2 },   // 低音提琴 x4
    { L"bass", 0.889, 0.568, 328.7 },   // 低音提琴 x4
    { L"bass", 0.914, 0.642, 337.4 },   // 低音提琴 x4
    { L"bassdrum", 0.647, 0.196, 0.0 },   // 大鼓 x1
    { L"bassoon", 0.657, 0.375, 272.7 },   // 大管 x3
    { L"bassoon", 0.700, 0.386, 272.7 },   // 大管 x3
    { L"bassoon", 0.739, 0.393, 271.6 },   // 大管 x3
    { L"cello", 0.681, 0.567, 327.1 },   // 大提琴 x10
    { L"cello", 0.797, 0.592, 335.9 },   // 大提琴 x10
    { L"cello", 0.697, 0.625, 329.2 },   // 大提琴 x10
    { L"cello", 0.611, 0.650, 327.1 },   // 大提琴 x10
    { L"cello", 0.808, 0.683, 347.1 },   // 大提琴 x10
    { L"cello", 0.708, 0.700, 341.3 },   // 大提琴 x10
    { L"cello", 0.625, 0.704, 331.5 },   // 大提琴 x10
    { L"cello", 0.822, 0.763, 0.0 },   // 大提琴 x10
    { L"cello", 0.728, 0.767, 0.0 },   // 大提琴 x10
    { L"cello", 0.636, 0.771, 0.0 },   // 大提琴 x10
    { L"clarinet", 0.432, 0.379, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.468, 0.379, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.507, 0.379, 89.2 },   // 单簧管 x3
    { L"conductor", 0.492, 0.725, 0.0 },   // 指挥 x1
    { L"cymbal", 0.292, 0.217, 0.0 },   // 钹 x1
    { L"flute", 0.400, 0.382, 84.2 },   // 长笛 x3
    { L"flute", 0.361, 0.389, 84.2 },   // 长笛 x3
    { L"flute", 0.329, 0.400, 74.4 },   // 长笛 x3
    { L"glockenspiel", 0.111, 0.408, 298.9 },   // 钟琴 x1
    { L"gong", 0.350, 0.183, 0.0 },   // 锣 x1
    { L"grand_piano", 0.494, 0.850, 270.9 },   // 三角钢琴 x1
    { L"horn", 0.261, 0.354, 233.3 },   // 圆号 x3
    { L"horn", 0.200, 0.389, 217.3 },   // 圆号 x3
    { L"horn", 0.157, 0.436, 203.2 },   // 圆号 x3
    { L"oboe", 0.546, 0.382, 92.0 },   // 双簧管 x3
    { L"oboe", 0.586, 0.386, 92.0 },   // 双簧管 x3
    { L"oboe", 0.629, 0.386, 92.0 },   // 双簧管 x3
    { L"snare", 0.214, 0.250, 0.0 },   // 军鼓 x1
    { L"timpani", 0.436, 0.133, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.547, 0.133, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.525, 0.200, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.458, 0.204, 0.0 },   // 定音鼓 x4
    { L"trombone", 0.489, 0.279, 0.4 },   // 长号 x2
    { L"trombone", 0.446, 0.282, 359.0 },   // 长号 x2
    { L"trumpet", 0.393, 0.293, 345.3 },   // 小号 x3
    { L"trumpet", 0.350, 0.300, 345.3 },   // 小号 x3
    { L"trumpet", 0.306, 0.311, 345.3 },   // 小号 x3
    { L"tuba", 0.625, 0.288, 0.0 },   // 大号 x2
    { L"tuba", 0.561, 0.289, 0.0 },   // 大号 x2
    { L"viola", 0.475, 0.475, 256.5 },   // 中提琴 x11
    { L"viola", 0.514, 0.475, 275.5 },   // 中提琴 x11
    { L"viola", 0.558, 0.483, 295.4 },   // 中提琴 x11
    { L"viola", 0.439, 0.492, 241.6 },   // 中提琴 x11
    { L"viola", 0.600, 0.496, 306.0 },   // 中提琴 x11
    { L"viola", 0.414, 0.504, 241.5 },   // 中提琴 x11
    { L"viola", 0.636, 0.525, 310.1 },   // 中提琴 x11
    { L"viola", 0.483, 0.571, 261.1 },   // 中提琴 x11
    { L"viola", 0.522, 0.571, 286.6 },   // 中提琴 x11
    { L"viola", 0.442, 0.583, 243.1 },   // 中提琴 x11
    { L"viola", 0.567, 0.592, 313.3 },   // 中提琴 x11
    { L"violin", 0.189, 0.592, 19.4 },   // 第一小提琴 x15
    { L"violin", 0.144, 0.629, 18.4 },   // 第一小提琴 x15
    { L"violin", 0.297, 0.638, 23.5 },   // 第一小提琴 x15
    { L"violin", 0.206, 0.646, 17.4 },   // 第一小提琴 x15
    { L"violin", 0.281, 0.671, 14.9 },   // 第一小提琴 x15
    { L"violin", 0.133, 0.683, 11.4 },   // 第一小提琴 x15
    { L"violin", 0.361, 0.686, 23.5 },   // 第一小提琴 x15
    { L"violin", 0.206, 0.700, 9.5 },   // 第一小提琴 x15
    { L"violin", 0.272, 0.717, 9.2 },   // 第一小提琴 x15
    { L"violin", 0.342, 0.725, 9.3 },   // 第一小提琴 x15
    { L"violin", 0.125, 0.729, 7.9 },   // 第一小提琴 x15
    { L"violin", 0.125, 0.771, 2.1 },   // 第一小提琴 x15
    { L"violin", 0.197, 0.771, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.267, 0.771, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.342, 0.771, 0.0 },   // 第一小提琴 x15
    { L"violin2", 0.293, 0.450, 214.3 },   // 第二小提琴 x12
    { L"violin2", 0.254, 0.482, 216.3 },   // 第二小提琴 x12
    { L"violin2", 0.358, 0.492, 230.0 },   // 第二小提琴 x12
    { L"violin2", 0.193, 0.493, 211.2 },   // 第二小提琴 x12
    { L"violin2", 0.311, 0.504, 230.4 },   // 第二小提琴 x12
    { L"violin2", 0.186, 0.536, 211.2 },   // 第二小提琴 x12
    { L"violin2", 0.275, 0.546, 220.6 },   // 第二小提琴 x12
    { L"violin2", 0.350, 0.554, 230.0 },   // 第二小提琴 x12
    { L"violin2", 0.236, 0.579, 211.2 },   // 第二小提琴 x12
    { L"violin2", 0.328, 0.600, 217.4 },   // 第二小提琴 x12
    { L"violin2", 0.408, 0.612, 234.8 },   // 第二小提琴 x12
    { L"violin2", 0.383, 0.654, 214.9 },   // 第二小提琴 x12
    { L"xylophone", 0.154, 0.318, 320.9 },   // 木琴 x1
};


// ---------------------------------------------------------------------------
// demo2 — the user's own demo projects (demo\demo2 *.smproj), each dumped
// with  StageMicing.exe --open "<file>" --dump-layout <out.txt>  and pasted
// back VERBATIM, exactly like the four tables above.  Never re-space,
// re-centre, re-face or reorder a row: a second --dump-layout must reproduce
// the source dump byte for byte.
//
// The stage each demo2 project was authored on — read straight out of the
// `stage W D` line of the source .smproj, and the size LoadPreset now RESTORES
// (before this it always built on 18 x 12, which turned a 9 x 7 quartet into a
// sparse corner of a big hall; the normalised rows are untouched by the size):
//   demo2 古乐团.smproj            37 rows   source stage 14 x 12
//   demo2 合唱乐团.smproj         154 rows   source stage 18 x 16
//   demo2 合唱钢琴.smproj          66 rows   source stage 18 x 12
//   demo2 弦乐四重奏 1.smproj       4 rows   source stage  9 x 7
//   demo2 弦乐四重奏 2.smproj       4 rows   source stage  9 x 7
//   demo2 钢琴三重奏.smproj         3 rows   source stage 14 x 10
//   demo2 钢琴四重奏.smproj         4 rows   source stage 14 x 10
//   demo2 钢琴独奏.smproj           1 row    source stage 14 x 12
// ---------------------------------------------------------------------------

const PresetRow kAncient[] = {
    { L"bass", 0.225, 0.567, 217.8 },   // 低音提琴 x2
    { L"bass", 0.175, 0.638, 217.8 },   // 低音提琴 x2
    { L"bassoon", 0.657, 0.362, 272.7 },   // 大管 x1
    { L"cello", 0.414, 0.533, 244.7 },   // 大提琴 x4
    { L"cello", 0.331, 0.546, 244.7 },   // 大提琴 x4
    { L"cello", 0.447, 0.631, 244.7 },   // 大提琴 x4
    { L"cello", 0.370, 0.650, 244.7 },   // 大提琴 x4
    { L"clarinet", 0.471, 0.354, 85.5 },   // 单簧管 x1
    { L"flute", 0.404, 0.367, 266.2 },   // 长笛 x2
    { L"flute", 0.346, 0.375, 267.0 },   // 长笛 x2
    { L"harpsichord", 0.514, 0.838, 270.7 },   // 大键琴 x1
    { L"horn", 0.418, 0.229, 225.4, L"圆号 2" },   // 圆号 2 x2
    { L"horn", 0.329, 0.242, 225.4 },   // 圆号 x2
    { L"oboe", 0.568, 0.354, 92.7 },   // 双簧管 x1
    { L"timpani", 0.504, 0.113, 0.0 },   // 定音鼓 x1
    { L"trombone", 0.611, 0.250, 0.0 },   // 长号 x1
    { L"trumpet", 0.529, 0.238, 0.0 },   // 小号 x1
    { L"viola", 0.611, 0.521, 302.5 },   // 中提琴 x6
    { L"viola", 0.654, 0.558, 309.0 },   // 中提琴 x6
    { L"viola", 0.696, 0.592, 302.5 },   // 中提琴 x6
    { L"viola", 0.561, 0.617, 302.5 },   // 中提琴 x6
    { L"viola", 0.604, 0.654, 309.0 },   // 中提琴 x6
    { L"viola", 0.646, 0.688, 302.5 },   // 中提琴 x6
    { L"violin", 0.389, 0.750, 0.0 },   // 第一小提琴 x8
    { L"violin", 0.279, 0.754, 0.0, L"第一小提琴 4" },   // 第一小提琴 4 x8
    { L"violin", 0.171, 0.825, 0.0 },   // 第一小提琴 x8
    { L"violin", 0.275, 0.825, 0.0, L"第一小提琴 4" },   // 第一小提琴 4 x8
    { L"violin", 0.389, 0.825, 0.0 },   // 第一小提琴 x8
    { L"violin", 0.279, 0.892, 0.0 },   // 第一小提琴 x8
    { L"violin", 0.389, 0.892, 0.0 },   // 第一小提琴 x8
    { L"violin", 0.175, 0.896, 0.0 },   // 第一小提琴 x8
    { L"violin2", 0.693, 0.771, 0.0 },   // 第二小提琴 x6
    { L"violin2", 0.693, 0.833, 0.0 },   // 第二小提琴 x6
    { L"violin2", 0.793, 0.838, 0.0 },   // 第二小提琴 x6
    { L"violin2", 0.693, 0.892, 0.0 },   // 第二小提琴 x6
    { L"violin2", 0.896, 0.892, 0.0 },   // 第二小提琴 x6
    { L"violin2", 0.793, 0.896, 0.0 },   // 第二小提琴 x6
};

const PresetRow kChoirOrch[] = {
    { L"bass", 0.286, 0.539, 223.6 },   // 低音提琴 x4
    { L"bass", 0.180, 0.583, 206.9 },   // 低音提琴 x4
    { L"bass", 0.255, 0.583, 211.4 },   // 低音提琴 x4
    { L"bass", 0.236, 0.642, 214.9 },   // 低音提琴 x4
    { L"bassdrum", 0.747, 0.331, 0.0 },   // 大鼓 x1
    { L"bassoon", 0.658, 0.450, 272.7 },   // 大管 x3
    { L"bassoon", 0.701, 0.458, 272.7 },   // 大管 x3
    { L"bassoon", 0.740, 0.464, 271.6 },   // 大管 x3
    { L"cello", 0.416, 0.536, 252.6 },   // 大提琴 x9
    { L"cello", 0.347, 0.555, 242.0 },   // 大提琴 x9
    { L"cello", 0.455, 0.577, 257.9 },   // 大提琴 x9
    { L"cello", 0.408, 0.602, 238.2 },   // 大提琴 x9
    { L"cello", 0.344, 0.623, 214.1 },   // 大提琴 x9
    { L"cello", 0.455, 0.655, 255.5 },   // 大提琴 x9
    { L"cello", 0.322, 0.664, 212.7 },   // 大提琴 x9
    { L"cello", 0.402, 0.680, 213.4 },   // 大提琴 x9
    { L"cello", 0.361, 0.727, 196.7 },   // 大提琴 x9
    { L"choir_singer", 0.364, 0.044, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.400, 0.044, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.444, 0.044, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.481, 0.044, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.678, 0.044, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.714, 0.044, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.758, 0.044, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.794, 0.044, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.200, 0.047, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.236, 0.047, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.281, 0.047, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.317, 0.047, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.522, 0.050, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.558, 0.050, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.603, 0.050, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.639, 0.050, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.364, 0.094, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.400, 0.094, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.444, 0.094, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.481, 0.094, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.678, 0.094, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.714, 0.094, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.758, 0.094, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.794, 0.094, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.200, 0.097, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.236, 0.097, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.281, 0.097, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.317, 0.097, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.522, 0.100, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.558, 0.100, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.603, 0.100, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.639, 0.100, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.203, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.239, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.283, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.319, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.364, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.400, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.444, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.481, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.519, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.556, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.600, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.636, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.681, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.717, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.761, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.797, 0.144, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.203, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.239, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.283, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.319, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.364, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.400, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.444, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.481, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.519, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.556, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.600, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.636, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.681, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.717, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.761, 0.194, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.797, 0.194, 0.0 },   // 合唱歌手 x64
    { L"clarinet", 0.433, 0.453, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.469, 0.453, 89.2 },   // 单簧管 x3
    { L"clarinet", 0.508, 0.453, 89.2 },   // 单簧管 x3
    { L"conductor", 0.511, 0.805, 0.0 },   // 指挥 x1
    { L"cymbal", 0.297, 0.362, 0.0 },   // 钹 x1
    { L"flute", 0.401, 0.455, 84.2 },   // 长笛 x3
    { L"flute", 0.362, 0.461, 84.2 },   // 长笛 x3
    { L"flute", 0.330, 0.469, 74.4 },   // 长笛 x3
    { L"glockenspiel", 0.105, 0.495, 298.9 },   // 钟琴 x1
    { L"gong", 0.355, 0.336, 0.0 },   // 锣 x1
    { L"horn", 0.249, 0.458, 233.3 },   // 圆号 x3
    { L"horn", 0.205, 0.473, 217.3 },   // 圆号 x3
    { L"horn", 0.158, 0.505, 203.2 },   // 圆号 x3
    { L"oboe", 0.547, 0.455, 92.0 },   // 双簧管 x3
    { L"oboe", 0.587, 0.458, 92.0 },   // 双簧管 x3
    { L"oboe", 0.630, 0.458, 92.0 },   // 双簧管 x3
    { L"snare", 0.236, 0.395, 0.0 },   // 军鼓 x1
    { L"solo_singer", 0.378, 0.903, 0.0, L"Soprano" },   // Soprano x4
    { L"solo_singer", 0.458, 0.906, 0.0, L"Alto" },   // Alto x4
    { L"solo_singer", 0.547, 0.906, 0.0, L"Tenor" },   // Tenor x4
    { L"solo_singer", 0.631, 0.906, 0.0, L"Bass" },   // Bass x4
    { L"timpani", 0.533, 0.283, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.444, 0.286, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.469, 0.323, 0.0 },   // 定音鼓 x4
    { L"timpani", 0.511, 0.323, 0.0 },   // 定音鼓 x4
    { L"trombone", 0.490, 0.378, 0.4 },   // 长号 x2
    { L"trombone", 0.447, 0.380, 359.0 },   // 长号 x2
    { L"trumpet", 0.394, 0.389, 345.3 },   // 小号 x3
    { L"trumpet", 0.351, 0.394, 345.3 },   // 小号 x3
    { L"trumpet", 0.307, 0.402, 345.3 },   // 小号 x3
    { L"tuba", 0.626, 0.385, 0.0 },   // 大号 x2
    { L"tuba", 0.562, 0.386, 0.0 },   // 大号 x2
    { L"viola", 0.563, 0.527, 262.5 },   // 中提琴 x11
    { L"viola", 0.522, 0.536, 260.5 },   // 中提琴 x11
    { L"viola", 0.616, 0.539, 283.0 },   // 中提琴 x11
    { L"viola", 0.674, 0.564, 298.2 },   // 中提琴 x11
    { L"viola", 0.522, 0.602, 264.9 },   // 中提琴 x11
    { L"viola", 0.569, 0.611, 280.8 },   // 中提琴 x11
    { L"viola", 0.622, 0.627, 291.3 },   // 中提琴 x11
    { L"viola", 0.669, 0.639, 301.5 },   // 中提琴 x11
    { L"viola", 0.533, 0.671, 280.9 },   // 中提琴 x11
    { L"viola", 0.572, 0.680, 295.2 },   // 中提琴 x11
    { L"viola", 0.613, 0.701, 304.3 },   // 中提琴 x11
    { L"violin", 0.152, 0.692, 23.5 },   // 第一小提琴 x15
    { L"violin", 0.222, 0.705, 19.4 },   // 第一小提琴 x15
    { L"violin", 0.122, 0.733, 14.8 },   // 第一小提琴 x15
    { L"violin", 0.180, 0.736, 18.4 },   // 第一小提琴 x15
    { L"violin", 0.236, 0.743, 17.4 },   // 第一小提琴 x15
    { L"violin", 0.302, 0.755, 14.9 },   // 第一小提琴 x15
    { L"violin", 0.136, 0.777, 11.4 },   // 第一小提琴 x15
    { L"violin", 0.211, 0.783, 9.5 },   // 第一小提琴 x15
    { L"violin", 0.361, 0.786, 9.3 },   // 第一小提琴 x15
    { L"violin", 0.280, 0.789, 9.2 },   // 第一小提琴 x15
    { L"violin", 0.105, 0.821, 3.3 },   // 第一小提琴 x15
    { L"violin", 0.169, 0.824, 2.1 },   // 第一小提琴 x15
    { L"violin", 0.236, 0.824, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.305, 0.824, 0.0 },   // 第一小提琴 x15
    { L"violin", 0.374, 0.824, 0.0 },   // 第一小提琴 x15
    { L"violin2", 0.744, 0.599, 316.8 },   // 第二小提琴 x14
    { L"violin2", 0.780, 0.636, 316.8 },   // 第二小提琴 x14
    { L"violin2", 0.705, 0.658, 316.8 },   // 第二小提琴 x14
    { L"violin2", 0.805, 0.698, 321.9 },   // 第二小提琴 x14
    { L"violin2", 0.730, 0.705, 326.2 },   // 第二小提琴 x14
    { L"violin2", 0.661, 0.721, 323.0 },   // 第二小提琴 x14
    { L"violin2", 0.727, 0.758, 336.0 },   // 第二小提琴 x14
    { L"violin2", 0.791, 0.761, 340.4 },   // 第二小提琴 x14
    { L"violin2", 0.661, 0.767, 335.6 },   // 第二小提琴 x14
    { L"violin2", 0.858, 0.767, 340.4 },   // 第二小提琴 x14
    { L"violin2", 0.805, 0.811, 358.9 },   // 第二小提琴 x14
    { L"violin2", 0.730, 0.815, 356.5 },   // 第二小提琴 x14
    { L"violin2", 0.883, 0.815, 358.9 },   // 第二小提琴 x14
    { L"violin2", 0.658, 0.818, 1.0 },   // 第二小提琴 x14
    { L"xylophone", 0.147, 0.430, 320.9 },   // 木琴 x1
};

const PresetRow kChoirPiano[] = {
    { L"choir_singer", 0.372, 0.121, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.408, 0.121, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.453, 0.121, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.489, 0.121, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.686, 0.121, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.722, 0.121, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.767, 0.121, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.803, 0.121, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.208, 0.125, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.244, 0.125, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.289, 0.125, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.325, 0.125, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.531, 0.129, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.567, 0.130, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.611, 0.130, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.647, 0.130, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.372, 0.188, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.408, 0.188, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.453, 0.188, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.489, 0.188, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.686, 0.188, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.722, 0.188, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.767, 0.188, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.803, 0.188, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.208, 0.192, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.244, 0.192, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.289, 0.192, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.325, 0.192, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.530, 0.196, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.567, 0.196, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.611, 0.196, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.647, 0.196, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.211, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.247, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.292, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.328, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.372, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.408, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.453, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.489, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.528, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.564, 0.254, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.608, 0.255, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.644, 0.255, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.689, 0.255, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.725, 0.255, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.769, 0.255, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.805, 0.255, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.211, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.247, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.292, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.328, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.372, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.408, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.453, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.489, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.528, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.564, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.608, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.644, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.689, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.725, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.769, 0.321, 0.0 },   // 合唱歌手 x64
    { L"choir_singer", 0.805, 0.321, 0.0 },   // 合唱歌手 x64
    { L"conductor", 0.507, 0.482, 0.0 },   // 指挥 x1
    { L"grand_piano", 0.508, 0.608, 270.4 },   // 三角钢琴 x1
};

const PresetRow kQuartet1[] = {
    { L"cello", 0.644, 0.700, 0.0 },   // 大提琴 x1
    { L"viola", 0.578, 0.521, 307.6 },   // 中提琴 x1
    { L"violin", 0.333, 0.671, 31.6 },   // 第一小提琴 x1
    { L"violin2", 0.394, 0.522, 231.3 },   // 第二小提琴 x1
};

const PresetRow kQuartet2[] = {
    { L"cello", 0.411, 0.507, 232.1 },   // 大提琴 x1
    { L"viola", 0.567, 0.507, 307.6 },   // 中提琴 x1
    { L"violin", 0.333, 0.671, 31.6 },   // 第一小提琴 x1
    { L"violin2", 0.622, 0.671, 328.8 },   // 第二小提琴 x1
};

const PresetRow kPianoTrio[] = {
    { L"cello", 0.579, 0.710, 334.3 },   // 大提琴 x1
    { L"grand_piano", 0.514, 0.535, 283.1 },   // 三角钢琴 x1
    { L"violin", 0.404, 0.715, 15.1 },   // 第一小提琴 x1
};

const PresetRow kPianoQuartet[] = {
    { L"cello", 0.584, 0.758, 334.3 },   // 大提琴 x1
    { L"grand_piano", 0.514, 0.535, 283.1 },   // 三角钢琴 x1
    { L"viola", 0.482, 0.680, 269.0 },   // 中提琴 x1
    { L"violin", 0.409, 0.762, 15.1 },   // 第一小提琴 x1
};

const PresetRow kPianoSolo[] = {
    { L"grand_piano", 0.500, 0.542, 270.0 },   // 三角钢琴 x1
};

struct PresetDef {
    S name;                // localized row label (see src/L10n.cpp)
    const PresetRow* rows;
    int count;
    // The stage (metres) this table was AUTHORED on, read from the `stage W D`
    // line of its source .smproj (demo\demo1 *.smproj / demo2 *.smproj).  Loading
    // the preset restores exactly this hall, because the rows are normalised to
    // the stage: the size only says how many metres one normalised unit is.
    double stageW;
    double stageD;
};

const PresetDef kPresets[] = {
    //                    name             rows  count  stage W  stage D
    {S::PresetEu, kEu, (int)(sizeof(kEu) / sizeof(kEu[0])), 18.0, 12.0},
    {S::PresetUs, kUs, (int)(sizeof(kUs) / sizeof(kUs[0])), 18.0, 12.0},
    {S::PresetEuPiano, kEuPiano, (int)(sizeof(kEuPiano) / sizeof(kEuPiano[0])), 18.0, 12.0},
    {S::PresetUsPiano, kUsPiano, (int)(sizeof(kUsPiano) / sizeof(kUsPiano[0])), 18.0, 12.0},
    {S::PresetAncient, kAncient, (int)(sizeof(kAncient) / sizeof(kAncient[0])), 14.0, 12.0},
    {S::PresetChoirOrch, kChoirOrch, (int)(sizeof(kChoirOrch) / sizeof(kChoirOrch[0])), 18.0, 16.0},
    {S::PresetChoirPiano, kChoirPiano, (int)(sizeof(kChoirPiano) / sizeof(kChoirPiano[0])), 18.0, 12.0},
    {S::PresetQuartet1, kQuartet1, (int)(sizeof(kQuartet1) / sizeof(kQuartet1[0])), 9.0, 7.0},
    {S::PresetQuartet2, kQuartet2, (int)(sizeof(kQuartet2) / sizeof(kQuartet2[0])), 9.0, 7.0},
    {S::PresetPianoTrio, kPianoTrio, (int)(sizeof(kPianoTrio) / sizeof(kPianoTrio[0])), 14.0, 10.0},
    {S::PresetPianoQuartet, kPianoQuartet, (int)(sizeof(kPianoQuartet) / sizeof(kPianoQuartet[0])), 14.0, 10.0},
    {S::PresetPianoSolo, kPianoSolo, (int)(sizeof(kPianoSolo) / sizeof(kPianoSolo[0])), 14.0, 12.0},
};

} // namespace

int PresetCount() { return (int)(sizeof(kPresets) / sizeof(kPresets[0])); }

S PresetNameId(int index) {
    if (index < 0 || index >= PresetCount()) return S::PresetEu;
    return kPresets[index].name;
}

void LoadPreset(Project& pr, int index) {
    if (index < 0 || index >= PresetCount()) return;
    const PresetDef& p = kPresets[index];

    // Every preset restores ITS OWN original stage size (14 x 12 古乐团, 18 x 16
    // 合唱乐团, 9 x 7 弦乐四重奏 …), read from the source .smproj.  The stage has
    // to change BEFORE the rows are laid out: a row is normalised to the stage
    // (x = nx * stage.w), so the size is what turns the normalised placement into
    // the metres the user authored — instruments only, nothing else to adjust.
    pr.stage.w = p.stageW > 0.0 ? p.stageW : 18.0;
    pr.stage.d = p.stageD > 0.0 ? p.stageD : 12.0;

    // A preset is a PLACEMENT, not a patch — loading one starts from an EMPTY
    // stage: every item goes (instruments AND microphones), and with the mics
    // their bindings (micGroups), the main pair (mainMics) and every cable that
    // ends on a microphone (aKind / bKind == 1 = an Item endpoint).  The cabling
    // DEVICES stay, and so does every device -> device trunk, which references no
    // microphone: opening a project and loading a preset must never leave a
    // microphone behind (「每个项目打开，不加任何麦克风」).
    pr.items.clear();
    pr.micGroups.clear();
    pr.mainMics.clear();
    pr.links.erase(std::remove_if(pr.links.begin(), pr.links.end(),
                                  [](const CableLink& l) {
                                      return l.aKind == 1 || l.bKind == 1;
                                  }),
                   pr.links.end());
    // Project::topo is a pure mirror of the wiring (see RebuildAllCablePaths in
    // src/Model.cpp), so it has to shed the microphone links with it
    pr.topo.links = pr.links;
    pr.topo.valid = !pr.links.empty();
    pr.Deselect();
    pr.selected = -1;

    for (int i = 0; i < p.count; ++i) {
        const PresetRow& r = p.rows[i];
        const CatItem* cat = FindItem(r.defId);
        if (!cat) continue;   // a stale catalogue id can never break a load
        Item it;
        it.id = pr.nextId++;
        it.kind = ItemKind::Instrument;
        it.defId = r.defId;
        // the demo2 tables carry the few names the user typed in their project
        // (Soprano / Alto / Tenor / Bass …); everything else keeps the catalogue
        // name, exactly like the four older presets
        if (r.label && *r.label) it.label = r.label;
        it.x = std::clamp(r.nx, 0.0, 1.0) * pr.stage.w;
        it.y = std::clamp(r.ny, 0.0, 1.0) * pr.stage.d;
        it.angle = r.angle;
        it.w = cat->w * CatScale(cat->id);
        it.h = cat->h * CatScale(cat->id);
        pr.items.push_back(it);
    }
}

bool DumpLayoutAsPresetRows(const Project& pr, const std::wstring& path,
                            std::wstring& err) {
    std::map<std::wstring, int> count;
    for (const Item& it : pr.items)
        if (it.kind == ItemKind::Instrument) ++count[it.defId];

    // STABLE order for a readable diff: by defId, then by y (back to front), then
    // by x (left to right).  Every value stays exactly the item's own x / y /
    // angle, so a dump can be pasted into a preset table and dumped again to the
    // same numbers (round-trip).
    std::vector<const Item*> rows;
    for (const Item& it : pr.items)
        if (it.kind == ItemKind::Instrument) rows.push_back(&it);
    std::stable_sort(rows.begin(), rows.end(), [](const Item* a, const Item* b) {
        if (a->defId != b->defId) return a->defId < b->defId;
        if (a->y != b->y) return a->y < b->y;
        return a->x < b->x;
    });

    std::wstring text =
        L"// --dump-layout: instrument placement of the open project,\n"
        L"// normalised to the stage (x/stage.w, y/stage.d) + the facing angle.\n"
        L"// Pasting this into a PresetRow table in src/Presets.cpp round-trips:\n"
        L"// the same rows come back out of a second dump.\n";
    for (const Item* p : rows) {
        const Item& it = *p;
        const CatItem* cat = FindItem(it.defId);
        // the comment carries the item's own label when it has one, otherwise
        // the catalogue name of the instrument type
        const std::wstring nm =
            !it.label.empty() ? it.label : (cat ? ItemName(*cat) : it.defId);
        const double nx = pr.stage.w > 0.001 ? it.x / pr.stage.w : 0.0;
        const double ny = pr.stage.d > 0.001 ? it.y / pr.stage.d : 0.0;
        text += dr::Fmt(L"    { L\"%s\", %.3f, %.3f, %.1f },   // %s x%d\n",
                        it.defId.c_str(), nx, ny, it.angle, nm.c_str(),
                        count[it.defId]);
    }

    std::ofstream f(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!f) {
        err = path;
        return false;
    }
    const std::string bytes = ToU8(text);
    f.write(bytes.data(), (std::streamsize)bytes.size());
    if (!f.good()) {
        err = path;
        return false;
    }
    err.clear();
    return true;
}

// ---------------------------------------------------------------------------
// the modal (预设): scrim + centred panel + one row per built-in preset + 取消,
// modelled on the delete-route confirmation (App::PaintCableConfirm) so both
// modals look and behave the same.  While it is open it owns the input.
// ---------------------------------------------------------------------------
void App::OpenPresetModal() {
    ui_.presetOpen = true;
    ui_.presetHover = -1;
    InvalidateAll();
}

void App::ClosePresetModal() {
    ui_.presetOpen = false;
    ui_.presetHover = -1;
    L_.pfScrim = L_.pfPanel = L_.pfCancel = {};
    L_.pfRow.clear();
    InvalidateAll();
}

void App::LoadPresetAt(int index) {
    if (RefuseLocked()) return;
    if (index < 0 || index >= PresetCount()) {
        ClosePresetModal();
        return;
    }
    const std::wstring name = L10n::T(PresetNameId(index));
    PushUndoSnapshot();   // loading a preset replaces the whole placement
    LoadPreset(pr_, index);
    // The canvas must fit the NEW stage: UpdateStageXf derives ppm_ from
    // pr_.stage.w / .d on every Layout(), so the fit itself follows the size —
    // but pan / zoom are view state left over from the stage that was just
    // replaced (a far pan or a 2x zoom on an 18 x 12 hall would push a 9 x 7
    // quartet off screen).  A preset opens on a view that fits its own stage.
    ui_.panX = 0.0;
    ui_.panY = 0.0;
    ui_.zoom = 1.0;
    ClosePresetModal();
    // the whole placement changed: fresh editor text, fresh layout, repaint
    RefreshAfterProjectChange();
    StatusMsg(dr::Fmt(L10n::T(S::PresetLoaded), name.c_str()));
}

void App::PaintPresetModal(Gdiplus::Graphics& g) {
    auto d = [&](float v) { return (int)std::lround(v * dpi_ * uiScale_); };
    const int n = PresetCount();
    if (!ui_.presetOpen) {
        L_.pfScrim = L_.pfPanel = L_.pfCancel = {};
        L_.pfRow.clear();
        return;
    }
    L_.pfScrim = {0, 0, cw_, ch_};
    dr::Fill(g, L_.pfScrim, dr::C(0x2B2723, 110));

    // Twelve presets do not fit one 46 px column inside a 1400 x 900 window with
    // a comfortable margin, so past six rows the list becomes TWO columns of
    // ceil(n / 2) rows (12 rows -> 2 x 6, panel 770 x 436).  Up / Down walk one
    // column, Left / Right switch columns and Enter loads the hovered row (see
    // App::KeyDown).
    const int cols = (n > 6) ? 2 : 1;
    const int rows = (n + cols - 1) / cols;   // presets per column
    const int rowH = d(46), rowGap = d(8);
    const int colW = d(360), colGap = d(14);
    const int w = d(18) + cols * colW + (cols - 1) * colGap + d(18);
    const int h = d(58) + rows * (rowH + rowGap) + d(54);
    const int ccx = cw_ / 2, ccy = ch_ / 2;
    L_.pfPanel = {ccx - w / 2, ccy - h / 2, ccx + w / 2, ccy + h / 2};
    dr::FillRound(g, dr::RF(L_.pfPanel), (float)d(12), dr::C(theme::kPanel));
    dr::StrokeRound(g, dr::RF(L_.pfPanel), (float)d(12),
                    dr::C(theme::kBorderStrong), 1.2f);

    RECT tr = {L_.pfPanel.left + d(20), L_.pfPanel.top + d(16),
               L_.pfPanel.right - d(20), L_.pfPanel.top + d(40)};
    dr::Text(g, L10n::T(S::FnPreset), Font(11.f, true), dr::C(theme::kText), tr, 0,
             true);

    L_.pfRow.assign((size_t)n, RECT{});
    for (int i = 0; i < n; ++i) {
        const int col = i / rows, row = i % rows;
        const int left = L_.pfPanel.left + d(18) + col * (colW + colGap);
        const int top = L_.pfPanel.top + d(52) + row * (rowH + rowGap);
        RECT r = {left, top, left + colW, top + rowH};
        L_.pfRow[(size_t)i] = r;
        const bool hov = ui_.presetHover == i;
        dr::FillRound(g, dr::RF(r), (float)d(8),
                      dr::C(hov ? theme::kHover : theme::kPanelAlt));
        dr::StrokeRound(g, dr::RF(r), (float)d(8),
                        dr::C(hov ? theme::kBorderStrong : theme::kBorder), 1.f);
        RECT nr = {r.left + d(16), r.top, r.right - d(16), r.bottom};
        dr::Text(g, L10n::T(PresetNameId(i)), Font(10.f), dr::C(theme::kText), nr, 0,
                 true);
        // a small chevron: this row loads something
        const float ax = (float)(r.right - d(22));
        const float ay = (float)((r.top + r.bottom) / 2);
        const float hw = (float)d(4), hh = (float)d(6);
        const Gdiplus::Color ac(hov ? dr::C(theme::kText) : dr::C(theme::kTextFaint));
        dr::Line(g, ax - hw, ay - hh, ax + hw * 0.2f, ay, ac, 1.4f);
        dr::Line(g, ax + hw * 0.2f, ay, ax - hw, ay + hh, ac, 1.4f);
    }

    const int bw = d(96), bh = d(32);
    const int by = L_.pfPanel.bottom - d(18) - bh;
    L_.pfCancel = {L_.pfPanel.right - d(20) - bw, by, L_.pfPanel.right - d(20),
                   by + bh};
    const bool hovCancel = ui_.presetHover == n;
    dr::FillRound(g, dr::RF(L_.pfCancel), (float)d(6),
                  dr::C(hovCancel ? theme::kHover : theme::kPanelAlt));
    dr::StrokeRound(g, dr::RF(L_.pfCancel), (float)d(6),
                    dr::C(theme::kBorderStrong), 1.f);
    dr::Text(g, L10n::T(S::SrCancel), Font(9.f), dr::C(theme::kText), L_.pfCancel, 1,
             true);
}

bool App::PresetModalClick(int x, int y) {
    if (!ui_.presetOpen) return false;
    POINT p = {x, y};
    for (size_t i = 0; i < L_.pfRow.size(); ++i) {
        if ((int)i >= PresetCount()) break;
        if (PtInRect(&L_.pfRow[i], p)) {
            LoadPresetAt((int)i);
            return true;
        }
    }
    // 取消 (and a click on the scrim) close it without touching anything
    ClosePresetModal();
    return true;
}
