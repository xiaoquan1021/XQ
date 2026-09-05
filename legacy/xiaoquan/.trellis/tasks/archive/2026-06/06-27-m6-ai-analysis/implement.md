# M6 AI 分析 — 实现清单(implement)

> 严格按 design.md。零外部依赖进公开 API(ONNX 只在 adapters/onnx 私有,且默认 OFF 不编译)。命令复用 M0。测试 CHECK 宏。
> 不破 M0~M5 的 38 测试。ONNX Runtime 未装 → 真实推理后挂,本里程碑做纯算 + mock 链路。

## 步骤 1:XQDomainType 加 AiAnalysis

- `src/core/XQDomainType.h`:enum 末尾追加 `AiAnalysis`;domainTypeToString switch 加 `case AiAnalysis: return "ai_analysis";`。
- `src/core/XQScene.cpp`:domain/group switch 加 AiAnalysis 分支(归入合适组,如 Simulations 或新 Analysis 组——参照现有分组,简单归 Simulations 或 Unknown 组即可,注明)。
- 检查 test_payload_and_groups.cpp / SvProjectReader.cpp 编译是否报漏分支,按需补。

## 步骤 2:XQAiAnalysis + payload(core,新建)

- `src/core/XQAiAnalysis.h/.cpp`:
  - `enum class AnalysisKind { Identify, FlowMetrics, SurrogatePrediction };`
  - `enum class AnalysisProvenance { Computed, ModelInferred, SurrogatePredicted };`
  - `struct NamedMetric { std::string name; double value; std::string unit; };`
  - `struct Annotation { std::string label; double arcLength; int faceId; double score; };`(location 用 arcLength + 可选 faceId)
  - 类 XQAiAnalysis:kind、modelId、provenance、vector<NamedMetric> metrics、vector<Annotation> annotations、NodeId sourceNode(has/set)、诊断字符串。getter/setter;metricByName 便捷查。
- `src/core/XQAiAnalysisPayload.h`:: public XQPayload,domainType==AiAnalysis,clone() 值深拷,analysis() const/非 const。

## 步骤 3:FlowMetricsService(services/ai,纯算,零 ONNX)

- `src/services/ai/FlowMetricsService.h/.cpp`(static,纯域,只 link xq_core):
  - `struct Request { double mu = 0.04; /*viscosity CGS*/ };`(或从 case 取;首版给默认 + 可覆盖)
  - enum Status { Ok, InvalidFlow }。
  - `Result analyzeFlow(const XQFlowResult& flow, const Request&)`:
    - 校验 flow.isConsistent() 且 times/segments 非空,否则 InvalidFlow。
    - **WSS**:每 segment 每时刻 τ=4μQ/(πR³),R=√(A/π);算时均 TAWSS、峰值 WSS_max(全 segment)。
    - **FFR**:最远端 segment 时均 P / 最近端 segment 时均 P(Pd/Pa)。
    - **OSI**:每 segment OSI=0.5(1−|Σ τΔt| / Σ|τ|Δt),取代表值(如最大 OSI)。
    - **压降**:inlet−outlet 时均 ΔP。
    - 填 XQAiAnalysis(kind=FlowMetrics, provenance=Computed, metrics=[FFR, TAWSS, WSS_max, OSI, dP], 可选风险 annotation FFR<0.8)。
  - `analyzeFlowCommand(scene, newId, name, flow, flowNodeId, req)` → AddNodeWithSourceRelationCommand(source=flowNodeId)。

## 步骤 4:AiService + identify/surrogate backend 抽象(services/ai)

