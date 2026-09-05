# XQ 重建主线(M0~M7)

## Goal

把血管影像建模 + 血流分析桌面应用按「库优先、UI 后挂、无治理层」重建。一条主线:
**路径 → 分割 → 建模 → 网格 → 流体 → AI 分析**。M1~M6 产出可单测的纯 C++ 服务库,
M7 才接进精简 Qt 壳。

> 本 task 是 M0~M7 的**父容器**,本身不直接产出代码。具体交付在 8 个子 task。
> 架构铁律、分层、坐标系、命令/undo、验收原则见 `.trellis/spec/XQ/`(随会话自动注入)。

## 三条铁律(贯穿所有子 task)

1. **库优先**:每条主线能力都是无 UI、无框架依赖、可单测的纯 C++ 服务库。
2. **无治理层**:不写 harness / ledger / contract / task-pack。一块能力对应一份文档,改完跑测试。
3. **外部库当 kernel,不当架构**:VTK / ITK / GDCM / ONNX 只在 adapter / service 私有实现里,不进公开 API。

## 参考来源(硬规则,见 spec/XQ/architecture/reference-sources.md)

- **代码起点**:`XQrebuild/`(只读,按需搬入干净 core/io/adapter)。
- **行为/算法参考**:SimVascular + MITK(及其底层 VTK/ITK/GDCM)。
- **排除**:`XQ1/` 全面作废,任何场景都不参考(失败产物,大量小问题)。

## 子 task 与依赖链

主线是严格线性依赖链;M2 的 AI 分割接口反向依赖 M6 的 ONNX 后端(接口契约先固定、mock 验证)。

```
M0 骨架 → M1 路径 → M2 分割 → M3 建模 → M4 网格 → M5 流体 → M6 AI 分析 → M7 前端
                      ╰─(AI 分割接口契约)─→ 由 M6 落地后端,M2 先 mock
```

| 子 task | 里程碑 | 依赖 | 覆盖 plan |
|---|---|---|---|
| 06-27-m0-skeleton | M0 骨架 | — | 01-core-and-io(M0) |
| 06-27-m1-path | M1 路径 | M0 | 01(M1)+ 02-path |
| 06-27-m2-segmentation | M2 分割 | M1(AI 接口契约与 M6 对齐) | 03-segmentation |
| 06-27-m3-modeling | M3 建模 | M2 | 04-modeling |
| 06-27-m4-meshing | M4 网格 | M3 | 05-meshing |
| 06-27-m5-flow | M5 流体 | M4 | 06-flow |
| 06-27-m6-ai-analysis | M6 AI 分析 | M5(指标);M2 共享分割接口 | 07-ai-analysis |
| 06-27-m7-frontend | M7 前端 | M1~M6 全部 | 08-frontend |

> Trellis 的 parent/child 不是依赖系统;依赖以本表与各子 prd 的「依赖」段为准。

## Acceptance Criteria(总验收)

- [x] 8 个子 task 各自验收达成(见各子 prd)。M0~M7 全部独立验证(全新构建 + 全量 ctest 真绿 + 假绿抽查)+ 复核 + 归档 + commit。
- [x] `ctest` 全绿(Release + 全量):主会话独立 build_verify 154/154 构建零错误,**ctest 44/44 真绿**(offscreen)。
- [x] `0007_H_AO_H` 端到端走通:`WorkflowIntegrationTest` 在真实 0007 上跑通 影像→路径→分割→建模→网格→流体求解→AI 指标,各步结果入项目树,全程 undo 回初始 + redo 恢复。scene 结构级 round-trip(test_new_domain_roundtrip)一致;payload 实体数据持久化为已记技术债(现有存档只存结构)。

> **总验收达成(2026-06-27)**。整条主线 M0~M7 完成。剩余技术债见下方滚动清单,均为非阻塞的后续加深/依赖项(W1 scene 收紧、payload 实体持久化、真实 ONNX 推理、M5 FFR 绝对压基准等)。

## Notes

