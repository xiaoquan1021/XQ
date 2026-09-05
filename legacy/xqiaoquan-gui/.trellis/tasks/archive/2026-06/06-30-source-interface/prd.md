# M8b-1 Source 接口定义(消费者依赖什么 + 兼容适配器)

## Goal

定义 XQ 几何/体素数据的**消费侧只读访问契约** —— `IVoxelSource` / `IGeometrySource` / `ReadLease` + 批量范围访问接口,并给出基于现有 vector handle 的内存(Resident)实现 + Handle 兼容适配器。

本任务只回答"**消费者依赖什么**":把散落在 segmentation / meshing / modeling / visualization / persistence 各处对 `XQMemoryImageBufferHandle`、`XQTriangleSurfaceGeometryHandle`、`XQTetVolumeMeshHandle` 的直接逐元素/整块访问,收敛到一组稳定的 Source 接口背后。**不改任何现有消费者的调用,不改驻留行为,不引入懒加载/卸载。** 严格串行 M8a→M8b-1。

## Background(理解阶段结论)

理解阶段对 5 个消费子系统做了全量调研(证据见 `.trellis/workspace/ocean/journal-1.md` 引用的 workflow 报告),核心规律三条:

1. **核心诉求是"整块一次拿到"**:除 segmentation 的 region-grow 随机泛洪、ONNX 的 roi 窗口、visualization 的 z-slab 暗示外,其余消费点全是 `for i in [0,count)` 的完整遍历。
2. **窗口化(region/slab)需求只集中在 voxel**:triangle/tet 侧零窗口需求。
3. **消费侧对几何/体素 100% 只读**:所有写入都是"构造全新 handle"(reader 重建、meshing/modeling 产物、ingest),没有"原地改输入"。

逐元素虚调用风险点(若把 `point(i)/scalarAt` 提升为跨接口虚函数会炸成 O(N) 虚派发):voxel 稠密渲染 `XQSceneRenderer.cpp:117`、阈值全扫 `SegmentationService.cpp:108`、region-grow `:146`、tet 质量统计 `VolumeMeshService.cpp:64`、persistence 两端摊平/重建 `XQProjectWriter.cpp:729`/`XQProjectReader.cpp:999`。

## Scope

### In scope

- 定义只读 span 视图类型(C++17,无 `std::span`)。
- 定义 `IVoxelSource`:`acquire_whole` / `acquire_region(extent[6])` / `acquire_slab(z)` + 免物化 `meta()`。
- 定义 `IGeometrySource`:`acquire_points` / `acquire_triangles`(含并行 faceId)/ `acquire_tetrahedra` + 免物化 `meta()`。
- 定义 `ReadLease`(RAII 借用句柄,只读,租约期内底层稳定)。
- 给基于现有 vector handle 的 Resident 实现。
- Handle 兼容适配器:让现有三种 handle 可作为对应 Source 暴露,**不改 handle 自身**。
- 单元测试:整块等价性、faceId 守恒、voxel 布局、region/slab 正确性、meta 免物化、acquire 次数、租约生命周期、只读契约。

### Out of scope(明确不做)

- **不迁移任何现有消费者**到 Source 接口(那是后续 M8b-2+;本任务只新增,不动旧调用)。
- 不改 `XQMemoryImageBufferHandle` / `XQTriangleSurfaceGeometryHandle` / `XQTetVolumeMeshHandle` 的现有 API。
- 不引入懒加载、卸载、磁盘驻留切换(驻留行为零改动)。
- 不定义写回/mutate 通道(消费侧全只读;批量构造入口留待后续)。
- 不接入 ONNX roi / 流式渲染(region/slab 只定义+测试,不改消费者)。

## Constraints

- **依赖方向**:Source 接口被 services / io / visualization / adapters 共同消费 → 必须落在最底层 `core`,零外部依赖(无 VTK/TetGen/Qt),纯 XQ 值类型。
- **C++17**:自定义 `ReadSpan<T>`,不依赖 C++20 `std::span`。
- **逐元素虚调用零容忍**:虚边界只在"取整块视图"这一层(每次 acquire 一次虚调用);视图内部访问全 non-virtual inline。
- **租约不变量**:`ReadLease` 存活期间其 span 指针稳定、不 realloc、不被 mutate;释放后视图失效。
- **元数据免物化**:`meta()` 在未 acquire 数据时即可返回正确 count/dims/type,且不分配数据缓冲。
- 验收按既有铁律:Release 全量 ctest 绿 + 假绿抽查;副作用调用不进 `assert`。

## Acceptance Criteria

- [ ] **AC1 整块等价性(geometry)**:经 `IGeometrySource` 一次 acquire 取得的 points / triangles / faceIds / tetrahedra 视图,逐元素与原 handle 的 `point(i)/triangle(i)/triangleFaceId(i)/tet(i)` 完全一致。
- [ ] **AC2 faceId 并行守恒**:`acquire_triangles` 的 faceId 视图与 triangle 视图等长且逐元素对应。
- [ ] **AC3 voxel 布局一致**:`acquire_whole` 返回的标量块按 x-fastest 布局,任取 (x,y,z) 经块内偏移取值与 handle `voxelIndex(x,y,z)+scalarAt` 等价。
- [ ] **AC4 region 正确性**:`acquire_region(extent)` 返回子块尺寸 = inclusive extent 体积,内容与整块对应子区域逐体素一致(含边界 extent);越界 extent 报错而非静默截断。
- [ ] **AC5 slab 正确性**:`acquire_slab(z)` 返回 dimX·dimY 标量,与整块第 z 平面逐体素一致;越界 z 报错。
- [ ] **AC6 元数据免物化**:未 acquire 数据时 `meta()`(count/dims/scalarType/componentCount/is_valid)返回正确值且不分配数据缓冲(可用分配计数或独立路径验证)。
- [ ] **AC7 每块单次 source 级访问**:整块遍历一个 handle 只发生一次 acquire(调用计数桩验证 acquire 次数 == entity 数,而非 O(N))。
- [ ] **AC8 租约稳定性**:`ReadLease` 存活期间 span 指针稳定有效;释放后视图不可再用(生命周期测试覆盖)。
- [ ] **AC9 只读契约**:`ReadLease` 不暴露任何写回 API;`ReadSpan<T>` 仅 const 访问。
- [ ] **AC10 外部库直喂形态**:`acquire_points` 视图可提供连续内存指针,可直接 memcpy 进 TetGen 风格 `REAL[3*n]`(以测试断言连续性与逐值相等证明,不实际链接 TetGen)。
- [ ] **AC11 零回归**:现有全部消费者与测试不改动,Release 全量 ctest 全绿。

## Notes

- design.md / implement.md 已补全(复杂架构任务)。
- 这是 M8b 系列第一步,后续 M8b-2+ 才迁移消费者到 Source;本任务保持纯增量,任意时刻可回滚而不影响主线。
- **冻结状态(2026-06-30 评审 R5)**:本阶段头文件标 **candidate-not-frozen**;`ReadLease` 的
  失败/取消/部分获取/线程安全(对抗并发驱逐)语义**留到 M8b-2 真实后备存储(mmap)存在后再定**
  —— 现在的 RAII"租约期底层稳定"是单线程视图契约,够本阶段用;并发读锁语义提前冻会冻错。
  对应 ROADMAP M8b-1 §R3 / M8b-2 §冲突。
