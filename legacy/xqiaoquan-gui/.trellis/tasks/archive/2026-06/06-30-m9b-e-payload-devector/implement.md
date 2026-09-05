# M9b-E 实现计划 — 分阶段 + 验收门 + 回滚点

> 顺序原则:**core(payload 字段 + clone,独立可测)→ io(reader opt-in,回归基线护体)→ services(解析助手 + 等价测试,惰性链打通)→ 消费者打通 + 全量验收**。每阶段独立编译 + 跑相关测试绿后再进下一阶段;全部完成跑 Release 全量 ctest。

## 前置(阶段 0)
- **依赖门**:确认 M9b-A(`MappedGeometrySource` + `GeometryResourceManager::acquireGeometrySource` + `GeometrySourceSpec` + borrow-faceId)**已合入**;读 A 的 design/头,**敲定 spec 入参形态**(决策 3:计数从 registry 推导 vs payload 携带、role 选择)。A 未合入不开工。
- 确认当前分支 Release 全量 ctest 绿(基线计数记录)。
- 对 round-trip / surface / mesh 载入留迁移前基线(payload 元素 dump 或现有断言)。
- **回滚点 R0**:基线 commit 前干净树。

## 阶段 1:payload assetId 字段 + clone(core)
**目标 AC1/AC2。** 独立于 reader/manager,先做。

1. `XQSurfaceModelPayload.h`:加成员 `AssetId geometryAsset_; bool hasGeometryAsset_=false;` + `setGeometryAssetId(const AssetId&)` / `hasGeometryAssetId()` / `geometryAssetId()`(与 `XQDataNode` 同款,include `core/asset/AssetId.h`)。`clone()`(:36-46)在末尾按"持 assetId 则拷 assetId 引用"补一行(决策 4 示意),深拷 handle 分支不动。
2. `XQMeshPayload.h`:同法(:37-50);assetId 覆盖整 payload 几何来源(surf + vol)。
3. **既有构造/`model()`/`mesh()`/`domainType()` 签名零改动**;既有 payload 测试不改即过。
4. 新增/扩充 payload 测试:① 持 handle clone 深拷不共享(沿用现有断言);② 持 assetId 无 handle clone 只拷 assetId、`hasTriangleGeometry()/hasVolumeTets()==false`、无几何分配。

**阶段 1 验收**:payload 单测绿;既有依赖 payload 的测试不改即过。
**回滚点 R1**:阶段 1 commit。

## 阶段 2:reader 惰性 opt-in(io)
**目标 AC3/AC5/AC6。** io 不引 services。

1. `XQProjectReader.h`:加 `struct XQProjectReadOptions { bool lazyGeometry = false; };` + `static Status load(path, out, const XQProjectReadOptions&)` 重载;原 `load(path, out)` 转发 `{}`(默认 eager,所有现有调用方零改)。
2. `XQProjectReader.cpp` rebuild 阶段(:2473-2508)把 options 透传进 `rebuild_payload`/`rebuild_surface`/`rebuild_mesh`(加 `bool lazyGeometry` 入参)。
3. `rebuild_surface`(:1377-1384):`hasTri && lazyGeometry && 该 asset 绑定节点` → **不调** `rebuild_triangle_geometry`(:1379),改 `payload->setGeometryAssetId(asset.id)`、几何 handle 留空;否则现行整块 load。`rebuild_mesh`(:1502-1540):surf 分支 + tet 分支(:1517-1539)同法。**asset.id 来源**:rebuild 循环 `it->id`(:2473-2481),或经 node_by_asset(:2461-2466)拿绑定;assetId 透传进 rebuild。
4. **不改文本段解析顺序**(R3):blob 行照常解析进 registry(:2417-2421),惰性只跳过 `store.get` 物化,不动 peek/计数(memory `xq-section-header-peek-needs-first-token`)。
5. 测试:lazy 载入后断言 payload `hasGeometryAssetId()==true` 且几何 handle 空、几何 blob 未整块解码(get 计数/探针);`lazyGeometry==false` 与老归档(无 assets 段)round-trip 字节一致。

**阶段 2 验收**:`test_project_roundtrip` / `test_project_versioned_save` 默认档不退化;新增 lazy 载入测试绿;io 目录无新增 services/vtk include。
**回滚点 R2**:阶段 2 commit。

## 阶段 3:services 惰性解析助手 + 等价测试(services)
**目标 AC4。** 依赖 A 的 `acquireGeometrySource`。

