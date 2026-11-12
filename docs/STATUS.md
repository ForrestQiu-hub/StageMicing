# 工作状态存档（2026-11-09 11:15）

## 构建与产物
- 源码：`src/`（16 个文件，C++20 / Win32 / GDI+，`/utf-8`）
- 构建：`build/`（CMake + MSVC），命令
  `"D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "E:\Stage Micing Scene\build" --config Release`
- 可执行：`StageMicing.exe`（根目录，11:11 版）＋ `build\Release\StageMicing.exe`
- **备份**：`backup_20261109_1115\`（src / assets / docs / CMakeLists.txt / README.md / exe）

## 当前功能状态
- 页签（6 页，重构已入代码，验收中）：01 项目信息 / 02 乐器摆位 / 03 话筒摆位 / 04 通道表 / 05 拓扑图 / 06 布线
- 舞台：乐器/话筒图标（点话筒 0.3×、主话筒程序绘制 1.6×、无斜线）、舞台内网格已修「溢出到素材栏/顶栏」
- 布线：右键连线（排异矩阵）、无线虚线无长度、删除需确认、线缆标签避让、先竖后横直连（不绕路）、后半台允许 45°
- 长度：05 为实长；03/04/PDF 为标准档向上取整（5/8/10/12/15/18/20/25/30）；可人工输入并标「自定义」；吊装四段（上升/吊顶/沿墙/地面）
- 文件：`.smproj` 保存/打开；旧文件兼容（数字箱→接口箱，ADDA 丢弃）
- 语言：中/英/德/法
- PDF：舞台图 01、通道表 03 均可导出

## 待办 / 进行中
1. 重构代理（5ba37083）正在做 GUI 自查：六页签与 `--layer` 映射、属性栏「标签/框不重叠 + 免滚动」审计、P1/03/2.5× 三张截图 —— **未回报**
2. 右栏滚动重影：本轮已修（控件创建即隐藏、ShowWindow 后立即 Layout、控件窗口区域按面板裁剪、滚动同步重绘），待最终确认
3. 后续可选：线缆清单 CSV/PDF 导出、04 拓扑图拥挤时滚动、PDF 字号统一

## 常用测试钩子
`--layer N`、`--scroll px`、`--zoom f`、`--select id|device id`、`--link aKind aId bKind bId`、`--click/--rclick/--move/--up/--key`、`--dump file`、`--fuzz`、`--linear`、`--nonudge`、`--tech 0..3`、`--turn id deg`
截图：`tools\shot.ps1 -Out <png>`（偶发返回旧帧，需比对哈希）
审计：`tools\_inspcheck.ps1`、`tools\_scroll_test.ps1`

## 2026-11-11 20:10 本轮（四项）
1. **调色板重排（max-min 路径）**：`tools/palette_design.py` 在贪心取色之后加了 `maxmin_order()`
   —— 同 40 色，按哈密顿路径把相邻槽位拉到最远（互补色交错）。相邻槽位最小 dE：
   **icon 31.9 / ink 24.2 / sep 24.2**（旧色轮 5.5；目标 ≥ 12）。`--dump-colors` 新增逐槽
   `iconDE inkDE sepDE` 表 + `min consecutive dE` 行，并在 < 12 时以 exit 2 失败。
   报告 `tools/_palette_report.txt`、色板图 `tools/_palette_order_sheet.png`。
2. **单个对象自定义颜色**：`Item::colorOverride`（0xRRGGBB，0 = 调色板），序列化为 item 记录尾部
   `color 0xRRGGBB`（旧文件无此对 → 0，向后兼容）。属性栏（乐器/声部）新增「颜色」行：色块 + `#RRGGBB`
   +「恢复默认」，点击走 `PickColor`（项目信息页同一个系统取色器），改动 undoable 且受应用锁限制。
   覆盖色同时作用于该对象图标与**绑定到它的**话筒／线缆／标签墨色（`MicBoundInk` → 
   `InstColorOverrideFor`）。
3. **布线页点击连接**：选中话筒后右栏新增「连接」区块（复用右栏），列出声卡／接口箱／无线接收器，
   点一行即连接（`ConnectMicToDevice` → `CreateCable`，改接替换旧线、单步 undo、编组按整组一条线），
   状态栏「已连接到 <设备>」，当前连接行标「已连接」。
4. **通道表「路由」单元格**：点该格（原属拖拽手柄，现只剩「通道」列）弹出小面板，列同一批设备，
   选中即连接（与 3 同一条代码路径）；面板限制在窗口内、最后绘制，Esc／点击外部关闭；该格不打开文本编辑器。

新增测试钩子：`--dump` 增加 `instcolor`、`itemcolor`、`connsec/connrow`、`chrouteitem/chrouterow`。
证据：`tools/_palette_order_sheet.png`、`_final_02.png`、`_final_wiring.png`、`_i2_shot_inspector.png`、
`_i2_02_override.png`、`_i2_wiring_override.png`、`_i2_picker_dialog.png`、`_i3_shot_connect.png`、
`_i4_shot_popup.png`、`_i4_shot_linked.png`、`_i4_table.pdf`。

