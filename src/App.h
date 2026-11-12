#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <array>
#include <string>
#include <vector>
#include <map>
#include "L10n.h"
#include "Model.h"

struct FieldBox {
    RECT label{};
    RECT box{};
    RECT edit{};
};

// Channel table columns: 0 = channel, 1 = model, 2 = instrument, 3 = remark,
// 4 = link (链路), 5 = length (长度).  The span is shared by the painter, the
// hit-test and the PDF exporter so a hidden column really collapses the layout
// everywhere.
constexpr int kChCols = 8;

// Top-bar tab index → ui_.layer.  FIVE tabs: 01 项目信息, 02 乐器摆位, 03 话筒摆位,
// 04 通道表, 05 布线.  The internal layer numbers are the STABLE identity every
// painter / hit-test / PDF exporter keys on, so the retired topology page left its
// value 4 free and the others keep theirs:
//   5 = project info, 0 = instruments, 1 = mics, 3 = channel table, 2 = cabling.
// (--layer 3 must stay the channel table: the PDF exporter depends on it, and
//  --layer 2 stays cabling.  --layer 4 is rejected.)
constexpr int kTabCount = 5;
inline int LayerOfTab(int tab) {
    static const int kMap[kTabCount] = {5, 0, 1, 3, 2};
    return (tab >= 0 && tab < kTabCount) ? kMap[tab] : 0;
}
inline int TabOfLayer(int layer) {
    for (int i = 0; i < kTabCount; ++i)
        if (LayerOfTab(i) == layer) return i;
    return 0;
}
inline const S* LayerNameIds() {
    static const S ids[kTabCount] = {S::LayerProject, S::Layer1, S::Layer2,
                                     S::Layer3, S::Layer5};
    return ids;
}

struct ChColSpan {
    double l[kChCols]{};   // text left edge
    double r[kChCols]{};   // text right edge
};

struct ChTableGeom {
    int x0 = 0, x1 = 0;
    int titleY = 0, titleH = 0;
    int chipY = 0, chipH = 0;
    RECT chip[kChCols]{};
    float chipFont = 8.5f;   // the chips shrink 1 pt when the band would overflow
    RECT colLabel{};
    // 「自动排序」: clears the manual row order and re-applies the section order
    RECT autoSort{};
    // pager: 「◀ 第 i / N 页 ▶」 left of 自动排序, plus the rows one page holds
    RECT pagePrev{};
    RECT pageNext{};
    RECT pageLabel{};
    int pageRows = 0;      // rows that FIT on one screen page (no clipped row)
    int headerY = 0, headerH = 0;
    int rowsY = 0, rowH = 0;
    int visibleRows = 0;
    ChColSpan cols;
};

struct UiLayout {
    RECT top{}, left{}, canvas{}, right{}, status{};
    RECT logo{}, brand{}, brandSub{}, version{};
    RECT layerTabs[kTabCount]{};
    // language dropdown: the closed button, plus the floating list that
    // PaintOverlay publishes only while it is open
    RECT langBox{};
    RECT langList{};
    RECT langRow[4]{};
    // 导出图片 scale chooser: the tiny popup under the export chip (2x / 4x / 8x)
    RECT pngScaleList{};
    RECT pngScaleRow[3]{};
    RECT pngScaleFoot{};       // the 「约 W × H px」 line under the three rows
    RECT libTitle{}, libCount{};
    RECT libTabs[2]{};
    RECT searchBox{}, searchEdit{};
    RECT libView{};
    RECT secProject{}, secStage{}, secSel{}, secAppearance{}, secLayers{};
    // layer 5 项目信息: the centred page panel + its title block.  secProject /
    // secStage / secAppearance are reused as the in-panel section headers.
    RECT p1Panel{};
    RECT p1Title{}, p1Rule{};
    FieldBox piece{}, venue{}, conductor{}, notes{};
    FieldBox sysEng{}, recEng{}, asstEng{};
    FieldBox wField{}, dField{}, oField{};
    FieldBox ceilingField{};      // 层高 (m), flown-cable model
    RECT presets[3]{};
    FieldBox selName{};
    FieldBox xField{}, yField{};
    FieldBox angleField{};
    FieldBox micHeight{}, spaceField{}, centerField{};
    RECT flownRow{}, flownCheck{};   // 吊装 checkbox
    FieldBox flyField{};             // 吊装高度 (m)
    FieldBox modelL{}, modelR{}, modelC{}, micModel{};
    FieldBox lenField{};
    RECT selInfo{}, delBtn{}, bindBtn{}, treeBtn{}, spotBtn{}, spotList{};
    RECT spotListRow[16]{};
    // channel table (layer 3)
    RECT chColLabel{};                 // "show columns" caption above the toggle chips
    RECT chChip[kChCols]{};            // column on/off chips
    RECT chPickList{};                 // floating instrument-type picker panel
    std::vector<RECT> chPickRow;       // picker row hit rects
    // 04 「链路 / 路由」 cell: the floating device picker panel + its row hit rects
    RECT chRoutePanel{};
    std::vector<RECT> chRouteRow;
    std::vector<int> chRouteDev;       // device id behind each row
    // two-level 对应乐器 picker: the open submenu panel + its row hit rects
    RECT spotSubPanel{};
    std::vector<RECT> spotSubRow;
    // 图层: 显示乐器文字 / 显示话筒文字 (two independent rows)
    RECT textRow[2]{}, textCheck[2]{};
    // 同类乐器 section header (empty when no instrument type has >= 3 instances)
    RECT secSameType{};
    // 同类乐器: one 「全选 <类型> (N)」 button per instrument defId with >= 3
    // instances (empty when no type qualifies)
    std::vector<RECT> sameTypeBtn;
    std::vector<std::wstring> sameTypeDef;
    int rightContent = 0;
    RECT colorRow[4]{}, colorSwatch[4]{}, colorHex[4]{};
    // per-item 颜色 row: the whole row (opens the picker), its label / swatch /
    // hex text, and the 「恢复默认」 button (empty while there is no override)
    RECT itemColorRow{}, itemColorLabel{}, itemColorSwatch{}, itemColorHex{},
        itemColorReset{};
    RECT layerRow[2]{}, layerCheck[2]{};
    RECT popup{};
    RECT fnSimple{}, fnSra{}, fnPdf{}, fnLabel{}, patternBtn[4]{}, techBtn[4]{}, techPreset[3]{};
    RECT fnPreset{};                 // 预设 chip (opens the preset modal)
    RECT divider[4]{};
    // top-bar 文件 dropdown: the closed button, the floating list and its rows
    // (打开 / 保存 / 另存为 / 注册 .smproj 文件关联 / 解除文件关联)
    RECT fnFile{};
    RECT fileList{};
    RECT fileRow[5]{};
    // layer 03 (话筒拉线): the whole inspector is the cabling panel
    RECT cbSec{};
    FieldBox cbCeilField{};                  // "布线" section header
    RECT cbAdd[kAddDeviceCount]{}; // ＋接口箱 / ＋声卡 / ＋无线接收器
    RECT cbList{};                 // device list panel
    std::vector<RECT> cbRow;       // device row hit rects (rebuilt by Layout)
    RECT cbTotals{};               // per-device cable totals + the grand total
    RECT cbDel{};                  // delete the selected device
    FieldBox cbName{}, cbChannels{};
    FieldBox cbX{}, cbY{};          // device position
    RECT cbKind{};                 // full kind name of the selected device
    RECT cbCap{}, cbInfo{};        // capacity line + capacity notes
    // 05: the selected microphone — info block + the route list of its cables
    RECT cbMiTitle{}, cbMiInfo{}, cbRouteSec{}, cbRouteEmpty{};
    std::vector<RECT> cbRouteRow;    // one row per cable of that microphone
    std::vector<RECT> cbRouteDel;    // 「删除路由」 button of each row
    std::vector<int> cbRouteLink;    // pr_.links index behind each row
    // 05 「连接」 block: click a device row to wire the selected microphone to it
    RECT cbConnSec{}, cbConnEmpty{};
    std::vector<RECT> cbConnRow;     // one row per device (声卡 / 接口箱 / 无线接收器)
    std::vector<int> cbConnDev;      // device id behind each row
    // delete-route confirmation modal (centred, painted in the overlay pass)
    RECT cfScrim{}, cfPanel{}, cfOk{}, cfCancel{}, cfText{};
    // big-group binding confirmation modal (same look as the delete-route one)
    RECT bgScrim{}, bgPanel{}, bgOk{}, bgCancel{}, bgTitle{}, bgMsg{};
    // 预设 modal: scrim + panel + one row per built-in preset + 取消
    RECT pfScrim{}, pfPanel{}, pfCancel{};
    std::vector<RECT> pfRow;         // preset row hit rects (published by the painter)
    // 关闭窗口时的未保存更改提示: scrim + panel + title/message + 保存/不保存/取消
    RECT xpScrim{}, xpPanel{}, xpTitle{}, xpMsg{};
    RECT xpSave{}, xpDiscard{}, xpCancel{};
    RECT cbLeftHint{};             // hint strip at the bottom of the library panel
    // app lock (4 digits): the top-bar padlock chip + the popover it opens
    RECT lockChip{};
    RECT lockPop{};
    RECT lockRow[3]{};             // the popover's buttons (one per state)
    RECT lockCells[4]{};           // the four digit cells of the locked state
    RECT lockOk{}, lockCancel{};   // 确定 / 取消 of the locked popover
    // the passcode modal (设置密码 / 修改密码): scrim + panel + cells + buttons
    RECT lmScrim{}, lmPanel{};
    RECT lmCells[4]{};
    RECT lmOk{}, lmCancel{};
};

