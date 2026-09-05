# 任务:存档 payload 实体数据持久化(payload-persistence)

你是 implement worker。当前激活任务 `06-28-payload-persistence`(父任务 `06-29-xq-rebuild`)。
**先读** task 目录下的 `prd.md` + `design.md` + `implement.md`(已通过 jsonl/task 注入),严格按 implement.md 的 Step 0~5 执行。

## 工作目录与可编辑范围
- 工作目录:`C:/Users/OCEAN/Desktop/XIAOQUAN/XQ`(源码在 `src/`,测试在 `tests/`)。
- **可编辑**:
  - `src/io/project/XQProjectWriter.{h,cpp}`
  - `src/io/project/XQProjectReader.{h,cpp}`
  - 新建 `tests/io/test_payload_roundtrip.cpp`
  - 注册测试的 CMakeLists(io 测试所在的 CMakeLists.txt)
- **只读参考**(别改):`src/core/*Payload.h`、`src/core/XQDataNode.h`、各 core 几何/值类型头、现有 io 测试。

## 目标(实体级 round-trip)
让 writer/reader 落盘+读回每类 payload 的**实体数据**,做到 save→load 后 payload 逐字段一致。
schema `1.1 → 1.2`,**向后兼容**:旧 1.1 档(无 payloads 段)仍能 load 成功(payload 空 + 诊断,不 ParseError)。

## 关键现状(我已实测,直接用,别重复摸索)
- **writer**(`XQProjectWriter.cpp`,149 行):纯文本逐行;`encode_field` 做 %HH 编码;
  body 顺序 = header → `scene`(nodes/relations/stale)→ `endScene` → `provenance`(records 1)→ `endProvenance` → `diagnostics 0` → `end`。
  **完全无 payload 实体。** schema 常量 `kSchemaVersion = "1.1"`。
- **reader**(`XQProjectReader.cpp`,618 行):严格逐行 + count 驱动解析;
  - 解析完 `endScene` 后,用 `line_is_single_token(lines, index, "provenance")` peek 决定是否有 provenance 段;
    `is_legacy_v10` 处理 1.0 旧档无 provenance 的情况——**新的 `payloads` 段照此 peek 模式插**:`endScene` 后先 peek `payloads`,有则解析,没有则进 provenance 分支。
  - 末尾强校验 `diagnostics 0` + `end` + `index == lines.size()`,新段插在 `endScene` 与 `provenance` 之间,不破坏末尾校验。
  - reader 现在**只建结构** `XQDataNode(id, domain_type, display_name)`,**没挂任何 payload**。
- **node↔payload 接口**(`XQDataNode.h`):
  - 取:`const std::shared_ptr<XQPayload>& node.payload()`(可能为 null);`node.domainType()` 区分 kind。
  - 设:`node.setPayload(XQDomainType domain, std::shared_ptr<XQPayload> payload)`。
  - reader 重建 payload 后,需要找到对应 node 挂回——注意现 reader 是先收集 ParsedNode 再 `scene().insert(...)` 建 node,你要在建完 node 后按 nodeId 找 node 挂 payload(看 `XQScene` 是否有 `find`/可变访问;reader 里已用 `parsed.scene().find(it->id)`)。
- **几何 handle**(纯 XQ 值类型,零 VTK,直接序列化):
  - `XQTriangleSurfaceGeometryHandle`:`pointCount()`/`triangleCount()`、`point(i)`→`Point3`、`triangle(i)`→`array<int,3>`、`triangleFaceId(i)`。
  - `XQTetVolumeMeshHandle`:`pointCount()`/`tetCount()`、`point(i)`、`tet(i)`→`array<int,4>`。
  - payload 包装:`XQSurfaceModelPayload::model()`→`XQSurfaceModel`(`hasTriangleGeometry()`/`triangleGeometry()`);`XQMeshPayload::mesh()`→`XQMesh`(`hasSurfaceTriangles()`/`surfaceTriangles()`、`hasVolumeTets()`/`volumeTets()`);`XQSimulationCasePayload::simulationCase()`→`XQSimulationCase`(纯值对象)。

