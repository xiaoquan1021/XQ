# scale-probe 执行计划(implement.md)

> 丢弃式 spike 的执行步骤。探针全在 `XQ/probe/`,CMake 默认 OFF,绝不动 `src/core/source/` 与现有消费者。

## 依赖前置
- M8b-1(`06-30-source-interface`)已完成并 commit(46a939d):Source 接口已落定,探针喂的就是它。
- 当前分支 `feat/m8b1-source-interface`;探针实现前确认/切到 `spike/scale-probe`(丢弃式,独立分支)。

## 步骤

### 0. 分支
切 `spike/scale-probe`(基于当前)。不动用户未提交改动。

### 1. CMake 开关(R6,先立隔离骨架)
- `CMakeLists.txt`:照抄 ONNX/TETGEN 模式加 `option(XQ_ENABLE_SCALE_PROBE "Build throwaway scale probe" OFF)` + `if(XQ_ENABLE_SCALE_PROBE) ... endif()` 块。
- 块内:`add_executable(scale_probe probe/main.cpp probe/<其余>.cpp)` + `target_link_libraries(scale_probe PRIVATE xq_core xq_io xq_visualization)` + `vtk_module_autoinit(TARGETS scale_probe MODULES ${VTK_LIBRARIES})`。不 add_test。
- 先验证 OFF 档主线 ctest 仍 50/50(空 if 块不影响)。

### 2. 共享 mmap 底座(probe/MmapBlob.{h,cpp})
- `CreateFileW`/`CreateFileMapping(PAGE_READONLY)`/`MapViewOfFile(FILE_MAP_READ)`,RAII 包 {hFile,hMap,base,size},自定义 deleter(`UnmapViewOfFile`+`CloseHandle`×2)。
- 工厂返回 `shared_ptr<const void> keepalive` + base/size。0 字节/非整页边界处理。
- `#include <windows.h>`(注意 memory:本机 Windows,Win32 非 POSIX mmap),链 `psapi`(给 §6 内存测量)。

### 3. 合成负载生成器(R1,probe/SyntheticLoad.{h,cpp})
- 确定性规则网格,两档:小(冒烟 ~1M)、大(20M 三角 / 10M tet)。索引 fit int32。
- **自建流式 blob writer**:`std::ofstream` 分块写 + `Sha256::update` 增量哈希,逐字节复刻 `BlobStore` encode(LE)+ 内容寻址路径 `blobs/<sha[0:2]>/<sha>.bin`,手填 `BufferRef`。角色名/components 严格对齐 writer 约定。
- 不复用 `BlobStore::put`(全量双份内存峰值,大档爆内存)。

### 4. MappedVoxelSource(R2,probe/MappedVoxelSource.{h,cpp})
- 实现 `IVoxelSource` 4 虚函数。`acquire_whole` 零拷贝 `borrow`(view.bytes 指 mmap 基址)。
- `acquire_region/slab`:实现 own 物化版 + 记录 strided-view 缺口。
- 完整性策略 A(全量验证)与 B(分段 Merkle,复用 `Sha256::update`/picosha2)各一实现,可切换。

### 5. MappedGeometrySource(probe/MappedGeometrySource.{h,cpp})
- 实现 `IGeometrySource`。`acquire_points` → `ReadSpan<Point3>(reinterpret_cast<const Point3*>(base),n)` 零拷贝;`acquire_tetrahedra` 同理 `<SourceTet>`;`acquire_triangles` tris 借用 + faceId 物化(实测 80MB/次代价)。

### 6. 并发 evict harness(R3,probe/EvictHarness.{h,cpp})
- 后台线程 + 前台持 lease。实验 A(use_count gating,预期安全)、实验 B(强制 unmap,制造悬空,证明缺并发 pin)。
- B 实验会触发访问违例 —— 在探针里隔离捕获(SEH/独立进程),记录结论不让它崩主流程。

### 7. 基线 + 报告(AC3/AC5,probe/main.cpp + probe/REPORT.md)
- `std::chrono::steady_clock` 三段计时 + `GetProcessMemoryInfo(PeakWorkingSetSize)`。Resident vs mmap 对比。
- VTK 上传:`XQSceneRenderer::addSurface/addVolumeMesh`(声明口径:整链含下游 filter)。
- main() 跑全流程打印数字 → 汇成 `probe/REPORT.md`:四类缺陷各给"能否承载/怎么改签名" + 完整性 A/B 对比。

### 8. 构建 + 跑(memory: xq-build-recipe / lf-bat / cmake-bad-cache)
```bash
# build_probe.bat(CRLF!):vcvars64 + Ninja + Release + CMAKE_PREFIX_PATH + -DXQ_ENABLE_SCALE_PROBE=ON
# 跑前 PATH 加 Externals/install/windows-x64/vtk-9.3.0/bin(否则缺 VTK DLL)
cmake -S . -B build_probe -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="..." -DXQ_ENABLE_SCALE_PROBE=ON
cmake --build build_probe   # 完整构建,别单 --target
./build_probe/scale_probe   # 小档冒烟先,再大档
```

### 9. 主线零污染验证(AC6)
- 关 ON 档,跑默认配置(OFF)全量 ctest:确认仍 50/50,`src/core/source/` 与现有消费者零改动(`git diff --stat` 只含 CMakeLists.txt + probe/ 新增)。

## 验收门
- [ ] AC1 大档 blob 产出 + SHA 校验通过。
- [ ] AC2 Mapped*Source 经接口吐 span 逐元素 == 生成器源数据。
- [ ] AC3 open-time/内存峰值/VTK 上传基线数字成形(Resident vs mmap)。
- [ ] AC4 harness A/B 跑通,明确 lease 并发安全结论 + 需要的签名。
- [ ] AC5 REPORT.md(四类缺陷 R4 + 完整性 R5 对比)产出。
- [ ] AC6 OFF 档主线 ctest 仍 50/50,主线零改动。

## 回滚点
丢弃式:整个 `XQ/probe/` + CMakeLists.txt 的 option 块可一并删除回到 M8b-1 态。探针代码本就可弃,结论(REPORT.md)回灌 M8b-1/M8b-2 后探针使命完成。

## 验证命令
- 构建:`cmake --build build_probe`(Release,ON 档)
- 主线回归:默认 OFF 档 `ctest -C Release`(50/50)
- 范围核查:`git diff --stat`(仅 CMakeLists.txt + probe/)