struct TileHit {
    RECT r{};
    int index = -1;   // index into the active catalogue, -1 for a group header
};

// The drawn SYMBOL scale of a microphone on 01/02 (the main mic 1.6×, a point /
// lavalier mic 0.3 × 1.7×).  It scales the line art only: every DISTANCE on the
// plot (capsule spacing, the Decca centre offset, cable terminals) is a real
// metre value × ppm_ and never passes through this factor.
double MicSymScaleOf(const Item& it);

// ONE colour per microphone, taken from its BOUND target (Item::spotType): the
// instrument / 声部 tint the 01/02 page already gives that target's own icon
// (MutedColor(defId); the 三角钢琴 is drawn in the user's pr.instColor, so its
// microphones follow that ink).  An UNBOUND microphone keeps the neutral
// microphone ink pr.micColor.  The 05 wiring page draws the microphone marker,
// its cable run, the run's collector stubs and the tag text in this one ink, so a
// 声部's microphones and their cables read as one colour family.
Rgb MicBoundInk(const Project& pr, const Item& it);

// DRAWN line art of that ink: a microphone symbol is a 3 px circle, a cable a
// 1.9 px line and a tag an 8 px text — the MutedColor palette is too LIGHT for
// those on a white plan (the neutral inks the page used before sat darker).  This
// keeps the HUE (the 声部 identity) and caps the luminance at the weight of the
// old neutral ink, so the colour family stays readable and nothing is saturated.
Rgb MicInkLine(const Rgb& tint);

// The muted palette's raw tint for a defId (the ICON colour, before MicInkLine()
// darkens it for cable / marker / tag ink).  Same value the 01/02 page paints on
// that target's own icon; exposed so the --dump-colors check can print the whole
// palette table without duplicating the assignment order.
Rgb StageMutedColor(const std::wstring& id);

// That assignment order itself (src/StageView.cpp kMutedOrder): the defIds that
// own a palette slot, in palette order, plus the palette size.  Colour = slot
// index — never a hash — so this is the only place the order is maintained.
int StageMutedOrderCount();
const wchar_t* StageMutedOrderAt(int i);
int StageMutedPaletteSize();

// CIELab dE between two palette SLOTS: drawnInk = false → the ICON tint, true →
// the ink MicInkLine() produces for cable / marker / tag.  The palette array is
// ordered along a max-min path, so the property to check is the minimum over
// CONSECUTIVE slots (--dump-colors prints it and fails below 12 dE).
double StageMutedPairDE(int slotA, int slotB, bool drawnInk);

// 0xRRGGBB → Rgb (the form Item::colorOverride is stored in).
inline Rgb RgbFromHex(unsigned v) {
    Rgb c;
    c.r = (std::uint8_t)((v >> 16) & 0xFFu);
    c.g = (std::uint8_t)((v >> 8) & 0xFFu);
    c.b = (std::uint8_t)(v & 0xFFu);
    return c;
}

// The ICON tint of one INSTRUMENT item: its own 颜色 override when set, else the
// shared rule (三角钢琴 draws pr.instColor, every other icon the muted palette slot
// of its defId; `muted` = the stage has >= 3 of some type, i.e. the palette is in
// use on this page).  Item::colorOverride therefore colours THAT instance only.
Rgb ItemIconTint(const Project& pr, const Item& it, bool muted);

// The 颜色 override carried by an instrument INSTANCE of `defId` (0 = none).  A
// microphone binds to a TYPE (Item::spotType), so its ink follows the override of
// that type's instance when the user gave it one (MicBoundInk consults this).
unsigned InstColorOverrideFor(const Project& pr, const std::wstring& defId);