## Step 1 必做(写代码前):字段清单
按 implement.md Step 1,逐个读 8 类 payload 对应的 core 类型头(`XQImageVolume`/`XQSurfaceModel`/`XQMesh`/`XQSimulationCase`/`XQFlowResultPayload`/`XQAiAnalysisPayload`/`XQPathPayload`/`XQSegmentationMaskPayload`),
列出每类**要落盘的字段 + 访问 getter**,标明哪些进主档文本、哪些走 sidecar(影像/掩膜 buffer)。把清单追加写进 task 目录的 `implement.md`(或新建 `field-spec.md`)再动手。确认 simCase 的 RCR/波形/RomSettings/FluidProperties 访问接口。

## 铁律(违反即 BLOCKER)
1. **io 零 VTK**:序列化只从 XQ 自有类型取数据;`rg "#include.*vtk" src/io` 必须空;`xq_io` link 不新增 VTK。
2. **向后兼容**:旧 1.1 档(无 payloads 段)load 成功,payload 空 + 发诊断,**绝不 ParseError**。
3. **坐标系/单位不静默假定**:影像 geometry 的 coordinateSystem(LPS)/spacing/origin/direction 缺失时**发诊断**,不默认填值。
4. **round-trip 无损**:double 用 `setprecision(17)` 或走二进制 sidecar(影像/掩膜 buffer);点坐标/参数 load 后逐字段等值。
5. **不破坏现有结构序列化**:`scene`/`relations`/`stale`/`provenance` 逻辑不动,原测试要绿。
6. **副作用不进 assert**:测试里 `reader::load` 等返回 Status 的调用**先取变量再判断**,绝不写进 assert(Release /DNDEBUG 会删 assert → 假绿 segfault)。

## sidecar(design §2)
影像/掩膜 buffer 写同目录 `.bin`,主档记 `bufferRef <relpath> <byteCount> <checksum>`;读时按 relpath 读、byteCount/checksum 校验,不符 → 诊断 + buffer 空(不崩)。路径用相对(相对主档目录)。

## 测试(Step 4)
新建 `tests/io/test_payload_roundtrip.cpp`(link `xq_io`,无需 Qt):
- 构造含各 payload 的小 scene(影像 4×4×2、表面 4 点 4 三角带 faceId、体网格几个 tet、simCase 带 RCR/波形/fluid)→ save 到 temp → load → 逐字段断言一致(点/三角/tet 数与内容、buffer、faceId、simCase 参数、影像 geometry)。
- 向后兼容:手写一个 1.1 档(无 payloads)→ load `Status::Ok` + payload 空 + 有诊断。
- 坐标系缺失档 → load 发诊断。
- CMake 注册 `test_payload_roundtrip` + `add_test`。
- `reader::load` 先取变量再判断,绝不进 assert。

## 验证(你跑,绿了再回报)
- 构建配方见 `.trellis/spec/XQ/core/build-and-test.md`(vcvars64 + CMAKE_PREFIX_PATH + offscreen ctest)。git-bash 调 .bat 用 `cmd //c "绝对路径"`;ctest 前 `set QT_QPA_PLATFORM=offscreen`。
- 至少跑 `ctest -R test_payload_roundtrip` + 三个原结构 round-trip 测试(`test_new_domain_roundtrip` / `test_project_roundtrip` / `test_project_versioned_save`)绿。
- `rg "#include.*vtk" src/io` 确认空。
- **全新 build 目录 + 全量 ctest 假绿抽查由主会话做,你不必做**,但你要保证你跑过的测试真绿(核对 exe 时间戳真变,别信增量假绿)。

## 禁止
- 不 git commit。
- 不改可编辑范围外的文件。
- 不 mock/跳过测试当通过。
- 不凭记忆猜 core 类型 API——读头确认。

## 回报
做完回报:改了哪些文件、schema 版本、各 payload 的序列化布局、sidecar 方案、你跑过哪些测试的真实结果(贴关键输出)、Step 1 字段清单写在哪。遇到设计取舍卡点(如某 payload 字段无 getter)先停下问主会话,别擅自扩 core API。
