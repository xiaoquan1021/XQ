# 03 分割(M2)

## 目标

从影像派生掩膜:既有传统算法(阈值/区域生长/连通域/形态学),也接入 **AI 分割**(深度模型,
经 `services/ai` 的 ONNX 后端)。输出统一为 XQ 自有掩膜 payload。

## 所有权

```
src/core/XQSegmentationMask.h / .cpp        掩膜 payload(若 XQrebuild 已有 XQSegmentation 则在其上扩展)
src/core/XQMemoryImageBufferHandle.h / .cpp 合成/解码标量缓冲(core 自有)
src/services/segmentation/SegmentationService.h / .cpp
tests/services/segmentation/SegmentationServiceTest.cpp
```

## 输入 / 输出

- 输入:`XQImageVolume` + core 自有的内存标量缓冲(`XQMemoryImageBufferHandle`)。
- 输出:`XQSegmentationMask` payload;可选的把掩膜节点加入 scene 的命令。掩膜保留来源影像关系
  (`ImageToSegmentationMask`)。

## 公开 API

```cpp
// 传统
thresholdMask(image, XQSegmentationThresholdParameters) -> shared_ptr<XQSegmentationMask>
regionGrowMask(image, XQRegionGrowingParameters)        -> shared_ptr<XQSegmentationMask>  // 阈值范围内 6-连通
keepLargestConnectedComponent(mask)                     -> shared_ptr<XQSegmentationMask>
// AI(经 services/ai;见 07)
aiSegmentMask(image, XQAiSegmentationRequest)           -> shared_ptr<XQSegmentationMask>
// 入场景
createMaskNodeCommand(name, mask)                       -> AddNodeCommand / AddNodeWithSourceRelationCommand
```

`XQAiSegmentationRequest` 描���模型标识、ROI、目标类别;具体推理由 `AiService` 执行(07),
本服务只负责"影像→AI 后端→掩膜"的 XQ 侧封装。

## 算法 kernel 边界

- 传统算法首版用 **core 自有代码**实现(可单测、零外部依赖)。
- ITK 可作为后续重采样/形态学的 kernel,置于 `adapters/itk`,**不进公开 API**。
- AI 推理统一走 `adapters/onnx`(见 07)。

## 校验

- 输入影像单组件;必须提供 `XQMemoryImageBufferHandle`;缓冲字节数与尺寸/标量类型一致。
- 阈值 lower ≤ upper;区域生长种子在影像 extent 内。
- 对空掩膜做 cleanup 返回保留几何/来源 id 的空掩膜。

## 禁止

- 公开 API 不依赖 ITK / VTK / Qt / 插件;领域分割不依赖 io 层解码缓冲类。
- 表面提取属于建模阶段(04),不在本服务持久化表面。

## 验收

- 阈值 / 区域生长 / 连通域在合成数据与 `0007` 影像上跑通,单测通过。
- `aiSegmentMask` 接口契约固定;以 mock 或小模型验证"影像→掩膜"链路(真实模型作为资产后挂)。
- 掩膜保留来源影像关系,可经命令入场景并 undo。