- 数据进入 XQ 自有对象才算验收;经 import shell / facade / 外部库对象图的"读取"不算。
- 本 task 只组织里程碑;实现各子 task 时按 channel-driven workflow 进 Phase 2。

## 跨里程碑遗留 / 技术债(滚动清单)

> 各里程碑 check 复核积累的、跨 task 的遗留项,集中在此防丢。每条标来源与建议处置时点。

- **[risk·重要] scene 可变性完整收紧**(M0 提出,M1 采用底线方案):`XQScene` mutator 仍 public,
  「scene 变更经命令栈」目前 service 层有保护、全局靠纪律。建议 service 最全时(M7 前接入前)统一用
  friend/const 边界收紧,并改造受影响的 fixture 测试。
- **[M+] stale 多跳传播**:`mark_source_changed` 只标记直接 derived,未递归下游。stale 真正被消费时补。
- **[清理] 测试 fixture 绝对路径**:CMake 用 `C:/Users/OCEAN/...` 绝对路径,换机即 break。参数化(`XQ_FIXTURE_ROOT`)。
- **[清理] expected-scene 双真值**:`test_svproject_reader` C++ 硬编码 vs `expected-scene.json` 未解析,易漂移。择一收敛。
- **[清理] CenterlineFrame 法向阈值偏松**(M1 提出):`alignment > 0.5` 抓不住渐进漂移,收紧到 >0.9 或补急弯用例。
- **[M2 接口对齐] AI 分割契约**:M2 的 `aiSegmentMask` / `XQAiSegmentationRequest` 需与 M6 `AiService::segment` 一致。
  → M2 已定最小契约(modelId + 可选 ROI + targetLabels + 纯虚 Backend),M6 落地真实 ONNX 后端时复用此契约。
- **[M3+] 掩膜多 label 连通**(M2 提出):`keepLargestConnectedComponent` 把所有非零 label 当同一前景连通,
  M2 单 label 无影响;多 label 掩膜会混连,后续支持多 label 时区分 label 值。
- **[清理] VTI 子区域 extent**(M2 提出):`loadVtiWithBuffer` 当前对全 extent 影像安全(VTK 连续);
  若未来支持非零起点 extent 子区域,`GetScalarPointer` 指向 extent 原点需注意。
- **[M4+ 多分支] capModel 中间环 faceId 冲突**(M3 提出):`ModelingService::capModel` 对开口环按主轴排序后,
  首=inlet(faceId=2)、末=outlet(faceId=3),中间环用 `outletFaceId+oi-1` → K≥3 个开口时第一个中间环 faceId=3 与 outlet 撞。
  简单管道(2 开口,主动脉单段)不触发,M3 验收用例为 2 开口管道。支持多分支血管放样时,需为中间 cap 定独立 faceId 语义。
- **[M4+ 多分支] 边界环提取非简单边界**(M3 提出):`extractBoundaryLoops` 用 `nextOf[a]=b` 存单条有向出边,
  若某顶点是两条边界边起点(8 字形/自交边界)会被覆盖。放样圆环边界每顶点恰一条出边,不触发;复杂拓扑时需改多重映射。
- **[清理] 三角面去重**(M3 提出):`XQTriangleSurfaceGeometryHandle::is_valid` 只查索引范围 + 三点互异,不查重复三角形。
  放样/封口不产生重复三角,不触发;若未来从外部读入三角网格,需补重复/退化面检查。
- **[M5+ kernel] TetGen/MMG 未引入**(M4 提出):体网格首版用 XQ 自有**质心星形剖分**,仅对相对质心星形可见的域正确;
  弯曲主动脉若质心落体外/壁附近会产翻转或退化四面体,由质量摘要(min/符号)暴露。需高质量体网格/重网格时走 `adapters/tetgen`、`adapters/mmg` kernel(plan/09)。
- **[M5 注意] M3 表面 winding 全局不一致**(M4 提出):M3 capModel 只保证 2-流形拓扑闭合(每边恰 2 三角),**不保证全局法线一致定向**——
  实测 wall 三角与 cap 扇三角相对体内质心符号相反。M4 VolumeMeshService 已用有符号体积对每个 tet 统一翻正(与 TetGen/VTK 输出约定一致),不受影响;
  但 M5 流体若需表面法线朝向(壁面/进出口方向)须自己定向,或回 M3 统一 winding。
