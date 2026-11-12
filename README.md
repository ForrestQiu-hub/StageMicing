# Stage Micing · 舞台摆位

古典音乐会的「舞台摆位 + 话筒布局」设计工具（Windows / C++20 / Win32 / GDI+）。

Stage Micing is a Windows tool for planning classical concert stage layouts and
microphone placement: lay out the orchestra to scale, place microphones, wire them to
stage boxes and the sound card, then export a stage plot (PNG) and a channel list (PDF).

## 下载 / Download

- 安装包（用户）：[`dist/StageMicing_Setup.exe`](dist/StageMicing_Setup.exe) —— 双击安装，免管理员权限。
- 开发版整包（含源码、构建树、工具、快照）：见 **Releases**。
- 四语说明书：`说明书\`（中 / English / Deutsch / Français）。

## 功能概览 / Features

- **五个页面**：项目信息 / 乐器摆位 / 话筒摆位 / 通道表 / 布线。
- **乐器 30 种**，按真实米制摆放，可改名、角度、位置，可单独指定颜色。
- **话筒**：主话筒（AB / 强度差 / 混合差 / Decca 树，实时 SRA）、点话筒、领夹、
  强度差单对象（XY / MS / Blumlein）、混合差（ORTF / NOS / DIN = 两支普通话筒绑定）。
- **38 个绑定目标**：具体乐器 + 环绕 / 天空 / 侧展 / 环境 / 弦乐组 / 木管组 / 铜管组 / 打击乐组。
- **通道表**：8 列（通道 / 型号 / 乐器 / 备注 / 路由 / 话筒线长度 / 距主话筒 / 幅角），
  支持排序、翻页、行内编辑、拖动重排、路由列直接选设备，表尾附布线汇总，可导出 PDF。
- **布线**：声卡 / 接口箱 / 无线接收器；实长与容量实时计算；编组话筒先汇聚再走主线；
  吊装线走向目标设备一侧的墙；线缆标签显示「通道号 + 话筒名」。
- **12 套内置预设**，来自真实工程，载入后恢复各自的原始舞台尺寸。
- **导出**：PDF（通道表）；PNG（舞台 / 布线，2×/4×/8×，图幅含全部内容，左上角印项目信息）。
- 中 / 英 / 德 / 法四语界面，四位应用锁（锁定只读），关闭前提示保存。

## 从源码编译 / Build from source

```
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

产物：`build\Release\StageMicing.exe`。运行时 `assets\` 必须与 exe 同目录。

## 目录 / Layout

```
src\            源码（UTF-8 无 BOM，勿用 PowerShell 文本命令改写）
assets\         图标素材（运行时必需）
demo\           示例工程（12 套预设对应的原始工程 + test.smproj）
docs\           设计文档（STATUS / CABLING / BACKUP-RULES）
tools\          辅助脚本（素材生成、测量、打包）
dist\           发布物（安装包）
说明书\          四语使用说明书
安装说明\        四语安装说明
功能总览.txt     全部功能一览
简介.txt         产品简介
```

## 版本 / Version

v1.0 —— 12 套预设、距主话筒与幅角列、全局标签排版、40 色调色板、应用锁、
PNG 抬头与三档倍率、关闭前保存提示。
