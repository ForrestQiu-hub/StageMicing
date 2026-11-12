#include "Catalog.h"
#include "L10n.h"

namespace {

const std::vector<CatGroup> kInstGroups = {
    {L"keys",    L"键盘",       L"KEYBOARD"},
    {L"strings", L"弦乐",       L"STRINGS"},
    {L"wood",    L"木管",       L"WOODWIND"},
    {L"brass",   L"铜管",       L"BRASS"},
    {L"perc",    L"打击乐",     L"PERCUSSION"},
    {L"voice",   L"人声与指挥", L"VOICE & PODIUM"},
    {L"misc",    L"其他",       L"OTHER"},
};

const std::vector<CatGroup> kMicGroups = {
    {L"main",     L"主话筒",     L"MAIN MICROPHONE"},
    {L"mic",      L"话筒",       L"MICROPHONE"},
    {L"lavalier", L"领夹话筒",   L"LAVALIER"},
    {L"mixed",    L"混合差",     L"MIXED"},
    {L"intensity",L"强度差",     L"INTENSITY"},
};

const std::vector<CatItem> kInstruments = {
    {L"grand_piano", L"三角钢琴", L"Grand Piano", L"keys", 1.60, 2.70},
    {L"harpsichord", L"大键琴",   L"Harpsichord", L"keys", 2.10, 1.05},
    {L"celesta",     L"钢片琴",   L"Celesta",     L"keys", 1.05, 0.70},
    // 立式钢琴: the celesta's model drawn slimmer and longer, an upright piano's
    // top view (1.45 m wide keyboard x 0.55 m deep case)
    {L"upright_piano", L"立式钢琴", L"Upright Piano", L"keys", 1.45, 0.55},

    {L"violin",      L"第一小提琴", L"Violin I",   L"strings", 1.05, 0.48},
    {L"violin2",     L"第二小提琴", L"Violin II",  L"strings", 1.05, 0.48},
    {L"viola",       L"中提琴",   L"Viola",       L"strings", 1.10, 0.49},
    {L"cello",       L"大提琴",   L"Cello",       L"strings", 1.35, 0.64},
    {L"bass",        L"低音提琴", L"Double Bass", L"strings", 1.65, 0.74},
    {L"harp",        L"竖琴",     L"Harp",        L"strings", 1.10, 1.45},

    {L"flute",       L"长笛",     L"Flute",       L"wood", 1.30, 0.40},
    {L"oboe",        L"双簧管",   L"Oboe",        L"wood", 1.15, 0.35},
    {L"clarinet",    L"单簧管",   L"Clarinet",    L"wood", 1.15, 0.40},
    {L"bassoon",     L"大管",     L"Bassoon",     L"wood", 1.52, 0.45},

    {L"horn",        L"圆号",     L"French Horn", L"brass", 0.80, 0.70},
    {L"trumpet",     L"小号",     L"Trumpet",     L"brass", 0.50, 0.78},
    {L"trombone",    L"长号",     L"Trombone",    L"brass", 0.62, 1.03},
    {L"tuba",        L"大号",     L"Tuba",        L"brass", 0.86, 0.86},

    {L"timpani",     L"定音鼓",   L"Timpani",     L"perc", 0.85, 0.85},
    {L"bassdrum",    L"大鼓",     L"Bass drum",   L"perc", 1.10, 1.10},
    {L"snare",       L"军鼓",     L"Snare drum",  L"perc", 0.70, 0.70},
    {L"cymbal",      L"钹",       L"Cymbal",      L"perc", 0.78, 0.78},
    {L"xylophone",   L"木琴",     L"Xylophone",   L"perc", 1.30, 0.72},
    {L"glockenspiel",L"钟琴",     L"Glockenspiel",L"perc", 1.00, 0.55},
    {L"gong",        L"锣",       L"Gong",        L"perc", 0.90, 0.90},
    {L"percussion",  L"打击乐",   L"Percussion",  L"perc", 1.55, 1.23},

    // 人声与指挥: the conductor plus the TWO singing characters — 独唱歌手 (the solo
    // singer, e.g. behind the piano in a concerto) and 合唱歌手 (the choir singer,
    // placed several times to build a choir block).  The old 女高音 / 男高音 / 歌手
    // triple is gone; a project that still names them is loaded with those objects
    // skipped and reported as 「已忽略 N 个无法识别的对象」.
    {L"conductor",   L"指挥",     L"Conductor",   L"voice", 1.02, 1.02},
    {L"solo_singer", L"独唱歌手", L"Solo Singer", L"voice", 0.72, 0.72},
    {L"choir_singer",L"合唱歌手", L"Choir Singer",L"voice", 0.72, 0.72},

    {L"custom",      L"自定义",   L"Custom",      L"misc", 1.10, 1.10},
};

const std::vector<CatItem> kMics = {
    {L"mainmic",  L"主话筒",   L"Main mic",   L"main",     0.40, 0.35},
    {L"mic",      L"话筒",     L"Microphone", L"mic",      1.00, 1.00},
    {L"lavalier", L"领夹话筒", L"Lavalier",   L"lavalier", 0.60, 0.45},

    {L"ortf",     L"ORTF",     L"ORTF",       L"mixed",    0.70, 0.50},
    {L"nos",      L"NOS",      L"NOS",        L"mixed",    0.70, 0.50},
    {L"din",      L"DIN",      L"DIN",        L"mixed",    0.70, 0.50},
    {L"xy",       L"XY",       L"XY",         L"intensity",0.55, 0.55},
    {L"ms",       L"MS",       L"MS",         L"intensity",0.55, 0.55},
    {L"blumlein", L"Blumlein", L"Blumlein",   L"intensity",0.55, 0.55},
};

// 对应乐器 targets that are NOT instruments: they never appear in the palette and
// never become an Item.  `group` = "spot" keeps them out of every palette query.
const std::vector<CatItem> kSpotExtras = {
    // the two BITDEPTH-FREE special sections (环绕声道 / 天空声道) behave exactly like
    // 侧展 / 环境: plain spotType strings, no stage object, bindable per mic / group
    {L"section_surround", L"环绕声道", L"Surround", L"spot", 0.0, 0.0},
    {L"section_height",   L"天空声道", L"Height",   L"spot", 0.0, 0.0},
    {L"section_side",     L"侧展", L"Side extension", L"spot", 0.0, 0.0},
    {L"section_ambience", L"环境", L"Ambience",       L"spot", 0.0, 0.0},
    // FOUR big categories are BINDABLE TARGETS: a mic may be bound to the whole
    // 弦乐组 / 木管组 / 铜管组 / 打击乐组 instead of one instrument, and the channel
    // table then puts it at the START of that section's block.  The keyboard, harp
    // and voice families have NO category target any more: their instruments are
    // listed FLAT in the picker and sort by their own instrument order.
    {L"section_strings",  L"弦乐组", L"Strings",   L"spot", 0.0, 0.0},
    {L"section_woodwind", L"木管组", L"Woodwind",  L"spot", 0.0, 0.0},
    {L"section_brass",    L"铜管组", L"Brass",     L"spot", 0.0, 0.0},
    {L"section_perc",     L"打击乐组", L"Percussion", L"spot", 0.0, 0.0},
};

// The two-level 对应乐器 picker: one row per big category, its instruments in the
// second panel.  The model, in one place:
//   * `key`    - the internal row id (also what a category click stores in
//                ui_.spotGroup while its second panel is open);
//   * `target` - the `section_*` spotType a group row binds, i.e. the whole
//                section instead of one instrument.  These are the four ids
//                SpotExtras() publishes, so IsSectionTarget() reads this table
//                instead of repeating them;
//   * ONLY 弦乐 / 木管 / 铜管 / 打击乐 own a submenu.  竖琴, the keyboard family
//     (三角钢琴 / 大键琴 / 钢片琴 / 立式钢琴) and 声乐 (独唱 / 合唱) are FLAT rows.
struct SpotGroupDef {
    const wchar_t* key;      // internal key
    const wchar_t* target;   // the category-level spotType id
    S name;                  // localised row label
    const wchar_t* items[9]; // instrument defIds, nullptr-terminated
};
const std::vector<SpotGroupDef> kSpotGroups = {
    // 竖琴 deliberately lives OUTSIDE 弦乐: it is a FLAT entry in the picker
    {L"strings",  L"section_strings",  S::CatStrings,
     {L"violin", L"violin2", L"viola", L"cello", L"bass", nullptr}},
    {L"woodwind", L"section_woodwind", S::CatWoodwind,
     {L"flute", L"clarinet", L"oboe", L"bassoon", nullptr}},
    {L"brass",    L"section_brass",    S::CatBrass,
     {L"horn", L"trumpet", L"trombone", L"tuba", nullptr}},
    // the percussion FAMILY only: the generic 打击乐 symbol stays a flat entry
    {L"perc",     L"section_perc",     S::CatPerc,
     {L"timpani", L"snare", L"bassdrum", L"gong", L"cymbal", L"xylophone",
      L"glockenspiel", nullptr}},
};

} // namespace