- `src/services/ai/AiService.h/.cpp`:
  - 复用 M2 的 `XQAiSegmentationBackend`(include services/segmentation/XQAiSegmentationRequest.h)。
  - 新增纯虚 backend:
    - `struct XQAiIdentifyRequest { std::string modelId; };` + `class XQAiIdentifyBackend { virtual XQAiAnalysis identify(...) = 0; };`(入参 surfaceModel/mesh + request)
    - `struct XQSurrogateRequest { std::string modelId; };` + `class XQSurrogateBackend { virtual std::shared_ptr<XQFlowResult> predict(...) = 0; };`
  - AiService 静态方法:
    - `segment(image, buffer, req, XQAiSegmentationBackend&)` → 委托 backend;null→诊断状态。
    - `identify(model, req, XQAiIdentifyBackend&)` → XQAiAnalysis(provenance=ModelInferred)。
    - `analyzeFlow(flow, req)` → 委托 FlowMetricsService。
    - `predictFlow(geom, req, XQSurrogateBackend&)` → shared_ptr<XQFlowResult>,**provenance 标 SurrogatePredicted**(在 result/或包装的 analysis 上标注;XQFlowResult 若无 provenance 字段,加一个可选 source 标记或在 analysis 层标 —— 最小改:predictFlow 返回的 flow 由 service 标记,或 AiService 提供 wrap 成 XQAiAnalysis(SurrogatePredicted) 的路径;实现时定一处并注释)。
  - 入 scene 命令复用 M0。

## 步骤 5:OnnxBackend 接口层(adapters/onnx,默认不编译)

- `src/adapters/onnx/OnnxBackend.h`:声明 OnnxBackend 实现 segment/identify/surrogate 三个纯虚接口 + XQ 自有 Tensor 描述(shape/data/归一化)。
- `src/adapters/onnx/OnnxBackend.cpp`:真实实现链接 onnxruntime —— **用 `#ifdef XQ_ENABLE_ONNX` 包裹,或仅在 option ON 时加入构建**。
  - CMake:`option(XQ_ENABLE_ONNX "Build real ONNX backend" OFF)`;OFF 时不加 onnx adapter 源 + OnnxBackendTest,不 find_package(onnxruntime)。
  - **ONNX Runtime include/对象只在此 .cpp**,不进任何 .h 公开 API。
- 不依赖 onnxruntime 的接口/Tensor 类型放 .h 可编译;真实推理体在 ifdef 内。

## 步骤 6:CMake 注册

- xq_core 加 XQAiAnalysis.cpp。
- xq_services 加 FlowMetricsService.cpp、AiService.cpp。
- `option(XQ_ENABLE_ONNX OFF)`;ON 时才加 adapters/onnx + OnnxBackendTest + find onnxruntime。
- 新增 test_flow_metrics_service / test_ai_service / test_ai_integration + add_test。

## 步骤 7:测试(全 CHECK 宏)

- `tests/services/ai/FlowMetricsServiceTest.cpp`:**核心**——合成 XQFlowResult,WSS=4μQ/πR³ 解析对照(误差阈值)、FFR=已知 Pd/Pa、OSI 振荡波形对照;InvalidFlow 诊断;命令 undo;payload 深拷。
- `tests/services/ai/AiServiceTest.cpp`:mock 三个 backend——segment 出掩膜入 scene+undo;identify 出 analysis(ModelInferred);predictFlow 出 flow 且标 SurrogatePredicted;backend null→诊断。
- `tests/services/ai/AiIntegrationTest.cpp`:构造/跑 0007 FlowResult → analyzeFlow 出 FFR/WSS,数值合理、入 scene。不依赖 onnxruntime。

## 完成门槛(worker 自检)

- 全新 build 目录(XQ_ENABLE_ONNX 默认 OFF)构建零错误;全量 ctest 真绿(M0~M5 的 38 + M6 新增,预期 ≥41)。
- 假绿抽查:篡改 WSS 公式(如 R³→R²)或 FFR 比值方向 → FlowMetrics 解析对照 Release FAIL → 恢复 → PASS。写进汇报。
- grep 确认 services/ai 与所有公开 .h 无 onnxruntime/ONNX include(只 OnnxBackend.cpp 在 ifdef 内可有)。
- 小抉择(指标代表值取法、风险阈值、Tensor 描述字段)参考血流力学标准 + plan 自定合理默认,别停下问。
- 汇报写 `.trellis/tasks/06-27-m6-ai-analysis/implement-report.md`:文件清单、ctest 数字、假绿证据、指标解析对照误差数值、ONNX 后挂说明、技术债。
</content>
