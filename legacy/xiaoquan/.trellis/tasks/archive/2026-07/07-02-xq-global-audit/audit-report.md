# XQ 全局审计报告(2026-07-02)

> 审计方式:10 维度并行 agent 扫描(568k tokens、304 次工具调用)+ 主审对每条发现逐一读源码验证。
> 「CONFIRMED」= 主审已核实证据行;「证伪」= 主审已找到反证,执行 agent 不得据此改代码。
> 行号以 2026-07-02 工作树为准;执行前 agent 须先 grep 定位符号(代码可能已动),不要盲改行号。

---

## P0-1 依赖逆流:adapter → services(CONFIRMED)

spec `.trellis/spec/XQ/architecture/index.md`:依赖**只能自上而下** `app -> services -> io/adapters -> core`。
三个 adapter 反向依赖 services:

| adapter | CMake | include |
|---|---|---|
| xq_adapter_onnx | `XQ/CMakeLists.txt:554-558` `PUBLIC xq_services` | `src/adapters/onnx/OnnxBackend.h:5-6` → `services/ai/AiService.h`、`services/segmentation/XQAiSegmentationRequest.h` |
| xq_adapter_tetgen | `XQ/CMakeLists.txt:593-596` `PUBLIC xq_core xq_services` | `src/adapters/tetgen/TetGenTetMesher.h:4` → `services/meshing/ITetMesher.h` |
| xq_adapter_mmg | `XQ/CMakeLists.txt:633-636` `PUBLIC xq_core xq_services xq_adapter_tetgen` | `src/adapters/mmg/MmgVolumeRemesher.h:4`、`TetGenThenMmg.h` → `services/meshing/ITetMesher.h` |

**根因**:port 接口(`ITetMesher`、AI 三 backend 抽象)定义在 services 层,adapter 实现它们只能反向 include,
并因此 PUBLIC 传递链接整个 xq_services。

**已核实可下沉(修复方案 A 的前提)**:
- `services/meshing/ITetMesher.h` 仅依赖 `core/XQTetVolumeMeshHandle.h`、`core/source/IGeometrySource.h` + std。
- `services/segmentation/XQAiSegmentationRequest.h` 仅依赖 `core/XQImageVolume.h`、`core/XQMemoryImageBufferHandle.h`、`core/XQSegmentationMask.h` + std。
- `services/ai/AiService.h` 内的 `XQAiIdentifyRequest`/`XQAiIdentifyBackend`/`XQSurrogateRequest`/`XQSurrogateBackend`(约 22-53 行)仅依赖 core 类型 + 前向声明。

三个接口全部只吃 core 类型,可无痛下沉到 core;箭头随即全部朝下,adapter 只需链 xq_core。执行细节见 `EXECUTE-P0.md`。

**不算问题**:`xq_adapter_mmg → xq_adapter_tetgen`(CMakeLists.txt:634)是文档化两段式设计
(TetGen 填充→MMG 优化,见 memory `mmg-cannot-tetrahedralize-from-surface`),保留。

---

## P0-2 符号链接/junction 逃逸(CWE-59,CONFIRMED,影响面已修正)

`isConfinedRelativePath` 是**纯词法**检查(拒空/绝对/root/`..`),从不解析符号链接,且三处各复制一份:
`BlobStore.cpp:119-136`、`GeometryResourceManager.cpp:80-95`、`XQProjectReader.cpp:979-993`。

- **mmap 路径**:`MmapBlob.cpp:68` `CreateFileW(..., FILE_ATTRIBUTE_NORMAL, ...)` 无 `FILE_FLAG_OPEN_REPARSE_POINT`,
  NTFS junction/symlink 被静默跟随。junction 创建**不需管理员权限**(`mklink /J`),攻击门槛低。
- **ifstream 路径**:`BlobStore.cpp:310` `fs::path(rootDir_) / ref.relPath` 后直接 `std::ifstream` 打开,同样跟随。

**影响面修正(主审复核,不同于安全 agent 的原始描述)**:
- 「读超大文件放大 DoS」在 `readVerified` 上**基本不成立**:该函数 322-328 行有尺寸门
  (`fileSize < ref.byteCount → TruncatedBlob`、`> ref.byteCount → ByteCountMismatch`),超大目标先被挡。
- 「内容任意替换」受**双重约束**:mmap 路径 `initBlob:70` 有 `mapped.size != totalBytes()` 尺寸门,
  且 `verifyBlob`(FullVerify 比 `BufferRef.sha256`、SegmentedMerkle 比 Merkle 根)做内容校验。目标须尺寸精确匹配、且无锚或锚匹配才可能替换成功。
- **真正残留、值得修的**:junction 指向**设备路径 / 命名管道 / 慢速 UNC** 时,`CreateFileW`/`ifstream::open` 跟随即可导致**跟随外部目标 / 挂起**(在任何尺寸/哈希校验之前发生),且违反「资产必须封闭在 .assets 内」的安全边界。

**结论**:漏洞真实但严重度 High→**Medium**。

