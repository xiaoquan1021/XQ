# EXECUTE-P1~P4 — 次要项可执行指令(所有选择已拍板)

> 执行者:便宜 agent。P1/P4 必做;P2「合并重复」已在 P0-2 完成,此处只剩「立档不改」项;P3 择两处高影响做。
> 每处先 grep 定位符号再改。禁止 `git add -A`。

---

## P1-1 AssetRegistry next_id_ 溢出保护【方案已定】

### 事实
`src/core/asset/AssetRegistry.cpp`:
- `createAsset`(第 10-20 行):`const AssetId id(next_id_); ++next_id_;` —— `next_id_` 到 `UINT64_MAX` 时 `++` 回绕到 0(=invalid),下一次发号从 0 开始 → id 复用/无效。
- `registerAsset`(第 38-39 行):`if (id.value() >= next_id_) next_id_ = id.value() + 1;` —— `id.value()==UINT64_MAX` 时 `+1` 回绕 0。
- `next_id_` 类型 `AssetId::ValueType`(见 AssetRegistry.h:51),`0` 是 `AssetId::invalid()`。

### 决策(不可更改)
两处发号前检查是否已耗尽,耗尽则**拒绝**(createAsset 返回 `AssetId::invalid()`;registerAsset 保持已成功插入的记录但不推进 next_id_ 到非法值)。不静默回绕。

### 改动
`createAsset` 开头加:
```cpp
if (next_id_ == 0 || next_id_ == (std::numeric_limits<AssetId::ValueType>::max)()) {
    // id space exhausted (or already wrapped): refuse rather than reissue 0/dup.
    return AssetId::invalid();
}
```
> 需要 `#include <limits>`。`next_id_==0` 兜住「曾被回绕」;`==max` 兜住「这次 ++ 会回绕」。
> 若 `AssetId::invalid()` 不可直接构造,用工程内既有的构造 invalid 的方式(先 grep `AssetId::invalid` 定义)。

`registerAsset` 的 `if (id.value() >= next_id_)` 分支改为:
```cpp
if (id.value() >= next_id_) {
    if (id.value() == (std::numeric_limits<AssetId::ValueType>::max)()) {
        // keep next_id_ unchanged; the +1 would wrap to the invalid id 0.
        // this asset is registered, but no further auto-id can be minted.
    } else {
        next_id_ = id.value() + 1;
    }
}
```

### 测试
`tests/core/test_asset_registry.cpp` 加用例:`registerAsset` 一个 value==UINT64_MAX 的 id 成功后,
`createAsset` 返回 invalid(不返回 0/不复用)。

---

## P1-2 mmap reinterpret_cast 对齐断言【方案已定,纯防御加固】

### 事实(已 grep 核实,范围仅限 MappedGeometrySource.cpp)
`src/io/source/MappedGeometrySource.cpp` 把 mmap base `reinterpret_cast` 成
`Point3*`(182)、`SourceTriangle*`(196)、`int*`(197)、`SourceTet*`(215),共 **4 处**。
> ⚠️ **`MappedVoxelSource.cpp` 不在本项范围**:已 grep 核实它**全文没有任何 `reinterpret_cast`**,
> 只按字节访问(`static_cast<const std::uint8_t*>(mapped_.base)` + 偏移 + `memcpy`,:174/:229),`alignof(uint8_t)==1`,
> 加对齐断言恒真无意义。**不要动 MappedVoxelSource.cpp,不要往里塞断言,不要把它列进 commit**。

### 决策(不可更改)
在 MappedGeometrySource.cpp 的 **4 处** reinterpret_cast 前各加 `assert(base % alignof(T) == 0)`
(release 下 assert 被删,零开销;调试构建捕获未来「多 blob 拼一 mapping」引入的错位)。**不改运行时行为,不加 release 分支**。

