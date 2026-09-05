# M9a 设计 — 渲染器/网格服务迁移到 Source + 上传核重写

> 依据本会话三路并行调研(renderer / 网格服务+适配器 / Source 层就绪度)的实测结论。
> 用户决策锁定:**驻留源迁移**(几何 mmap 源/工厂留 M9b)、**一次迁完渲染器 + 网格服务/适配器两家**、flow/MMG/path 出范围。

## 架构总览

迁移方向:消费者从"直接吃 `XQTriangleSurfaceGeometryHandle&` / `XQTetVolumeMeshHandle&` + 逐元素访问器"改为"吃 `const IGeometrySource&` + 经 lease 取连续 span"。

```
                         ┌─────────────────────────────────────────────┐
调用方(持 handle)──────►│ ResidentSurfaceSource / ResidentTetSource     │  (core/source,已存在)
  XQMainWindow            │   包 shared_ptr<const handle>,零拷贝借用     │
  VolumeMeshService       └──────────────────┬──────────────────────────┘
                                             │ const IGeometrySource&
                          ┌──────────────────┴──────────────────────────┐
                          ▼                                              ▼
            ┌──────────────────────────┐              ┌──────────────────────────────┐
            │ renderer (visualization) │              │ TetGen / 网格服务 / io writer  │
            │  build_surface/build_vol │              │  acquire_*().span() 连续访问    │
            │  → VTK 批量 SetData       │              │  → REAL[3n] / flatten / 守恒    │
            └──────────────────────────┘              └──────────────────────────────┘
```

驻留源是现成的(`ResidentSurfaceSource`/`ResidentTetSource` 已实现 `IGeometrySource`,零拷贝借用底层 vector)。本里程碑不建 mmap 几何源、不建几何工厂——调用方在调用点构造 Resident*Source 即可。

---

## 决策 1:renderer lease 生命周期 = **copy-on-upload(上传后即拷贝,不借用)**

### 背景(实测)
renderer 管线用 `vtkPolyDataNormals`/`vtkGeometryFilter`/`vtkExtractEdges` 经 `GetOutputPort()` 连接(`XQSceneRenderer.cpp:344-359,384-397`),这些 filter **在 render 时惰性执行**,不在 `addSurface`/`addVolumeMesh` 调用内执行。actor/mapper 被 `renderer_` 长期持有,而 renderer 没有任何地方存放 lease。

### 取舍
- **方案 A borrow-to-actor(零拷贝借用随 actor)**:`vtkDoubleArray::SetArray(ptr, n, save=1)` 借用 lease 指针,但 lease 必须活到 actor 销毁。renderer 需新增"actor → 持有的 lease"映射且管理其生命周期 = 新增可见状态、与现有 `RemoveAllViewProps` 清理语义耦合,且 resident 路径下 keepalive 已是底层 vector 的 shared_ptr,borrow 的唯一收益(省一次拷贝)在 resident 下无内存收益(数据本就驻留)。
- **方案 B copy-on-upload(选定)**:从 lease span **一次性批量拷贝**进 VTK 拥有的数组(`vtkDoubleArray` 自有缓冲),lease 在 `build_*` 返回即释放。**消除逐元素 storm(实测瓶颈)的同时不引入悬垂指针风险**,不改 renderer 生命周期模型。

### 结论
选 **B**。瓶颈是逐元素虚调用 + per-tet 堆分配(`SetPoint`/`InsertNextCell`/`vtkNew<vtkTetra>`),不是那一次连续拷贝。批量拷贝 `3n` doubles / `4n` ints 是 memcpy 级,远低于 10M 次虚调用。零拷贝借用留待 M9b 的 mmap 几何源 + 渐进上传(那时数据不驻留、借用才有内存意义,且届时统一设计 actor-lease 生命周期)。

