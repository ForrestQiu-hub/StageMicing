#pragma once
#include "L10n.h"   // for the S enum used by SpotGroupName()
#include <string>
#include <vector>

struct CatItem {
    const wchar_t* id;
    const wchar_t* zh;
    const wchar_t* en;
    const wchar_t* group;
    double w;   // footprint width in metres (top view)
    double h;   // footprint depth in metres
    // German / French names, appended last so a row may leave them out: nullptr
    // falls back to `en` in ItemName(), a missed row can never break a language.
    const wchar_t* de = nullptr;
    const wchar_t* fr = nullptr;
};

struct CatGroup {
    const wchar_t* id;
    const wchar_t* zh;
    const wchar_t* en;
    // German / French section heading (nullptr → the English one)
    const wchar_t* de = nullptr;
    const wchar_t* fr = nullptr;
};

const std::vector<CatItem>& Instruments();
const std::vector<CatItem>& Mics();
const std::vector<CatGroup>& InstrumentGroups();
const std::vector<CatGroup>& MicGroups();
// Two EXTRA 对应乐器 targets that are not instruments and are never placed on the
// stage: 「侧展 / Side extension」 and 「环境 / Ambience」.  They live in the spot-mic
// binding pickers only (right inspector, 点话筒 list, 03 对应乐器 cell) and are
// stored as ordinary Item::spotType strings (section_side / section_ambience), so
// an old .smproj that never heard of them still loads.
const std::vector<CatItem>& SpotExtras();
inline bool IsSpotExtra(const std::wstring& id) {
    return id == L"section_side" || id == L"section_ambience" ||
           id == L"section_surround" || id == L"section_height";
}
// Localized name of a 对应乐器 key: the two extras, a catalogue instrument, or the
// raw string for anything unknown (an old project's binding must still print).
std::wstring SpotTypeName(const std::wstring& id);
// A BINDABLE BIG CATEGORY (弦乐 / 木管 / 铜管 / 打击乐 / 键盘乐器 / 竖琴 / 声乐): a mic
// may be bound to the whole section instead of one instrument.
bool IsSectionTarget(const std::wstring& id);
// two-level 对应乐器 picker: the category rows and their instruments
int SpotGroupCount();
const wchar_t* SpotGroupKey(int g);
const wchar_t* SpotGroupTarget(int g);
S SpotGroupName(int g);
std::vector<std::wstring> SpotGroupItems(int g);
// which category an instrument belongs to (nullptr when it has none)
const wchar_t* SpotTargetOfDefId(const std::wstring& defId);
int SpotGroupIndexOfDefId(const std::wstring& defId);
const CatItem* FindItem(const std::wstring& id);
// global size factor: everything except the grand piano is 90%
double CatScale(const std::wstring& id);
// nominal stereo recording angle of a technique item (0 when not a technique)
double ItemSRA(const std::wstring& id);
std::wstring ItemName(const CatItem& it);
std::wstring GroupName(const CatGroup& g);