### 改动(4 处一致)
以 `acquire_points` 为例:
```cpp
static_assert(sizeof(Point3) == 3 * sizeof(double), "Point3 must be packed double[3]");
assert(reinterpret_cast<std::uintptr_t>(src.mapped.base) % alignof(Point3) == 0);
const Point3* data = reinterpret_cast<const Point3*>(src.mapped.base);
```
> 文件顶部确保 `#include <cassert>` 和 `#include <cstdint>`。对 `SourceTriangle`(196)/`int`(197)/`SourceTet`(215)各处照做。
> **若某处 grep 找不到对应 reinterpret_cast(代码已动),按符号重新定位,不要硬塞。**

### 测试
无需新测试(assert 只在 debug 生效,现有 debug 构建的 ctest 若通过即证明当前数据对齐成立)。

---

## P2 设计债 — 仅「立档不改」(合并重复项已在 P0-2 完成)

以下**本任务不动代码**,仅作为后续独立任务的立项依据。执行 agent 遇到勿顺手重构(工程量大、易引入回归):

1. **XQProjectReader.cpp(2790 行)/ XQProjectWriter.cpp(1087 行)monolith**:建议拆
   Parser(token→结构值)/ Validator(不变量)/ Builder(→XQPayload)。**须单独开任务 + 完整回归**。
2. **XQMainWindow(368 行)+ 7 controller God Object**:建议拆 WindowShell / WorkflowCoordinator / ResourceRegistry。
3. **ResidentKey=`pair<AssetId,int>` + 8 个 `kResident*` magic int**(GeometryResourceManager.cpp:21-28):
   建议 `enum class ResidentKind` + `struct ResidentKey{AssetId; ResidentKind;}`。
4. **命令类手动 `inserted_`/`captured_` 标志**(XQSceneCommands.h):建议 `std::optional` 承载状态。
5. **游标令牌解析 50+ 处 `!tok_flag(...)||!tok_keyword(...)` 链**:建议解析器组合器。属 1 的子项,一并做。

> 这 5 项已在本文件立档,**不需要**再单独建文件。后续要做时按此清单逐项开任务。

---

## P3 性能 — 只做两处高影响 voxel 循环【方案已定】

> 其余逐元素循环(polyline SetPoint/InsertCellPoint、pressure InsertNextValue、faceId SetValue)量级小或已预分配,**不改**(见 audit-report 证伪/降级说明)。

### P3-1 影像体 GetScalarPointer 每 voxel 一次 → 基址 + 步进

`src/visualization/XQSceneRenderer.cpp` 约 126 行的三重循环:
```cpp
// before: per-voxel GetScalarPointer(x,y,z) recomputes offset each call
for (int z = 0; z < dimZ; ++z)
 for (int y = 0; y < dimY; ++y)
  for (int x = 0; x < dimX; ++x) {
    float* scalar = static_cast<float*>(image->GetScalarPointer(x, y, z));
    ...
  }
```
改为取一次基址、行主序线性推进:
```cpp
// after: fetch base once, advance linearly (VTK image is contiguous row-major x-fastest)
float* base = static_cast<float*>(image->GetScalarPointer(0, 0, 0));
std::size_t o = 0;
for (int z = 0; z < dimZ; ++z)
 for (int y = 0; y < dimY; ++y)
  for (int x = 0; x < dimX; ++x, ++o) {
    float* scalar = base + o;
    ...   // 其余逻辑不变(useBuffer 分支照旧,只是写到 *scalar)
  }
```
> 前提:该 vtkImageData 单组件、float 标量、无 extent 偏移(SetExtent 从 0 起)。执行前 grep 确认该 image 的
> `SetDimensions`/`SetExtent`/`AllocateScalars(VTK_FLOAT,1)`。若组件数≠1 或 extent 非 0-based,**不要改这处**,在 checklist 标注跳过原因。

### P3-2 分割掩膜 InsertNextPoint 无 reserve → 先数后填

