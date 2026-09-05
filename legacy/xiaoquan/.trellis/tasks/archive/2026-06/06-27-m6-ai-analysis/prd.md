# M6 AI 分析:AiService 三类 + 统一 ONNX 后端

## Goal

为主线提供三类 AI 能力,统一以 **C++ 内置 ONNX Runtime** 为推理后端(`.onnx` 模型,无 Python):

1. **几何/影像分割或识别** —— 影像→掩膜(供 M2),或几何→标注(狭窄/斑块/分支识别)。
2. **流体结果指标/风险** —— 由流场(M5 的 `XQFlowResult`)算 FFR、WSS、OSI、压力梯度等指标 + 风险评估。
3. **代理模型预测(跳过求解)** —— 由几何/网格直接预测流场或关键指标,替代昂贵 CFD。

## 依赖

- 前置:**M5**(指标分析消费 `XQFlowResult`)。
- 共享接口:**M2**——`segment` / `XQAiSegmentationRequest` 契约与 M2 的 `aiSegmentMask` 对齐。
- 后续:M7 前端 AI 面板。

## Requirements

### 所有权

```
src/adapters/onnx/OnnxBackend.h / .cpp        ONNX Runtime 封装(加载 .onnx、张量进出)
src/core/XQAiAnalysis.h / .cpp                AI 分析结果 payload(掩膜引用 / 标注 / 指标 / 预测流场)
src/services/ai/AiService.h / .cpp            统一入口,分发三类分析
src/services/ai/SurrogateFlowModel.h / .cpp   代理模型(几何/网格 -> 流场/指标)
src/services/ai/FlowMetricsService.h / .cpp   流体指标/风险(可纯算 + 可模型增强)
tests/services/ai/AiServiceTest.cpp
tests/adapters/onnx/OnnxBackendTest.cpp
```

### 统一后端(OnnxBackend)

- 封装 ONNX Runtime C++ API:加载模型、绑定输入/输出张量、run、取结果。
- `services/ai` 之上只见 XQ 自有张量/请求/结果类型;**ONNX Runtime 对象不出现在公开 API**。
- 模型以资产提供(`.onnx` + 元数据描述输入形状/归一化/类别);模型缺失发诊断,不崩溃。

### 公开 API(AiService)

```cpp
segment(image, XQAiSegmentationRequest)         -> shared_ptr<XQSegmentationMask>   // 供 M2
identify(modelOrMesh, XQAiIdentifyRequest)      -> XQAiAnalysis                     // 狭窄/斑块/分支标注
analyzeFlow(flowResult, XQFlowMetricsRequest)   -> XQAiAnalysis                     // FFR/WSS/OSI/风险
predictFlow(geometryOrMesh, XQSurrogateRequest) -> shared_ptr<XQFlowResult>         // 直接出流场/指标
```

结果作为 `XQAiAnalysis`(或既有 payload)入 scene,保留 derived 来源关系,受 stale 传播管理。

### 首版策略

- **流体指标先纯算落地**(FFR/WSS/OSI 可由流场直接计算,不依赖模型),保证 M5→M6 链路可验收。
- 分割与代理模型**先固定接口契约**,用 mock/小模型验证链路,真实权重作为资产后挂。

### 校验

- 模型输入形状/归一化与请求一致;不匹配发诊断。
- 代理模型预测的流场附"预测" provenance,与求解结果区分。
- 指标计算要求流场有效(节点/时间齐全)。

## 约束

- 不引入 Python 运行时;公开 API 不暴露 ONNX Runtime 对象;不直接改 scene;预测结果不得伪装成求解结果(provenance 标明代理模型)。
- ONNX Runtime 需新增到 `Externals/externals.manifest`(CPU 版起步)。
- 参考来源:指标定义(FFR/WSS/OSI)与分割/识别语义参考 **SimVascular/MITK 及相关文献**;**XQ1 不参考**。

## Acceptance Criteria(plan「M6 验收」+ 07「验收」)

- [ ] `OnnxBackend` 能加载一个 `.onnx` 并完成一次推理(测试用最小模型)。
- [ ] 流体指标(FFR/WSS 至少其一)由 `0007` 流场纯算得出,数值合理,单测通过。
- [ ] `segment` / `predictFlow` 接口契约固定,mock/小模型链路通过;结果入 scene 且带正确 provenance。

## Notes

- 复杂 task(含模型契约 + 数值指标),进 Phase 2 前补 design.md / implement.md。