struct UiState {
    int layer = 0;
    int libTab = 0;
    int hoverLayer = -1;
    int hoverLibTab = -1;
    int hoverTile = -1;
    int hoverPreset = -1;
    int hoverColor = -1;
    int hoverRow = -1;
    int hoverLang = -1;        // 0 while the language button is hovered
    bool langOpen = false;     // language dropdown list is open
    int langHoverRow = -1;     // hovered row inside the open list
    // 导出图片 scale chooser (ordinary 2x / HD 4x / ultra 8x).  It hangs under the
    // export chip on the drawing pages and is painted last, like the other popups.
    bool pngScaleOpen = false;
    int pngScaleHoverRow = -1;
    int hoverDel = -1;
    int hoverFn = -1;
    int hoverPattern = -1;
    int hoverTech = -1;
    int hoverSearch = 0;
    int focusEdit = 0;
    double scroll = 0;
    double rightScroll = 0;
    std::wstring search;
    // stage view
    double zoom = 1.0;
    double panX = 0, panY = 0;
    // stage-plot display toggles (简化 / SRA / 文字)
    bool simple = false;
    bool showSra = true;
    // 显示文字 is split into two PROJECT flags (pr_.showInstrumentText / pr_.showMicText);
    // hoverRow 9 highlights 显示乐器文字, 10 highlights 显示话筒文字

    int spotListOpen = -1;
    int spotHover = -1;
    int spotScroll = 0;
    // channel table: which of the six columns are visible
    bool chColVis[kChCols] = {true, true, true, true, true, true, true, true};
    int hoverChChip = -1;
    int hoverChAutoSort = 0;    // 「自动排序」 button under the cursor (0 / 1)
    // 03 table pages (no scrolling): the page shown + the hovered pager button (1 prev, 2 next)
    int chPage = 0;
    int hoverChPageBtn = 0;
    // 对应乐器 picker: the open submenu (group index or -1) and the hovered submenu row
    int spotGroup = -1;
    int hoverSpotSub = -1;
    int hoverChRow = -1;        // data row under the cursor (status-bar hover)
    // drag-reorder of the table rows: the row the press armed, whether the drag
    // really started (4 px), and the row it would be dropped on
    int chDragRow = -1;
    bool chDragActive = false;
    int chDropRow = -1;
    POINT chDragStart{};
    int chSpotOpen = -1;        // item id whose instrument-type picker is open
    int chSpotAnchorX = 0;
    int chSpotAnchorY = 0;
    int chSpotHover = -1;
    int chSpotScroll = 0;
    // 04 channel table 「链路 / 路由」 cell: a floating DEVICE picker (the cell itself
    // stays read-only).  chRouteOpen = the microphone item id it was opened for.
    int chRouteOpen = -1;
    int chRouteAnchorX = 0;
    int chRouteAnchorY = 0;
    int chRouteHover = -1;
    // palette popup ("add instrument")
    int popupCat = -1;
    int popupIndex = -1;
    // palette drag-to-place: a press on a tile arms the drag, moving further than
    // 4 px turns it into a real drag that drops the item on the stage
    int dragCat_ = -1;          // catalogue index under the press, -1 = disarmed
    int dragTab_ = -1;          // 0 = instruments, 1 = mics (tab at press time)
    bool paletteDragging_ = false;   // passed the 4 px threshold
    POINT dragStart_{};         // press point (client coords)
    POINT dragPt_{};            // latest cursor point (client coords)
    // top-bar 文件 dropdown
    bool fileOpen = false;
    int fileHoverRow = -1;
    // layer 03 cabling hover state
    int hoverCbAdd = -1;        // 0..2 add-device buttons
    int hoverCbRow = -1;        // device list row
    int hoverCbDel = 0;

    int hoverCbDevice = -1;     // device id under the cursor on the stage
    int hoverCable = -1;        // cable index under the cursor on layer 03
    // 05 microphone inspector: hovered route row / its delete button
    int hoverCbRoute = -1;
    int hoverCbRouteDel = -1;
    // 05 「连接」 block: the device row under the cursor (click = connect)
    int hoverCbConn = -1;
    // delete-route confirmation: the pr_.links index waiting for 确定 (-1 = closed)
    int confirmLink = -1;
    int confirmHover = 0;       // 0 = none, 1 = 确定, 2 = 取消
    // binding a BIG / SPREAD microphone group asks first: the group will be wired
    // as ONE cable that converges (collector stubs), which the user must confirm.
    // While it is open the modal owns the input; 确定 creates the group.
    bool bindConfirmOpen = false;
    int bindHover = 0;          // 0 = none, 1 = 确定, 2 = 取消
    int bindKind = 0;           // MicGroup::kind of the group waiting for 确定
    std::wstring bindTech;      // technique key (kind == 2)
    std::vector<int> bindIds;   // the members that would form the group
    // 预设 modal: open flag + hovered row (-1 none, 0..n-1 preset, n = 取消)
    bool presetOpen = false;
    int presetHover = -1;
    // --nonudge: paint the near-parallel cable split off (A/B regression check);
    // the cosmetic split is otherwise always on
    bool noCableNudge = false;
    // right-click connect: a pending source endpoint (kind 0 = device, 1 = mic)
    int pendingKind = 0;
    int pendingId = 0;
    // transient status-bar message
    std::wstring msg;
    int msgWarn = 0;
    DWORD msgUntil = 0;
};

enum CtrlId : int {
    ID_PIECE = 100,
    ID_VENUE,
    ID_CONDUCTOR,
    ID_NOTES,
    ID_SELNAME, ID_PIANOLEN,
    ID_X, ID_Y, ID_ANGLE,
    ID_MICHEIGHT,
    ID_MAINSPACE,
    ID_MAINCENTER,
    ID_MODELL, ID_MODELR, ID_MODELC,
    ID_SYSENG, ID_RECENG, ID_ASSTENG,
    ID_MICMODEL,
    ID_CHCELL,
    ID_WIDTH,
    ID_DEPTH,
    ID_OFF, ID_CEILING, ID_FLYH,
    ID_SEARCH,
    ID_DEVNAME, ID_DEVCH, ID_DEVX, ID_DEVY,
};

class App {
public:
    static App& Get();
    int Run(HINSTANCE hInst, int nCmdShow);
    static LRESULT CALLBACK StaticProc(HWND, UINT, WPARAM, LPARAM);