> AC8 在 copy-on-upload 下天然满足(无借用指针),但仍保留:design 选定策略 = copy,测试验证 `addSurface`/`addVolumeMesh` 后释放源 handle 渲染仍正确(证明无悬垂借用)。

---

## 决策 2:ITetMesher 注入缝 = **改吃 `const IGeometrySource&`**

### 背景
`ITetMesher::tetrahedralize(const XQTriangleSurfaceGeometryHandle& surface, const TetMeshParams&)`(`ITetMesher.h:54`)硬依赖具体 surface handle。ripple 面:实现 `TetGenTetMesher`、`TetGenThenMmg`;调用方 `VolumeMeshService`;以及 `MeshingController`/`XQMainWindow` 的装配点;测试 `test_tetgen_volume_mesh`/`test_mmg_volume_mesh`/`VolumeMeshServiceTest`。

### 取舍
- **传 lease(`GeometryLease<Point3>` + `TriangleLease`)**:调用方先 acquire 再传,接口签名暴露 lease 类型、调用方要管 lease 生命周期 = 把 Source 内部概念泄漏进 mesher 接口。
- **传 `const IGeometrySource&`(选定)**:mesher 内部自己 `acquire_points()`/`acquire_triangles()`,生命周期自洽(lease 在 mesher 内 RAII);调用方只需把 handle 包进 `ResidentSurfaceSource` 传引用。语义最干净、与体素侧 `IVoxelSource&` 消费范式一致。

### 结论
选 **`const IGeometrySource&`**。`ITetMesher.h` 改签名 + include(去 `XQTriangleSurfaceGeometryHandle` fwd,加 `IGeometrySource`);
- `TetGenTetMesher::tetrahedralize` 内 `acquire_points().span()` 喂 pointlist、`acquire_triangles().view()` 喂 facets + facetmarkerlist;
- `TetGenThenMmg`(orchestrator)转发同签名;
- `VolumeMeshService::buildVolumeMesh` 把入参 surface handle 包 `ResidentSurfaceSource` 后传给注入的 mesher;
- 测试的 mock/直调一起改(铁律:接口加/改方法,所有 mock/stub 一起补)。

> 注意:`meta().triangleCount==0` 或非闭合时 mesher 的既有失败语义(`TetMeshResult.ok=false`)保持不变,经 `meta()`/`span().empty()` 判定。

---

## 决策 3:上传核 VTK 9.3 批量 API 形态

### points(surface + tet 共用)
```cpp
auto lease = src.acquire_points();           // GeometryLease<Point3>
const auto& span = lease.span();             // ReadSpan<Point3>, .data()=const Point3*
static_assert(sizeof(Point3) == 3*sizeof(double), "Point3 must be packed double[3]");
vtkNew<vtkDoubleArray> arr;
arr->SetNumberOfComponents(3);
// copy-on-upload:VTK 拥有缓冲,lease 可立即释放
arr->SetArray(/*copy*/ ...);                 // 用 SetNumberOfTuples + memcpy,或 InsertTuples 批量
vtkNew<vtkPoints> pts; pts->SetData(arr);
```
实现取 **拷贝进 VTK 自有数组**(决策 1)。具体:`arr->SetNumberOfTuples(n)` 后 `std::memcpy(arr->GetPointer(0), span.data(), 3*n*sizeof(double))`(`Point3` packed 已 static_assert)。

### surface cells(VTK 9 split offsets/connectivity)
`TriangleView.triangles` = `ReadSpan<SourceTriangle>`(`array<int,3>`)= 连续 `int[3n]`。VTK 9 `vtkCellArray` 需 **offsets(`0,3,6,…,3n`)+ connectivity**:
```cpp
vtkNew<vtkCellArray> cells;
// connectivity: int32 → 拷进 vtkTypeInt32Array(n*3);offsets: 生成 i*3,长度 n+1
cells->SetData(offsetsArr, connArr);         // 二者均 VTK 自有
polyData->SetPolys(cells);
```
offsets 是 O(n+1) 生成(唯一必需的额外缓冲);connectivity 从 `SourceTriangle` int32 批量拷贝。faceId 用 `vtkFloatArray`(`SetNumberOfTuples(n)` + 从 `TriangleView.faceIds` 并行 span 批量填,转 float),`GetCellData()->SetScalars`。

