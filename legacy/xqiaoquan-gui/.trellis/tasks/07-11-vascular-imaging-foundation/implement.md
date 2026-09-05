# Implementation Plan: 血管影像外部依赖地基

## Current Phase

当前处于 Trellis Phase 2 / execution。依赖审计已归档，依赖整改已有功能报告但仍待任务收口，真实数据门尚未取得实际合格数据。六个生产 children 已创建并完成初版规划，必须按依赖顺序逐个激活。

## Planned Task Tree

任务树已按下表建立。每个 child 独立完成 planning、implementation 和 check；依赖写入 child PRD，不靠目录顺序猜测。用户要求立即执行完整壳，但仍只激活当前依赖已满足的 child。

| 顺序 | 计划 child slug | 交付物 | 硬依赖 |
| --- | --- | --- | --- |
| 1 | `vascular-foundation-dependency-audit` | ABI/模块/许可/可复现构建报告与最小真实探针 | 无 |
| 2 | `vascular-foundation-data-gate` | 真实增强 CT/CTA + 参考 mask + hash/许可/空间核验 + 冻结指标 | 无，可与 1 并行研究 |
| 3 | `vascular-foundation-itk-preprocess` | ITK 3D 去噪 + Frangi/Sato vesselness adapter | 1、2 的数据格式结论 |
| 4 | `vascular-foundation-auto-segmentation` | 自动传统分割、形态学、连通/重连、XQ mask | 2、3 |
| 5 | `vascular-foundation-itk-vtk-bridge` | 官方桥、LPS/mm/direction 守恒、表面入口 | 1、3 |
| 6 | `vascular-foundation-centerline-tree` | vtkvmtk 探针或真实 3D fallback、半径、拓扑 | 1、4、5 |
| 7 | `vascular-foundation-real-meshing` | 真实血管表面上的合规体网格与质量报告 | 1、4、5 |
| 8 | `vascular-foundation-e2e` | 真 CTA 自动全链、持久化、GUI、实机验收 | 2-7 |

父任务不会因为部分 child 完成而标记完成。第 8 项和父级全范围 check 均通过后才进入 finish。

## Execution Order and Exit Gates

### Step 1 - Dependency / ABI / license audit

Actions:

- 从 Externals manifest、安装树、CMake cache 和实际 imported targets 生成依赖清单。
- 冻结 MSVC、x64、C++ 标准、runtime、Release/Debug、VTK/ITK module set。
- 将 ITK `find_package` 收敛到实际 components，建立 3D vesselness/bridge 编译探针。
- 核对 vtkvmtk 最小 C++ source/patch/license，运行 VTK 9.3 real-surface probe。
- 核对 TetGen 1.5.1 与仓内 AGPL LICENSE、MMG license 和目标研究/分发方式。

Exit gate:

- 审计报告没有“待核对但先算通过”的项。
- 每个计划采用的 backend 有可复现 configure/build/run 命令。
- 不可采用的 backend 被明确拒绝并指定下一路径。

### Step 2 - Real CTA data gate

Actions:

- 调研并实际取得至少一套增强 CT/CTA + vessel reference mask。
- 核对许可、去标识、来源、完整性、DICOM identity、label mapping。
- 建立 external data bundle 和 `XQ_VASCULAR_TEST_DATA_ROOT`。
- 固定 tune/validation case 和指标阈值；gold 仅用于评分。

Exit gate:

- 影像和 mask 在同一 LPS physical space 中数值对齐。
- source/license/hash 文档完整。
- 最终指标与阈值在算法最终验证前冻结。

### Step 3 - ITK 3D preprocessing

Actions:

- 建立 XQ voxel source -> `itk::Image<float,3>` 私有转换。
- 实现 spacing-aware anisotropic diffusion。
- 实现 multi-scale Hessian vesselness，先 Frangi，Sato 作为可选同接口对照。
- 输出 XQ-owned result、diagnostics 和 provenance。

Exit gate:

- 倾斜 direction 单测与真实 CTA 运行均通过。
- 合成单测只证明数学/边界；真实 CTA 证明生产执行。
- 没有逐切片替代三维 filter。

### Step 4 - Automatic traditional segmentation

Actions:

- 实现 vesselness/intensity 自动阈值或 hysteresis 逻辑。
- 接 ITK morphology、connected components、relabel 和明确的细支/分叉保留策略。
- 如使用 level-set，只允许自动初始 mask，不允许人工 seed 进入验收路径。
- 输出 XQ mask、atomic command 和失败诊断。

Exit gate:

- 冻结验证集上运行无需人工干预。
- 所有预定分割/连通/分支指标完整报告并过线。
- 金 mask 未被生产路径读取。

### Step 5 - ITK/VTK bridge

Actions:

- 接入 ITKVtkGlue official bridge。
- 处理方向矩阵、scalar ownership 和 VTK lazy pipeline 生命周期。
- 建立 mask/vesselness -> VTK image -> surface 的同空间测试。