const std::vector<CatItem>& Instruments() { return kInstruments; }
const std::vector<CatItem>& Mics() { return kMics; }
const std::vector<CatGroup>& InstrumentGroups() { return kInstGroups; }
const std::vector<CatGroup>& MicGroups() { return kMicGroups; }
const std::vector<CatItem>& SpotExtras() { return kSpotExtras; }
bool IsSectionTarget(const std::wstring& id) {
    // the seven ids are the targets of the picker table above: read them from
    // there so a new category cannot be added to one place only
    for (const auto& grp : kSpotGroups)
        if (id == grp.target) return true;
    return false;
}

int SpotGroupCount() { return (int)kSpotGroups.size(); }
const wchar_t* SpotGroupKey(int g) {
    return (g >= 0 && g < (int)kSpotGroups.size()) ? kSpotGroups[(size_t)g].key : L"";
}
const wchar_t* SpotGroupTarget(int g) {
    return (g >= 0 && g < (int)kSpotGroups.size()) ? kSpotGroups[(size_t)g].target : L"";
}
S SpotGroupName(int g) {
    return (g >= 0 && g < (int)kSpotGroups.size()) ? kSpotGroups[(size_t)g].name
                                                   : S::CatStrings;
}
std::vector<std::wstring> SpotGroupItems(int g) {
    std::vector<std::wstring> out;
    if (g < 0 || g >= (int)kSpotGroups.size()) return out;
    for (const wchar_t* p : kSpotGroups[(size_t)g].items) {
        if (!p) break;
        out.push_back(p);
    }
    return out;
}
const wchar_t* SpotTargetOfDefId(const std::wstring& defId) {
    for (const auto& grp : kSpotGroups)
        for (const wchar_t* p : grp.items)
            if (p && defId == p) return grp.target;
    return nullptr;
}
int SpotGroupIndexOfDefId(const std::wstring& defId) {
    for (size_t g = 0; g < kSpotGroups.size(); ++g)
        for (const wchar_t* p : kSpotGroups[g].items)
            if (p && defId == p) return (int)g;
    return -1;
}