    // ---- painting -------------------------------------------------------
    void PaintTopBar(Gdiplus::Graphics& g);
    void LayoutTopBar();   // measured right-aligned chip cluster + tab strip
    void PaintLeft(Gdiplus::Graphics& g);
    void PaintStage(Gdiplus::Graphics& g);
    void PaintRight(Gdiplus::Graphics& g);
    void PaintStatus(Gdiplus::Graphics& g);
    void PaintOverlay(Gdiplus::Graphics& g);
    void PaintChannelTable(Gdiplus::Graphics& g);
    void PaintChannelPicker(Gdiplus::Graphics& g);
    // 04 「链路 / 路由」 cell: the floating DEVICE picker (drawn last, like the
    // instrument picker) and the ONE connect action it shares with the 布线 page
    void PaintChannelRoute(Gdiplus::Graphics& g);
    bool ConnectMicToDevice(int micId, int devId);
    // layer 5 项目信息: the centred page panel on the canvas
    void PaintProjectPage(Gdiplus::Graphics& g);
    struct ChRow {
        int ch = 0;
        int itemId = 0;
        int capsule = -1;   // -1 = none, 0 = L, 1 = C, 2 = R (main microphone)
        std::wstring model;
        std::wstring inst;
        std::wstring remark;
        std::wstring link;      // 链路: "接口箱 1 → 声卡" (empty = no cable)
        double len = 0.0;       // 长度 in metres (RESERVED length), 0 = no cable
        bool custom = false;    // the length is a manual override (「自定义」)
        bool bad = false;       // the stored cable breaks a capacity / length rule
        // --- tail block: one row per INTERFACE BOX → SOUND CARD cable ---------
        bool summary = false;   // a 布线 summary row, not a numbered channel
        bool separator = false; // the 「布线」 separator above the summary rows
        std::wstring chText;    // 通道 cell text for a summary row (「8 通道」)
        // 距主话筒: plan distance from this row's microphone (or its group's
        // convergence point) to the MAIN mic capsule on ITS side — L or R only,
        // never the centre capsule.  Negative = not applicable ("—").
        double toMain = -1.0;
        // 幅角: azimuth seen from the L/R midpoint of the main microphone, measured
        // from the UPSTAGE axis (screen up = world −Y).  Positive = to the RIGHT.
        // hasAz = false → not applicable ("—").
        double azimuth = 0.0;
        bool hasAz = false;
    };
    std::vector<ChRow> BuildChannelRows() const;
    // 布线 summary: one row per 接口箱 → 声卡 cable (device-to-device link), shown
    // as a tail block under the channel rows (screen and PDF)
    std::vector<ChRow> BuildSummaryRows() const;
    // what the table actually paints: channel rows, then 「布线」 + summary rows
    std::vector<ChRow> BuildChannelDisplayRows() const;
    ChColSpan ChannelColumns(double x0, double x1, double chW, double gap,
                             double pad) const;
    ChTableGeom ChannelTableGeom() const;
    int ChannelPickerMaxRows(int yTop) const;
    void OpenChannelCell(int x, int y);
    void CloseChannelCell();
    // 通道表 「话筒线长度」 caption including the active mode:
    // 「话筒线长度(推荐)」 / 「话筒线长度(实际)」 — the header cell toggles it
    std::wstring ChannelLenHeader() const;
    // Move one channel-table row (and the rows of the same item — the main mic's
    // capsules) to another position and store the MANUAL order in the project.
    void ReorderChannelRow(int fromRow, int toRow);
    // binding a group of microphones: a big / widely spread selection needs a
    // confirmation first (it becomes ONE converging cable)
    void RequestMicGroup(const std::vector<int>& ids, int kind,
                         const std::wstring& tech);
    void CreateMicGroup(const std::vector<int>& ids, int kind,
                        const std::wstring& tech);
    void PaintBindConfirm(Gdiplus::Graphics& g);
    void ConfirmBindGroup();
    void CancelBindGroup();
    // farthest pair among the microphones waiting for the group confirmation
    double BindGroupMaxDist() const;
    // 「自动排序」: clear the manual channel order and return to the section order
    void ResetChannelOrder();
    // 03 table paging (◀ ▶ / PageUp·PageDown / ←·→): flip one whole page
    void ChPageMove(int delta);
    // ---- two-level 对应乐器 picker ----------------------------------------
    // one row of a picker level; the TOP level lists the current binding, 侧展,
    // the big categories (a submenu each) and 环境 last
    struct SpotRow {
        int kind = 0;   // 0 current, 2 target, 3 group, 4 instrument
        std::wstring id;      // spotType target id or instrument defId
        std::wstring label;
        std::wstring right;   // count / 「▸」 / 「解除」
    };
    std::vector<SpotRow> SpotTopRows(int micId) const;
    std::vector<SpotRow> SpotSubRows(int group) const;
    int SpotGroupIndexOfKey(const std::wstring& key) const;
    void ApplySpotRow(const SpotRow& row, int micId);    void ExportPdf();
    bool ExportPdfTo(const std::wstring& path);
    // 「导出图片…」: a REAL png of the whole drawing (stage or 布线 page), sized to
    // the content bounding box — never to the window
    // supersampled, TILED png export.  scaleMul = output px per metre as a
    // multiple of the on-screen ppm (2 = 普通, 4 = 高清 (default), 8 = 超清).
    bool ExportPngTo(const std::wstring& path, int scaleMul = 4);
    // total output pixels for a scale multiplier (for the size confirmation)
    void PngExportSize(int scaleMul, int& w, int& h, double& worldW,
                       double& worldH) const;
    // 「导出图片」 header band: the 01 项目信息 fields printed in the blank band
    // ABOVE the drawing, top-left aligned (user: 「在图片上方空白处写上第一页填的
    // 信息，比如左上角」).  The band and its TEXT keep the ON-SCREEN pixel size at
    // every export multiplier (2× / 4× / 8×): only the drawing underneath grows.
    struct PngHeadLine {
        std::wstring text;
        float fontPx = 11.f;   // design px handed to App::Font (Font adds +2)
        bool bold = false;
        int rowPx = 0;         // row height in output px (DPI-scaled, D())
    };
    // the header lines, formatted, with every empty field skipped
    std::vector<PngHeadLine> PngHeaderLines() const;
    // extra canvas height (px) the band adds ABOVE the drawing; 0 = no band
    // (a project with every 项目信息 field empty exports exactly as before)
    int PngHeaderBandPx() const;
    // fill the band with the export canvas background and paint the header
    // top-left; `withText` false (SM_PNG_NOTEXT) leaves the blank band only
    void PaintPngHeaderBand(Gdiplus::Graphics& g, int pageW, bool withText);
    int pngScaleMul_ = 4;   // remembered choice: 普通 2 / 高清 4 / 超清 8
    void SavePngDialog();
    // one row of the 导出图片 scale chooser was picked: remember it (project field
    // pr_.pngScale, written to the .smproj) and run the normal save dialog/export
    void PickPngScale(int mul);
    // the state half of a pick (no dialog): remembered multiplier + project field.
    // PickPngScale = ApplyPngScale + the PNG save dialog; --pngpick N uses this
    // one so the scripted path and the GUI share the same remembering code.
    void ApplyPngScale(int mul);
    // the chooser's three scales, in display order
    static const int kPngScales[3];
    // ---- freeze protection / progress ---------------------------------------
    // A PNG / PDF export runs synchronously on the UI thread, so the window cannot
    // repaint while it works.  busy_ pins a WAIT cursor (WM_SETCURSOR honours it, so
    // a mouse move cannot flip it back) and the status line says what is going on;
    // EndBusy() restores both even when the export fails.
    bool busy_ = false;
    void BeginBusy(const std::wstring& msg);
    void EndBusy();
    // timings (ms) of the last paint / export — status bar, --dump and the SM_TIMING
    // log (TimingLog)
    double lastPaintMs_ = 0.0;
    double lastPngMs_ = 0.0;
    double lastPdfMs_ = 0.0;
    void TimingLog(const std::wstring& line);

