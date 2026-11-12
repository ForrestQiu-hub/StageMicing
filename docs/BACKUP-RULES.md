# 版本与备份规则（本项目固定流程）

## 一旦修好构建，立刻做一次完整快照
快照目录命名：`snapshots\<yyyyMMdd_HHmm>_<里程碑>\`

## 快照内容（每次都要全）
- `src\`
- `assets\`
- `docs\`
- `tools\`
- `CMakeLists.txt`
- `README.md`
- `StageMicing.exe`（根目录那份）

## 生成方式（PowerShell，在项目根目录执行）
```powershell
$root = 'E:\Stage Micing Scene'
$dst  = Join-Path $root ('snapshots\' + (Get-Date -Format 'yyyyMMdd_HHmm') + '_green')
New-Item -ItemType Directory -Path $dst -Force | Out-Null
robocopy "$root\src"      "$dst\src"      /E /NFL /NDL /NJH /NJS /NP | Out-Null
robocopy "$root\assets"   "$dst\assets"   /E /NFL /NDL /NJH /NJS /NP | Out-Null
robocopy "$root\docs"     "$dst\docs"     /E /NFL /NDL /NJH /NJS /NP | Out-Null
robocopy "$root\tools"    "$dst\tools"    /E /NFL /NDL /NJH /NJS /NP | Out-Null
Copy-Item "$root\CMakeLists.txt" "$dst\" -Force
Copy-Item "$root\README.md"      "$dst\" -Force -ErrorAction SilentlyContinue
Copy-Item "$root\StageMicing.exe" "$dst\" -Force
```

## 恢复方式（改坏了才用）
1. 关掉所有 StageMicing 进程；
2. 从最近一个 `snapshots\*_green` 里把 `src\ assets\ docs\ tools\ CMakeLists.txt` 覆盖回项目根目录（**不要**直接删目录，用 robocopy 覆盖）；
3. 重新编译：
   `"D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "E:\Stage Micing Scene\build" --config Release`
4. 编译通过后同步根目录 exe，再继续改。

## 规矩
- **每完成一批功能且构建为 0 错误**，就做一次 `_green` 快照；
- 大改动（删页面、改数据结构、批量改预设）**动手前**先做一次快照，命名用 `_before_<改动名>`；
- 快照只增不删；项目根目录的 `backup_*` 目录保留作历史。
- 出现“改坏”时，不做局部抢救超过 15 分钟——**直接从最近 `_green` 开新版本再改**。
