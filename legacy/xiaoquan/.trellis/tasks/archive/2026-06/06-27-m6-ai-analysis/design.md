# M6 AI 分析 — 技术设计(design)

> 基于 M0~M5 实际接口核查(XQAiSegmentationBackend 契约 / XQFlowResult / XQSegmentationMask / 命令 / payload)+ plan/07 + plan/09 依赖策略。
> AI 推理统一 ONNX(无 Python);**ONNX Runtime 对象绝不进公开 API**。core 零依赖、命令/undo。

## 关键环境约束(已核查,影响验收范围)

- **ONNX Runtime 未安装**(plan/09 明确"Externals 当前没有,需新增到 manifest")。Externals/install/windows-x64 无 onnxruntime。
- → 本机**无法验收"加载真实 .onnx 完成推理"**(OnnxBackend 需链接 onnxruntime)。这是环境依赖缺口,非设计问题。
- **务实分层(完全符合 plan「首版策略」)**:
  1. **FlowMetricsService 纯算**(FFR/WSS/OSI 由 XQFlowResult 直接算,**零 ONNX 依赖**)→ 完整落地 + 验收。这是 plan M6 验收硬项("流体指标由 0007 流场纯算得出")。
  2. **AiService + XQAiAnalysis + mock backend**:接口契约 + 链路用 mock 验证(复用 M2 已建的纯虚 Backend 模式,**不需真实 onnxruntime**)→ 验收"segment/predictFlow 契约固定 + mock 链路通过 + 结果入 scene 带 provenance"。
  3. **真实 OnnxBackend(链接 onnxruntime)**:依赖缺失 → 接口层定义 + 标记 NOT BUILT,**记技术债,待 Externals 加 onnxruntime 后落地**。不在本机验收。
- 这样 M6 在不装 onnxruntime 前提下达成 plan 验收的**可达全部**;真实 ONNX 推理是已知后挂项。

## M0~M5 现状核查(已读源码)

- **M2 已建 AI 契约**(services/segmentation/XQAiSegmentationRequest.h):`XQAiSegmentationRequest{modelId, hasRoi, roi[6], targetLabels}` +
  纯虚 `XQAiSegmentationBackend::segment(image, buffer, request) -> shared_ptr<XQSegmentationMask>`。注释已声明 M6 复用此接口落地真实 ONNX。
  → **M6 直接复用,不另造分割契约**。
- **M5 XQFlowResult**:times[]、segments[]、flowQ/pressureP/areaA(`[segment][time]`,CGS:Q cm³/s、P dyn/cm²、A cm²)、isConsistent、sourceCaseNode。
  → FFR/WSS/OSI 的输入齐全。
- 命令复用 M0 AddNodeWithSourceRelation/AddNode/ReplacePayload;payload : public XQPayload + domainType + clone。
- `XQDomainType`:无 AiAnalysis 枚举 → 末尾追加(同步 XQDomainType.h domainTypeToString + XQScene.cpp 两处 switch,如 M5 经验,无 default 漏分支会编译报错)。
- `XQSegmentationMask`(M2)、`XQSurfaceModel`(M3)、`XQMesh`(M4)、`XQImageVolume`(M0)作为 AI 输入类型已具。

## 核心设计抉择

### 1. XQAiAnalysis + XQAiAnalysisPayload(core,新建)

- `XQAiAnalysis`:通用 AI 分析结果。字段:
  - `AnalysisKind kind`(Identify / FlowMetrics / SurrogatePrediction)。
  - `std::string modelId`(空=纯算,非空=模型产);`Provenance provenance`(Computed / ModelInferred / SurrogatePredicted)——**预测结果必须可与求解结果区分**(plan 硬要求)。
  - `std::vector<NamedMetric> metrics`(`{name, value, unit}`,如 {"FFR",0.78,""}、{"WSS_max",..,"dyn/cm^2"})。
  - `std::vector<Annotation> annotations`(可选:`{label, location(arcLength/faceId), score}`,狭窄/斑块/分支)。
  - `NodeId sourceNode`(来源:flowResult / model / mesh / image 节点);诊断摘要。
- `XQAiAnalysisPayload`:: public XQPayload,domainType==AiAnalysis,clone() 深拷(值类型)。零依赖。

### 2. FlowMetricsService(services/ai,纯算,零 ONNX 依赖)

> plan:FFR/WSS/OSI 等可由流场**直接计算**(首版纯算)。这是 M6 验收硬项。

- `analyzeFlow(const XQFlowResult& flow, const Request& req) -> Result(XQAiAnalysis)`。
- **指标(CGS,定义参考血流力学标准)**:
  - **FFR(Fractional Flow Reserve)**:远端时均压 / 近端时均压(Pd/Pa)。由 pressureP 各 segment 的时均值,取最远端 segment 时均 P 比最近端(inlet 侧)时均 P。范围 (0,1],<0.8 提示显著狭窄(给风险标注)。
  - **WSS(Wall Shear Stress)**:圆管泊肃叶壁面切应力 τ_w = 4μQ/(πR³),R=√(A/π)。每 segment 每时刻算,取时均 WSS、峰值 WSS_max、TAWSS(time-averaged)。单位 dyn/cm²。
  - **OSI(Oscillatory Shear Index)**:OSI = 0.5·(1 − |∫τ dt| / ∫|τ| dt),用各 segment WSS 矢量(1D 沿轴有符号)时间积分。范围 [0,0.5]。
  - **压力梯度**:沿程 ΔP(inlet−outlet 时均)。