    // ---- input ----------------------------------------------------------
    void MouseMove(int x, int y);
    void MouseDown(int x, int y);
    void MouseWheel(int delta, int x, int y);
    void OnKeyDown(int vk);
    int StageHitTest(int x, int y, bool& lockedHit);
    bool PointToWorld(int x, int y, double& wx, double& wy) const;
    void DragTo(int x, int y);
    void MouseUp(int x, int y);
    void SyncXYEdits();
    void SyncAngleEdit();
    bool RotateHandlePos(const Item& it, double& hx, double& hy) const;
    bool RotateHandleHit(int x, int y) const;
    void RotateTo(int x, int y);
    // Rotate a microphone (or the whole MicGroup it belongs to) about the group
    // centroid by the same angle delta.  Shared by the on-canvas handle and the
    // --turn test hook so both exercise exactly the same code.
    void RotateGroupBy(int itemId, double daDeg);
    // Ctrl+A: every ItemKind::Instrument (layer 0), every ItemKind::Mic (layer 1)
    // or every cabling device (layer 2) of the CURRENT layer becomes the selection.
    void SelectAllInLayer();
    // the 同类乐器 buttons use the very same selection container (selIds/selected)
    // ids that travel together when the selection is moved: selIds plus the
    // partners of every grouped microphone, so a bound array keeps its geometry
    std::vector<int> SelectedMoveIds() const;
    // TRUE when the current selection holds at least one ItemKind::Mic AND at
    // least one ItemKind::Instrument (`extraId` is the object a press is about to
    // add / collapse to, so a drag can be judged before the selection changes).
    // Such a selection is never moved, rotated or nudged as one: on a given page
    // only one kind is editable, so the drag would carry a LOCKED object across
    // the page it does not belong to.  Every move path refuses and reports
    // S::MixedKindsMove instead of pushing an undo snapshot.
    bool SelectionMixedKinds(int extraId = -1) const;
    // screen-space bounding box of the current selection (false when empty)
    bool SelectionBBox(RECT& box) const;
    // rotate handle of a MULTI selection: straight above the bbox centre
    bool RotateHandlePosMulti(double& hx, double& hy) const;
    // orbit every selected item about (px,py) and add daDeg to every angle —
    // the multi-selection counterpart of RotateGroupBy, with the selection's
    // bounding-box centre as the pivot
    void RotateSelectionBy(double px, double py, double daDeg);
    void ClearHover();

    // ---- shell ----------------------------------------------------------
    void Layout();
    // THE one vertical rhythm for every labelled field box: the label owns its
    // own line, the box starts below it with a real gap, and the EDIT rect is
    // derived from the box (never from a second, hand-added offset).  Used by the
    // right inspector AND by the 项目信息 page so the two can never drift apart.
    FieldBox FieldRect(int x, int w, int h, int y) const;
    // field box chrome shared by PaintRight and PaintProjectPage
    void PaintFieldBox(Gdiplus::Graphics& g, const FieldBox& f, int ctrlId,
                       bool enabled);
    // layer 5 项目信息 page layout: centres the panel in the canvas and fills
    // every field rect the page paints
    void LayoutProjectPage();
    // The ONE placement path for every native inspector EDIT: `r` must be the very
    // rect the painter fills (FieldBox::edit), never a second derivation of the
    // y rhythm.  The control is hidden when it does not intersect `panel`, hidden
    // when `show` is false, and otherwise CLIPPED to `panel` with a window region,
    // because a child window is only clipped to the parent client area — without
    // the region a scrolled field paints over the top bar / status bar even though
    // the GDI+ painter clipped it away.
    void PlaceInspectorEdit(HWND h, const RECT& r, bool show, const RECT& panel);
    // 吊装 (flown) toggle: shared by the 01/02 checkbox and the 05 one, so both
    // pages set the same height default, re-route and repaint.
    void ToggleFlown();
    void SyncEdits();
    void UpdateStageXf();
    void AddItem(int catIndex);
    // same creation logic as AddItem, but places the new item at the given world
    // position (used by drag & drop); returns the selected item, or nullptr
    Item* AddItemAt(int catIndex, double wx, double wy);
    // the plain-click behaviour of a palette tile: highlight twins and open the
    // 添加乐器 / 话筒 popup underneath it
    void PaletteTileClick(int catIndex);
    void CancelPaletteDrag();
    void DeleteSelected();
    void BindSelected(int kind);
    void MarkMainMics();
    void SplitTechnique();
    // 混合差: two ordinary mics (same CreateItem path) placed, then bound
    void PlaceTechniquePair(const std::wstring& tech, double x, double y, double ang);
    double MainMicSRA() const;
    std::vector<std::wstring> SpotTypeList() const;
    Item* SpotMicInSel();
    // every member a spot-mic binding applies to: the whole bound group (2 mics,
    // Decca tree, technique pair) or the microphone itself
    std::vector<int> SpotTargetsOf(int micId) const;
    // bind (inst non-empty) or clear the 对应乐器 of a mic / its whole group
    void SetSpotType(const std::vector<int>& ids, const std::wstring& inst);
    std::wstring SelectedInstType() const;
    void UnbindSelected();
    void CopySelected();
    void PasteClipboard();
    void SetLanguage(Lang lang);
    void InvalidateAll();
    // Every mutation funnels through here: fresh editor text + layout + paint, and
    // the transient per-page caches dropped, so all five pages read the same data.
    void RefreshAfterProjectChange();
    // ---- undo / redo (snapshot based) ------------------------------------
    // PushUndoSnapshot() records the CURRENT state: call it right BEFORE a
    // mutation.  A snapshot is the whole project as .smproj TEXT, so a restore is
    // exactly the state a saved file would load back.
    void PushUndoSnapshot();
    // a drag / rotate gesture records exactly ONE snapshot, at its first motion
    void GestureUndo();
    void Undo();
    void Redo();
    void ResetUndoHistory();
    void CreateControls();
    void UpdateCueBanners();
    void PickColor(Rgb& c);
    std::wstring ControlText(HWND h);

