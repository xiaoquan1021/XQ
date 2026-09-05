# M2 分割 — 执行计划(implement)

> 构建/测试配方见 spec/XQ/core/build-and-test.md。验收:Release 全量 ctest 真绿(M0+M1 的 28 + M2 新增),
> 阈值/区域生长/连通域在合成数据 + 0007 影像跑通,AI 契约固定 + mock 链路通。

## 关键事实(已核查)
- `XQImageVolume` 的 `ImageBufferHandle` 与 `XQSegmentation` 的 `SegmentationMaskHandle` 都是**句柄、无真实体素**。
- `VtkImageAdapter::loadVti` 只产 XQImageVolume(几何/类型),**不解码真实标量**。
- 所以 M2 必须:(a) core 新建真标量缓冲;(b) 让 adapter 能把 VTI 真实标量解码进该缓冲(供 0007 跑通)。

## 执行步骤

1. **XQMemoryImageBufferHandle(core)**
   - `src/core/XQMemoryImageBufferHandle.h/.cpp`:持 vector<byte> + scalarType + dimensions + componentCount;
     按体素索引读标量(归一 double),构造校验字节数一致。零外部依赖。
   - 单测:构造校验、读回标量正确、字节数不一致报错。

2. **XQSegmentationMask(core)+ XQSegmentationMaskPayload**
   - `src/core/XQSegmentationMask.h/.cpp`:真实 label 体素(vector<uint8 或 label>)+ geometry + sourceImageNode + labels;
     foregroundVoxelCount、按索引读 label。**新建,不改句柄式 XQSegmentation**(避免破坏 M0 测试)。
   - `XQSegmentationMaskPayload : public XQPayload`(domainType==SegmentationMask,clone 深拷贝)。
   - 单测:mask 体素读写、payload domain/clone。

3. **SegmentationService(services/segmentation,core 自有算法,零依赖)**
   - thresholdMask / regionGrowMask(6-连通)/ keepLargestConnectedComponent;校验(lower≤upper、种子在 extent、空掩膜 cleanup 保留几何+来源)。
   - createMaskNodeCommand → M0 既有命令(掩膜带 sourceImageNode 用 AddNodeWithSourceRelation;否则 AddNode)。
   - service 只产掩膜+产命令,不碰 scene mutator(沿用 M1 底线范式)。
   - 单测 `tests/services/segmentation/SegmentationServiceTest.cpp`:合成数据(已知形状→已知体素数/连通块)验证三算法 + 入场景命令 + undo。

4. **AI 最小契约 + mock 后端**
   - `XQAiSegmentationRequest`(最小:modelId + 可选 ROI + targetLabels)。
   - `XQAiSegmentationBackend`(纯虚 segment(image,buffer,request)→mask);**mock 实现**验证链路。
   - `aiSegmentMask(image, buffer, request, backend)` 走后端。**ONNX 不出现**。
   - 单测:mock 后端 → 影像→请求→掩膜链路通,掩膜保留来源关系。

5. **adapter:VTI 真实标量解码(供 0007 跑通)**
   - 扩展 adapters/vtk:把 VTI 标量解码进 `XQMemoryImageBufferHandle`(VTK 在 adapter 私有实现,不进公开 API)。
   - service 仍只接受 core 的 buffer——解码在 adapter,符合「领域分割不依赖 io 解码类」。
   - 集成测:0007 影像解码出 buffer → 阈值/区域生长跑通,产非空掩膜,保留来源影像关系。

6. **CMake**:新增 core/service 源 + 测试 add_test;segmentation service 库只 link xq_core(AI 契约头也不引 onnx)。

## 验证(照 build-and-test.md)
- 配置+构建+全量 ctest(Release,offscreen)。期望:28 旧 + M2 新增全绿。
- 0007 集成:解码影像→分割→非空掩膜+来源关系。

## Review 闸
- 防假绿:新测试 CHECK 风格,副作用先求值到变量;抽查至少 1 个(篡改→Release FAIL→还原→PASS)。
- service 层零外部依赖(grep 无 Q*/vtk/itk);不碰 scene mutator;不新建命令类型。
- AI 契约不引入 onnx;mock 后端不依赖任何外部库。
- 改完先回归 M0/M1 的 28 测试。

## 回滚点
- step1(buffer 绿)/ step2(mask+payload 绿)/ step3(service 绿)/ step5(0007 集成绿)各为快照。
- 若 VTI 标量解码牵扯过大 → 先用合成数据跑通三算法(满足"合成数据单测"),0007 集成作为本步收尾;不削弱算法本身。

## 不做
- 不实现真实 ONNX(M6);不做表面提取(M3);不引入 ITK 形态学(后续 adapter)。
- 不动 XQProjectReader/Writer round-trip;清理项(fixture 绝对路径等)按需,不扩面。
