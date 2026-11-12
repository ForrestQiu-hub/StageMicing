# App.cpp 恢复任务说明

## 目标
`C:\Users\Lenovo\AppData\Local\...` 之外，本项目：`E:\Stage Micing Scene`。
`src/App.cpp` 在一次错误补丁中被截断：`MouseDown` 起往后约 1000 行丢失，
导致源码无法编译。**可运行的 exe 仍在**（根目录 `StageMicing.exe`、`build\Release\StageMicing.exe`，13:00 构建）。

任务：重建 `src/App.cpp` 中缺失的函数，使项目重新可编译、可运行，行为与现有 exe 一致。

## 已确认仍然完好的文件
- `src/App.h`（全部声明）、`src/Model.h/.cpp`、`src/Catalog.*`、`src/StageView.cpp`
  （含 PaintStage、PaintChannelTable、BuildChannelRows 之外的辅助、StageHitTest、ItemRects、
  RotateHandle*、PointToWorld、MutedColor 等）、`src/LeftPanel.cpp`、`src/RightPanel.cpp`、
  `src/TopBar.cpp`、`src/StatusBar.cpp`、`src/Draw.*`、`src/L10n.*`、`src/Pdf.*`、`src/Assets.*`、
  `src/Theme.h`、`CMakeLists.txt`、`tools/make_assets.py`、`assets/`。

## App.cpp 仍需保留的已存在实现
`Run, OnCreate, CreateControls, WndProc, StaticProc, Layout, Paint, PaintOverlay,
SyncEdits, SyncXYEdits, SyncAngleEdit, ControlText, Font, RebuildFonts, InvalidateAll,
UpdateStageXf, UpdateCueBanners, PickColor, SetLanguage, CopySelected, PasteClipboard,
MarkMainMics, UnbindSelected, MouseDown(截断，需修好)`

## 需要重建（签名见 App.h）
```
AddItem(int catIndex), DeleteSelected(), MouseMove(int,int), MouseUp(int,int),
MouseWheel(int delta,int x,int y), OnKeyDown(int vk), ParseEdit(int id),
DragTo(int,int), ClearHover(), BindSelected(int kind), SplitTechnique(),
MainMicSRA() const, SpotTypeList() const, SelectedInstType() const, SpotMicInSel(),
OpenChannelCell(int,int), CloseChannelCell(), ExportPdf(), ExportPdfTo(path),
ApplyDpi?（若声明存在）, D?（若声明存在）, PaintChannelTable/BuildChannelRows（这两个在 StageView.cpp，确认不重复定义）
```
注意：`BuildChannelRows` 和 `PaintChannelTable` 已在 `StageView.cpp` 定义；App.cpp 里不要重复。

## 关键行为（从现有界面与其它文件可推断）
- 图层：0 乐器、1 话筒、3 通道表（4 页签）；`--layer N`、`--en/--zh`、`--pdf 路径` 命令行参数在 Run 中解析（已存在）。
- 舞台点击：`StageHitTest(x,y,locked)` 返回 id；乐器在话筒层可选中但不可拖（locked），反之亦然。
  拖动只在 `!locked` 时开始；`DragTo` 用 `PointToWorld`，0.05 m 网格，±200 m 夹取；组内话筒整体移动（`pr_.groupOf`）。
- 旋转手柄：`RotateHandleHit` → `rotating_`，`RotateTo` 按极角设置角度（`NormAngle`）。
- Ctrl+点击：多选切换（`pr_.selIds`）。
- 键盘：Delete/Backspace 删除；Ctrl+C/V 复制粘贴（偏移 0.4 m）；Ctrl+滚轮缩放（0.25–5，围绕光标）。
- 滚轮：先看是否在点话筒列表（`L_.spotList`）内 → 滚动列表；否则在右侧面板 → 滚动 `ui_.rightScroll`；再否则画布缩放或素材库滚动。
- `ParseEdit(id)`：编辑框实时解析，ID 见 App.h 枚举（项目字段、舞台尺寸、选中对象 X/Y/角度/长度/话筒高度/型号/主话筒 L·R·C、主话筒间距/中置、通道表格子等）。
- PDF：`ExportPdfTo(path)` 生成单页 PDF——页头项目信息、中间画布（2× 位图）、页脚备注或通道表（三线表 + 文字图像），`Pdf.h` 提供 PdfDoc/GlyphText/Polyline 等 API。
- 通道表：`BuildChannelRows()`（StageView.cpp）返回 `ChRow{ch,itemId,model,inst}`；表格点击 `OpenChannelCell` 复用 `eMicModel_` 编辑型号。

## 验收
1. `cmake --build build --config Release` 通过；
2. 运行 `build\Release\StageMicing.exe --layer 3 --pdf <tmp>.pdf` 生成合法 PDF（`%PDF`/`%%EOF`）；
3. 截图核对（`tools\shot.ps1`）与旧 exe 界面一致。

构建：
```
"C:\...\CMake\bin\cmake.exe" --build "E:\Stage Micing Scene\build" --config Release
```
（完整路径：D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe）
