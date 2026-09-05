# 2026-07-14 新会话交接：DICOM 壳与自动 Path 纠偏

> 新会话必须先读本文件，再决定继续当前 child、修订任务，或切换到父级/最终验收任务。
> 不得只根据自动测试数字继续执行。

## 当前状态

- 分支：`feat/google-earth-shell-a`。
- 当前任务：`07-12-shell-a-centerline-b`，状态必须保持 `in_progress`。
- `D:\XQ` 只读。
- 工作区约有 185 个共享未提交变更；不得清理、回退或覆盖未知改动。
- 禁止 Claude；不要提交、推送或归档任务。

## Trellis 续接结论

- 当前 child **不重写**：`07-12-shell-a-centerline-b` 的合法范围就是
  `SegmentationMask -> Path + VesselProfile`，继续保持 `in_progress`，不得把
  DICOM 自动分割、通用 ROI、完整中心线树或网格偷偷塞进本任务。
- 完整的“真实 DICOM -> 自动处理/Mask -> 自动 Path，替代手工 Path”属于
  `07-12-shell-reuse-v2-full-delivery`；其中 `07-12-vascular-foundation-auto-segmentation`
  负责自动 Mask，后续 centerline-tree 必须复用本 child 已实现的 thinning/distance
  kernel，不得重写。
- `07-12-shell-a-v1-alignment` 只按 A1-A15 关闭壳档 A v1；其原文明确不以完整自动分割为
  前置条件。不得再混淆“壳档 A v1 完成”和“完整自动处理壳 v2 完成”。
- 新会话运行 `trellis continue` 会接回当前 child。先读本文件；当前实现已经完成自动验证，
  下一阶段应先按本 child 范围复核/check，而不是重复 DICOM 基础验收或扩写自动分割代码。

## 用户不可再被误解的目标

用户要的是 `D:\XQ` 规划的 **DICOM 医学影像处理壳子**，并要求尽可能复用前人工作，
用自动流程取代能够被取代的旧手工代码。产品起点是 DICOM，不是现成
`SegmentationMask`，也不是预制 `.xqproj`。

正确产品方向：

```text
run_xq.bat
-> 打开真实 DICOM Series
-> GDCM/ITK 读成 patient LPS/mm Volume
-> ITK 自动影像处理/分割得到 SegmentationMask
-> 自动 Centerline B 得到 Path + VesselProfile
-> PathValidate
-> 保存并重开 .xqproj
```

`.xqproj` 只用于保存/恢复工作现场；不得把它描述为首次验收的必需输入。

旧的 `Pick Control Points -> Generate Path -> 手工轮廓` 是兼容/旁路能力。用户已经明确要求
自动化前人工作能够替代的旧流程应被替代，因此不得再把手工 Path 作为目标主流程或下一步
验收路线。

## 已完成并自动验证的 Centerline B

- 真实 ITKThickness3D `v5.3.0`，commit
  `36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb`。
- ITK 5.4 `SignedMaurerDistanceMapImageFilter` 使用物理 spacing 计算半径。
- 26 邻域图、mm 毛刺剪枝、确定性单主路径。
- 输出 `XQPath + VesselProfile`，原子 Scene/Asset 发布，带 lineage、stale、undo/redo。
- Modules 页已有 `SegmentationMask` 选择器和 `Build Centerline B`，成功后可运行
  `PathValidate`。
- Flow ON/OFF 自动验证通过：ON 105/105，OFF 97/97；聚焦 16/16。
- 自动日志：`.trellis/workspace/ocean/shell-a-canonical-logs/20260714-151527/`。

这些证据只证明 **已有 Mask -> 自动单 Path**，不证明 DICOM 到 Mask、完整自动壳、中心线树
或临床质量。

## D:\XQ 中已有但尚未完整产品化的自动 Path 前人工作

1. `D:\XQ\research\ITKMinimalPathExtraction-35dd8e83b7df2059876e6835a5741eb3d45973bf`
   - commit `35dd8e83b7df2059876e6835a5741eb3d45973bf`，Apache-2.0。
   - 提供 ITK Fast Marching/最小代价路径 kernel，需要 speed image 和明确端点。
2. `D:\XQ\research\ITKTubeTK-1.3.5\examples\Applications\SegmentTubeUsingMinimalPath`
   - 是 TubeTK 的 MinimalPath 管状结构示例；不能把 TubeTK 描述为完整最终中心线方案。
3. `XQ/third_party/itk_minimal_path`
   - 已 vendored，但当前 CMake 只把它编进
     `xq_vascular_roi_development_analyze`；生产 GUI/adapter 尚未消费。
4. `ITKThickness3D + SignedMaurer`
   - 已进入生产 Centerline B，是当前真正产品化的自动 Mask->Path 路线。
5. `vtkvmtkPolyDataCenterlines`
   - 只作为可选质量升级；VTK 9.3 风险和许可/ABI 边界使其不能成为默认或 SuperBuild。

MinimalPath 可用于自动分割中的断支重连，但不能单独等同于“从任意原始 DICOM 无条件生成
最终 Path”。XQ 仍需提供 speed image、端点/拓扑策略、契约转换和失败语义。不得重写
MinimalPath optimizer 或 ITK thinning kernel。

## 真实机器已经通过的部分

正确启动入口：

```text
C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat
```

真实 DICOM 样例目录：

```text
D:\XQ\data\dicom\tcia_lidc_idri_0957_ct\series
```

该目录有 `1-01.dcm` 到 `1-65.dcm`。用户在 2026-07-14 实机确认以下全部可用：

