# M8b-1 执行计划

> design.md 的可执行落地。纯增量:只新增 `core/source/` 文件 + CMake 测试目标,不改任何现有文件(除 CMakeLists.txt 追加 source 文件与测试目标)。

## 依赖前置

- 串行:M8a(payload-persistence)已归档完成(commit 317a9b4),本任务在其上。
- 当前分支 `feat/m8a-payload-persistence-v2`;实现前确认/切到 M8b-1 工作分支(见步骤 0)。

## 步骤

### 0. 分支准备
- 确认工作分支(若仍在 m8a 分支,新建 `feat/m8b1-source-interface`)。不动用户未提交改动。

### 1. ReadSpan + 视图载荷 + 租约(纯头文件)
- `src/core/source/ReadSpan.h`:`template<class T> ReadSpan`(§2.1)。
- `src/core/source/SourceViews.h`:`VoxelView` / `TriangleView` / `VoxelMeta` / `GeometryMeta`(§2.2、§2.4)。`VoxelView::scalarAt` inline 解码,复用 `XQMemoryImageBufferHandle::scalarSize` 规则(若该函数 static 可直接调;否则在 SourceViews 内置同规则 inline 函数,不改 handle)。
- `src/core/source/ReadLease.h`:`VoxelLease` / `GeometryLease<T>` / `TriangleLease`,move-only,borrow/own 工厂(§2.3)。可能需要少量 `.cpp`(若工厂非平凡)。

### 2. 抽象接口(纯头文件)
- `src/core/source/IVoxelSource.h`、`src/core/source/IGeometrySource.h`(§3)。

### 3. Resident 实现 + Handle 适配器
- `src/core/source/ResidentVoxelSource.{h,cpp}`(§4.1):whole 借用、region/slab 物化、越界 invalid。
- `src/core/source/ResidentSurfaceSource.{h,cpp}`(§4.2):points/triangles 借用、faceId 物化、tet 空。
- `src/core/source/ResidentTetSource.{h,cpp}`(§4.3):points/tets 借用、triangle 空。

### 4. CMake 接入
- `CMakeLists.txt`:把上述 `.cpp` 追加进 `xq_core STATIC` 源列表(§9..32 区块)。
- 新增测试可执行 `test_source_interface`,`target_link_libraries(... PRIVATE xq_core)`,`add_test` 注册(对照现有 `test_image_volume` 等模式)。

### 5. 测试(`tests/core/test_source_interface.cpp`)
逐条 AC 断言(不把副作用调用包进 assert):
- AC1:构造小 surface/tet handle → adapter → acquire_points/triangles/tetrahedra 逐元素 == handle。
- AC2:faceIds.size()==triangles.size() 且逐元素 == `triangleFaceId(i)`(handle 用 tagged addTriangle 制造非零 faceId)。
- AC3:构造已知 bytes 的 voxel handle(多 ScalarType:UInt8/Int16/Float32 各一)→ acquire_whole → 任取 (x,y,z) `scalarAt` == handle。
- AC4:acquire_region(子 extent)尺寸==体积 + 内容逐体素 == whole 对应子区域;越界 extent → view.valid==false。
- AC5:acquire_slab(z) == whole 第 z 平面;越界 z → valid==false。
- AC6:仅调 meta()(不 acquire)→ count/dims/type 正确(分配计数桩或仅断言值,且测试中 meta 路径不触 data span)。
- AC7:acquire 计数桩(包一层计数 source 或在测试里计数 acquire 调用)→ 遍历整块只 acquire 一次。
- AC8:lease move 后原 lease 失效语义;lease 比 adapter 活得久(adapter reset 后 span 仍有效,因 keepalive)。
- AC9:编译期保证(ReadSpan 无非 const 成员;静态断言 `!std::is_assignable` 于 `span[i]` 视情况)。
- AC10:`reinterpret_cast<const double*>(points.data())` 前 3*n 值 == 逐 Point3 展开;`Triangle`/`Tet` 同理对 int。
- AC11:不改现有测试;全量 ctest。

### 6. 构建 + 验收(XQ build recipe，memory: xq-build-recipe)
```bash
# Release 全量构建(vcvars64 + CMAKE_PREFIX_PATH，offscreen)
# 见 memory xq-build-recipe / lf-bat 坑：构建脚本用 CRLF
cmake --build <build-dir> --config Release   # 完整构建，勿用单 --target(memory: ninja-target 假绿)
ctest --test-dir <build-dir> -C Release --output-on-failure
# 期望:全部测试通过(含原有 49 + 新增 source 测试)
```

### 7. 假绿抽查(memory: no-sideeffect-in-assert / ninja-target)
- 篡改 `acquire_region` 的 x-fastest 偏移计算 → 重编(完整 build)→ 跑 → 必须有 region 测试 FAIL → 恢复 → PASS。
- 篡改 faceId 物化下标(如 `i` → `i+1`)→ 必须 AC2 FAIL → 恢复。
- 核对 exe 时间戳真变(memory ninja-target 假绿坑)。

## 验收门(全绿才算完成)
- [ ] Release 全量 ctest 绿(原有测试零回归 + 新增 source 测试全过)。
- [ ] 假绿抽查:两处篡改各触发对应 FAIL,恢复后 PASS,exe 时间戳真变。
- [ ] AC1~AC11 全满足。
- [ ] 仅新增 `core/source/` + CMake 追加;`git diff --stat` 不含任何现有 .cpp/.h 逻辑改动(除 CMakeLists.txt)。

## 回滚点
- 纯增量:任一步失败 `git checkout -- CMakeLists.txt` + 删 `src/core/source/` + `tests/core/test_source_interface.cpp` 即回到 M8a 状态,主线不受影响。

## 验证命令汇总
- 构建:完整 `cmake --build`(Release)
- 测试:`ctest -C Release --output-on-failure`
- 范围核查:`git diff --stat`(确认未动现有逻辑)