## 2026-11-12 01:07 本轮（内置预设 12 个 + 独唱歌手不合并）
1. **独唱歌手（`solo_singer`）不参与同类合并**：`App::BuildTypeHulls`（`src/StageView.cpp`，≥3 过滤之后）
   与 `App::SameTypeGroups`（`src/App.cpp`）都跳过它 —— 舞台不再出现「独唱歌手 ×N」标题，右侧
   「全选 独唱歌手」也不再出现；`count` 表仍保留计数，所以独唱站进大编制里不会打断后者的合并标题。
   `choir_singer` 保持合并（合唱是一个声部）。验证 fixture `tmp_tests/fixture_solo4.smproj`
   （4 独唱 + 4 小提琴 + 3 合唱）：dump 无 solo hull、4 条独立「独唱歌手」label、小提琴 ×4 / 合唱 ×3 仍在，
   `sametype` 无 solo 行。截图 `tools/_i1_solo4.png`。
2. **八个 demo2 工程做成内置预设**：`demo\demo2 *.smproj` 逐个 `--open … --dump-layout`，行逐字粘进
   `src/Presets.cpp` 新表 kAncient 37 / kChoirOrch 154 / kChoirPiano 66 / kQuartet1 4 / kQuartet2 4 /
   kPianoTrio 3 / kPianoQuartet 4 / kPianoSolo 1，`kPresets[]` 共 12 项；`PresetRow` 新增可选 `label`
   承载用户自己起的人名（Soprano/Alto/Tenor/Bass、圆号 2、第一小提琴 4），其余行仍是四字段原样。
   `src/L10n.h` / `src/L10n.cpp` 末尾追加 8 个 id × 4 语言（不动任何已有索引）。
3. **预设弹窗双列 + 键盘**：>6 行改两列（12 → 2×6，dpi 1.25 下面板 962×547，1400×900 窗口内余量
   219 / 176）；↑↓ 列内移动、←→ 换列、回车载入、Esc 关闭。旧四行仍单列。
4. **构建环境修复**：源码移到 `dev\` 后 POST_BUILD 的 assets 复制指向 `${CMAKE_SOURCE_DIR}/../assets`
   （根目录 `assets`），build 恢复 0 错误；根 `StageMicing.exe` 仍由构建后手动复制。
验证：12 个 preset `--dump` = 86/85/87/86/37/154/66/4/4/3/4/1 件、mic 0、stage 18×12；`--dump-layout`
与源 dump 逐行一致（8 个 demo2 全部字节相同；三管美式两把小提琴 y 只差 2 mm、三位小数后并列，
重排后顺序互换 —— 原有数据精度问题，未改动用户数据）。demo2 原舞台尺寸：古乐团 14×12、合唱乐团 18×16、
合唱+钢琴 18×12、四重奏 1/2 9×7、钢琴三重奏/四重奏 14×10、钢琴独奏 14×12（只有合唱乐团比 18×12 深）。
证据：`tools/_i2_preset12.png`、`tools/_i2_preset12_1400x900.png`、`tools/_i1_solo4.png`。

## 2026-11-12 01:20 本轮（预设恢复各自原始舞台尺寸）
1. **每个内置预设恢复它自己的原始舞台尺寸**（以前一律 18×12）：`src/Presets.cpp` 的 `PresetDef`
   增加 `stageW` / `stageD`，`kPresets[12]` 逐项填源工程 `.smproj` 的 `stage W D` 一行；
   `LoadPreset()` 先写 `pr.stage.w/d` 再按归一化行摆位（`x = nx*stage.w`），所以位置/角度分毫不动。
   尺寸（读自 `demo\*.smproj` 的 `stage` 行）：三管欧式/美式/欧式钢协/美式钢协 18×12（不变）、
   古乐团 14×12、合唱乐团 18×16、合唱+钢琴 18×12、弦乐四重奏 1/2 9×7、钢琴三重奏/四重奏 14×10、
   钢琴独奏 14×12。`src/Presets.h` 注释同步。
2. **视图重新适配新舞台**：`App::LoadPresetAt` 载入后把 `ui_.panX/panY/zoom` 归零（`UpdateStageXf`
   本来每次都按 `pr_.stage.w/d` 算 fit，但旧的平移/缩放会让小舞台跑出画面）；项目信息页宽/深字段
   仍读 `pr.stage`，自动显示新尺寸。
验证：12 个 `--preset N --dump` = 86/85/87/86/37/154/66/4/4/3/4/1 件、mic 0，stage 分别为
18×12 / 18×12 / 18×12 / 18×12 / 14×12 / 18×16 / 18×12 / 9×7 / 9×7 / 14×10 / 14×10 / 14×12；
`--dump-layout` 与源工程 dump 逐行一致（含 5 合唱乐团；美式两把小提琴同 y 的顺序互换仍是原有精度问题）。
fit 数字：9×7 → ppm 87.51（18×12 为 43.76，宽度减半即放大一倍），18×16 → oy 227.2、底边 927 < 画布 1087。
证据：`tools/_stage_preset/p7_stage_9x7.png`、`p7_info_9x7.png`、`p5_stage_18x16.png`、`p5_info_18x16.png`。
