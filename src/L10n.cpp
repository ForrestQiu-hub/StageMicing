#include "L10n.h"

namespace {

using S_ = S;

const wchar_t* kZh[] = {
    L"舞台摆位",                             // AppName
    L"古典音乐会 · 乐器与话筒摆位",            // AppSubtitle
    L"乐器摆位", L"话筒摆位", L"通道表", L"布线",  // Layer1-5 (04 拓扑图 retired)
    L"即将",                                  // Soon
    L"素材库", L"乐器", L"话筒",               // Library / tabs
    L"搜索名称…",                             // SearchHint
    L"没有匹配的素材",                         // NoResults
    L"项目信息", L"项目名称", L"场地", L"指挥", L"备注",  // project
    L"舞台尺寸", L"宽度", L"深度", L"外围", L"m",              // stage
    L"音乐厅", L"教堂", L"沙龙",               // presets
    L"选中对象", L"名称", L"未选择对象", L"删除选中",            // selection
    L"外观颜色", L"乐器线条", L"话筒线条", L"文字", L"舞台底色", // appearance
    L"图层", L"显示乐器", L"显示话筒",          // layers
    L"观众席", L"舞台外围空间",                 // stage canvas
    L"舞台",
    L"适应窗口",                                // fit
    L"Ctrl+拖动 = 框选 · 拖动空白 = 平移视图 · 单击放置 · 拖动移动 · 右键连线",  // StatusHint
    L"第 %d 层 · %s · 即将推出",                // status soon
    L"中", L"EN", L"v0.1 · 摆位编辑",           // lang / version
    L"键盘", L"弦乐", L"木管", L"铜管", L"打击乐", L"人声与指挥",
    L"电容式", L"动圈 / 铝带", L"特殊用途", L"环境 / 立体声",
    L"%d 个元素", L"乐器", L"话筒", L"位置", L"方向",
    L"钢琴长度", L"绑定为 2 话筒组", L"绑定为 3 话筒组", L"解除绑定", L"绑定话筒",
    L"X (m)", L"Y (m)", L"角度 (°)", L"添加乐器", L"简化界面", L"恢复图标", L"高度 (m)", L"链接组", L"Decca 树", L"主话筒", L"全指向", L"心形", L"超心形", L"8字", L"间距 (m)", L"添加话筒", L"宽心形", L"AB", L"混合差", L"强度差", L"制式", L"Decca 树", L"中置突出 (m)", L"绑定点话筒", L"解除点话筒", L"显示文字", L"L 型号", L"R 型号", L"C 型号", L"系统", L"录音", L"助理", L"型号", L"通道", L"型号", L"乐器", L"通道表", L"导出 PDF", L"备注", L"显示列",
    // layer 03 cabling (phase 1) + project file menu
    L"布线", L"设备清单", L"＋模拟接口箱", L"＋数字接口箱", L"＋声卡", L"＋ADDA 转换器",
    L"模拟接口箱", L"数字接口箱", L"声卡", L"ADDA 转换器",
    L"＋无线接收器", L"无线接收器",
    L"名称", L"型号", L"通道数", L"协议", L"端口数", L"每口通道数", L"接口面朝向",
    L"上", L"右", L"下", L"左",
    L"%d × %d = %d 通道", L"%s · %.1f kHz：每口上限 %d 通道",
    L"! 需求 %d 通道，容量只有 %d 通道", L"尚未添加设备", L"删除设备",
    L"右键连线 · 拖动移动 · Del 删除",
    L"文件", L"保存项目", L"另存为…", L"打开项目",
    L"项目已保存：", L"保存失败：", L"打开失败：",
    // right-click wiring
    L"选择要连接的第二个对象（右键空白处取消）", L"不能连接同一个对象",
    L"这两个对象无法连接", L"已取消连接",
    L"已连接 %s → %s（%d 通道）", L"已删除线缆 %s → %s",
    L"容量不足：需要 %d 通道，%s 只有 %d 通道可用", L"%d 通道 · %.1f m",
    L"输入超载：%s 已接 %d 路，容量 %d 路",
    L"无线 %d 通道",
    // phase 2: automatic routing + channel assignment
    L"自动布线", L"自动布线：%d 路 · %.1f m", L"%d 路话筒没有可用容量",
    L"已取消自动布线，可重新编辑；再次点「自动布线」会重新生成", L"无可用容量",
    L"路由", L"话筒线长度", L"%s 共 %d 路 / %.1f m（含预留）", L"合计 %d 路 / %.1f m（含预留）",
    L"自动布线结果已锁定，请先“取消自动布线”再移动",
    L"取消自动布线", L"确认取消", L"再次点击“确认取消”即可解除锁定", L"没有可取消的自动布线",
    L"采样率", L"确定", L"取消", L"请先“取消自动布线”再修改采样率",
    L"层高", L"吊装", L"吊装高度", L"模拟", L"吊",
    L"吊装 %.1f m + 吊顶 %.1f m + 沿墙 %.1f m + 地面 %.1f m = %.1f m → %.1f m 含预留",
    L"%d 支吊装话筒的吊装高度超过层高，已按层高计算",
    L"有线话筒不能接无线接收器", L"无线话筒必须先接无线接收器", L"协议不匹配：%s ≠ %s",
    // A: the merged stagebox (appended last, after CbExProto)
    L"＋接口箱", L"接口箱",
    // 05 inspector: selected microphone + route list + delete modal
    L"通道数", L"绑定的乐器", L"是", L"否", L"路由", L"尚未布线", L"删除路由",
    L"正常", L"超容量", L"无线", L"确定删除这条路由吗？",
    // length pipeline
    L"自定义", L"高度 %.1f + 地面 %.1f = %.1f m",
    L"吊装 %.1f + 吊顶 %.1f + 沿墙 %.1f + 地面 %.1f = %.1f m",
    L"%.1f m → 标准 %.1f m", L"%s 共 %d 路 / %.1f m（实长）",
    L"合计 %d 路 / %.1f m（实长）", L"%d 通道 · 已用 %d",
    // 01 项目信息（新增首页）
    L"项目信息",
    // 预设（内置管弦乐编制）
    L"预设", L"三管编制 · 欧式", L"三管编制 · 美式", L"已载入 %s",
    // 舞台尺寸变化：布局整体平移，保持居中
    L"舞台尺寸已调整，布局保持居中",
    // 美式钢协（用户自己的美式 + 三角钢琴编制）
    L"美式钢协",
    // Ctrl+A 全选当前层的全部对象
    L"已全选 %d 个",
    // 撤销 / 重做
    L"无可撤销", L"无可重做", L"已撤销", L"已重做", L"撤销失败",
    // 欧式钢协（用户自己的欧式 + 三角钢琴编制）
    L"欧式钢协",
    // 框选（橡皮筋多选）
    L"框选 %d 个",
    // 载入项目时跳过的未知对象（已下线的乐器类型）
    L"已忽略 %d 个无法识别的对象",
    // 整组话筒的「对应乐器」绑定
    L"已绑定 %d 支话筒 → %s", L"已解除 %d 支话筒的乐器绑定",
    // 话筒与乐器混选：不能一起移动 / 旋转 / 微调
    L"话筒与乐器不能一起移动",
    // 通道表：手动拖动了一行
    L"已把第 %d 行移到第 %d 行",
    // 编组话筒汇集后的实长组成
    L"高度 %.1f + 汇集 %.1f + 主干 %.1f = %.1f m",
    // 03 通道表工具条：自动排序按钮 + 状态提示
    L"自动排序", L"已恢复自动排序",
    L"已连过：这条线缆已存在", L"已改接 %s → %s（%d 路）",
    L"编组话筒较多", L"编组话筒较多/间距较大（%d 支 · 最远 %.1f m），将按一条线缆汇集，是否继续？",
    L"（吊装设备高度）", L"第 %d 页 / 共 %d 页",
    L"下页从第 %d 行开始",
    L"竖琴",
    // ---- 应用锁（4 位数字） ----
    L"设置密码…", L"修改密码…", L"锁定", L"解锁", L"确定", L"取消",
    L"密码错误，请重新输入", L"设置密码（4 位数字）", L"修改密码：先输入当前密码",
    L"修改密码：输入新密码", L"已锁定，只读", L"密码已设置", L"密码已修改",
    L"已解锁", L"已锁定", L"应用锁", L"输入 4 位数字",
    // ---- .smproj 文件关联 ----
    L"注册 .smproj 文件关联", L"解除文件关联", L"已注册 .smproj 文件关联",
    L"已解除 .smproj 文件关联", L"文件关联写入失败",
    // ---- 同类型 ≥3 个的组标签 ----
    L"%s ×%d",
    // ---- MS 制式对象的两个型号字段 ----
    L"显示乐器文字", L"显示话筒文字",
    // ---- 同类乐器 ----
    L"同类乐器", L"全选 %s (%d)", L"已全选 %s ×%d",
    L"导出图片…", L"已导出图片", L"导出图片失败",
    L"距主话筒", L"幅角", L"距主话筒",
    L"M 型号", L"S 型号",
    // ---- 导出图片 清晰度选择 ----
    L"普通", L"高清", L"超清", L"约 %d × %d px",
    // ---- 话筒线长度：推荐 / 实际 ----
    L"推荐", L"实际", L"已切换为实际长度", L"已切换为推荐长度",
    // ---- 导出进度（导出占用 UI 线程，先给出提示与等待光标） ----
    L"正在导出图片…（%d×）", L"正在导出 PDF…", L"%s · %.1f 秒",
    // ---- 单个对象自定义颜色 ----
    L"颜色", L"恢复默认", L"已设为自定义颜色 #%06X", L"已恢复默认颜色",
    // ---- 布线「连接」区块 ----
    L"连接", L"已连接到 %s", L"已连接",
    // ---- 关闭窗口时的未保存更改提示 ----
    L"是否保存更改？", L"保存", L"不保存", L"取消",
    // demo2 built-in presets (预设 popup)
    L"古乐团",
    L"合唱乐团",
    L"合唱+钢琴",
    L"弦乐四重奏 1",
    L"弦乐四重奏 2",
    L"钢琴三重奏",
    L"钢琴四重奏",
    L"钢琴独奏",
    // ---- SHORT chip labels (top bar / 03 toolbar) — see App::FitChip ------
    L"文件", L"导出图片", L"导出 PDF", L"简化界面", L"恢复图标", L"预设",
    L"自动排序",
    // ---- 03 显示列 chips: the compact form when the band is tight -------
    L"通道", L"型号", L"乐器", L"备注", L"路由", L"线长", L"距主", L"幅角",
};

const wchar_t* kEn[] = {
    L"Stage Micing",
    L"Classical stage & microphone planning",
    L"Instruments", L"Microphones", L"Channel list", L"Wiring",
    L"soon",
    L"LIBRARY", L"Instruments", L"Microphones",
    L"Search…",
    L"No matching symbols",
    L"PROJECT", L"Work", L"Venue", L"Conductor", L"Notes",
    L"STAGE", L"Width", L"Depth", L"Off-stage", L"m",
    L"Hall", L"Church", L"Salon",
    L"SELECTION", L"Name", L"Nothing selected", L"Delete selected",
    L"APPEARANCE", L"Instrument lines", L"Microphone lines", L"Text", L"Stage fill",
    L"LAYERS", L"Show instruments", L"Show microphones",
    L"AUDIENCE", L"OFF-STAGE",
    L"Stage",
    L"fit to window",
    L"Ctrl+drag = band select · drag empty = pan view · drag to move (instruments locked) · type coordinates at right",  // StatusHint
    L"Layer %d · %s · coming soon",
    L"中", L"EN", L"v0.1 · static UI",
    L"KEYBOARD", L"STRINGS", L"WOODWIND", L"BRASS", L"PERCUSSION", L"VOICE & PODIUM",
    L"CONDENSER", L"DYNAMIC / RIBBON", L"SPECIALTY", L"AMBIENCE / STEREO",
    L"%d elements", L"Instrument", L"Microphone", L"Pos", L"Dir",
    L"Piano length", L"Bind 2-mic group", L"Bind 3-mic group", L"Unbind", L"Bind mics",
    L"X (m)", L"Y (m)", L"Angle (°)", L"Add instrument", L"Simple", L"Restore icons", L"Height (m)", L"Link group", L"Decca tree", L"Main mics", L"Omni", L"Cardioid", L"Supercard", L"Fig-8", L"Spacing (m)", L"Add microphone", L"Wide card", L"AB", L"Mixed", L"Intensity", L"Technique", L"Decca Tree", L"Centre offset (m)", L"Bind spot mic", L"Clear spot mic", L"Show text", L"L model", L"R model", L"C model", L"System engineer", L"Recording engineer", L"Assistant engineer", L"Model", L"Ch", L"Model", L"Instrument", L"Channel list", L"Export PDF", L"Remark", L"Columns",
    // layer 03 cabling (phase 1) + project file menu
    L"CABLING", L"DEVICES", L"+ Analog Stagebox", L"+ Digital Stagebox",
    L"+ Audio Interface", L"+ ADDA Converter",
    L"Analog Stagebox", L"Digital Stagebox", L"Audio Interface", L"ADDA Converter",
    L"+ Wireless Receiver", L"Wireless Receiver",
    L"Name", L"Model", L"Channels", L"Protocol", L"Ports", L"Ch / port", L"Face",
    L"Up", L"Right", L"Down", L"Left",
    L"%d × %d = %d ch", L"%s · %.1f kHz: max %d ch per port",
    L"! %d channels requested, capacity is %d", L"No devices yet", L"Delete device",
    L"06 Wiring: add boxes / the interface on the right, drag to move, Delete to remove; right-click two objects to create the cable",
    L"File", L"Save project", L"Save as…", L"Open project",
    L"Project saved: ", L"Could not save: ", L"Could not open: ",
    // right-click wiring
    L"Right-click the second object (right-click empty space to cancel)",
    L"An object cannot be connected to itself",
    L"These two objects cannot be connected", L"Connection cancelled",
    L"Connected %s → %s (%d ch)", L"Cable removed: %s → %s",
    L"Capacity exceeded: needs %d ch, %s has only %d",
    L"%d ch · %.1f m",
    L"Input overload: %s has %d ch connected, capacity %d",
    L"wireless %d ch",
    // phase 2: automatic routing + channel assignment
    L"Auto-route", L"Auto-route: %d ch · %.1f m", L"%d mics have no free capacity",
    L"Auto-routing cancelled — the objects are editable again", L"no free capacity",
    L"Link", L"Mic cable", L"%s: %d ch / %.1f m", L"Total: %d ch / %.1f m",
    L"Auto layout is locked — press “取消自动布线” before moving anything",
    L"Cancel auto layout", L"Confirm cancel",
    L"Click “Confirm cancel” again to unlock everything",
    L"There is no auto layout to cancel",
    L"Sample Rate", L"OK", L"Cancel",
    L"Cancel the auto layout before changing the sample rate",
    L"Ceiling", L"Flown", L"Fly height", L"analog", L"FLY",
    L"riser %.1f m + ceiling %.1f m + wall %.1f m + floor %.1f m = %.1f m → %.1f m with slack",
    L"%d flown mics hang above the ceiling; the ceiling height was used",
    L"A wired microphone cannot feed a wireless receiver", L"A wireless microphone must go through a receiver first", L"Protocol mismatch: %s ≠ %s",
    // A: the merged stagebox
    L"+ Stagebox", L"Stagebox",
    // 05 insp: microphone + routes + delete confirmation
    L"Channels", L"Bound instrument", L"Yes", L"No", L"Routing", L"Not wired yet",
    L"Delete route",
    L"normal", L"over capacity", L"wireless", L"Delete this route?",
    // length pipeline
    L"custom", L"height %.1f + floor %.1f = %.1f m",
    L"flown %.1f + ceiling %.1f + wall %.1f + floor %.1f = %.1f m",
    L"%.1f m → standard %.1f m", L"%s: %d ch / %.1f m (actual)",
    L"Total: %d ch / %.1f m (actual)", L"%d ch · used %d",
    // 01 project info (the new first page)
    L"Project",
    // presets (built-in orchestra layouts)
    L"Presets", L"Triple Wind · European", L"Triple Wind · American", L"Loaded %s",
    // stage resize: the layout keeps its centre on the new stage
    L"Stage resized, the layout stays centred",
    // American Piano Concerto
    L"American Piano Concerto",
    // Ctrl+A selected every object of the current layer
    L"Selected all %d",
    // undo / redo
    L"Nothing to undo", L"Nothing to redo", L"Undone", L"Redone", L"Undo failed",
    // European Piano Concerto
    L"European Piano Concerto",
    // rubber-band multi-select
    L"Marquee selected %d",
    // objects skipped while opening a project (retired instrument type)
    L"Ignored %d unknown object(s)",
    // 对应乐器 binding of a whole microphone group
    L"%d microphone(s) bound to %s", L"%d microphone(s) unbound",
    // mixed microphone / instrument selection: never moved as one
    L"Microphones and instruments cannot be moved together",
    // channel table: a row was dragged to a new place
    L"Moved channel row %d to %d",
    // composition of a converged group cable
    L"height %.1f + collectors %.1f + main run %.1f = %.1f m",
    // 03 通道表工具条：自动排序按钮 + 状态提示
    L"Auto sort", L"Restored the automatic order",
    L"Already wired", L"Re-routed %s → %s (%d ch)",
    L"Large microphone group", L"%d microphones, farthest %.1f m apart — they will be wired as ONE converging cable. Continue?",
    L" (flown gear height)", L"Page %d of %d",
    L"next page starts at row %d",
    L"HARP",
    // ---- app lock (4 digits) ----
    L"Set passcode…", L"Change passcode…", L"Lock", L"Unlock", L"OK", L"Cancel",
    L"Wrong passcode, try again", L"Set a 4-digit passcode",
    L"Change passcode: enter the current one",
    L"Change passcode: enter the new one", L"Locked — read only",
    L"Passcode set", L"Passcode changed", L"Unlocked", L"Locked",
    L"App lock", L"Enter 4 digits",
    // ---- .smproj file association ----
    L"Register .smproj association", L"Remove file association",
    L".smproj association registered", L".smproj association removed",
    L"Could not write the file association",
    // ---- same-type group label ----
    L"%s ×%d",
    // ---- MS technique object: its two capsule model fields ----
    L"Instrument text", L"Microphone text",
    // ---- same-type instruments ----
    L"Same type", L"Select all %s (%d)", L"Selected all %s ×%d",
    L"Image", L"Image exported", L"Could not export the image",
    L"To main mic", L"Azimuth", L"To main mic",
    L"M model", L"S model",
    // ---- 导出图片 scale chooser ----
    L"Standard", L"High", L"Ultra", L"about %d × %d px",
    // ---- mic cable length: recommended / actual ----
    L"Recommended", L"Actual", L"Switched to actual lengths",
    L"Switched to recommended lengths",
    // ---- export progress (the export blocks the UI thread) ----
    L"Exporting image… (%d×)", L"Exporting PDF…", L"%s · %.1f s",
    // ---- per-object custom colour ----
    L"Color", L"Reset", L"Custom color #%06X", L"Color reset to the palette",
    // ---- wiring 「Connect」 block ----
    L"Connect", L"Connected to %s", L"Connected",
    // ---- unsaved changes on window close ----
    L"Save changes?", L"Save", L"Don't save", L"Cancel",
    // demo2 built-in presets (预设 popup)
    L"Chamber Orchestra",
    L"Choir Orchestra",
    L"Choir + Piano",
    L"String Quartet 1",
    L"String Quartet 2",
    L"Piano Trio",
    L"Piano Quartet",
    L"Piano Solo",
    // ---- SHORT chip labels (top bar / 03 toolbar) — see App::FitChip ------
    L"File", L"Image", L"PDF", L"Simple", L"Restore", L"Presets",
    L"Auto sort",
    L"Ch", L"Model", L"Inst", L"Remark", L"Link", L"Cable", L"To main",
    L"Azimuth",
};

// One entry per line, each tagged with its S id, so a new language can be
// added or checked column-by-column against the enum above.
const wchar_t* kDe[] = {
    L"Stage Micing",                    // AppName
    L"Klassische Bühne & Mikrofonplanung",  // AppSubtitle
    L"Instrumente",                     // Layer1
    L"Mikrofone",                       // Layer2
    L"Kanalliste",                      // Layer3
    L"Verkabelung",                     // Layer5
    L"bald",                            // Soon
    L"BIBLIOTHEK",                      // Library
    L"Instrumente",                     // TabInstruments
    L"Mikrofone",                       // TabMics
    L"Suchen…",                         // SearchHint
    L"Keine passenden Symbole",         // NoResults
    L"PROJEKT",                         // SectionProject
    L"Werk",                            // FieldPiece
    L"Ort",                             // FieldVenue
    L"Dirigent",                        // FieldConductor
    L"Notizen",                         // FieldNotes
    L"BÜHNE",                           // SectionStage
    L"Breite",                          // FieldWidth
    L"Tiefe",                           // FieldDepth
    L"Außerhalb",                       // FieldOffstage
    L"m",                               // UnitMeter
    L"Saal",                            // PresetHall
    L"Kirche",                          // PresetChurch
    L"Salon",                           // PresetSalon
    L"AUSWAHL",                         // SectionSelection
    L"Name",                            // FieldName
    L"Nichts ausgewählt",               // NoSelection
    L"Auswahl löschen",                 // DeleteItem
    L"DARSTELLUNG",                     // SectionAppearance
    L"Instrumentenlinien",              // ColorInstrument
    L"Mikrofonlinien",                  // ColorMic
    L"Text",                            // ColorText
    L"Bühnenfarbe",                     // ColorStage
    L"EBENEN",                          // SectionLayers
    L"Instrumente anzeigen",            // ShowInstruments
    L"Mikrofone anzeigen",              // ShowMics
    L"PUBLIKUM",                        // Audience
    L"AUSSERHALB",                      // OffstageArea
    L"Bühne",                           // StatusStage
    L"ans Fenster anpassen",            // StatusFit
    L"Strg+Ziehen = Rahmenauswahl · leeres Feld = Ansicht · Ziehen = verschieben (Instrumente gesperrt) · Koordinaten rechts",  // StatusHint
    L"Ebene %d · %s · bald verfügbar",  // StatusSoon
    L"中",                              // LangZh
    L"EN",                              // LangEn
    L"v0.1 · statische Oberfläche",     // VersionTag
    L"TASTATUR",                        // CatKeys
    L"STREICHER",                       // CatStrings
    L"HOLZBLÄSER",                      // CatWoodwind
    L"BLECHBLÄSER",                     // CatBrass
    L"SCHLAGZEUG",                      // CatPerc
    L"STIMME & PODIUM",                 // CatVoice
    L"KONDENSATOR",                     // CatCondenser
    L"DYNAMISCH / BÄNDCHEN",            // CatDynamic
    L"SPEZIAL",                         // CatSpecial
    L"RAUM / STEREO",                   // CatAmbience
    L"%d Elemente",                     // MetricCount
    L"Instrument",                      // TypeInstrument
    L"Mikrofon",                        // TypeMic
    L"Pos",                             // PosLabel
    L"Richt.",                          // DirLabel
    L"Klavierlänge",                    // FieldPianoLength
    L"Als 2-Mikrofon-Gruppe binden",    // Bind2
    L"Als 3-Mikrofon-Gruppe binden",    // Bind3
    L"Lösen",                           // UnbindMic
    L"Mikrofone binden",                // BindMic
    L"X (m)",                           // XLabel
    L"Y (m)",                           // YLabel
    L"Winkel (°)",                      // AngleLabel
    L"Instrument hinzufügen",           // AddInstrument
    L"Einfach",                         // FnSimplify
    L"Symbole wiederherstellen",        // FnRestore
    L"Höhe (m)",                        // FieldMicHeight
    L"Gruppe verknüpfen",               // LinkGroup
    L"Decca-Baum",                      // DeccaTree
    L"Hauptmikrofone",                  // MainMic
    L"Kugel",                           // PatOmni
    L"Niere",                           // PatCard
    L"Superniere",                      // PatSuper
    L"Acht",                            // PatFig8
    L"Abstand (m)",                     // FieldSpacing
    L"Mikrofon hinzufügen",             // AddMic
    L"Breite Niere",                    // PatWideCard
    L"AB",                              // TechAB
    L"Gemischt",                        // TechMixed
    L"Intensität",                      // TechIntensity
    L"Technik",                         // FieldTechnique
    L"Decca-Baum",                      // TechDecca
    L"Mittenversatz (m)",               // FieldCenter
    L"Stützmikrofon binden",            // SpotMic
    L"Stützmikrofon lösen",             // SpotClear
    L"Text anzeigen",                   // ShowText
    L"L-Modell",                        // ModelL
    L"R-Modell",                        // ModelR
    L"C-Modell",                        // ModelC
    L"Systemtechniker",                 // SysEngineer
    L"Tonmeister",                      // RecEngineer
    L"Tonassistent",                    // AsstEngineer
    L"Modell",                          // FieldModel
    L"Kanal",                           // ChNum
    L"Modell",                          // ChModel
    L"Instrument",                      // ChInst
    L"Kanalliste",                      // ChTitle
    L"PDF",                             // FnPdf
    L"Bemerkung",                       // ChRemark
    L"Spalten",                         // ColVisible
    // layer 03 cabling (phase 1) + project file menu
    L"Verkabelung",                     // CbTitle
    L"Geräteliste",                     // CbDeviceList
    L"+ Analoge Stagebox",              // CbAddAnalog
    L"+ Digitale Stagebox",             // CbAddDigital
    L"+ Audio-Interface",               // CbAddSoundCard
    L"+ ADDA-Wandler",                  // CbAddAdda
    L"Analoge Stagebox",                // CbKindAnalog
    L"Digitale Stagebox",               // CbKindDigital
    L"Audio-Interface",                 // CbKindSoundCard
    L"ADDA-Wandler",                    // CbKindAdda
    L"+ Funkempfänger",                 // CbAddReceiver
    L"Funkempfänger",                   // CbKindReceiver
    L"Name",                            // CbDevName
    L"Modell",                          // CbDevModel
    L"Kanäle",                          // CbChannels
    L"Protokoll",                       // CbProtocol
    L"Ports",                           // CbPorts
    L"Kanäle/Port",                     // CbPerPort
    L"Anschlussseite",                  // CbFaceDir
    L"Oben",                            // CbFaceUp
    L"Rechts",                          // CbFaceRight
    L"Unten",                           // CbFaceDown
    L"Links",                           // CbFaceLeft
    L"%d × %d = %d Kanäle",             // CbCapacityFmt
    L"%s · %.1f kHz: max. %d Kanäle je Port",  // CbProtoLimitFmt
    L"! %d Kanäle gefordert, Kapazität nur %d", // CbCapWarnFmt
    L"Noch keine Geräte",               // CbNoDevice
    L"Gerät löschen",                   // CbDelDevice
    L"06 Verkabelung: Geräte rechts hinzufügen, ziehen zum Verschieben, Entf löscht; Rechtsklick auf zwei Objekte erzeugt das Kabel",  // CbLeftHint
    L"Datei",                           // FnFile
    L"Projekt speichern",               // FnSave
    L"Speichern unter…",                // FnSaveAs
    L"Projekt öffnen",                  // FnOpen
    L"Projekt gespeichert: ",           // FileSaved
    L"Speichern fehlgeschlagen: ",      // FileSaveErr
    L"Öffnen fehlgeschlagen: ",         // FileOpenErr
    // right-click wiring
    L"Zweites Objekt mit Rechtsklick wählen (Rechtsklick ins Leere bricht ab)",  // LinkPickTarget
    L"Ein Objekt kann nicht mit sich selbst verbunden werden",                  // LinkSame
    L"Diese beiden Objekte können nicht verbunden werden",                      // LinkRejected
    L"Verbindung abgebrochen",                                                  // LinkCancel
    L"Verbunden: %s → %s (%d Kanäle)",                                          // LinkCreatedFmt
    L"Kabel entfernt: %s → %s",                                                 // LinkDeletedFmt
    L"Kapazität überschritten: %d Kanäle nötig, %s hat nur %d",             // LinkOverFmt
    L"%d Kanäle · %.1f m",                                                      // CbCableTagFmt
    L"Eingangsüberlast: %s hat %d Kanäle belegt, Kapazität %d",                // CbOverloadFmt
    L"Funk %d Kan.",
    // phase 2: automatic routing + channel assignment
    L"Auto-Routing",                                                            // CbAutoRoute
    L"Auto-Routing: %d Kanäle · %.1f m",                                        // CbAutoDoneFmt
    L"%d Mikrofone ohne freie Kapazität",                                       // CbAutoWarnFmt
    L"Auto-Routing verworfen — die Objekte sind wieder editierbar",             // CbAutoCancelled
    L"keine freie Kapazität",                                                   // CbNoCapacity
    L"Kette",                                                                   // CbLink
    L"Mikrofonkabel",                                                          // CbLen
    L"%s: %d Kanäle / %.1f m",                                                  // CbDevTotalFmt
    L"Gesamt: %d Kanäle / %.1f m",                                              // CbGrandTotalFmt
    L"Auto-Routing ist gesperrt — zuerst „取消自动布线“ drücken",                 // CbAutoLocked
    L"Auto-Routing aufheben",                                                   // CbAutoCancel
    L"Aufheben bestätigen",                                                     // CbAutoCancelConfirm
    L"Nochmals „Aufheben bestätigen“ klicken, um zu entsperren",                // CbAutoCancelHint
    L"Es gibt kein Auto-Routing zum Aufheben",                                  // CbAutoNone
    L"Abtastrate",                                                              // SrTitle
    L"OK",                                                                      // SrOk
    L"Abbrechen",                                                               // SrCancel
    L"Vorher das Auto-Routing aufheben, dann die Abtastrate ändern",             // SrLocked
    L"Raumhöhe",                                                                // FieldCeiling
    L"Geflogen",                                                                // FieldFlown
    L"Hängehöhe",                                                               // FieldFlyHeight
    L"analog",                                                                  // CbAnalog
    L"FLG",                                                                     // CbFlyBadge
    L"Steig %.1f m + Decke %.1f m + Wand %.1f m + Boden %.1f m = %.1f m → %.1f m mit Reserve",  // CbFlyBreakdownFmt
    L"%d geflogene Mikrofone hängen über der Decke; Raumhöhe verwendet",        // CbFlyOverFmt
    L"Ein Kabelmikrofon kann keinen Funkempfänger speisen", L"Ein Funkmikrofon muss zuerst in den Empfänger", L"Protokoll passt nicht: %s ≠ %s",
    // A: die zusammengelegte Stagebox
    L"+ Stagebox", L"Stagebox",
    // 05: Mikrofon + Routen + Löschdialog
    L"Kanäle", L"Zugewiesenes Instrument", L"Ja", L"Nein", L"Routing",
    L"Noch nicht verkabelt", L"Route löschen",
    L"normal", L"über Kapazität", L"Funk", L"Diese Route löschen?",
    // Längen-Pipeline
    L"Benutzerdef.", L"Höhe %.1f + Boden %.1f = %.1f m",
    L"Steig %.1f + Decke %.1f + Wand %.1f + Boden %.1f = %.1f m",
    L"%.1f m → Standard %.1f m", L"%s: %d Kan. / %.1f m (Ist)",
    L"Gesamt: %d Kan. / %.1f m (Ist)", L"%d Kan. · belegt %d",
    // 01 Projektinfo (neue erste Seite)
    L"Projekt",
    // Voreinstellungen (eingebaute Orchesteraufstellungen)
    L"Presets", L"Dreifach-Holzbläser · Europäisch",
    L"Dreifach-Holzbläser · Amerikanisch", L"%s geladen",
    // Bühnengröße geändert: die Aufstellung bleibt mittig
    L"Bühnengröße geändert, die Aufstellung bleibt mittig",
    // Amerikanisches Klavierkonzert
    L"Amerikanisches Klavierkonzert",
    // Strg+A: alle Objekte der aktuellen Ebene ausgewählt
    L"Alle %d ausgewählt",
    // Rückgängig / Wiederholen
    L"Nichts rückgängig zu machen", L"Nichts zu wiederholen", L"Rückgängig gemacht",
    L"Wiederholt", L"Rückgängig fehlgeschlagen",
    // Europäisches Klavierkonzert
    L"Europäisches Klavierkonzert",
    // Rahmenauswahl (Gummiband-Mehrfachauswahl)
    L"Rahmenauswahl: %d",
    // beim Öffnen übersprungene unbekannte Objekte
    L"%d unbekannte Objekte ignoriert",
    // «Zugeordnetes Instrument» einer ganzen Mikrofon-Gruppe
    L"%d Mikrofon(e) gebunden an %s", L"%d Mikrofon(e) gelöst",
    // gemischte Auswahl aus Mikrofonen und Instrumenten: nicht gemeinsam bewegen
    L"Mikrofone und Instrumente können nicht gemeinsam bewegt werden",
    // Kanaltabelle: eine Zeile wurde verschoben
    L"Kanalzeile %d nach %d verschoben",
    // Zusammensetzung eines gebündelten Gruppenkabels
    L"Höhe %.1f + Sammel %.1f + Hauptlauf %.1f = %.1f m",
    // 03 通道表工具条：自动排序按钮 + 状态提示
    L"Auto-Sort", L"Automatische Reihenfolge wiederhergestellt",
    L"Bereits verbunden", L"Umgesteckt: %s → %s (%d Kan.)",
    L"Große Mikrofon-Gruppe", L"%d Mikrofone, bis zu %.1f m auseinander — sie werden als EIN zusammenlaufendes Kabel verdrahtet. Fortfahren?",
    L" (Höhe der geflogenen Geräte)", L"Seite %d von %d",
    L"nächste Seite ab Zeile %d",
    L"HARFE",
    // ---- App-Sperre (4 Ziffern) ----
    L"Code setzen…", L"Code ändern…", L"Sperren", L"Entsperren", L"OK", L"Abbrechen",
    L"Falscher Code, bitte erneut", L"4-stelligen Code festlegen",
    L"Code ändern: aktuellen Code eingeben", L"Code ändern: neuen Code eingeben",
    L"Gesperrt — nur Ansicht", L"Code gesetzt", L"Code geändert", L"Entsperrt",
    L"Gesperrt", L"App-Sperre", L"4 Ziffern eingeben",
    // ---- .smproj-Dateizuordnung ----
    L".smproj-Zuordnung registrieren", L"Dateizuordnung entfernen",
    L".smproj-Zuordnung registriert", L".smproj-Zuordnung entfernt",
    L"Dateizuordnung konnte nicht geschrieben werden",
    // ---- Gruppenbeschriftung ----
    L"%s ×%d",
    L"Instrumententext", L"Mikrofontext",
    L"Gleicher Typ", L"Alle %s (%d)", L"%s ×%d ausgewählt",
    L"Bild", L"Bild exportiert", L"Bild konnte nicht exportiert werden",
    L"Zur Hauptmikrof.", L"Azimut", L"Hauptmik.",
    L"M-Modell", L"S-Modell",
    // ---- Bildauflösung für den PNG-Export ----
    L"Normal", L"Hoch", L"Sehr hoch", L"ca. %d × %d px",
    // ---- Mikrofonkabellänge: empfohlen / tatsächlich ----
    L"Empfohlen", L"Tatsächlich", L"Auf tatsächliche Längen umgestellt",
    L"Auf empfohlene Längen umgestellt",
    // ---- Exportfortschritt ----
    L"Bild wird exportiert… (%d×)", L"PDF wird exportiert…", L"%s · %.1f s",
    // ---- eigene Farbe je Objekt ----
    L"Farbe", L"Standard", L"Eigene Farbe #%06X", L"Farbe auf die Palette zurückgesetzt",
    // ---- «Verbinden»-Block der Verkabelungsseite ----
    L"Verbinden", L"Verbunden mit %s", L"Verbunden",
    // ---- ungespeicherte Änderungen beim Schließen ----
    L"Änderungen speichern?", L"Speichern", L"Nicht speichern", L"Abbrechen",
    // demo2 built-in presets (预设 popup)
    L"Kammerorchester",
    L"Chororchester",
    L"Chor + Klavier",
    L"Streichquartett 1",
    L"Streichquartett 2",
    L"Klaviertrio",
    L"Klavierquartett",
    L"Klaviersolo",
    // ---- SHORT chip labels (top bar / 03 toolbar) — see App::FitChip ------
    L"Datei", L"Bild", L"PDF", L"Einfach", L"Symbole", L"Presets",
    L"Auto-Sort",
    L"Kanal", L"Modell", L"Instr.", L"Bem.", L"Kette", L"Kabel", L"Haupt",
    L"Azimut",
};

const wchar_t* kFr[] = {
    L"Stage Micing",                    // AppName
    L"Scène classique & planification des micros",  // AppSubtitle
    L"Instruments",                     // Layer1
    L"Microphones",                     // Layer2
    L"Liste des canaux",                // Layer3
    L"Câblage",                         // Layer5
    L"bientôt",                         // Soon
    L"BIBLIOTHÈQUE",                    // Library
    L"Instruments",                     // TabInstruments
    L"Micros",                          // TabMics
    L"Rechercher…",                     // SearchHint
    L"Aucun symbole correspondant",     // NoResults
    L"PROJET",                          // SectionProject
    L"Œuvre",                           // FieldPiece
    L"Lieu",                            // FieldVenue
    L"Chef d'orchestre",                // FieldConductor
    L"Notes",                           // FieldNotes
    L"SCÈNE",                           // SectionStage
    L"Largeur",                         // FieldWidth
    L"Profondeur",                      // FieldDepth
    L"Hors scène",                      // FieldOffstage
    L"m",                               // UnitMeter
    L"Salle",                           // PresetHall
    L"Église",                          // PresetChurch
    L"Salon",                           // PresetSalon
    L"SÉLECTION",                       // SectionSelection
    L"Nom",                             // FieldName
    L"Aucune sélection",                // NoSelection
    L"Supprimer la sélection",          // DeleteItem
    L"APPARENCE",                       // SectionAppearance
    L"Lignes des instruments",          // ColorInstrument
    L"Lignes des micros",               // ColorMic
    L"Texte",                           // ColorText
    L"Fond de scène",                   // ColorStage
    L"CALQUES",                         // SectionLayers
    L"Afficher les instruments",        // ShowInstruments
    L"Afficher les micros",             // ShowMics
    L"PUBLIC",                          // Audience
    L"HORS SCÈNE",                      // OffstageArea
    L"Scène",                           // StatusStage
    L"ajuster à la fenêtre",            // StatusFit
    L"Ctrl+glisser = cadre · glisser le vide = vue · glisser = déplacer (instruments verrouillés) · coordonnées à droite",  // StatusHint
    L"Calque %d · %s · bientôt disponible",  // StatusSoon
    L"中",                              // LangZh
    L"EN",                              // LangEn
    L"v0.1 · interface statique",       // VersionTag
    L"CLAVIERS",                        // CatKeys
    L"CORDES",                          // CatStrings
    L"BOIS",                            // CatWoodwind
    L"CUIVRES",                         // CatBrass
    L"PERCUSSION",                      // CatPerc
    L"VOIX & PODIUM",                   // CatVoice
    L"STATIQUE",                        // CatCondenser
    L"DYNAMIQUE / RUBAN",               // CatDynamic
    L"SPÉCIAL",                         // CatSpecial
    L"AMBIANCE / STÉRÉO",               // CatAmbience
    L"%d éléments",                     // MetricCount
    L"Instrument",                      // TypeInstrument
    L"Microphone",                      // TypeMic
    L"Pos",                             // PosLabel
    L"Dir",                             // DirLabel
    L"Longueur du piano",               // FieldPianoLength
    L"Lier comme groupe de 2 micros",   // Bind2
    L"Lier comme groupe de 3 micros",   // Bind3
    L"Délier",                          // UnbindMic
    L"Lier les micros",                 // BindMic
    L"X (m)",                           // XLabel
    L"Y (m)",                           // YLabel
    L"Angle (°)",                       // AngleLabel
    L"Ajouter un instrument",           // AddInstrument
    L"Simple",                          // FnSimplify
    L"Restaurer les icônes",            // FnRestore
    L"Hauteur (m)",                     // FieldMicHeight
    L"Lier le groupe",                  // LinkGroup
    L"Arbre Decca",                     // DeccaTree
    L"Micros principaux",               // MainMic
    L"Omni",                            // PatOmni
    L"Cardioïde",                       // PatCard
    L"Supercardioïde",                  // PatSuper
    L"Figure-8",                        // PatFig8
    L"Espacement (m)",                  // FieldSpacing
    L"Ajouter un microphone",           // AddMic
    L"Cardioïde large",                 // PatWideCard
    L"AB",                              // TechAB
    L"Mixte",                           // TechMixed
    L"Intensité",                       // TechIntensity
    L"Technique",                       // FieldTechnique
    L"Arbre Decca",                     // TechDecca
    L"Décalage central (m)",            // FieldCenter
    L"Lier le micro d'appoint",         // SpotMic
    L"Retirer le micro d'appoint",      // SpotClear
    L"Afficher le texte",               // ShowText
    L"Modèle L",                        // ModelL
    L"Modèle R",                        // ModelR
    L"Modèle C",                        // ModelC
    L"Ingénieur système",               // SysEngineer
    L"Ingénieur du son",                // RecEngineer
    L"Ingénieur assistant",             // AsstEngineer
    L"Modèle",                          // FieldModel
    L"Canal",                           // ChNum
    L"Modèle",                          // ChModel
    L"Instrument",                      // ChInst
    L"Liste des canaux",                // ChTitle
    L"PDF",                             // FnPdf
    L"Remarque",                        // ChRemark
    L"Colonnes",                        // ColVisible
    // layer 03 cabling (phase 1) + project file menu
    L"Câblage",                         // CbTitle
    L"APPAREILS",                       // CbDeviceList
    L"+ Stagebox analogique",           // CbAddAnalog
    L"+ Stagebox numérique",            // CbAddDigital
    L"+ Interface audio",               // CbAddSoundCard
    L"+ Convertisseur ADDA",            // CbAddAdda
    L"Stagebox analogique",             // CbKindAnalog
    L"Stagebox numérique",              // CbKindDigital
    L"Interface audio",                 // CbKindSoundCard
    L"Convertisseur ADDA",              // CbKindAdda
    L"+ Récepteur sans fil",            // CbAddReceiver
    L"Récepteur sans fil",              // CbKindReceiver
    L"Nom",                             // CbDevName
    L"Modèle",                          // CbDevModel
    L"Canaux",                          // CbChannels
    L"Protocole",                       // CbProtocol
    L"Ports",                           // CbPorts
    L"Canaux/port",                     // CbPerPort
    L"Face",                            // CbFaceDir
    L"Haut",                            // CbFaceUp
    L"Droite",                          // CbFaceRight
    L"Bas",                             // CbFaceDown
    L"Gauche",                          // CbFaceLeft
    L"%d × %d = %d canaux",             // CbCapacityFmt
    L"%s · %.1f kHz : %d canaux max par port",  // CbProtoLimitFmt
    L"! %d canaux demandés, capacité %d",       // CbCapWarnFmt
    L"Aucun appareil",                  // CbNoDevice
    L"Supprimer l'appareil",            // CbDelDevice
    L"06 Câblage : ajoutez les appareils à droite, glissez pour déplacer, Suppr pour supprimer ; clic droit sur deux objets crée le câble",  // CbLeftHint
    L"Fichier",                         // FnFile
    L"Enregistrer le projet",           // FnSave
    L"Enregistrer sous…",               // FnSaveAs
    L"Ouvrir un projet",                // FnOpen
    L"Projet enregistré : ",            // FileSaved
    L"Échec de l'enregistrement : ",    // FileSaveErr
    L"Échec de l'ouverture : ",         // FileOpenErr
    // right-click wiring
    L"Clic droit sur le second objet (clic droit dans le vide pour annuler)",  // LinkPickTarget
    L"Un objet ne peut pas être relié à lui-même",                            // LinkSame
    L"Ces deux objets ne peuvent pas être reliés",                            // LinkRejected
    L"Connexion annulée",                                                     // LinkCancel
    L"Relié : %s → %s (%d canaux)",                                           // LinkCreatedFmt
    L"Câble supprimé : %s → %s",                                              // LinkDeletedFmt
    L"Capacité dépassée : %d canaux demandés, %s n'en a que %d",      // LinkOverFmt
    L"%d canaux · %.1f m",                                                    // CbCableTagFmt
    L"Surcharge d'entrée : %s a %d canaux connectés, capacité %d",            // CbOverloadFmt
    L"radio %d can.",
    // phase 2: automatic routing + channel assignment
    L"Routage auto",                                                          // CbAutoRoute
    L"Routage auto : %d canaux · %.1f m",                                     // CbAutoDoneFmt
    L"%d micros sans capacité disponible",                                    // CbAutoWarnFmt
    L"Routage auto annulé — les objets sont de nouveau modifiables",          // CbAutoCancelled
    L"aucune capacité disponible",                                            // CbNoCapacity
    L"Chaîne",                                                                // CbLink
    L"Câble micro",                                                          // CbLen
    L"%s : %d canaux / %.1f m",                                               // CbDevTotalFmt
    L"Total : %d canaux / %.1f m",                                            // CbGrandTotalFmt
    L"Le routage auto est verrouillé — cliquez d'abord sur « 取消自动布线 »",   // CbAutoLocked
    L"Annuler le routage auto",                                               // CbAutoCancel
    L"Confirmer l'annulation",                                                // CbAutoCancelConfirm
    L"Cliquez encore sur « Confirmer l'annulation » pour déverrouiller",       // CbAutoCancelHint
    L"Aucun routage auto à annuler",                                          // CbAutoNone
    L"Fréquence d'échantillonnage",                                            // SrTitle
    L"OK",                                                                     // SrOk
    L"Annuler",                                                                // SrCancel
    L"Annulez d'abord le routage auto avant de changer la fréquence",          // SrLocked
    L"Hauteur sous plafond",                                                    // FieldCeiling
    L"Suspendu",                                                                // FieldFlown
    L"Hauteur de suspension",                                                   // FieldFlyHeight
    L"analogique",                                                              // CbAnalog
    L"SUS",                                                                     // CbFlyBadge
    L"montée %.1f m + plafond %.1f m + mur %.1f m + sol %.1f m = %.1f m → %.1f m avec marge",  // CbFlyBreakdownFmt
    L"%d micros suspendus dépassent le plafond ; hauteur du plafond utilisée",  // CbFlyOverFmt
    L"Un micro filaire ne peut pas alimenter un récepteur", L"Un micro HF doit d'abord passer par le récepteur", L"Protocoles incompatibles : %s ≠ %s",
    // A : le boîtier fusionné
    L"+ Boîtier", L"Boîtier",
    // 05 : micro + routes + confirmation de suppression
    L"Canaux", L"Instrument associé", L"Oui", L"Non", L"Routage",
    L"Pas encore câblé", L"Supprimer la route",
    L"normal", L"surcapacité", L"HF", L"Supprimer cette route ?",
    // pipeline des longueurs
    L"personnalisé", L"hauteur %.1f + sol %.1f = %.1f m",
    L"montée %.1f + plafond %.1f + mur %.1f + sol %.1f = %.1f m",
    L"%.1f m → standard %.1f m", L"%s : %d can. / %.1f m (réel)",
    L"Total : %d can. / %.1f m (réel)", L"%d can. · utilisé %d",
    // 01 infos projet (nouvelle première page)
    L"Projet",
    // préréglages (dispositions d'orchestre intégrées)
    L"Présets", L"Triple bois · Européen", L"Triple bois · Américain",
    L"%s chargé",
    // taille de scène modifiée : la disposition reste centrée
    L"Scène redimensionnée, la disposition reste centrée",
    // concerto pour piano américain
    L"Concerto pour piano américain",
    // Ctrl+A : tous les objets du calque courant sont sélectionnés
    L"%d éléments sélectionnés",
    // annuler / rétablir
    L"Rien à annuler", L"Rien à rétablir", L"Annulé", L"Rétabli",
    L"Échec de l'annulation",
    // concerto pour piano européen
    L"Concerto pour piano européen",
    // sélection rectangulaire (multi-sélection au lasso)
    L"Sélection rectangulaire : %d",
    // objets inconnus ignorés à l'ouverture du projet
    L"%d objets inconnus ignorés",
    // « instrument associé » de tout un groupe de micros
    L"%d micro(s) lié(s) à %s", L"%d micro(s) délié(s)",
    // sélection mixte micros / instruments : jamais déplacée d'un bloc
    L"Les micros et les instruments ne peuvent pas être déplacés ensemble",
    // tableau des canaux : une ligne a été déplacée
    L"Ligne %d déplacée en %d",
    // composition d'un câble de groupe convergé
    L"hauteur %.1f + collecteurs %.1f + trajet %.1f = %.1f m",
    // 03 通道表工具条：自动排序按钮 + 状态提示
    L"Tri auto", L"Ordre automatique rétabli",
    L"Déjà relié", L"Recâblé %s → %s (%d canaux)",
    L"Grand groupe de micros", L"%d micros, jusqu'à %.1f m d'écart — ils seront câblés comme UN seul câble convergent. Continuer ?",
    L" (hauteur du matériel suspendu)", L"Page %d sur %d",
    L"page suivante à partir de la ligne %d",
    L"HARPE",
    // ---- verrou d'application (4 chiffres) ----
    L"Définir un code…", L"Changer le code…", L"Verrouiller", L"Déverrouiller",
    L"OK", L"Annuler", L"Code incorrect, réessayez", L"Code à 4 chiffres",
    L"Changer le code : saisir le code actuel", L"Changer le code : nouveau code",
    L"Verrouillé — lecture seule", L"Code défini", L"Code modifié",
    L"Déverrouillé", L"Verrouillé", L"Verrou d'application", L"Saisir 4 chiffres",
    // ---- association de fichiers .smproj ----
    L"Associer les fichiers .smproj", L"Supprimer l'association",
    L"Association .smproj enregistrée", L"Association .smproj supprimée",
    L"Impossible d'écrire l'association",
    // ---- étiquette de groupe ----
    L"%s ×%d",
    L"Texte des instruments", L"Texte des micros",
    L"Même type", L"Tout sélectionner %s (%d)", L"%s ×%d sélectionnés",
    L"Image", L"Image exportée", L"Échec de l'export de l'image",
    L"Vers micro princ.", L"Azimut", L"Micro princ.",
    L"Modèle M", L"Modèle S",
    // ---- résolution de l'export PNG ----
    L"Normal", L"Haute", L"Très haute", L"env. %d × %d px",
    // ---- longueur de câble micro : recommandée / réelle ----
    L"Recommandée", L"Réelle", L"Passé aux longueurs réelles",
    L"Passé aux longueurs recommandées",
    // ---- progression de l'export ----
    L"Export de l'image… (%d×)", L"Export du PDF…", L"%s · %.1f s",
    // ---- couleur personnalisée par objet ----
    L"Couleur", L"Par défaut", L"Couleur personnalisée #%06X", L"Couleur réinitialisée",
    // ---- bloc « Connexion » de la page câblage ----
    L"Connexion", L"Connecté à %s", L"Connecté",
    // ---- modifications non enregistrées à la fermeture ----
    L"Enregistrer les modifications ?", L"Enregistrer", L"Ne pas enregistrer", L"Annuler",
    // demo2 built-in presets (预设 popup)
    L"Orchestre de chambre",
    L"Orchestre choral",
    L"Chœur + piano",
    L"Quatuor à cordes 1",
    L"Quatuor à cordes 2",
    L"Trio avec piano",
    L"Quatuor avec piano",
    L"Piano solo",
    // ---- SHORT chip labels (top bar / 03 toolbar) — see App::FitChip ------
    L"Fichier", L"Image", L"PDF", L"Simple", L"Icônes", L"Présets",
    L"Tri auto",
    L"Canal", L"Modèle", L"Instr.", L"Rem.", L"Chaîne", L"Câble", L"Micro",
    L"Azimut",
};

static_assert(sizeof(kZh) / sizeof(kZh[0]) == (int)S::Count, "zh table size");
static_assert(sizeof(kEn) / sizeof(kEn[0]) == (int)S::Count, "en table size");
static_assert(sizeof(kDe) / sizeof(kDe[0]) == (int)S::Count, "de table size");
static_assert(sizeof(kFr) / sizeof(kFr[0]) == (int)S::Count, "fr table size");

Lang g_lang = Lang::Zh;

const wchar_t* const* Table(Lang l) {
    switch (l) {
    case Lang::Zh: return kZh;
    case Lang::En: return kEn;
    case Lang::De: return kDe;
    case Lang::Fr: return kFr;
    }
    return kEn;
}

} // namespace

namespace L10n {

void Set(Lang l) { g_lang = l; }
Lang Get() { return g_lang; }
bool IsZh() { return g_lang == Lang::Zh; }

const wchar_t* T(S id) {
    int i = (int)id;
    if (i < 0 || i >= (int)S::Count) return L"";
    return Table(g_lang)[i];
}

const wchar_t* Code(Lang l) {
    switch (l) {
    case Lang::Zh: return L"中";
    case Lang::En: return L"EN";
    case Lang::De: return L"DE";
    case Lang::Fr: return L"FR";
    }
    return L"EN";
}

const wchar_t* Label(Lang l) {
    switch (l) {
    case Lang::Zh: return L"中文";
    case Lang::En: return L"English";
    case Lang::De: return L"Deutsch";
    case Lang::Fr: return L"Français";
    }
    return L"English";
}

} // namespace L10n