### tet cells(消除 per-tet vtkTetra)
`acquire_tetrahedra().span()` = `SourceTet`(`array<int,4>`)= `int[4n]`。同 surface:offsets=`i*4`(长 n+1)+ connectivity int32,`SetData` 后:
```cpp
grid->SetCells(VTK_TETRA, cells);            // 单一 cell type,无逐 tet vtkNew<vtkTetra>
```
替换 `build_volume:180-189` 的 `Allocate` + per-tet 循环。

### int32 vs vtkIdType
VTK 默认 `vtkIdType`=int64。源连接是 int32。两条路:① 用 `vtkTypeInt32Array` 做 connectivity(VTK 9 `SetData` 接受 32 位连接,内部按需);② 拷进 `vtkIdTypeArray`(int64,多一次 widening 拷贝)。**选 ①**(零 widening,直接 int32),offsets 同用 32 位。实现时核对 VTK 9.3 `vtkCellArray::SetData` 的 32 位重载签名(`vtkCellArray::SetData(vtkTypeInt32Array* offsets, vtkTypeInt32Array* connectivity)`)。

> 渲染管线(normals/geometryFilter/extractEdges/lut/faceId 着色)与空几何 guard **完全保留**,只换 `build_surface`/`build_volume` 的内部填充方式。输出必须像素/网格等价。

---

## 分层与依赖
- renderer(visualization)、网格服务(services)、TetGen 适配器(adapters)、io writer(io)消费 `IGeometrySource` 抽象。VTK 只在 visualization/adapters 私有实现,io/core 无 `vtk*`。
- 新增/改动 link 边:无新增跨层边(visualization/services/adapters 已可见 core/source;`ResidentSurfaceSource`/`ResidentTetSource` 在 core/source)。`ITetMesher.h` 的 include 从 `XQTetVolumeMeshHandle.h` + fwd `XQTriangleSurfaceGeometryHandle` 改为 + `core/source/IGeometrySource.h`(core 内,方向合法)。
- Source 公共签名零改动(static_assert 成立),本里程碑是消费者迁移。

## 验证策略
- **不退化基线**:迁移前对现有测试场景(test_scene_renderer / VolumeMeshServiceTest / test_tetgen_volume_mesh / project round-trip)留快照(渲染像素 hash 或元素级 dump),迁移后逐项对比等价。
- **上传核可核**:grep 确认 `build_surface`/`build_volume` 内无 `SetPoint(` 逐点、无 `InsertNextCell` 逐元素、无 `vtkNew<vtkTetra>`。
- **TetGen 守恒**:四面体化后顶点/连接/faceId(走 trifacemarkerlist)与迁移前逐一对比(参 memory `tetgen-plc-volume-mesh`)。
- **生命周期**:`addSurface` 后释放源 handle、再 render,验证无悬垂(copy-on-upload 应天然通过)。
- 铁律:Release + 全量 ctest 全绿 + 假绿抽查;`acquire_*` 等副作用调用不进 assert。

## 风险
- **R1 VTK 9.3 `SetData` 32 位重载签名**:需实编核对(别凭记忆),先写一个最小 surface 上传跑通再铺开。
- **R2 ITetMesher ripple 漏改 mock**:`MeshingController`/`XQMainWindow`/3 个测试的装配点,改签名后逐个编译驱动补全。
- **R3 winding/法线**:体网格法线别假设同向(memory `xq-surface-winding-not-consistent`),管线既有 `AutoOrientNormals` 保留不动。
- **R4 faceId float 精度**:faceId 是 int 转 float 着色(现状如此),保持等价不改。