    // ---- cabling layer 03 (src/Cabling.cpp) ------------------------------
    void LayoutCabling();
    void PaintCablingLayer(Gdiplus::Graphics& g);
    void PaintDevices(Gdiplus::Graphics& g);
    void PaintCables(Gdiplus::Graphics& g);
    void PaintCablingMics(Gdiplus::Graphics& g);
    void PaintCablingHint(Gdiplus::Graphics& g);
    void PaintCablingRight(Gdiplus::Graphics& g);
    void PaintCablingMicInfo(Gdiplus::Graphics& g);   // selected mic + route list
    void PaintCableConfirm(Gdiplus::Graphics& g);     // delete-route modal (overlay)
    // a route is only removed after 确定 in the modal (right-click a cable and the
    // per-row 「删除路由」 button both go through here)
    void RequestDeleteCable(int linkIndex);
    void ConfirmDeleteCable();
    Device* AddDevice(DevKind kind);
    void DeleteSelectedDevice();
    int DeviceHitTest(int x, int y) const;
    int MicMarkerHitTest(int x, int y) const;
    int ConnectableHitTest(int x, int y, int& kind) const;
    int CableHitTest(int x, int y) const;
    void DeviceDragTo(int x, int y);
    bool CablingInspectorDown(int x, int y);
    // the device the SELECTED microphone's cable feeds (a bound group's single
    // cable counts for every member); -1 = not wired.  The 「连接」 block of the
    // 05 inspector marks that row 已连接 with it.
    int MicConnectedDevice(int micId) const;
    // right-click wiring: pick a source, then a target; right-click a cable removes it
    void MouseDownRight(int x, int y);
    bool CanConnect(int ka, int ia, int kb, int ib) const;
    void CreateCable(int ka, int ia, int kb, int ib);
    void StatusMsg(const std::wstring& text, bool warn = false);
    // the cable router terminal radius: the drawn microphone marker in metres, so
    // a cable really touches the circle (see src/Cabling.cpp)
    int MicMarkerRadiusPx() const;
    int MicMarkerRadiusPx(const Item& it) const;
    // the DRAWN marker radius of one microphone, in device px, unrounded (the
    // painter and the router share it so the cable terminal lands on the circle)
    double MicMarkerRadiusF(const Item& it) const;
    double MicRadiusM() const;
    // Canvas view: wheel zoom plus drag panning — a left-drag on EMPTY canvas
    // space, middle-drag anywhere on the canvas, or Space + left-drag.  Ctrl +
    // left-drag on empty canvas is the rubber band instead (see BandBegin
    // below), and a plain click there still only drops the selection (PanEnd).
    bool panning_ = false;
    bool panSpace_ = false;       // Space is held down
    bool panMoved_ = false;       // the drag really moved the view
    bool panDeselect_ = false;    // empty-canvas press: deselect if it never moves
    POINT panStart_{};
    double panStartX_ = 0, panStartY_ = 0;
    double lastMicRadiusM_ = -1.0;   // the zoom-dependent mic marker radius in metres
    void PanBegin(int x, int y, bool deselectOnClick);
    void PanTo(int x, int y);
    void PanEnd();

    // ---- rubber-band multi-select (src/StageView.cpp) ----------------------
    // A CTRL + left-drag that starts on EMPTY canvas space draws a dashed
    // selection rectangle from the press point to the cursor and, on release,
    // selects every object of the CURRENT layer whose rotated footprint
    // intersects it: the instruments on layer 0, the microphones on layer 1, the
    // cabling devices on layer 2.  The band ADDS to the existing selection (that
    // is what Ctrl means here); without Ctrl the empty-canvas drag pans the view
    // instead.  Nothing moves and the old selection survives until the button is
    // released; Esc cancels the band and leaves the selection alone.  A drag
    // under 3 px is a plain click (which, on empty space, deselects).  The band
    // is painted above the stage objects and under every modal, clipped to the
    // canvas.
    void BandBegin(int x, int y, bool add);
    void BandTo(int x, int y);
    void BandEnd(int x, int y);
    void BandCancel();
    bool BandActive() const { return band_ && bandMoved_; }
    RECT BandRect() const;                    // press point -> cursor, ordered
    void PaintRubberBand(Gdiplus::Graphics& g);
    // the device footprint (src/Cabling.cpp owns the geometry) vs the band
    bool DeviceHitsBand(const Device& dv, const RECT& band) const;
    // one object's rotated footprint (screen space) vs the band
    bool BandHitsItem(const Item& it, const RECT& band) const;
    bool band_ = false;          // the gesture is live
    bool bandMoved_ = false;     // it passed the 3 px threshold and really draws
    bool bandAdd_ = false;       // Ctrl was held: add to the existing selection
    POINT bandStart_{};          // press point (client coords)
    POINT bandPt_{};             // latest cursor point (client coords)


    // ---- orchestra presets (src/Presets.cpp) ------------------------------
    // 预设: a top-bar chip opens a centred modal that lists the built-in
    // orchestra layouts (三管编制 · 欧式 / 三管编制 · 美式 / 欧式钢协 / 美式钢协,
    // one row each, in that order).  Picking one REPLACES the whole
    // placement: instruments AND microphones (+ mic groups, main pair and every
    // mic cable) on an empty stage; only the cabling devices stay.
    void OpenPresetModal();
    void ClosePresetModal();
    void PaintPresetModal(Gdiplus::Graphics& g);   // overlay pass (publishes L_.pf*)
    bool PresetModalClick(int x, int y);           // true = the click was consumed
    void LoadPresetAt(int index);

    // ---- stage size (src/App.cpp) ----------------------------------------
    // The one place a stage resize happens (the 音乐厅 / 教堂 / 沙龙 buttons and
    // a manual 宽度 / 深度 edit).  When the stage grows or shrinks the whole
    // layout must stay CENTRED: every item and every cabling device is
    // translated by half the size delta, so the ensemble keeps its distance to
    // the stage centre.  Icons are never scaled.
    void SetStageSize(double w, double d);

    // ---- project file (src/ProjectIO.cpp) --------------------------------
    // true = the project was actually written (a cancelled 另存为 is false, so the
    // close prompt knows the user did NOT save and must not exit)
    bool SaveProjectDialog(bool saveAs);
    void OpenProjectDialog();
    bool SaveProjectTo(const std::wstring& path);
    bool OpenProjectFrom(const std::wstring& path);
    void AfterProjectLoaded();
    void UpdateTitle();

