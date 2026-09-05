# M6 AI 分析 — 实现报告(implement-report)

状态:**完成,全部门槛达成**。全新 build 目录(`build_m6`,XQ_ENABLE_ONNX 默认 OFF)零错误;全量 ctest(Release+offscreen)**41/41 真绿**(M0~M5 的 38 + M6 新增 3);假绿抽查两处均 FAIL→恢复→PASS。

## 1. 新增 / 改动文件清单

### 新增(core)
- `src/core/XQAiAnalysis.h` / `.cpp` — 通用 AI/后处理分析值对象。`AnalysisKind{Identify,FlowMetrics,SurrogatePrediction}`、`AnalysisProvenance{Computed,ModelInferred,SurrogatePredicted}`、`NamedMetric{name,value,unit}`、`Annotation{label,arcLength,faceId,score}`;kind/modelId/provenance/metrics/annotations/sourceNode/diagnostic + `metricByName`。零依赖,值深拷。
- `src/core/XQAiAnalysisPayload.h` — `: public XQPayload`,domainType==AiAnalysis,clone() 值深拷。

### 新增(services/ai,纯算 + 链路)
- `src/services/ai/FlowMetricsService.h` / `.cpp` — 纯算 FFR/WSS/OSI/dP,零 ONNX。`Request{mu=0.04, ffrRiskThreshold=0.8, referencePressure=0.0}`;`Status{Ok,InvalidFlow,NullScene}`;`analyzeFlow` + `analyzeFlowCommand`(AddNodeWithSourceRelationCommand,source=flowNodeId)。
- `src/services/ai/AiService.h` / `.cpp` — 统一入口。复用 M2 `XQAiSegmentationBackend`;新增纯虚 `XQAiIdentifyBackend`/`XQSurrogateBackend` + 请求结构;`segment`/`identify`/`analyzeFlow`/`predictFlow`/`buildAnalysisCommand`。

### 新增(adapters/onnx,默认不编译)
- `src/adapters/onnx/OnnxBackend.h` — `XQTensor`(shape/data/归一化,XQ 自有边界类型,零 onnxruntime);`OnnxBackend` 同时实现三个纯虚 backend 接口 + pImpl(`struct Impl` 不透明)。
- `src/adapters/onnx/OnnxBackend.cpp` — `#include <onnxruntime_cxx_api.h>` 与 `Ort::` 全部包在 `#ifdef XQ_ENABLE_ONNX` 内;真实推理体 TODO(技术债)。else 分支为 well-formed 占位(isAvailable()==false)。

### 新增(tests)
- `tests/services/ai/FlowMetricsServiceTest.cpp` — 核心解析对照(见 §4)。
- `tests/services/ai/AiServiceTest.cpp` — mock 三个 backend 链路。
- `tests/services/ai/AiIntegrationTest.cpp` — 真实 0007 → FlowSolver1D → analyzeFlow → 入 scene。
- `tests/adapters/onnx/OnnxBackendTest.cpp` — 仅 XQ_ENABLE_ONNX=ON 编译。

### 改动
- `src/core/XQDomainType.h` — enum 末尾加 `AiAnalysis`;domainTypeToString 加 `case AiAnalysis → "ai_analysis"`。
- `src/core/XQScene.cpp` — `groupForDomain` 加 `case AiAnalysis → Group::Simulations`(挂在仿真链,与 FlowResult 同组)。两处 switch 全覆盖,无 default,编译无漏分支报错。
- `CMakeLists.txt` — xq_core 加 XQAiAnalysis.cpp;xq_services 加 FlowMetricsService.cpp + AiService.cpp;3 个新测试 target + add_test;test_ai_integration 加 tinyxml2 PATH;`option(XQ_ENABLE_ONNX OFF)` 块(ON 时 find_package(onnxruntime CONFIG) + xq_adapter_onnx + test_onnx_backend)。
- `build_m6.bat` — 构建脚本(同配方,不含 onnxruntime)。

## 2. ctest 结果数字