- 校验:flow.isConsistent()、times/segments 非空,否则诊断(InvalidFlow)。
- **数值验收门槛(防假绿)**:用**合成 XQFlowResult**(已知恒定 Q/A/P)对 WSS/FFR 解析值精确对照(τ_w=4μQ/πR³ 闭式、FFR=已知 Pd/Pa),相对误差阈值。OSI 用已知振荡波形对照。这些是真断言可证伪。
- `analyzeFlowCommand(...)` → AddNodeWithSourceRelationCommand(source=flowResult 节点)。

### 3. AiService(services/ai,统一入口,持 backend 引用)

- 统一分发三类(plan 公开 API):
  - `segment(image, buffer, XQAiSegmentationRequest, XQAiSegmentationBackend&) -> shared_ptr<XQSegmentationMask>`:**复用 M2 契约**,委托 backend;null→诊断。
  - `identify(modelOrMesh, XQAiIdentifyRequest, IdentifyBackend&) -> XQAiAnalysis`:狭窄/斑块/分支标注,委托 backend(纯虚)。
  - `analyzeFlow(flow, req) -> XQAiAnalysis`:委托 FlowMetricsService(纯算,无 backend)。
  - `predictFlow(geometryOrMesh, XQSurrogateRequest, SurrogateBackend&) -> shared_ptr<XQFlowResult>`:代理模型,委托 backend;结果 provenance=SurrogatePredicted。
- backend 抽象均为**纯虚接口**(同 M2 模式),真实实现由 OnnxBackend 提供,测试用 mock。
- 入 scene 走命令,保留来源关系(derived),受 stale 传播管理。

### 4. OnnxBackend(adapters/onnx,接口层 + 真实实现标记后挂)

- `src/adapters/onnx/OnnxBackend.h`:定义封装类,实现上述 backend 纯虚接口(segment/identify/surrogate)。
  - **真实 .cpp 链接 onnxruntime**:因 onnxruntime 未装,**本里程碑不编译真实实现到默认构建**(CMake 用 option `XQ_ENABLE_ONNX` 默认 OFF;OFF 时不加入 onnx adapter 源 + 测试)。
  - 接口/张量转换类型(XQ 自有 Tensor 描述:shape/data/归一化)定义齐全,供 onnxruntime 到位后填实现。
  - **ONNX Runtime 对象绝不出现在 services/ai 之上的公开 API**(只在 OnnxBackend.cpp 私有)。
- 技术债:Externals 加 onnxruntime + 置 XQ_ENABLE_ONNX=ON 后,实现真实加载/推理 + OnnxBackendTest(最小 .onnx 一次推理)。

### 5. 命令 / 入 scene / provenance

- XQAiAnalysis → buildAnalysisCommand → AddNodeWithSourceRelationCommand(source=输入节点)。
- 代理预测的 XQFlowResult → provenance 必须标 SurrogatePredicted(plan:不得伪装成求解结果)。

## 校验(plan)

- 模型输入形状/归一化与请求一致 → 不匹配诊断(在 backend/AiService 校验请求)。
- 代理预测流场附"预测"provenance,与求解区分。
- 指标计算要求流场有效(节点/时间齐全)。
- 非法返回带状态失败,不抛异常。

## 测试(全 CHECK 宏,Release 真绿;不破 M0~M5 的 38 测试)

- `tests/services/ai/FlowMetricsServiceTest.cpp`(核心,纯算可证伪):
  - 合成 XQFlowResult → WSS=4μQ/πR³ 解析对照、FFR=已知 Pd/Pa 对照、OSI 振荡波形对照(相对误差阈值);InvalidFlow 诊断;命令 + undo;payload 深拷。
- `tests/services/ai/AiServiceTest.cpp`(链路,mock backend):
  - mock SegmentationBackend → segment 返回掩膜入 scene + provenance + undo;mock IdentifyBackend → identify 出 XQAiAnalysis;mock SurrogateBackend → predictFlow 出 XQFlowResult 且 provenance=SurrogatePredicted;backend 返回 null → 诊断。
- `tests/services/ai/AiIntegrationTest.cpp`(端到端,读真实 0007 链路):
  - 由 0007 跑 M5 FlowResult(或构造)→ analyzeFlow 出 FFR/WSS 指标,数值合理、入 scene。**不依赖 onnxruntime**。
- **OnnxBackendTest**:XQ_ENABLE_ONNX=ON 时才编译(默认 OFF 跳过);记技术债。

## 验收对照(plan「M6 验收」)

- [可达] 流体指标(FFR/WSS)由 0007 流场纯算得出,数值合理,单测通过 ✅(FlowMetricsService)。
- [可达] segment/predictFlow 接口契约固定,mock 链路通过,结果入 scene + 正确 provenance ✅(AiService + mock)。
- [后挂·技术债] OnnxBackend 加载 .onnx 完成一次推理 —— **onnxruntime 未装,接口已定,实现+测试待依赖到位**。

## 风险

- onnxruntime 未装 → 真实推理验收推迟(已分层规避,核心纯算 + mock 链路可验收)。显著记技术债 + PROGRESS。
- XQDomainType 加 AiAnalysis 须同步两处 switch(无 default 编译报漏分支)。
- WSS/FFR/OSI 定义须用标准血流力学公式(CGS),解析对照防算错。
- 不破 M0~M5 的 38 测试;XQ_ENABLE_ONNX 默认 OFF 不影响默认构建。
</content>
