#include "Catalog.h"
#include "L10n.h"

namespace {

const std::vector<CatGroup> kInstGroups = {
    {L"keys",    L"键盘",       L"KEYBOARD",     L"Tasteninstrumente", L"Claviers"},
    {L"strings", L"弦乐",       L"STRINGS",      L"Streicher",         L"Cordes"},
    {L"wood",    L"木管",       L"WOODWIND",     L"Holzbläser",        L"Bois"},
    {L"brass",   L"铜管",       L"BRASS",        L"Blechbläser",       L"Cuivres"},
    {L"perc",    L"打击乐",     L"PERCUSSION",   L"Schlagwerk",        L"Percussions"},
    {L"voice",   L"人声与指挥", L"VOICE & PODIUM", L"Stimme",          L"Voix"},
    {L"misc",    L"其他",       L"OTHER",        L"Sonstiges",         L"Autres"},
};

const std::vector<CatGroup> kMicGroups = {
    {L"main",     L"主话筒",     L"MAIN MICROPHONE", L"Hauptmikrofon",     L"Micro principal"},
    {L"mic",      L"话筒",       L"MICROPHONE",      L"Mikrofon",          L"Micro"},
    {L"lavalier", L"领夹话筒",   L"LAVALIER",        L"Ansteckmikrofon",   L"Micro-cravate"},
    {L"mixed",    L"混合差",     L"MIXED",           L"Gemischt",          L"Mixte"},
    {L"intensity",L"强度差",     L"INTENSITY",       L"Intensität",        L"Intensité"},
};

const std::vector<CatItem> kInstruments = {
    {L"grand_piano", L"三角钢琴", L"Grand Piano", L"keys", 1.60, 2.70,
     L"Klavier", L"Piano à queue"},
    {L"harpsichord", L"大键琴",   L"Harpsichord", L"keys", 2.10, 1.05,
     L"Cembalo", L"Clavecin"},
    {L"celesta",     L"钢片琴",   L"Celesta",     L"keys", 1.05, 0.70,
     L"Celesta", L"Célesta"},
    // 立式钢琴: the celesta's model drawn slimmer and longer, an upright piano's
    // top view (1.45 m wide keyboard x 0.55 m deep case)
    {L"upright_piano", L"立式钢琴", L"Upright Piano", L"keys", 1.45, 0.55,
     L"Aufrechtes Klavier", L"Piano droit"},

    {L"violin",      L"第一小提琴", L"Violin I",   L"strings", 1.05, 0.48,
     L"Violine I", L"Violon I"},
    {L"violin2",     L"第二小提琴", L"Violin II",  L"strings", 1.05, 0.48,
     L"Violine II", L"Violon II"},
    {L"viola",       L"中提琴",   L"Viola",       L"strings", 1.10, 0.49,
     L"Bratsche", L"Alto"},
    {L"cello",       L"大提琴",   L"Cello",       L"strings", 1.35, 0.64,
     L"Violoncello", L"Violoncelle"},
    {L"bass",        L"低音提琴", L"Double Bass", L"strings", 1.65, 0.74,
     L"Kontrabass", L"Contrebasse"},
    {L"harp",        L"竖琴",     L"Harp",        L"strings", 1.10, 1.45,
     L"Harfe", L"Harpe"},

    {L"flute",       L"长笛",     L"Flute",       L"wood", 1.30, 0.40,
     L"Querflöte", L"Flûte"},
    {L"oboe",        L"双簧管",   L"Oboe",        L"wood", 1.15, 0.35,
     L"Oboe", L"Hautbois"},
    {L"clarinet",    L"单簧管",   L"Clarinet",    L"wood", 1.15, 0.40,
     L"Klarinette", L"Clarinette"},
    {L"bassoon",     L"大管",     L"Bassoon",     L"wood", 1.52, 0.45,
     L"Fagott", L"Basson"},

    {L"horn",        L"圆号",     L"French Horn", L"brass", 0.80, 0.70,
     L"Horn", L"Cor"},
    {L"trumpet",     L"小号",     L"Trumpet",     L"brass", 0.50, 0.78,
     L"Trompete", L"Trompette"},
    {L"trombone",    L"长号",     L"Trombone",    L"brass", 0.62, 1.03,
     L"Posaune", L"Trombone"},
    {L"tuba",        L"大号",     L"Tuba",        L"brass", 0.86, 0.86,
     L"Tuba", L"Tuba"},

    {L"timpani",     L"定音鼓",   L"Timpani",     L"perc", 0.85, 0.85,
     L"Pauken", L"Timbales"},
    {L"bassdrum",    L"大鼓",     L"Bass drum",   L"perc", 1.10, 1.10,
     L"Große Trommel", L"Grosse caisse"},
    {L"snare",       L"军鼓",     L"Snare drum",  L"perc", 0.70, 0.70,
     L"Kleine Trommel", L"Caisse claire"},
    {L"cymbal",      L"钹",       L"Cymbal",      L"perc", 0.78, 0.78,
     L"Becken", L"Cymbales"},
    {L"xylophone",   L"木琴",     L"Xylophone",   L"perc", 1.30, 0.72,
     L"Xylophon", L"Xylophone"},
    {L"glockenspiel",L"钟琴",     L"Glockenspiel",L"perc", 1.00, 0.55,
     L"Glockenspiel", L"Jeu de timbres"},
    {L"gong",        L"锣",       L"Gong",        L"perc", 0.90, 0.90,
     L"Gong", L"Tam-tam"},
    {L"percussion",  L"打击乐",   L"Percussion",  L"perc", 1.55, 1.23,
     L"Schlagwerk", L"Percussions"},

    // 人声与指挥: the conductor plus the TWO singing characters — 独唱歌手 (the solo
    // singer, e.g. behind the piano in a concerto) and 合唱歌手 (the choir singer,
    // placed several times to build a choir block).  The old 女高音 / 男高音 / 歌手
    // triple is gone; a project that still names them is loaded with those objects
    // skipped and reported as 「已忽略 N 个无法识别的对象」.
    {L"conductor",   L"指挥",     L"Conductor",   L"voice", 1.02, 1.02,
     L"Dirigent", L"Chef d'orchestre"},
    {L"solo_singer", L"独唱歌手", L"Solo Singer", L"voice", 0.72, 0.72,
     L"Solosänger", L"Chanteur solo"},
    {L"choir_singer",L"合唱歌手", L"Choir Singer",L"voice", 0.72, 0.72,
     L"Chorsänger", L"Choriste"},

    {L"custom",      L"自定义",   L"Custom",      L"misc", 1.10, 1.10,
     L"Benutzerdefiniert", L"Personnalisé"},
};

const std::vector<CatItem> kMics = {
    {L"mainmic",  L"主话筒",   L"Main mic",   L"main",     0.40, 0.35,
     L"Hauptmikrofon", L"Micro principal"},
    {L"mic",      L"话筒",     L"Microphone", L"mic",      1.00, 1.00,
     L"Punktmikrofon", L"Micro ponctuel"},
    {L"lavalier", L"领夹话筒", L"Lavalier",   L"lavalier", 0.60, 0.45,
     L"Ansteckmikrofon", L"Micro-cravate"},

    {L"ortf",     L"ORTF",     L"ORTF",       L"mixed",    0.70, 0.50, L"ORTF", L"ORTF"},
    {L"nos",      L"NOS",      L"NOS",        L"mixed",    0.70, 0.50, L"NOS", L"NOS"},
    {L"din",      L"DIN",      L"DIN",        L"mixed",    0.70, 0.50, L"DIN", L"DIN"},
    {L"xy",       L"XY",       L"XY",         L"intensity",0.55, 0.55, L"XY", L"XY"},
    {L"ms",       L"MS",       L"MS",         L"intensity",0.55, 0.55, L"MS", L"MS"},
    {L"blumlein", L"Blumlein", L"Blumlein",   L"intensity",0.55, 0.55,
     L"Blumlein", L"Blumlein"},
};

// 对应乐器 targets that are NOT instruments: they never appear in the palette and
// never become an Item.  `group` = "spot" keeps them out of every palette query.
const std::vector<CatItem> kSpotExtras = {
    // the two BITDEPTH-FREE special sections (环绕声道 / 天空声道) behave exactly like
    // 侧展 / 环境: plain spotType strings, no stage object, bindable per mic / group
    {L"section_surround", L"环绕声道", L"Surround", L"spot", 0.0, 0.0,
     L"Surround", L"Surround"},
    {L"section_height",   L"天空声道", L"Height",   L"spot", 0.0, 0.0,
     L"Höhe", L"Hauteur"},
    {L"section_side",     L"侧展", L"Side extension", L"spot", 0.0, 0.0,
     L"Seitenerweiterung", L"Extension latérale"},
    {L"section_ambience", L"环境", L"Ambience",       L"spot", 0.0, 0.0,
     L"Ambiente", L"Ambiance"},
    // FOUR big categories are BINDABLE TARGETS: a mic may be bound to the whole
    // 弦乐组 / 木管组 / 铜管组 / 打击乐组 instead of one instrument, and the channel
    // table then puts it at the START of that section's block.  The keyboard, harp
    // and voice families have NO category target any more: their instruments are
    // listed FLAT in the picker and sort by their own instrument order.
    {L"section_strings",  L"弦乐组", L"Strings",   L"spot", 0.0, 0.0,
     L"Streichergruppe", L"Groupe cordes"},
    {L"section_woodwind", L"木管组", L"Woodwind",  L"spot", 0.0, 0.0,
     L"Holzbläsergruppe", L"Groupe bois"},
    {L"section_brass",    L"铜管组", L"Brass",     L"spot", 0.0, 0.0,
     L"Blechbläsergruppe", L"Groupe cuivres"},
    {L"section_perc",     L"打击乐组", L"Percussion", L"spot", 0.0, 0.0,
     L"Schlagwerkgruppe", L"Groupe percussions"},
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

// The catalogue names the 对应乐器 keys in four languages now, so this helper only
// has to find the row: EVERY id that owns a CatItem (an instrument, a microphone or
// one of the section_* extras) goes through ItemName(), which is the single place
// the pickers, the inspector binding line, the 03 乐器 column and the PDF read.
std::wstring SpotTypeName(const std::wstring& id) {
    if (id.empty()) return std::wstring();
    if (const CatItem* c = FindItem(id)) return ItemName(*c);
    struct Cat { const wchar_t* id; const wchar_t* zh; const wchar_t* en;
                 const wchar_t* de; const wchar_t* fr; };
    // the three category targets WITHOUT a catalogue row (no palette symbol)
    static const Cat kCats[] = {
        {L"section_keyboard", L"键盘乐器组", L"Keyboards", L"Tasteninstrumente",
         L"Claviers"},
        {L"section_harp", L"竖琴组", L"Harp", L"Harfe", L"Harpe"},
        {L"section_voice", L"声乐组", L"Voices", L"Gesang", L"Voix"},
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
    return id;
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
    // four-language lookup: zh / en / de / fr, with `de` / `fr` falling back to
    // `en` when a row left them out (so a missed row can never break a language)
    const auto pick = [](const wchar_t* a, const wchar_t* fallback) -> std::wstring {
        if (a && *a) return a;
        return fallback && *fallback ? fallback : L"";
    };
    switch (L10n::Get()) {
    case Lang::Zh: return pick(it.zh, it.en);
    case Lang::De: return pick(it.de, it.en);
    case Lang::Fr: return pick(it.fr, it.en);
    default: return pick(it.en, it.zh);
    }
}

std::wstring GroupName(const CatGroup& g) {
    const auto pick = [](const wchar_t* a, const wchar_t* fallback) -> std::wstring {
        if (a && *a) return a;
        return fallback && *fallback ? fallback : L"";
    };
    switch (L10n::Get()) {
    case Lang::Zh: return pick(g.zh, g.en);
    case Lang::De: return pick(g.de, g.en);
    case Lang::Fr: return pick(g.fr, g.en);
    default: return pick(g.en, g.zh);
    }
}