- **[清理] M3 应在生成时直接标 triangleFaceId**(M4 落地):M4 已给 `XQTriangleSurfaceGeometryHandle` 加 per-triangle faceId 标签,
  并让 M3 loft/cap 生成时填(wall=1/inlet=2/outlet=3)。此项已消除(原计划 M4 重建归属),仅记录该接口已建立,后续读入路径模型也应填标签。
- **[M4+ 加强] 体网格与原 TetGen 网格对照**(M4 提出):MeshingIntegrationTest 只验"能生成合理闭合体网格 + face 全链路 + 质量摘要",
  未与 `0007/Meshes/0090_0001.{msh,vtu}`(原 TetGen/MMG)做逐单元/统计量对照。可作后续加强(经 MSHMeshReader 读入对比量级)。
- **[M6 消费侧] FlowSegment.faceId 与 RomSettings inlet/outletFaceIds 未接入 solve**(M5 提出):M5 的 FlowIntegrationTest 中
  FlowSegment.faceId 置 0(中心线分段与边界 cap 的映射未实现);RomSettings.inlet/outletFaceIds 数据契约已具但 solve 暂未消费。
  M6 消费 FlowResult 指标(或多分支求解)时接入分段↔face 映射。
- **[M7 存档] M5 新字段/结果未序列化**(M5 提出):XQSimulationCase 新增字段(RCR/波形/RomSettings/FluidProperties)与 XQFlowResult
  未进 XQProjectWriter/Reader,存档 round-trip 未做。M7 存档统一处理(.sjb 扩展元素 round-trip)。
- **[加深] 瞬态 1D 用弹性壁近似 rigid + 无波速解析对照**(M5 提出):FlowSolver1D::solve 为显式格式稳定性用有限弹性壁(β=1e6),
  非 .sjb 的真 rigid;两条硬验收门槛(稳态泊肃叶 + 0D RCR 阶跃)走专用稳态/0D 路径已达成且可证伪,但瞬态线性波速尚无第三条解析校验。
  真 rigid 瞬态需隐式/特征线格式;波速对照可作加深。
- **[加深] 截面积来源单一**(M5 提出):A0(x) 仅从 contour 多边形面积(鞋带)取;表面模型切片 / 网格来源未实现。
- **[依赖缺口·重要] 真实 ONNX 推理未落地**(M6 提出):onnxruntime 未装(Externals 无,plan/09 待加 manifest)。M6 已落地 FlowMetricsService 纯算(完整验收)+ AiService/mock 链路 + OnnxBackend 接口层(CMake `option(XQ_ENABLE_ONNX OFF)`,真实实现+include 仅在 OnnxBackend.cpp 的 `#ifdef XQ_ENABLE_ONNX` 内)。
  待 Externals 装 onnxruntime → 配置加 `-DXQ_ENABLE_ONNX=ON` + onnxruntime 进 CMAKE_PREFIX_PATH → 实现三 backend 方法体(XQTensor↔Ort::Session)+ OnnxBackendTest(最小 .onnx 一次推理)。segment 复用 M2 XQAiSegmentationBackend 契约。
- **[M5 下游] FFR 压力基准伪影**(M6 提出):M5 瞬态用相对压(P=β(√A−√A0)/A0 围绕 0),致 0007 上 FFR=1.75>1(非 bug,相对压基准伪影)。M6 用 Request.referencePressure 务实规避 + 合成绝对压单测精确锁 FFR 方向/正确性。
  根治:M5 求解器输出绝对压(带 RCR 远端基准),则 FFR 自然落 (0,1] 生理区间。
- **[M6 后挂] identify/surrogate 真实模型契约**(M6 提出):几何→张量编码(点/面采样、归一化)未定义,随真实模型契约落地;当前 AiService 的 identify/predictFlow 接口固定 + mock 验证链路。