**本机实测校正(2026-07-02,MSVC C++17 junction 探针)** —— 修复方案据此定型:
- `mklink /J` 无管理员权限即可建目录 junction(退出码 0)。攻击门槛确认极低。
- **`fs::is_symlink` 对 junction 一律返回 false**(连 junction 目录本身 `is_symlink_junction_dir=0`)。
  → 原先"在 `readVerified` 加 `is_symlink` 拒绝"的设想**实测无效,已作废**。
- `fs::ifstream` 静默跟随 junction 读到外部内容(探针 `ifstream_read=SECRET_FROM_OUTSIDE`)。
- **`fs::weakly_canonical` 穿透 junction 解析到外部真实路径**,与 root 的 canonical 做元素前缀比较,
  逃逸判 `inside=0`、正常 blob 判 `inside=1`。→ **这是唯一可靠手段**,修复采用之(`isPathWithinRoot`)。

**新发现缺口(recon 核实)**:GRM 的 **voxel 链路 `acquireVoxelSource`(GeometryResourceManager.cpp:188)
只做 nullptr/empty 检查,连词法 confinement 都没有**——`../` / 绝对路径逃逸未被拦截(geometry 链路有
`validateGeometryBlobRef:103`,voxel 链路漏了)。P0-2 一并补齐。

修复正确且低成本:共享工具提供 `isConfinedRelativePath`(词法)+ `isPathWithinRoot`(canonical 物理封闭);
两条读路径打开前调物理校验;`FILE_FLAG_OPEN_REPARSE_POINT` 留作最终组件的第二道防线。细节见 `EXECUTE-P0.md`。

---

## P0-3 恶意工程 path payload 致 resample 死循环(CWE-834/400,CONFIRMED)

`XQPath::resample`(`XQPath.cpp:209-245`):
```cpp
if (sampleSpacing <= 0.0) return ResampleStatus::InvalidSpacing;   // 只挡非正数
...
for (double arcLength = 0.0; arcLength < totalLength; arcLength += sampleSpacing)
    samples.push_back(sample);                                     // 无迭代/容量上限
```
`XQProjectReader.cpp:1298`:
```cpp
if (spacing > 0.0) { path.resample(spacing); }   // 忽略返回值,且无 spacing 下限校验
```
`spacing` 来自工程文件(`tok_double`,只在 <=0 时拒),控制点弧长也由工程文件决定。
构造两控制点相距 ~1e9、`sampleSpacing 1e-9`,加载即迭代 ~1e18 次、`samples` 无界增长 → 挂起 + 内存耗尽。

**修复**:在 reader 端对 `spacing` 设合理下限(相对总弧长的比例上限,即限制最大采样点数),
或让 `resample` 返回点数上限保护并让 reader 检查返回值。细节见 `EXECUTE-P0.md`。

---

## P1 健壮性(CONFIRMED,非安全)

1. **AssetRegistry next_id_ 回绕**:`AssetRegistry.cpp:38-39` `if (id.value() >= next_id_) next_id_ = id.value() + 1;`
   当 `id.value() == UINT64_MAX` 时 `+1` 回绕到 0,后续 `createAsset` 从 0 重新发号 → id 复用。概率极低,但一行可修。
2. **mmap reinterpret_cast 对齐**:`MappedGeometrySource.cpp:182/196/197/215` 把 mmap base `reinterpret_cast` 成
   `Point3*`/`SourceTriangle*`/`int*`/`SourceTet*`。base 是页对齐(≥4K)所以首元素 OK,但**多 blob 拼一个 mapping** 或
   非 8 字节倍数偏移时无 `alignof` 断言。当前是「一 blob 一 mapping」故安全,属**防御性加固**,非当前 bug。
   已有 `static_assert(sizeof(Point3)==3*sizeof(double))`(181 行),建议补 `assert(base % alignof(T) == 0)`。
   **仅 MappedGeometrySource.cpp**:MappedVoxelSource.cpp 全文无 reinterpret_cast(按字节 memcpy),不在此项范围。

---

## P2 设计债(择要执行,拆分类只立档)

**立即可做(与 P0 合并)**:
- **重复 confinement + checkedMul**:`isConfinedRelativePath` 3 份、`checkedMul` 2 份
  (`BlobStore.cpp:80` / `GeometryResourceManager.cpp:59`)。提取到共享 header。

**立档、本任务不执行(工程量大,单开任务)**:
- `XQProjectReader.cpp` 2790 行、`XQProjectWriter.cpp` 1087 行 monolith:建议按 Parser/Validator/Builder 分离。
- `XQMainWindow` 368 行 + 7 controller God Object:建议拆 WindowShell / WorkflowCoordinator / ResourceRegistry。
- `ResidentKey = pair<AssetId,int>` + 8 个 `kResident*` magic int(`GeometryResourceManager.cpp:21-28`):建议 `enum class ResidentKind`。
- 命令类手动 `inserted_`/`captured_` 标志(`XQSceneCommands.h`):建议用 `std::optional` 承载状态。

---

## P3 性能反模式(CONFIRMED,择高影响执行)