1. services 新增 `acquireGeometrySource(const XQPayload&, GeometryResourceManager&, ...)`(自由函数或合适宿主):持 handle → wrap `ResidentSurfaceSource`/`ResidentTetSource`(M9a 路径);持 `geometryAssetId` → `manager.acquireGeometrySource(id, spec)`(spec 按 A 形态,决策 3)。返回统一的几何源句柄(A 的 `GeometrySourceHandle` 或等价,带 pin)。
2. 等价测试(services,可见 manager + io):同一工程 eager 载出 handle、lazy 载 + 经助手取源,`acquire_points()/acquire_triangles()/acquire_tetrahedra()` span 逐元素对比全等(points/tris/faceId/tets)。**副作用 `acquire_*`/`get` 取出后再断言**(R4)。
3. 假绿抽查:篡改一处 blob 字节 → 等价断言变红。

**阶段 3 验收**:等价测试绿;假绿抽查通过。
**回滚点 R3**:阶段 3 commit。

## 阶段 4:消费者打通 + 全量验收
**目标 AC7/AC8 + 一处消费者。**

1. **消费者打通**:`XQMainWindow.cpp:275-292` 渲染 dispatch 渐进迁——持 handle 照旧 `addSurface(*model.triangleGeometry())`;持 assetId 经助手取 `IGeometrySource`。**若 A 的 renderer 吃源入口未就绪**,本步在 services 层证明取源正确即可,渲染接入留 Notes follow-up(不阻塞 E 验收)。
2. **AC7 分层**:grep io/core 无 `vtk`/`services` 新增 include;Source 头 static_assert 编译通过;link 边方向核对无环。
3. **AC8 Release 全量 ctest**(`build_m8b2.bat` 同款配方:vcvars64 + CMAKE_PREFIX_PATH + offscreen):全绿,新增测试计入。
4. **假绿抽查**:篡改源/blob 一处确认对应断言变红(`acquire_*`/`get` 不进 assert)。

## 验证命令
```bash
# Release 配置 + 构建 + 全量 ctest(CRLF .bat,比照 build_m8b2.bat 复制改 build 目录名)
cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\build_m9be.bat"
# reader 惰性档不整块物化核查(确认 lazy 路径不调 rebuild_triangle_geometry 的 get)
rg -n 'setGeometryAssetId|lazyGeometry|rebuild_triangle_geometry' XQ/src/io/project/XQProjectReader.cpp
# 分层核查:io/core 不得新增 services / vtk
rg -n 'services/|vtk' XQ/src/io XQ/src/core --glob '!*build*'
# Source 签名零改动核查
rg -n 'virtual .* acquire_|static_assert' XQ/src/core/source/IGeometrySource.h XQ/src/core/source/ReadLease.h
```
（build_m9be.bat 比照 build_m8b2.bat:CRLF 换行,run exe 需 vtk-9.3.0/bin 在 PATH;memory `lf-bat-fake-ninja-not-found` / `cmake-bad-cache-ninja-loop` / `ninja-target-incremental-fakegreen-trap`——增量后核对 exe 时间戳真变。）

## 回滚策略
- 各阶段独立 commit(R1/R2/R3),任一阶段验收失败 `git reset --hard` 回上一回滚点(destructive,需用户确认)。
- 阶段解耦:阶段 1(core payload)独立可用、不依赖 A;阶段 2(reader)只增 opt-in、默认档不退化可单独回滚;阶段 3(services 解析)依赖 A,回滚不影响 1/2。

## 子代理派发(可选)
- 阶段 0 读 A 的 design/头敲定 spec、阶段 2 定位所有 `XQProjectReader::load` 调用方,可派子代理扫;实现改动主会话亲自做(跨层 ripple + 连续编译验证)。

## 依赖与关联
- **依赖**:M9b-A(`MappedGeometrySource` + `acquireGeometrySource` + `GeometrySourceSpec` + borrow-faceId)必须先合入。M8a/M8b-1/M8b-2/M9a 已归档。
- 关联 memory:`m8a-wholefile-sha-vs-streaming`(根由)、`m8b1-consumer-access-patterns`、`no-sideeffect-in-assert`、`xq-section-header-peek-needs-first-token`、`xq-build-recipe`、`lf-bat-fake-ninja-not-found`、`cmake-bad-cache-ninja-loop`、`ninja-target-incremental-fakegreen-trap`、`commit-check-gitignore-present`。
- 完成后:线2(reader 常驻 + 瞬时双份)在 opt-in 下打掉;父里程碑改动面最大一项收口,A/B/C/E 全部交付。
