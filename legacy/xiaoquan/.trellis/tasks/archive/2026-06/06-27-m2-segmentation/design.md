# M2 分割 — 技术设计(design)

> 基于对 M0/M1 实际接口的核查(XQImageVolume / XQSegmentation / 命令 / payload),非 plan 文字。
> 遵循 spec/XQ:core 零依赖、命令/undo、坐标 LPS/mm、参考 SimVascular/MITK 不看 XQ1。

## M0/M1 现状核查(已读源码)

- `XQImageVolume`:几何/scalarType/componentCount/intensityRange/window/rescale/voxelToWorld 齐全;
  但 `ImageBufferHandle` 是**句柄,只有 is_valid/voxelCount,不含真实标量数据**(core 零依赖设计:像素缓冲不在 core)。
- `XQSegmentation`(core 已有):id/sourceImageNode/geometry/labels + `SegmentationMaskHandle`
  (**也是句柄,只有 voxelCount/labelCount,无真实体素**)。
- 命令(M0):`AddNodeCommand(scene*, XQDataNode, label)`、`AddNodeWithSourceRelationCommand(scene*, node, source, label)`、
  `ReplacePayloadCommand`。M1 已建立 service→只产命令→不碰 mutator 的范式(底线方案)。
- payload 范式(M1):`XQXxxPayload : public XQPayload`,domainType()+clone()。

## 核心设计抉择

### 1. XQMemoryImageBufferHandle(core 新增,真标量缓冲)
- 现有句柄不含真实数据,分割算法需要逐体素读/写 → **新建 core 自有的真内存标量缓冲**。
- `src/core/XQMemoryImageBufferHandle.h/.cpp`:持 `std::vector<std::byte>` + scalarType + dimensions;
  提供按体素索引读标量(归一到 double)、构造校验(字节数 == voxelCount × scalarSize × component)。
- 零外部依赖。合成数据测试与真实影像解码(io/adapter 解码后填入)都用它。
- **分割算法输入 = XQImageVolume(几何/类型)+ XQMemoryImageBufferHandle(真标量)**,二者分离符合 prd 校验项。

### 2. 掩膜表示:新建 XQSegmentationMask(core)+ XQSegmentationMaskPayload
- 现有 `XQSegmentation` 的 mask 是句柄无真实体素;分割算法要写真实 label 体素 → 需真掩膜。
- **决策:新建 `XQSegmentationMask`**(core,持真实 label 体素 vector<uint8/label> + geometry + sourceImageNode + labels),
  而非硬塞进句柄式 `XQSegmentation`(避免破坏 M0 已绿的 segmentation 测试)。
- `XQSegmentationMaskPayload : public XQPayload`(domainType()==SegmentationMask)承载它,入 scene。
- 保留来源影像关系:掩膜带 sourceImageNode,入场景用 `AddNodeWithSourceRelationCommand`(source=影像节点)。

### 3. SegmentationService(src/services/segmentation,纯领域,零外部依赖)
按 prd 公开 API:
- 传统(**core 自有算法,零依赖**):
  - `thresholdMask(image, buffer, XQSegmentationThresholdParameters)` → shared_ptr<XQSegmentationMask>;lower≤upper 校验。
  - `regionGrowMask(image, buffer, XQRegionGrowingParameters)` → 阈值范围内 6-连通区域生长;种子在 extent 内校验。
  - `keepLargestConnectedComponent(mask)` → 连通域标记保留最大;空掩膜返回保留几何/来源 id 的空掩膜。
- AI(最小契约 + mock,见下):`aiSegmentMask(image, XQAiSegmentationRequest, backend)` → shared_ptr<XQSegmentationMask>。
- 入场景:`createMaskNodeCommand(name, mask)` → `AddNodeWithSourceRelationCommand`(掩膜带 sourceImageNode 时)
  或 `AddNodeCommand`。返回 M0 既有命令,不新建命令类型。
- service 只产掩膜 + 产命令,不持/不改 scene(沿用 M1 底线范式)。

### 4. AI 分割最小契约(M2 定,M6 落地;经用户确认「最小契约 + mock」)
- `XQAiSegmentationRequest`(core 或 services/ai 公共头):**最小字段** —— modelId(string)、可选 ROI(extent)、
  targetLabels(vector<int>)。不预设 M6 的指标/代理模型类型。
- 后端抽象 `XQAiSegmentationBackend`(纯虚:`segment(image, buffer, request) -> XQSegmentationMask`),
  M2 用一个 **mock 实现**(如阈值化伪装成"AI 输出"或固定形状)验证"影像→请求→后端→掩膜"链路。
  M6 落地真实 ONNX 后端实现同一接口。**ONNX 不在 M2 出现**。
- 放置:契约头放 services/segmentation 或公共 services 头;M6 的 AiService::segment 复用此 request 类型。

## 约束 / 注意(spec)
- 公开 API 不依赖 ITK/VTK/Qt/插件;领域分割不依赖 io 解码缓冲类(用 core 的 XQMemoryImageBufferHandle)。
- 表面提取属 M3,不在本服务持久化表面。
- 算法语义参考 SimVascular/MITK(及底层 ITK/VTK 的阈值/区域生长/连通域语义),**不看 XQ1**。
- 验收纪律:Release 全量 ctest;副作用别进 assert/CHECK 条件(用 CHECK 风格,先求值到变量)。

## 风险
- 连通域/区域生长是真算法,需合成数据数值校验(已知形状→已知体素数/连通块数)。
- XQMemoryImageBufferHandle 的 scalarType 分发要覆盖至少 0007 影像用的类型(看 VtkImageAdapter 解码出什么)。
- 不破坏 M0/M1 的 28 个测试。