1. `run_xq.bat` 启动。
2. “打开 DICOM 序列”选择整个 `series` 目录。
3. 发现并选择 Series。
4. DICOM 在 MPR/3D 中显示并可浏览。
5. 保存为 `.xqproj`。
6. 关闭、重新通过 `run_xq.bat` 启动并打开 `.xqproj`，影像恢复。

这只关闭了真实启动、DICOM IO、基本显示和持久化的实机检查。自动
`DICOM -> Mask -> Centerline B -> PathValidate` 仍未实机通过。

## 已确认的问题与错误路线

1. 曾把预制 `.xqproj`/Mask 当作首次验收前提；错误。首次输入必须是 DICOM。
2. 曾因 `D:\XQ` 没有兼容 `.xqproj` 而说无法验收；错误。DICOM 基础壳已经实机验收。
3. 曾直接让用户双击 `build_shell_a_on\xq_app.exe`；错误。运行时缺
   `vtkRenderingLOD-9.3.dll` 等 DLL。必须用 `XQ\run_xq.bat` 设置锁定运行时 PATH。
4. `build_gui_wt.bat` 是构建入口，`verify_canonical_shell_a.bat` 是自动验证入口，
   二者都不是日常 GUI 启动入口。
5. 曾把固定肝脏/门静脉 ROI 自动分割页面描述成通用 DICOM 壳；错误。它只是特定病例/策略，
   不能代表产品身份。
6. 曾把半自动手工 Path/Contour 工作流作为下一步验收；错误。用户要求复用自动 Path 前人
   工作取代可替代的旧手工路线。
7. 曾把 Centerline B 的自动测试全绿等同完整壳完成；错误。它只证明 Mask->Path child。
8. 曾忽略 `ITKMinimalPathExtraction` 已下载、锁版本、vendored 并有开发证据；错误。但也不得
   反向夸大：它尚未进入生产 GUI target。
9. 回答时混淆了“通用 DICOM 壳”“血管样例”“肝/门静脉算法”和“Centerline B child”。新会话
   必须先说明讨论的是产品壳、输入算法还是某个 child。

## PowerShell 中文乱码规则

Windows PowerShell 5.1 对无 BOM UTF-8 文件的默认 `Get-Content` 解码可能按 ANSI/OEM 处理。
本会话第一次读取中文 `prd.md` 时因此出现乱码。以后必须：

```powershell
Get-Content -Raw -Encoding UTF8 <path>
```

如还要把中文传给/接收自 native process，可显式设置：

```powershell
$utf8 = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8
```

注意：设置 console/output encoding 不能修复已经被 `Get-Content` 错误解码的文本。看到乱码后
不得据此修改文件，必须用 `-Encoding UTF8` 重新读取。文件编辑继续使用 `apply_patch`，不得用
PowerShell `Set-Content` 重写并引入 BOM/换行 churn。

本机另有一个会让 Trellis 命令静默失败的 PATH 坑：裸 `python` 当前解析到
`C:\Users\OCEAN\AppData\Local\Microsoft\WindowsApps\python.exe`（商店 stub，版本
`0.0.0.0`），运行 `get_context.py` 会直接失败且可能没有错误文本。Trellis Python 脚本必须用：

```powershell
& 'C:\software\anaconda\python.exe' -X utf8 .\.trellis\scripts\get_context.py
& 'C:\software\anaconda\python.exe' -X utf8 .\.trellis\scripts\get_context.py --mode phase
```

这与中文文件解码是两个独立问题：`-X utf8` 负责 Python I/O，`Get-Content -Encoding UTF8`
负责 PowerShell 文件读取，两者不能互相替代。

## 当前真正缺口

- Centerline B 的真实桌面输入仍需一个由正常产品流程生成的兼容 `SegmentationMask`。
- 通用 DICOM 到自动处理结果的产品路线尚未闭合；现有自动分割 UI 是特定肝/门静脉 ROI 路线。
- locked MinimalPath 当前仍是 development-only target，未产品化为通用自动 Path/重连能力。
- 旧手工 Path UI 仍存在；如果“取代”要求从产品主入口移除/降级，当前任务尚未完成该要求。
- Centerline B 的真实 ITK GUI 点击、Path/Profile 生成和 PathValidate 尚未由用户实机确认。

## 新会话的第一步

运行 `trellis continue` 后，先读本文件并向用户用三句话复述：

1. 已通过：真实 DICOM 导入、MPR/3D、保存和重开。
2. 未通过：自动 DICOM->Mask->Path 的产品闭环和旧手工 Path 替代。
3. 任务边界：当前 `shell-a-centerline-b` 不重写，只拥有 Mask->Path；完整自动壳替代属于
   `shell-reuse-v2-full-delivery` 及其 vascular-foundation 依赖。

未经这一步，不要继续写代码，不要再次让用户手工点 Path，也不要重复基础 DICOM 验收。

## Memory 位置

`trellis mem` 只读索引 Codex 会话 JSONL，没有人工“写入 memory”的子命令。本轮可执行交接已
落在以下长期文件中，新会话应以这些文件而不是聊天摘要为准：

- 本文件：当前任务状态、误区、已验证事实、缺口和续接动作。
- `prd.md` / `evidence.md`：当前 child 的范围与证据边界。
- `.trellis/spec/XQ/core/acceptance.md`：DICOM-first 与自动 Path 的验收契约。
- `.trellis/spec/XQ/core/build-and-test.md`：PowerShell UTF-8、正确 launcher 和 Python PATH 坑。
- `.trellis/spec/XQ/architecture/reference-sources.md`：前人实现复用边界。
- `.trellis/workspace/ocean/journal-1.md` session 12：本次会话记录，状态为 in_progress。
