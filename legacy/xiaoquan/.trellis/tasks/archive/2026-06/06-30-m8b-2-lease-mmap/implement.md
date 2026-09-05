# M8b-2 执行计划(implement.md)

> design.md 的可执行落地。范围:全栈打通(manager + lease 并发 + 生产 MappedVoxelSource + 分段 Merkle + image 体素链接入)。几何 payload 去 vector 化 + 消费者迁移留 M9a。

## 依赖前置
- M8b-1(46a939d)+ scale-probe(cabe074)已完成。
- 当前分支 `spike/scale-probe`;实现前切回主线起 `feat/m8b2-runtime-memory`(基于 main 或 M8b-1 提交,不带探针弃码)。

## 阶段(有内在依赖,分批 + 每批跑构建)

### 0. 分支
从 M8b-1 提交(46a939d 所在主线)起 `feat/m8b2-runtime-memory`。确认不含 `XQ/probe/`(探针在 spike 分支,弃码,不进生产分支)。

### 1. io 底座:MmapBlob 生产化 + MerkleSidecar(xq_io)
- `src/io/source/MmapBlob.{h,cpp}`:从探针提升(Win32 只读映射 + RAII + shared_ptr keepalive)。头不泄漏 windows.h。
- `src/io/blob/MerkleSidecar.{h,cpp}`:写(段大小 + 逐段 SHA + root,复用 Sha256)、读(读 `.merkle` + 重算 root 比对锚 + 按段惰性校验)。
- CMake:挂进 xq_io 源列表。
- 单测:MerkleSidecar round-trip + 段表 root 校验 + 篡改段检出。

### 2. 生产 MappedVoxelSource(xq_io)
- `src/io/source/MappedVoxelSource.{h,cpp}`:实现 core `IVoxelSource`;acquire_whole 零拷贝 borrow、region/slab own 物化、越界 invalid;完整性走 MerkleSidecar(老档无 sidecar 回退 FullVerify)。绕开 BlobStore::get。
- 单测(AC3/AC4):正确性 + 分段 Merkle 惰性(bytesHashed 随触碰量)+ 防篡改 + 越界 invalid。

### 3. ReadLease 并发契约定稿(core,仅契约文本 + spec,签名不变)
- 核对 `ReadLease.h` 注释 + `spec/XQ/core/source-interface.md`:把 keepalive 从"延寿"强化为"驱逐 gating pin:可驱逐存储 evictor 必须 use_count==1 才回收"。**不改 C++ 签名**(AC5:static_assert/AC8/AC9 仍成立)。
- 跑 M8b-1 全部 source 测试确认零回归(签名未动)。

### 4. GeometryResourceManager(services)
- `src/services/resource/GeometryResourceManager.{h,cpp}`:assetId→ResidentBlock 映射 + LRU + 预算 + mutex;acquireVoxelSource/acquireGeometrySource(只吐 core 抽象);按需驻留 + 缓存命中 + refcount gating LRU evict(仅 use_count==1,acquire/evict 共享锁防 TOCTOU)。
- CMake:`xq_services` 新增 **PRIVATE** link `xq_io`(manager .cpp 构造 MappedVoxelSource,头只暴露 core 类型)。核对方向无环。
- 单测(AC1/AC2):按需驻留/二次命中缓存;预算 LRU + **并发 refcount gating**(后台 evictor + 前台持 lease,活跃块不被驱逐)。并发测试注意 pin-before-evict 同步(memory `evict-harness-pin-before-evict-race`)。

### 5. reader/writer schema 1.3 + open-time 不物化体素(io)
- writer:体素 asset 旁产 `.merkle` + 主档记 root(schema 1.2→1.3)。
- reader:体素链不在 load 期 `store.get` 物化——只注册 assetId+BufferRef+root 到 Registry(几何 rebuild 不动,留 M9a)。老档兼容(无 root → FullVerify;几何路径不变)。
- 单测:schema 1.3 round-trip + 老档(1.2)向后兼容 + open 后体素未物化(AC7)。

### 6. image 体素链接入 manager(全栈打通,services/adapters/visualization)
- `VtkImageAdapter`/image 加载:体素注册为 asset,下游经 manager 取(不再整卷直喂)。
- `SegmentationService`/`AiService`:image 体素入参改为经 manager 取 `IVoxelSource&`/持 lease(seg region-grow 走 acquire_whole view + inline scalarAt)。
- `XQSceneRenderer::addImageSlice`:image 体素从 manager 取 source。**只动 image 这条;surface/volume/mask 不动**。
- 集成测试(AC6):加载→取数→分割/渲染端到端,结果与改造前等价。

### 7. 全量验收 + 假绿抽查
- Release 全量构建 + ctest(原有零回归 + 新增全过)。
- 假绿抽查:篡改某段字节→触碰段 acquire FAIL(AC4);并发 evict 活跃块不被回收(AC2)。exe 时间戳真变(memory ninja-target)。副作用不进 assert(memory no-sideeffect-in-assert)。
- AC8 分层核查:core 无新 load/evict/mmap;io 无 VTK;`git diff` 范围符合。

## 验收门(全绿才完成)
- [ ] AC1~AC8 全满足(见 prd)。
- [ ] Release 全量 ctest 绿(原有零回归 + 新增 io/services/集成测试全过)。
- [ ] 假绿抽查:分段篡改 + 并发驱逐各触发预期行为,恢复后绿,exe 时间戳真变。
- [ ] 分层零违反:`xq_services`→`xq_io` 是唯一新增 link 边(PRIVATE);core/io 无 VTK/evict 污染。
- [ ] M8b-1 接口公共签名零改动(仅 spec 契约文本强化)。

## 回滚点
- 分阶段提交(io 底座 / MappedVoxelSource / manager / schema / 接入各一批),任一批失败可回退该批。
- image 体素链接入(阶段 6)是唯一动现有消费者的批次,风险最高,单独提交、单独验收;失败不影响 manager/io 底座已落地部分。

## 验证命令
- 构建:完整 `cmake --build`(Release,build 配方见 memory xq-build-recipe)
- 测试:`ctest -C Release --output-on-failure`
- 范围核查:`git diff --stat`(确认未动 surface/mesh/seg 几何 payload 与 renderer 几何路径)