    // ---- 关闭窗口：未保存更改的确认 (src/App.cpp) --------------------------
    // WM_CLOSE (close button, Alt+F4 and the system menu all become WM_CLOSE)
    // with a dirty project opens the modal 是否保存更改？ instead of destroying the
    // window: 保存 runs the normal save, 不保存 exits, 取消 / Esc / a click on the
    // scrim stays in the app.  A clean project, or a locked (read-only) app,
    // exits immediately — the prompt never appears for either.
    void RequestClose();
    // the one click path the three buttons AND the --closechoice test hook share
    // (0 = 保存, 1 = 不保存, 2 = 取消)
    void ExitPromptAnswer(int choice);
    void PaintExitPrompt(Gdiplus::Graphics& g);
    bool ExitPromptClick(int x, int y);   // true = the click was consumed
    // every native EDIT is a real HWND and would paint over the painted prompt
    void HideAllChildEdits();
    bool ExitPromptOpen() const { return exitOpen_; }

    // ---- .smproj file association (src/Assoc.cpp) -------------------------
    // HKCU\Software\Classes\.smproj → StageMicing.Project +
    // HKCU\Software\Classes\StageMicing.Project (DefaultIcon + shell\open\command
    // = "\"<exe>\" \"%1\"").  HKCU only: no elevation, uninstalls cleanly.
    std::wstring startupPng_;   // --png <file>: export the drawing and quit
    int startupPngPick_ = 0;    // --pngpick 2|4|8: pick a chooser row before --png
    int startupLenMode_ = -1;   // --lenmode 0|1: 通道表 话筒线长度 mode (after --open)
    int startupAssoc_ = 0;   // --register-assoc (1) / --unregister-assoc (2)
    bool RegisterSmproj();
    bool UnregisterSmproj();

    // ---- app lock (4-digit passcode, src/Lock.cpp) ------------------------
    // applock.dat next to the executable stores salt + SHA256(salt ‖ code); the
    // code itself is never written, and a missing / corrupt file means 「no lock」.
    bool Locked() const { return lockState_ == 2; }
    bool HasLockCode() const { return lockState_ >= 1; }
    int LockState() const { return lockState_; }
    // THE gate: every mutation entry point starts with
    //     if (RefuseLocked()) return;
    // so a locked app refuses with 「已锁定，只读」 before it can record an undo
    // snapshot, move an object or write the project.
    bool RefuseLocked();
    void LockLoad();
    bool LockSaveCode(const std::wstring& code);
    bool LockCheck(const std::wstring& code) const;
    // applock.dat writer: salt + hash + the LOCKED flag (never the code)
    bool LockWriteFile(const std::vector<unsigned char>& salt,
                       const std::vector<unsigned char>& hash, bool locked);
    void LockApplyCode();
    void LockBeginSet();
    void LockBeginChange();
    void LockNow();
    void LockCloseAll();
    bool LockAnyUiOpen() const { return lockOpen || lockModal != 0; }
    void PaintLockChip(Gdiplus::Graphics& g);
    void PaintLockPopover(Gdiplus::Graphics& g);   // overlay pass (publishes rects)
    bool LockClick(int x, int y);                  // true = the click was consumed
    bool LockModalClick(int x, int y);
    void LockKey(int vk);

    // ---- same-type (defId) groups (item 4, src/StageView.cpp) --------------
    // One instrument defId with >= 3 instances still gets a convex hull built
    // (rotated footprints + 0.25 m padding) — but ONLY as the anchor for the
    // label: the outline is not painted any more.  A CLEAN hull (no item of
    // another defId inside) suppresses the per-instance labels and draws ONE
    // 「类型 ×N」 at its bottom edge; a mixed hull keeps the per-instance labels.
    struct TypeHull {
        std::wstring defId;
        std::wstring name;                        // localized type name
        int count = 0;
        std::vector<std::pair<double, double>> pts;   // world metres
        bool clean = true;                        // no other defId inside
        double labelX = 0, labelY = 0;            // bottom-edge label anchor
    };
    std::vector<TypeHull> BuildTypeHulls() const;
    // the LABEL-ONLY pass: no outline, one caption per clean group (they are
    // collected and placed by the shared label placer, see PaintStage)
    std::vector<TypeHull> BuildTypeHullLabels() const;

    // ---- 同类乐器: 「全选 <类型> (N)」 ---------------------------------------
    // every instrument defId with >= 3 instances, in catalogue order
    std::vector<std::pair<std::wstring, int>> SameTypeGroups() const;
    void SelectSameType(const std::wstring& defId);

    // ---- shared canvas label placement -------------------------------------
    // Places ONE label around (want) trying below → above → left → right and then
    // small nudges, so it overlaps neither the already placed labels (avoid) nor
    // a drawn symbol/device box (obstacles), and never leaves the canvas.  When
    // every candidate collides it returns the LEAST overlapping one — a label
    // must never disappear while its 显示文字 switch is on.
    RECT PlaceCanvasLabel(const RECT& want, const std::vector<RECT>& avoid,
                          const std::vector<RECT>& obstacles, const RECT& canvas,
                          int* overlapScore) const;

    Gdiplus::Font* Font(float px, bool bold = false);

    HINSTANCE hinst_ = nullptr;
    HWND hwnd_ = nullptr;
    HBRUSH fieldBrush_ = nullptr;
    HFONT uiFont_ = nullptr;
    float dpi_ = 1.f;
    float uiScale_ = 1.f;
    int cw_ = 0, ch_ = 0;

    Project pr_;
    UiState ui_;
    UiLayout L_;
    std::vector<TileHit> tiles_;

    double ppm_ = 20, ox_ = 0, oy_ = 0;
    double fieldL_ = 0, fieldT_ = 0, fieldW_ = 0, fieldH_ = 0;

    Gdiplus::FontFamily* family_ = nullptr;
    std::map<int, Gdiplus::Font*> fonts_;
    // PNG export renders at N x the on-screen pixels-per-metre, so every canvas text
    // (labels, tags, ruler) must grow by the same factor; Font() multiplies the design
    // size by this while an export runs (1.0 on screen).
    float exportFontScale_ = 1.f;

