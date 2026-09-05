# M2 分割:SegmentationService 传统算法 + AI 分割接口

## Goal

从影像派生掩膜:既有传统算法(阈值/区域生长/连通域/形态学),也接入 **AI 分割**(深度模型,
经 `services/ai` 的 ONNX 后端)。输出统一为 XQ 自有掩膜 payload。

## 依赖

- 前置:**M1**(core payload、命令栈、`XQImageVolume`)。
- 接口对齐:**M6**——`aiSegmentMask` 走 `AiService::segment`,本里程碑**先固定接口契约 + mock/小模型**
  验证链路,真实推理后端由 M6 落地。
- 后续:M3 建模(掩膜→表面提取属建模阶段,不在本服务持久化表面)。

## Requirements

### 所有权

```
src/core/XQSegmentationMask.h / .cpp        掩膜 payload(XQrebuild 已有 XQSegmentation 则扩展)
src/core/XQMemoryImageBufferHandle.h / .cpp 合成/解码标量缓冲(core 自有)
src/services/segmentation/SegmentationService.h / .cpp
tests/services/segmentation/SegmentationServiceTest.cpp
```

### 公开 API

```cpp
// 传统(首版 core 自有代码实现,零外部依赖)
thresholdMask(image, XQSegmentationThresholdParameters) -> shared_ptr<XQSegmentationMask>
regionGrowMask(image, XQRegionGrowingParameters)        -> shared_ptr<XQSegmentationMask>  // 阈值范围内 6-连通
keepLargestConnectedComponent(mask)                     -> shared_ptr<XQSegmentationMask>
// AI(经 services/ai;契约见 M6)
aiSegmentMask(image, XQAiSegmentationRequest)           -> shared_ptr<XQSegmentationMask>
// 入场景
createMaskNodeCommand(name, mask)                       -> AddNodeCommand / AddNodeWithSourceRelationCommand
```

掩膜保留来源影像关系(`ImageToSegmentationMask`)。

### 算法 kernel 边界

- 传统算法首版 **core 自有代码**(可单测、零外部依赖)。
- ITK 作为后续重采样/形态学 kernel,置于 `adapters/itk`,**不进公开 API**。
- AI 推理统一走 `adapters/onnx`(M6)。

### 校验

- 输入影像单组件;必须提供 `XQMemoryImageBufferHandle`;缓冲字节数与尺寸/标量类型一致。
- 阈值 lower ≤ upper;区域生长种子在影像 extent 内;空掩膜 cleanup 返回保留几何/来源 id 的空掩膜。

## 约束

- 公开 API 不依赖 ITK/VTK/Qt/插件;领域分割不依赖 io 层解码缓冲类;表面提取属建模(M3)。
- 参考来源:分割算法语义参考 **SimVascular/MITK**(及底层 ITK/VTK);**XQ1 不参考**。

## Acceptance Criteria(plan「M2 验收」+ 03「验收」)

- [ ] 阈值 / 区域生长 / 连通域在合成数据与 `0007` 影像上跑通,单测通过。
- [ ] `aiSegmentMask` 接口契约固定;以 mock 或小模型验证"影像→掩膜"链路(真实模型作为资产后挂)。
- [ ] 掩膜保留来源影像关系,可经命令入场景并 undo。

## Notes

- 复杂 task,进 Phase 2 前补 design.md / implement.md。
- AI 接口契约需与 M6 的 `XQAiSegmentationRequest` / `AiService::segment` 保持一致。