- 全新独立目录 `XQ/build_m6`,`-DXQ_ENABLE_ONNX` 未指定(默认 OFF)。
- 构建:141/141 链接成功,`BUILD_OK`,零错误零警告告警。
- `set QT_QPA_PLATFORM=offscreen` + `ctest -C Release --output-on-failure`:
  - **100% tests passed, 0 tests failed out of 41**(Total 3.78~4.82 s)。
  - 旧 38 全过 + 新增 `test_flow_metrics_service`(#27)、`test_ai_service`(#28)、`test_ai_integration`(#29)全过。
- 确认默认构建未 find onnxruntime(CMakeCache 仅 option 描述行,无 onnxruntime 路径条目);未生成 onnx adapter lib / onnx exe。

## 3. 假绿抽查证据(防假绿)

均在 build_m6 重建后只跑 test_flow_metrics_service:

| 篡改点 | 文件:行 | 触发断言 | observed vs expected | 误差 |
|---|---|---|---|---|
| WSS 公式 R³→R²(`radius*radius*radius`→`radius*radius`) | FlowMetricsService.cpp:53 | line 125 `relErr(wssMax,expectedWss)<1e-9` FAIL | 0.12732395 vs 0.06366198(R=2→R³/R²=2) | relErr=**1.000**(精确 2 倍) |
| FFR 比值反向(`pdMean/paMean`→`paMean/pdMean`) | FlowMetricsService.cpp:156 | line 136 `relErr(ffr,0.7)<1e-9` FAIL | 1.42857143 vs 0.7(=1/0.7) | — |

两次均:篡改 → Release FAIL → 恢复原公式 → 重建 → 全量 ctest 41/41 PASS。WSS 对照用 A=4π(R=2,R³=8≠R²=4)确保 R³→R² 可证伪(若用 R=1 则抓不到,已规避)。

## 4. 指标解析对照误差数值(FlowMetricsServiceTest)

合成 XQFlowResult(恒定/已知波形),阈值 relErr < 1e-9(WSS/FFR/dP)、1e-12(OSI 零值):
- **WSS 闭式**:τ=4μQ/(πR³),μ=0.04,Q=10,A=4π→R=2 → analytic=0.06366198 dyn/cm²;observed WSS_max/TAWSS relErr **0.000e+00**。
- **FFR**:Pa=1.0e5,Pd=7.0e4 → 0.7,relErr **<1e-9**;反向用例 Pa=1e5/Pd=9.5e4→0.95(<1.0 锁方向)。
- **dP**:Pa−Pd=3.0e4,relErr **<1e-9**。
- **OSI**:对称 ±7 振荡 → 0.5(relErr<1e-9);单向 {3,5,4} → 0(|·|<1e-12)。
- InvalidFlow:不一致/空/负面积/绝对 Pa≤0(默认 ref=0)四路径;命令 undo/redo;payload 深拷(clone 后改副本不动原件)。

### 0007 端到端实测(AiIntegrationTest)
真实 aorta_final.ctgr + inflow_1d.flow → FlowSolver1D(2 cycles)→ analyzeFlow:
`FFR=1.7542  TAWSS=10.580 dyn/cm²  WSS_max=40.419 dyn/cm²  OSI=0.4894  dP=-94617.7 dyn/cm²`,入 scene 派生关系=1,provenance=Computed,source=flowNode。
- WSS(TAWSS~10、WSS_max~40 dyn/cm²)生理合理(主动脉 O(1–100));OSI∈[0,0.5] 构造保证。
- **FFR=1.75 说明(设计抉择,非 bug)**:FlowSolver1D 记录的是**弹性壁相对压**(P=β(√A−√A0)/A0,围绕 0,实测 −167860..9370 dyn/cm²,seg0 时均 −109431)。FFR 是绝对压比,故 Request 加 `referencePressure`(默认 0=序列即绝对压,供单测精确对照;集成测试按数据把最负段时均压抬到生理下限 60 mmHg=8.0e4)。1D 远端 RCR 把最远端段压力顶高于最近端 → FFR>1,这是相对压基准下的已知伪影,公式与方向自洽(dP<0 ⟺ FFR>1,已断言)。FFR=Pd/Pa 的精确解析正确性与方向由单测严格锁定。

## 5. ONNX 后挂说明(技术债)

- **现状**:onnxruntime 未安装(Externals/install/windows-x64 无 onnxruntime,plan/09 待加 manifest)。本里程碑落地:① FlowMetricsService 纯算(完整验收)② AiService + mock 三 backend(链路验收)③ OnnxBackend 接口层(默认不编译)。
- **option 怎么设**:`option(XQ_ENABLE_ONNX "..." OFF)`。默认 OFF → 不 `find_package(onnxruntime)`、不加 `xq_adapter_onnx` 源、不加 `test_onnx_backend`、不注册其 add_test。
- **打开方式(待依赖到位)**:Externals 装好 onnxruntime 后,配置时加 `-DXQ_ENABLE_ONNX=ON` 并把 onnxruntime 根加入 CMAKE_PREFIX_PATH。届时编译 `OnnxBackend.cpp`(`#ifdef XQ_ENABLE_ONNX` 分支生效,link `onnxruntime::onnxruntime`)+ `test_onnx_backend`。
- **铁律遵守(grep 已验证)**:`#include <onnxruntime_cxx_api.h>` 与 `Ort::` 仅出现在 `src/adapters/onnx/OnnxBackend.cpp` 的 `#ifdef XQ_ENABLE_ONNX` 内;services/ai 与 core 与 OnnxBackend.h **零** onnxruntime include/对象(仅注释提及)。无 Python。XQTensor 为 XQ 自有边界类型,不含任何 onnxruntime 类型。

## 6. 技术债清单

1. **真实 ONNX 推理未实现**(依赖缺口):OnnxBackend::segment/identify/predict 三方法体为 TODO,需 onnxruntime 到位后:image/几何→XQTensor(应用 normalizeMean/Std)→ Ort::Session 推理 → 输出转 XQSegmentationMask / XQAiAnalysis / XQFlowResult。OnnxBackendTest 同步补真实最小 .onnx 一次推理。
2. **FFR 压力基准**:依赖求解器相对压 + 调用方提供的 referencePressure。更完备方案:让 XQFlowResult 或求解器输出绝对压(带 RCR 远端基准),则 FFR 不需调用方传基准、0007 上 FFR 落回 (0,1] 生理区间。当前以 referencePressure 显式参数 + 集成测试数据驱动基准务实规避,FFR 精确性由合成绝对压单测保证。
3. **identify/surrogate 真实模型**:几何→张量编码(点/面采样、归一化)尚未定义,随真实模型契约落地。
