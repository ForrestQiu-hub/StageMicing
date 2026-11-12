#pragma once
#include <string>

#include "L10n.h"
#include "Model.h"

// ---------------------------------------------------------------------------
// ORCHESTRA PRESETS（预设）— built-in orchestra layouts (twelve):
//     0 三管编制 · 欧式 (86)   1 三管编制 · 美式 (85)
//     2 欧式钢协 (87)          3 美式钢协 (86)
//     4 古乐团 (37)            5 合唱乐团 (154)
//     6 合唱+钢琴 (66)         7 弦乐四重奏 1 (4)
//     8 弦乐四重奏 2 (4)       9 钢琴三重奏 (3)
//    10 钢琴四重奏 (4)        11 钢琴独奏 (1)
//
// Every table is the USER'S OWN placement, dumped from their saved project with
// the --dump-layout hook and pasted back verbatim (see src/Presets.cpp); the
// positions and the angles are theirs and must never be "tidied".
//
// A preset is a table of players in NORMALISED stage coordinates, so it can be
// instantiated on any stage size:
//     x = nx * stage.w        y = ny * stage.d        angle = degrees
// nx = 0 is the left stage edge, nx = 1 the right one; ny = 0 is the BACK edge
// and ny = 1 the FRONT (audience) edge (see src/Presets.cpp for the angle
// convention and for why the front is at ny = 1).
//
// EVERY preset also carries the STAGE SIZE (metres) its table was authored on,
// read out of the source demo .smproj, and loading it restores that stage:
//     三管编制 · 欧式 / 美式 / 欧式钢协 / 美式钢协   18 x 12   (unchanged)
//     古乐团 14 x 12   合唱乐团 18 x 16   合唱+钢琴 18 x 12
//     弦乐四重奏 1 9 x 7   弦乐四重奏 2 9 x 7
//     钢琴三重奏 14 x 10   钢琴四重奏 14 x 10   钢琴独奏 14 x 12
// The stage is set BEFORE the rows are laid out, so the normalised placement
// lands exactly where the user authored it.
//
// Loading a preset REPLACES the placement and starts from an EMPTY stage: every
// item is dropped (instruments AND microphones), together with the mic groups,
// the main pair (mainMics) and every cable whose endpoint is a microphone.  The
// cabling devices and their device -> device trunks stay, as do the project
// fields.  Nothing is read from or written to disk.
// ---------------------------------------------------------------------------
struct PresetRow {
    const wchar_t* defId;   // catalogue id, see src/Catalog.cpp
    double nx;              // normalised x: 0 = left edge, 1 = right edge
    double ny;              // normalised y: 0 = BACK edge, 1 = FRONT edge
    double angle;           // facing angle in degrees (0 = icon front points back)
    // OPTIONAL per-item label (nullptr = the catalogue name).  A few demo2
    // players are NAMED in the user's project (Soprano / Alto / Tenor / Bass,
    // 圆号 2 …) and that name is part of their data, so it rides along with the
    // placement.  Rows without one keep the plain four-field form, so every
    // table stays the verbatim --dump-layout output.
    const wchar_t* label = nullptr;
};

// number of built-in presets (twelve: the four 三管编制 / 钢协 tables plus the
// eight demo2 layouts 古乐团 … 钢琴独奏)
int PresetCount();

// localized row label of preset `index` (S::PresetEu / S::PresetUs /
// S::PresetEuPiano / S::PresetUsPiano / S::PresetAncient … S::PresetPianoSolo)
S PresetNameId(int index);

// Replace everything on the stage (`pr`) with preset `index`: the items
// (instruments AND microphones), the mic groups, the main pair and every
// microphone cable are dropped, then the preset's instruments are instantiated.
// Cabling devices and device -> device trunks are left untouched.
void LoadPreset(Project& pr, int index);

// Dev / test hook (--dump-layout <file>): write the current instrument
// placement as ready-to-paste PresetRow lines, normalised to the stage.
// Returns false and fills `err` when the file cannot be written.
bool DumpLayoutAsPresetRows(const Project& pr, const std::wstring& path,
                            std::wstring& err);
