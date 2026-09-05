# 07 AI 分析(M6)

## 目标

为主线提供三类 AI 能力,统一以 **C++ 内置 ONNX Runtime** 为推理后端(`.onnx` 模型,无 Python):

1. **几何/影像上的分割或识别** —— 影像→掩膜(供 03),或几何→标注(狭窄/斑块/分支识别)。
2. **流体结果的指标/风险分析** —— 由流场(06 的 `XQFlowResult` 或读入的 CFD 结果)算
   FFR、WSS、OSI、压力梯度等指标,并给风险评估。
3. **代理模型预测(跳过求解)** —— 由几何/网格直接预测流场或关键指标,替代昂贵 CFD。

## 所有权

```
src/adapters/onnx/OnnxBackend.h / .cpp        ONNX Runtime 封装(加载 .onnx、张量进出)
src/core/XQAiAnalysis.h / .cpp                AI 分析结果 payload(掩膜引用 / 标注 / 指标 / 预测流场)
src/services/ai/AiService.h / .cpp            统一入口,分发三类分析
src/services/ai/SurrogateFlowModel.h / .cpp   代理模型(几何/网格 -> 流场/指标)
src/services/ai/FlowMetricsService.h / .cpp   流体指标/风险(可纯算 + 可模型增强)
tests/services/ai/AiServiceTest.cpp
tests/adapters/onnx/OnnxBackendTest.cpp
```

## 统一后端(OnnxBackend)

- 封装 ONNX Runtime C++ API:加载模型、绑定输入/输出张量、run、取结果。
- `services/ai` 之上只见 XQ 自有的张量/请求/结果类型;**ONNX Runtime 对象不出现在公开 API**。
- 模型以资产形式提供(`.onnx` + 元数据描述输入形状/归一化/类别)。模型缺失时发诊断,不崩溃。

## 公开 API(AiService)

```cpp
// 1) 分割/识别
segment(image, XQAiSegmentationRequest)   -> shared_ptr<XQSegmentationMask>   // 供 03 调用
identify(modelOrMesh, XQAiIdentifyRequest)-> XQAiAnalysis                     // 狭窄/斑块/分支标注

// 2) 流体指标/风险
analyzeFlow(flowResult, XQFlowMetricsRequest) -> XQAiAnalysis                 // FFR/WSS/OSI/风险

// 3) 代理模型(跳过求解)
predictFlow(geometryOrMesh, XQSurrogateRequest) -> shared_ptr<XQFlowResult>   // 直接出流场/指标
```

所有结果作为 `XQAiAnalysis`(或既有 payload,如掩膜/流场)入 scene,保留来源关系(derived),
受 stale 传播管理。

## 三类的实现分层

| 类别 | 纯算部分 | 模型部分(ONNX) |
|---|---|---|
| 分割/识别 | 后处理(连通域/平滑) | 主体:分割网络 / 检测网络 |
| 流体指标/风险 | FFR/WSS/OSI 等可由流场**直接计算**(首版纯算) | 风险分级 / 模式识别可后挂模型 |
| 代理模型 | 输入特征提取、输出重建 | 主体:几何/网格→流场回归网络 |

> 首版策略:**流体指标可先纯算落地**(不依赖模型),保证 06→07 链路即可验收;分割与代理模型
> 先固定接口契约,用 mock/小模型验证链路,真实权重作为资产后挂。

## 输入 / 输出契约

- 输入:`XQImageVolume` / `XQSurfaceModel` / `XQMesh` / `XQFlowResult` + 类型化请求(模型 id、ROI、
  归一化参数)。
- 输出:`XQSegmentationMask` / `XQFlowResult` / `XQAiAnalysis`(标注、指标表、预测场)。
- 张量与几何之间的转换在 service 内完成;调用方只给 XQ 自有对象。

## 校验

- 模型输入形状 / 归一化与请求一致;不匹配发诊断。
- 代理模型预测的流场附"预测"provenance,与求解结果区分(可复现、可对照 06)。
- 指标计算要求流场有效(节点/时间齐全)。

## 禁止

- 不引入 Python 运行时;公开 API 不暴露 ONNX Runtime 对象;不直接改 scene。
- 预测结果不得伪装成求解结果(provenance 必须标明来源为代理模型)。

## 验收

- `OnnxBackend` 能加载一个 `.onnx` 并完成一次推理(测试用最小模型)。
- 流体指标(FFR/WSS 至少其一)由 `0007` 流场纯算得出,数值合理,单测通过。
- `segment` / `predictFlow` 接口契约固定,mock/小模型链路通过;结果入 scene 且带正确 provenance。