    std::wstring exeDir_;
    // ---- app lock state + UI (src/Lock.cpp) ------------------------------
    int lockState_ = 0;        // 0 = no code, 1 = code set + unlocked, 2 = locked
    std::vector<unsigned char> lockSalt_, lockHash_;
    bool lockOpen = false;     // the padlock popover is open
    int lockModal = 0;         // 0 closed, 1 设置, 2 修改(current), 3 修改(new)
    std::wstring lockInput;    // the digits typed so far (max 4)
    bool lockErr = false;      // red hint (wrong code) until the next input
    // hover state of the chip / popover / modal
    int hoverLock = 0;         // 1 while the padlock chip is under the cursor
    int lockHoverRow = -1;
    int hoverLockCell = -1;
    int hoverLockOk = 0;
    int hoverLockCancel = 0;
    int hoverLmCell = -1;
    int lockModalHoverOk = 0;
    int lockModalHoverCancel = 0;
    bool syncing_ = false;
    // true while Layout() re-runs itself after clamping rightScroll: guards the
    // single extra pass so a stored scroll offset can never outlive the rects
    // that were built from it
    bool relaying_ = false;
    bool commitEdit_ = false;   // true while committing a numeric edit (Enter / focus loss)
    std::wstring pdfExportPath_;
    std::wstring projectPath_;      // last saved / opened .smproj
    // unsaved changes: set by PushUndoSnapshot() (every mutation already records
    // an undo snapshot), cleared by a successful save / load.  A fresh start, a
    // just-loaded file and a just-saved file are therefore clean and close without
    // the prompt; a locked app also closes without it (read-only never asks).
    bool dirty_ = false;
    // the exit prompt: open flag + hovered button (0 none, 1 保存, 2 不保存, 3 取消)
    bool exitOpen_ = false;
    int exitHover_ = -1;
    std::wstring startupOpen_;      // --open <path>
    std::wstring startupSave_;      // --save <path> (save and exit, used for tests)
    // --close: post WM_CLOSE after the startup hooks, so a script can exercise the
    // close prompt; --closechoice save|discard|cancel answers it through the very
    // same handler the painted buttons call (0/1/2, -1 = answer by hand)
    bool startupClose_ = false;
    int closeChoice_ = -1;
    std::wstring dumpPath_;         // --dump <path> (write the client layout, tests)
    std::wstring dumpLayoutPath_;   // --dump-layout <path> (instrument rows, tests)
    std::wstring dumpColorsPath_;   // --dump-colors <path> (muted palette table, tests)
    int startupPreset_ = -1;        // --preset <index> (instantiate a preset at launch)
    // --link <aKind> <aId> <bKind> <bId> … scripted wiring for regression tests
    std::vector<std::array<int, 4>> startupLinks_;
    // --adddev <kind> <x> <y> <faceDir> … scripted device placement for tests
    // (kind is the DevKind value; both box values create the ONE stagebox)
    std::vector<std::array<double, 4>> startupAddDevs_;
    // --turn <itemId> <degrees> … scripted rotation for regression tests
    std::vector<std::pair<int, double>> startupTurns_;
    // --select <itemId> | --select device <deviceId> … scripted selection
    int startupSelect_ = 0;
    int startupSelDevice_ = 0;
    int startupScroll_ = 0;   // --scroll <logical px> … preset inspector scroll
    int startupTech_ = -1;    // --tech <0..3> … main microphone technique
    std::wstring startupFuzz_;   // --fuzz <file> … connection-matrix fuzz run
    // --click x y / --rclick x y / --move x y / --key vk: scripted input.  The
    // startup code calls the very same handlers a real mouse / keyboard event
    // reaches, so a screenshot after launch shows the interactive state.
    std::vector<std::array<int, 3>> startupInputs_;

    HWND ePiece_ = nullptr, eVenue_ = nullptr, eConductor_ = nullptr,
         eNotes_ = nullptr, eSelName_ = nullptr, ePianoLen_ = nullptr, eWidth_ = nullptr,
         eDepth_ = nullptr, eOff_ = nullptr, eSearch_ = nullptr;
    HWND eX_ = nullptr, eY_ = nullptr, eAngle_ = nullptr, eMicH_ = nullptr, eSpace_ = nullptr, eCenter_ = nullptr;
    HWND eModelL_ = nullptr, eModelR_ = nullptr, eModelC_ = nullptr;
    HWND eSysEng_ = nullptr, eRecEng_ = nullptr, eAsstEng_ = nullptr;
    HWND eMicModel_ = nullptr;
    HWND eChCell_ = nullptr;
    HWND eDevName_ = nullptr, eDevCh_ = nullptr;
    HWND eDevX_ = nullptr, eDevY_ = nullptr;
    HWND eCeiling_ = nullptr, eFlyH_ = nullptr;
    int chEditItem_ = -1;
    int chEditCol_ = 1;      // 1 = model, 3 = remark
    int chEditCapsule_ = -1; // main mic capsule when the row is a capsule row
    bool drag_ = false;
    int dragId_ = -1;
    std::vector<Item> clip_;
    bool rotating_ = false;
    int rotId_ = -1;
    // multi-selection rotation: the pivot is frozen for the whole gesture, so the
    // moving selection can never feed back into the angle it is dragging
    double rotPivotX_ = 0, rotPivotY_ = 0;   // world coords of the bbox centre
    double rotBase_ = 0;                     // handle angle at the grab (degrees)
    double rotApplied_ = 0;                  // angle already applied (degrees)
    // --ctrl test hook: GetKeyState cannot be faked by a scripted --key event.
    // It is queued IN the input script, so the very next --key gets the Ctrl.
    bool scriptCtrl_ = false;
    // --ctrlband test hook: the same idea for the MOUSE — the next scripted
    // --click (and the drag it starts) is handled as if Ctrl were held down.
    bool scriptCtrlNext_ = false;
    // ---- undo / redo stacks (capped; a snapshot is the project text) --------
    static constexpr size_t kUndoDepth = 50;
    std::vector<std::wstring> undoStack_, redoStack_;
    bool restoring_ = false;         // true while restoring: nothing may be recorded
    bool gestureUndoPushed_ = false; // one snapshot per drag / rotate gesture
    double dragDx_ = 0, dragDy_ = 0;
    bool dragMoved_ = false;
    int downX_ = 0, downY_ = 0;
    // device drag on layer 03
    bool devDrag_ = false;
    int devDragId_ = -1;
    double devDragDx_ = 0, devDragDy_ = 0;
    bool devDragMoved_ = false;

private:
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp);
    void Paint(HDC hdc);
    void OnCreate();
    void RebuildFonts();
    void ApplyDpi();
    void ParseEdit(int id);
    // restore a snapshot: the project, the kept-by-id selection and the cleared
    // transient UI state (pending cable, pickers, cell editor, confirm modal)
    void RestoreSnapshot(const std::wstring& text);
    // shared item factory; usePos == false runs the free-spot scan
    Item* CreateItem(int catIndex, int tab, bool usePos, double wx, double wy);
    int D(int v) const { return (int)(v * dpi_ * uiScale_ + 0.5f); }
};