// The two extras are the only keys whose name needs all four languages, so they
// are spelled out here instead of widening CatItem.
std::wstring SpotTypeName(const std::wstring& id) {
    if (id.empty()) return std::wstring();
    if (id == L"section_side") {
        switch (L10n::Get()) {
        case Lang::Zh: return L"侧展";
        case Lang::De: return L"Seitenerweiterung";
        case Lang::Fr: return L"Extension latérale";
        default: return L"Side extension";
        }
    }
    if (id == L"section_ambience") {
        switch (L10n::Get()) {
        case Lang::Zh: return L"环境";
        case Lang::De: return L"Ambiente";
        case Lang::Fr: return L"Ambiance";
        default: return L"Ambience";
        }
    }
    if (id == L"section_surround") {
        switch (L10n::Get()) {
        case Lang::Zh: return L"环绕声道";
        case Lang::De: return L"Surround";
        case Lang::Fr: return L"Enveloppe";
        default: return L"Surround";
        }
    }
    if (id == L"section_height") {
        switch (L10n::Get()) {
        case Lang::Zh: return L"天空声道";
        case Lang::De: return L"Höhe";
        case Lang::Fr: return L"Hauteur";
        default: return L"Height";
        }
    }    // the big categories, as bindable targets
    struct Cat { const wchar_t* id; const wchar_t* zh; const wchar_t* en;
                 const wchar_t* de; const wchar_t* fr; };
    // A CATEGORY row / binding target reads 「弦乐组」 … 「声乐组」 (the section as a
    // whole); the four tables carry the simple nouns for the other languages (no
    // "group" suffix).  This is the ONE place the picker rows, the inspector's
    // binding line, the 03 乐器 column and the PDF get their text from.
    static const Cat kCats[] = {
        {L"section_strings",  L"弦乐组", L"Strings",   L"Streicher",  L"Cordes"},
        {L"section_woodwind", L"木管组", L"Woodwinds", L"Holzbläser", L"Bois"},
        {L"section_brass",    L"铜管组", L"Brass",     L"Blechbläser", L"Cuivres"},
        {L"section_perc",     L"打击乐组", L"Percussion", L"Schlagzeug", L"Percussions"},
        {L"section_keyboard", L"键盘乐器组", L"Keyboards", L"Tasteninstrumente", L"Claviers"},
        {L"section_harp",     L"竖琴组", L"Harp",      L"Harfe",      L"Harpe"},
        {L"section_voice",    L"声乐组", L"Voices",    L"Gesang",     L"Voix"},
    };
    for (const auto& c : kCats) {
        if (id != c.id) continue;
        switch (L10n::Get()) {
        case Lang::Zh: return c.zh;
        case Lang::De: return c.de;
        case Lang::Fr: return c.fr;
        default: return c.en;
        }
    }
    const CatItem* c = FindItem(id);
    return c ? ItemName(*c) : id;
}