渲染器 `XQSceneRenderer.cpp` 多处逐元素 VTK 调用。**注意**:faceId 那处(244-247 行)其实**已经**先
`SetNumberOfTuples` 再 `SetValue`(非 `InsertNext`),开销可接受,安全 agent 描述略夸大。真正值得改的是两处三重 voxel 循环:
- `:126` 影像体:内层每 voxel 一次 `image->GetScalarPointer(x,y,z)`(每次重算偏移)。1024³ 时约 10 亿次虚调用。改成 `GetScalarPointer(0,0,0)` 取基址 + 行主序步进。
- `:901` 分割掩膜:三重循环内 `points->InsertNextPoint(...)` 无 `reserve`,动态扩容抖动。改成先数非零 voxel `reserve` 再填。

其余(polyline `SetPoint`/`InsertCellPoint`、pressure `InsertNextValue`)点数量级小(≤1e4),低优先,`EXECUTE-P1-P4.md` 只列不强制。

---

## P4 测试覆盖缺口(CONFIRMED,补关键项)

优先补(与 P0/P1 修复配套的负面测试**必须**补):
- **BlobStore/GeometryResourceManager confinement 负面测试**:除已有 `../outside.bin`,补 junction/symlink 逃逸被拒的用例(P0-2 配套)。
- **resample DoS 保护测试**:微小 spacing + 大弧长 → 加载被拒或点数有界(P0-3 配套)。
- `GeometrySourceResolver.cpp` 无专属测试文件,仅集成测试间接覆盖 → 补单测。
- 服务层失败枚举:`FlowController::solve` 仅测 Ok/InvalidBoundaryConditions;`ModelingService` 的 AlreadyClosed/InvalidModel/NullScene;`AiService` 三 backend null 返回。

低优先(立档不强制):单测/集成 58:6 倒挂、MeshQuality 直接单测、并发 undo/redo 竞态测试。

---

## 已证伪的误报(执行 agent 严禁据此改代码)

| 安全/agent 原始发现 | 证伪依据 |
|---|---|
| `GeometryResourceManager.cpp:369`/`:177` CWE-362 竞态 | `acquireVoxelSource:170`、`evictToBudget:352`、`evictToBudgetLocked` 全程持同一 `mutex_`;注释明写 "Acquire shares mutex_ with us... no TOCTOU dangling"。**误报** |
| `GeometryResourceManager.cpp:295` checkedMul 未防溢出 | `validateGeometryBlobRef` 113-114 行正是用 `checkedMul`;该函数 59 行有 `b > max/a` 溢出检查。**误报** |
| `XQProjectReader.cpp:2673` stem 含分隔符逃逸 | `std::filesystem::path::stem()` 只返回文件名主干,对 `../../evil.xqproj` 返回 `evil`,不含 `../`。**误报** |
| `MappedVoxelSource.cpp:208`/`XQProjectReader.cpp:596` 整数溢出 | 64 位 Windows 上 `65536³=2⁴⁸` 不溢出 size_t;且 acquire_region 有 `x1>=dimX` 边界检查。仅 32 位理论成立,本机 64 位。**不适用** |
| `readVerified` 读超大文件放大 DoS(BlobStore:331) | 322-328 行有 byteCount 尺寸门,超大目标先被 TruncatedBlob/ByteCountMismatch 挡。**大幅降级**(仅剩 junction 跟随,并入 P0-2) |
| `XQSceneRenderer.cpp:246` faceId 逐元素 SetValue 性能 | 244 行已 `SetNumberOfTuples` 预分配 + `SetValue`(非 InsertNext),开销可接受。**降级为低优先** |
| library_leakage: `XQMainWindow.h:6` 暴露 QMainWindow | app 层是 UI 入口,继承 QMainWindow 天经地义,spec 允许 Qt 只出现在 app/。**非违规** |

## 曾存疑项 —— 已拍板(不改,附理由)

> 用户要求「不把选择留到关键时刻」,以下两项已定为**保留不改**,不再留给执行时裁定。

- **SurfaceLodBuilder.h 暴露 vtkSmartPointer/vtkPolyData**(`src/visualization/SurfaceLodBuilder.h:6-7,33,53,65`):
  **决策 = 保留不改**。该文件在 `visualization/` 层,不在 core/services;spec 允许 VTK 出现在渲染层。
  visualization 是 disposable 可视化产物层,已 grep 确认仅 `XQSceneRenderer.cpp` + `test_surface_lod.cpp` 引用,无跨层扩散。
  仅当日后把 visualization 提为公开服务库时才需 pimpl 隐藏——届时另开任务。

- **`XQMainWindow.cpp:326` / `XQSceneRenderer.cpp:519` 自动降级**:**决策 = 保留不改**。
  这两处是产品既有设计(网格加载失败回退表面几何、LOD 构建失败回退全几何),非本次新增。
  CLAUDE.md「别加降级」针对「AI 擅自新增」,强删历史设计会改变产品可见行为、反而违反同一规则精神。
  若日后要改属新需求,单独提,不在本任务范围。