`src/visualization/XQSceneRenderer.cpp` 约 901 行三重循环 `points->InsertNextPoint(...)`:
在循环前先扫一遍数非零 voxel,`points->Allocate` / `SetNumberOfPoints` 预留:
```cpp
// after: pre-count non-zero voxels, allocate once, then fill by SetPoint.
vtkIdType nnz = 0;
for (int z = 0; z < dimZ; ++z)
 for (int y = 0; y < dimY; ++y)
  for (int x = 0; x < dimX; ++x)
    if (mask.labelAt(mask.voxelIndex(x, y, z)) != 0) ++nnz;
points->SetNumberOfPoints(nnz);
vtkIdType pi = 0;
for (int z = 0; z < dimZ; ++z)
 for (int y = 0; y < dimY; ++y)
  for (int x = 0; x < dimX; ++x) {
    const std::size_t index = mask.voxelIndex(x, y, z);
    if (mask.labelAt(index) == 0) continue;
    double world[3] = {double(x), double(y), double(z)};
    if (hasGeometry) voxel_to_world(geometry, x, y, z, world);
    points->SetPoint(pi++, world[0], world[1], world[2]);
  }
```
> 双扫比原来的动态扩容更快(避免多次 realloc)。确认 vtkPoints 用 SetPoint 前需 SetNumberOfPoints(已在上)。

### P3 验收
渲染相关 ctest 全绿(`test_scene_renderer*`)。这两处是纯等价优化,断言输出点数/像素不变即可
(见 memory:渲染快照测试对亚像素不敏感,**这两处改动不改点集,只改填充方式**,快照必然一致)。

---

## P4 测试覆盖 — 补关键缺口【清单已定】

> P0-2/P0-3 的负面测试已在 EXECUTE-P0 要求,这里是**额外**独立补的。

### P4-1 GeometrySourceResolver 无专属测试(必做)
`src/services/resource/GeometrySourceResolver.cpp` 仅集成测试间接覆盖。新建
`tests/services/resource/test_geometry_source_resolver.cpp`(若已存在则补用例——先 grep 确认;
git status 显示该文件已 M,说明存在,**补用例而非新建**),覆盖:
- 正常 resolve 出 surface/tet/both 各分支返回有效 handle;
- 缺 blob / 非法 spec 返回无效 handle(不崩)。
在 CMake 注册对应 test target(参考同目录 GeometryResourceManagerTest 的注册方式)。

### P4-2 服务层失败枚举覆盖(必做)
补以下失败分支的断言(在各自已有的 *Test.cpp 内加用例,不新建文件):
- `FlowController::solve`:除 Ok/InvalidBoundaryConditions 外,覆盖其余 Status 枚举值(先读 FlowController.h 枚举全集)。
- `ModelingService`:`AlreadyClosed` / `InvalidModel` / `NullScene` 各一用例。
- `AiService`:`segment`/`identify`/`predictFlow` 在 backend 返回 null 时映射到 `InvalidInput` 的用例。

### P4-3 立档不强制(不做,仅记录)
- 单测:集成 = 58:6 倒挂,建议补单服务层深度集成(后续任务)。
- `MeshQuality::triangleQuality/tetQuality` 直接单测(目前集成间接覆盖)。
- `XQCommandStack` 并发 undo/redo、`XQScene` 多线程 insert/remove 竞态测试。

### P4 验收
新增/补充测试全部纳入 ctest 且全绿;`GeometrySourceResolver` 有独立可见的测试用例。

---

## P1~P4 commit 划分

```
# commit 4 (P1):fix: AssetRegistry id 溢出保护 + mmap 视图对齐断言
git add src/core/asset/AssetRegistry.cpp tests/core/test_asset_registry.cpp \
        src/io/source/MappedGeometrySource.cpp
# 注意:MappedVoxelSource.cpp 无 reinterpret_cast,不在本 commit(别 add 它,git add 未改文件是 no-op 但会误导)

# commit 5 (P3):refactor: 渲染器 voxel 填充去逐元素虚调用
git add src/visualization/XQSceneRenderer.cpp

# commit 6 (P4):test: 补 GeometrySourceResolver 与服务层失败分支覆盖
git add tests/services/resource/test_geometry_source_resolver.cpp \
        tests/ui/controllers/... tests/services/... CMakeLists.txt
```
> P2 无 commit(全部立档不改)。commit message 中文,类型前缀。**逐条精确 add,禁止 -A。**