double ItemSRA(const std::wstring& id) {
    if (id == L"ortf") return 96.0;
    if (id == L"nos") return 81.0;
    if (id == L"din") return 100.0;
    if (id == L"xy") return 130.0;
    if (id == L"ms") return 120.0;
    if (id == L"blumlein") return 76.0;
    return 0.0;
}

double CatScale(const std::wstring& id) {
    return id == L"grand_piano" ? 1.0 : 0.90;
}

const CatItem* FindItem(const std::wstring& id) {
    for (const auto& it : kInstruments) if (id == it.id) return &it;
    for (const auto& it : kMics) if (id == it.id) return &it;
    for (const auto& it : kSpotExtras) if (id == it.id) return &it;
    return nullptr;
}

std::wstring ItemName(const CatItem& it) {
    // the two singing characters are the only palette entries whose name needs all
    // four languages, so they are spelled out here instead of widening CatItem
    const std::wstring id(it.id ? it.id : L"");
    if (id == L"solo_singer") {
        switch (L10n::Get()) {
        case Lang::Zh: return L"独唱歌手";
        case Lang::De: return L"Solosänger";
        case Lang::Fr: return L"Chanteur solo";
        default: return L"Solo Singer";
        }
    }
    if (id == L"choir_singer") {
        switch (L10n::Get()) {
        case Lang::Zh: return L"合唱歌手";
        case Lang::De: return L"Chorsänger";
        case Lang::Fr: return L"Choriste";
        default: return L"Choir Singer";
        }
    }
    return L10n::IsZh() ? std::wstring(it.zh) : std::wstring(it.en);
}

std::wstring GroupName(const CatGroup& g) {
    return L10n::IsZh() ? std::wstring(g.zh) : std::wstring(g.en);
}