Exit gate:

- dimensions、spacing、origin、direction、LPS 和 world round-trip 在自动测试与真实样本上守恒。
- GUI 不需要私有 flip/transpose 才能对齐。

### Step 6 - Centerline/radius/topology

Actions:

- 先运行 vtkvmtk 有界探针；通过才生产化。
- 未通过则补齐真实 3D skeletonization 依赖，使用 distance map、graph extraction、pruning 和 branch resampling。
- 实现统一 XQ centerline tree validator/persistence。

Exit gate:

- 真实 mask 自动得到连通 centerline tree、正半径和有效拓扑。
- 指标达到 Step 2 冻结阈值。
- 验收链不读取人工 XQPath/Contour。

### Step 7 - Real vessel meshing

Actions:

- 生成并验证闭合 surface。
- 在许可批准的 ON build 中运行 fill backend + MMG。
- 对真实血管报告翻转、体积、域外、退化、quality histogram、boundary marker。

Exit gate:

- 真实 case 质量门通过；程序生成弯管仅作为补充回归。
- 若许可/后端未解决，本 child 和父任务保持未完成。

### Step 8 - Real-data E2E and physical-machine acceptance

Actions:

- 新增生产 headless E2E：DICOM import -> preprocess -> segment -> tree -> surface -> mesh -> save/reopen。
- GUI 调同一 workflow，展示参数 profile、进度、诊断和各层结果。
- 运行负向矩阵、Release 全量 tests 和用户实机验收。

Exit gate:

- 自动成功路径无 mock、Noop、gold input、人工 Path/Contour 或测试专用算法。
- 保存重开后 geometry/provenance/lineage/metrics 一致。
- 实机验收记录存在。

## Planned Validation Targets

目标名在对应 child 实现时最终确定，但验证职责不得删除：

```powershell
# Default regression build
cmake --build XQ/build_gui --config Release
ctest --test-dir XQ/build_gui -C Release --output-on-failure

# Real vascular foundation build (illustrative option names; child design freezes exact names)
cmake -S XQ -B XQ/build_vascular -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DXQ_VASCULAR_TEST_DATA_ROOT=<authorized-external-root> `
  -DXQ_ENABLE_VASCULAR_FOUNDATION=ON `
  -DXQ_ENABLE_TETGEN=ON `
  -DXQ_ENABLE_MMG=ON
cmake --build XQ/build_vascular --config Release
ctest --test-dir XQ/build_vascular -C Release --output-on-failure

# Focus areas expected in the final suite
ctest --test-dir XQ/build_vascular -C Release --output-on-failure `
  -R "vascular_dependency|vascular_data|itk_preprocess|vesselness|auto_vessel_segmentation|itk_vtk_bridge|centerline_tree|real_vessel_mesh|vascular_foundation_e2e"

cmd /c XQ\run_xq.bat
```

The option and target names above are planning placeholders, not claims that they exist today. Completion evidence must quote the actual final commands and outputs.

## Check Plan

The final check must independently verify:

1. Spec and architecture boundaries.
2. Actual external linkage/execution, not header or directory presence.
3. Data provenance, license and gold/input separation.
4. 3D physical-coordinate fidelity.
5. Quantitative segmentation/centerline/radius/topology metrics against frozen thresholds.
6. Real-surface mesh validity and quality.
7. Negative-path atomicity and diagnostic quality.
8. Headless/GUI production-path identity.
9. Full Release regression plus real-data ON build.
10. Honesty of completion wording.

## Risk and Rollback Points

- Keep each backend behind a narrow XQ-owned interface so vtkvmtk/skeleton and mesher choices can change without rewriting services/GUI.
- Land dependency probes separately from algorithm code; a failed probe can be removed without weakening contracts.
- Do not persist ITK/VTK native objects; rollback remains file/contract safe.
- Do not change project schema until the exact centerline-tree persistence contract is reviewed.
- Do not remove existing threshold/region-grow/manual Path workflows during foundation construction; keep them as legacy/manual tools until the automatic path is accepted, but never use them to pass the new gate.
- Preserve user untracked `CHECK-*`, `EXECUTE-*`, `XQ/xq_app_dist/` and existing handoff files. No `git add .`, `git clean`, hard reset or broad workspace cleanup.

## Activation Gate

Current execution gate:

- [x] User explicitly requires the complete reused shell and authorizes execution.
- [x] Dependency/data gates and all six production children exist.
- [x] Six production children have PRD/design/implement artifacts.
- [ ] Curated context validates for every child before activation.
- [ ] Dependency remediation task is formally closed without overstating parent completion.
- [ ] Actual CTA/reference-mask bytes, license, hashes, alignment and frozen metrics close the data gate before final algorithm acceptance.
- [ ] Activate only the next dependency-ready child; no parallel duplicate centerline implementation.
