#pragma once
#include <string>

enum class Lang { Zh, En, De, Fr };

// Every user-facing string in the shell lives here.  Adding a language means
// one more column in L10n.cpp; the S enum stays language independent.
enum class S {
    AppName, AppSubtitle,
    Layer1, Layer2, Layer3, Layer5, Soon,
    Library, TabInstruments, TabMics, SearchHint, NoResults,
    SectionProject, FieldPiece, FieldVenue, FieldConductor, FieldNotes,
    SectionStage, FieldWidth, FieldDepth, FieldOffstage, UnitMeter,
    PresetHall, PresetChurch, PresetSalon,
    SectionSelection, FieldName, NoSelection, DeleteItem,
    SectionAppearance, ColorInstrument, ColorMic, ColorText, ColorStage,
    SectionLayers, ShowInstruments, ShowMics,
    Audience, OffstageArea, StatusStage, StatusFit, StatusHint, StatusSoon,
    LangZh, LangEn, VersionTag,
    CatKeys, CatStrings, CatWoodwind, CatBrass, CatPerc, CatVoice,
    CatCondenser, CatDynamic, CatSpecial, CatAmbience,
    MetricCount, TypeInstrument, TypeMic, PosLabel, DirLabel,
    FieldPianoLength, Bind2, Bind3, UnbindMic, BindMic,
    XLabel, YLabel, AngleLabel, AddInstrument, FnSimplify, FnRestore, FieldMicHeight, LinkGroup, DeccaTree, MainMic, PatOmni, PatCard, PatSuper, PatFig8, FieldSpacing, AddMic, PatWideCard, TechAB, TechMixed, TechIntensity, FieldTechnique, TechDecca, FieldCenter, SpotMic, SpotClear, ShowText, ModelL, ModelR, ModelC, SysEngineer, RecEngineer, AsstEngineer, FieldModel, ChNum, ChModel, ChInst, ChTitle, FnPdf,
    ChRemark, ColVisible,
    // layer 03 — cabling devices (phase 1)
    CbTitle, CbDeviceList, CbAddAnalog, CbAddDigital, CbAddSoundCard, CbAddAdda,
    CbKindAnalog, CbKindDigital, CbKindSoundCard, CbKindAdda,
    CbAddReceiver, CbKindReceiver,
    CbDevName, CbDevModel, CbChannels, CbProtocol, CbPorts, CbPerPort, CbFaceDir,
    CbFaceUp, CbFaceRight, CbFaceDown, CbFaceLeft,
    CbCapacityFmt, CbProtoLimitFmt, CbCapWarnFmt, CbNoDevice, CbDelDevice,
    CbLeftHint,
    // project file menu
    FnFile, FnSave, FnSaveAs, FnOpen, FileSaved, FileSaveErr, FileOpenErr,
    // right-click wiring (layer 03)
    LinkPickTarget, LinkSame, LinkRejected, LinkCancel, LinkCreatedFmt,
    LinkDeletedFmt, LinkOverFmt, CbCableTagFmt, CbOverloadFmt,
    CbRadioTagFmt,
    // phase 2: automatic routing + channel assignment
    CbAutoRoute, CbAutoDoneFmt, CbAutoWarnFmt, CbAutoCancelled, CbNoCapacity,
    CbLink, CbLen, CbDevTotalFmt, CbGrandTotalFmt,
    CbAutoLocked, CbAutoCancel, CbAutoCancelConfirm, CbAutoCancelHint, CbAutoNone,
    // sample-rate dialog
    SrTitle, SrOk, SrCancel, SrLocked,
    // flown (吊装) microphones + ceiling height
    FieldCeiling, FieldFlown, FieldFlyHeight, CbAnalog, CbFlyBadge,
    CbFlyBreakdownFmt, CbFlyOverFmt,
    CbExWiredRx, CbExRadioWired, CbExProto,
    // the merged 「接口箱 / Stagebox」 (A) — appended last so no existing index moves
    CbAddBox, CbKindStagebox,
    // 05 inspector: the selected microphone, its route list and the delete modal
    CbMiCh, CbMiInst, CbYes, CbNo, CbRouteSec, CbNoRoute, CbDelRoute,
    CbStateOk, CbStateOver, CbStateRadio, CbDelRouteConfirm,
    // cable length pipeline: real (05) / reserved (03, 04) / manual override
    CbCustom, CbLenComposeFmt, CbFlyComposeFmt, CbLenReservedFmt,
    CbDevTotalRealFmt, CbGrandTotalRealFmt, CbCapUsedFmt,
    // 01 项目信息 — the new first page (appended last so no index moves)
    LayerProject,
    // 预设（内置管弦乐编制）— appended last so no existing index moves
    FnPreset, PresetEu, PresetUs, PresetLoaded,
    // stage resize: the ensemble is translated to stay centred
    StageRecentred,
    // 美式钢协 (American Piano Concerto) — appended last so no existing index moves
    PresetUsPiano,
    // Ctrl+A select-all status line (「已全选 N 个」)
    SelectAllFmt,
    // undo / redo status lines
    NoUndo, NoRedo, Undone, Redone, UndoFailed,
    // 欧式钢协 (European Piano Concerto) — appended last so no existing index moves
    PresetEuPiano,
    // rubber-band multi-select status line (「框选 N 个」)
    BandSelFmt,
    // objects the loader skipped while opening a project (an item type the
    // app no longer knows, e.g. the retired 管风琴 / organ)
    IgnoredFmt,
    // 对应乐器 binding of a whole microphone group (a bound pair binds as one unit)
    SpotBoundFmt, SpotUnboundFmt,
    // a selection that mixes microphones with instruments is never moved /
    // rotated / nudged as one (appended last so no existing index moves)
    MixedKindsMove,
    // channel table: a row was dragged to a new place (appended last)
    ChOrderMoved,
    // cable composition of a CONVERGED group (height + collector stubs + main run)
    CbLenConvergeFmt,
    // channel table: the 「自动排序」 button and its status line
    ChAutoSort, ChAutoSorted,
    // re-connecting a microphone replaces its cable (no duplicate pair)
    LinkAlready, LinkRerouted,
    // big-group binding confirmation modal
    BindBigTitle, BindBigFmt,
    // 层高 label: the parenthetical half is drawn in a smaller font
    FieldCeilingSub,
    // 03 channel table: page-break label 「第 i 页 / 共 N 页」
    ChPageFmt, ChPageNextFmt,
    // the picker's own 竖琴 row (harp lives OUTSIDE the strings group)
    CatHarp,
    // ---- app lock (4-digit) — appended last so no existing index moves ------
    LockSet, LockChange, LockLockNow, LockUnlock, LockOk, LockCancel,
    LockWrong, LockSetTitle, LockChangeTitle, LockNewTitle,
    LockReadOnly, LockSetDone, LockChangedDone, LockUnlockedDone, LockNowLocked,
    LockChipHint, LockDigits,
    // ---- .smproj file association ------------------------------------------
    FnAssoc, FnUnassoc, AssocDone, AssocRemoved, AssocFailed,
    // ---- same-type group hull label (「<类型名> ×N」) ------------------------
    HullCountFmt,
    // ---- 显示文字 split in two -------------------------------------------
    ShowInstrText, ShowMicText,
    // ---- 同类乐器: select-all buttons in the inspector ---------------------
    SectionSameType, SameTypeBtnFmt, SelSameFmt,
    // ---- 导出图片… (PNG of the whole drawing) ------------------------------
    FnExportPng, FnExportPngDone, FnExportPngFail,
    // ---- 03 channel table: 「距主话筒」 column ------------------------------
    ChToMain, ChAzimuth, ChToMainShort,
    // ---- MS technique object: its two capsule model fields -----------------
    ModelM, ModelS,
    // ---- 导出图片 scale chooser (普通 2x / 高清 4x / 超清 8x) ----------------
    // the row labels carry no number: the chooser appends 「 2× 」「 4× 」「 8×」
    PngScaleNormal, PngScaleHigh, PngScaleUltra,
    PngScaleSizeFmt,     // 「约 %d × %d px」
    // ---- 通道表 话筒线长度 推荐 / 实际 (header click) -------------------------
    LenModeReserved, LenModeReal, LenModeOnReal, LenModeOnReserved,
    // ---- progress / freeze protection (export runs on the UI thread) --------
    BusyPngFmt,     // 「正在导出图片…（%d×）」
    BusyPdf,        // 「正在导出 PDF…」
    BusyDoneFmt,    // 「%s · %.1f s」
    // ---- per-item 颜色 override (inspector row + its status lines) ----------
    Color,          // 「颜色」
    ColorReset,     // 「恢复默认」
    ColorSetFmt,    // 「已设为自定义颜色 #%06X」
    ColorResetDone, // 「已恢复默认颜色」
    // ---- 布线 page 「连接」 block: click a device to wire the selected mic -----
    CbConnSec,      // 「连接」
    CbConnDoneFmt,  // 「已连接到 %s」
    CbConnMark,     // 「已连接」 (the row of the device already wired)
    // ---- 关闭窗口时的未保存更改提示 (保存 / 不保存 / 取消) --------------------
    AskSaveMsg,     // 「是否保存更改？」
    AskSaveSave,    // 「保存」
    AskSaveDiscard, // 「不保存」
    AskSaveCancel,  // 「取消」
    // demo2 built-in presets (预设 popup, rows 4..11 of twelve) — appended
    // last so no existing index moves
    PresetAncient,         // 古乐团
    PresetChoirOrch,       // 合唱乐团
    PresetChoirPiano,      // 合唱+钢琴
    PresetQuartet1,        // 弦乐四重奏 1
    PresetQuartet2,        // 弦乐四重奏 2
    PresetPianoTrio,       // 钢琴三重奏
    PresetPianoQuartet,    // 钢琴四重奏
    PresetPianoSolo,       // 钢琴独奏
    Count
};

namespace L10n {

void Set(Lang l);
Lang Get();
bool IsZh();
const wchar_t* T(S id);

// Short badge for the top-bar language dropdown: 中 / EN / DE / FR.
const wchar_t* Code(Lang l);
// Language name written in that language: 中文 / English / Deutsch / Français.
const wchar_t* Label(Lang l);

} // namespace L10n
