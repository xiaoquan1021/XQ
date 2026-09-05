# M8b-2 运行时内存模型(资源管理器 + lease 并发原语 + mmap 后备 + 完整性)

## Goal

把"数据由谁持有、何时加载、何时释放"这一层立起来:实现 `GeometryResourceManager`(services)按 assetId 管理几何/体素数据的**按需驻留 + 内存预算 + refcount 驱动驱逐**,落地**生产级 `MappedVoxelSource`**(xq_io,Win32 mmap + 分段 Merkle 完整性 + map→verify→reinterpret 零拷贝读路径),并把 **M8b-1 `ReadLease` 的并发语义定稿**(从单线程 move-only RAII 升级为对抗并发驱逐的协作式读锁)。

**全栈打通验证**:用当前最大、且游离在 scene payload 之外的 **image 体素链**(adapter 一次性 decode 整卷驻留 → service/renderer 经引用消费)作为 manager 的首个真实接入——改为经 manager 按 assetId 懒映射,端到端跑通(加载 → 经 `IVoxelSource` 取数 → 渲染/分割消费),证明 manager 的真实价值。

这是 scale-probe 探针打穿四类缺陷后的收口里程碑:**冻结门(libXQ/Source 1.0)在本里程碑完成后才到**。

## Background(探针已备齐的决策依据)

scale-probe(commit cabe074,`XQ/probe/REPORT.md`)实测结论,本里程碑直接采纳:
- **完整性**:large(128MiB)下分段 Merkle vs 全量验证 = **128× 惰性差距**(5.8ms/1MiB vs 805ms/128MiB)→ 分段 Merkle 必选。
- **lease 并发**:协作式 use_count gating 实测 SAFE;强制 unmap → borrow view 悬空 AV。→ 选**协作式**(REPORT §1③选项 a),lease 公共签名可不变,只强化契约 + manager 侧 gating。
- **零拷贝读路径**:mmap 基址直接 `ReadSpan<uint8_t>` / `reinterpret_cast<Point3*>`,绕开 `BlobStore::get`(两遍全量)。MmapBlob 的 shared_ptr+RAII 拆除顺序可提升为生产。
- **AoS 不改**:reinterpret 零拷贝成立,瓶颈在 VTK 上传(M9a/M9b),与 Source 形状无关。

## Scope

### In scope
- **GeometryResourceManager**(services):assetId → 经 AssetRegistry 取 BufferRef → 构造/缓存 Source → 按内存预算 LRU + refcount gating 驱逐。面向 `IVoxelSource`/`IGeometrySource` 抽象编程(resident 与 mmap 后备无差别)。
- **生产级 MappedVoxelSource**(xq_io):Win32 mmap 只读映射 + 分段 Merkle 完整性 + map→verify→reinterpret 零拷贝,实现 `IVoxelSource`。
- **分段 Merkle 完整性**:段哈希表持久化(段表存储方案见 design 决策),复用 `Sha256`/picosha2,按 acquire 触碰范围惰性校验。
- **ReadLease 并发语义定稿**:协作式 use_count gating;契约文本从"延寿"强化为"驱逐 gating pin,可驱逐存储的 evictor 必须 use_count==1 才回收";公共 C++ 签名冻结不变;Resident*Source 不改。spec 更新。
- **内存预算 + LRU evict**:驻留字节累加 vs 上限,超限 honor-refcount LRU 扫描。

### Out of scope(明确划归 M9a/后续)
- **真实体素/几何链接入 manager(全栈打通消费者)→ M9a**。
  - **2026-06-30 范围修订(实现期查实)**:原 prd 把"image 源体素链接入 manager"列为本里程碑的全栈验证目标,**该前提不成立**:源 image 体素由 `VtkImageAdapter::loadVtiWithBuffer` 直接解码 `.vti`(`VtkImageAdapter.cpp:156`),**从未存成 M8a content-addressed blob**,且仅在测试中被调用、不走 project reader——manager 映射的是 M8a blob,这条链无 blob 可映射,"open-time 不物化"无对象可证。
  - reader 真正在 load 期物化的 M8a 体素 blob 是**分割 mask 体素**(`XQProjectReader.cpp:1271-1279`),但拆 `XQSegmentationMask` 的 voxels 值成员属动 core payload 结构 = M9a 性质(消费者迁移)。
  - 故"某条体素链全栈接入"整体移交 M9a(与消费者迁移同性、同批做)。M8b-2 交付经验证的**机制层**,冻结门只需"真 mmap + 并发 evict 喂过接口且接口存活"——已由 manager + 生产 MappedVoxelSource + 并发测试达成。
- surface/mesh/seg-mask payload 去 vector 化与消费者迁移(renderer/writer/meshing)→ M9a。
- renderer VTK 上传核重写、LOD/分块/渐进上传、faceId 借用 span、region strided-view → M9a/M9b。
- 不复用 `XQ/probe/` 弃码。

## Constraints
- **分层**(ROADMAP 锁死,唯一无坑维度):MappedVoxelSource→xq_io、GeometryResourceManager→services、**绝不进 core**;Source 契约锁死裸 typed span,io 永不需要 VTK。新增 `xq_services → xq_io` link 边(方向合法,无环)。
- **依赖倒置**:io 实现 core 的 IVoxelSource 接口、services 消费 IVoxelSource* 抽象;AssetRegistry(core)只管身份/描述/血缘,manager(services)填 load/evict/budget 边界,manager 持/查 Registry 不反向。
- **lease 并发**:协作式 gating——manager 持有与 lease keepalive 同一 shared_ptr;acquire 与 evict 决策共享锁避免 TOCTOU;keepalive 控制块所有权收敛到 manager 独占基线(否则 use_count 永 >1 无法驱逐)。
- **零回归**:M8b-1 接口实现与 Resident*Source 不改语义;现有 Release 全量 ctest 仍绿(50/50 起步,新增本里程碑测试)。
- 验收按铁律:Release + 全量 ctest + 假绿抽查;副作用调用不进 assert。

## Acceptance Criteria
- [ ] **AC1 manager 按需驻留**:`GeometryResourceManager` 以 assetId 为 key,首次 acquire 时经 AssetRegistry→BufferRef 构造 Source 并驻留,二次命中缓存不重复映射(可计数验证)。
- [ ] **AC2 内存预算 + refcount evict**:驻留字节超预算触发 LRU 驱逐;**只驱逐 use_count==1 的块**,活跃 lease 持有的块不被驱逐(并发测试覆盖)。
- [ ] **AC3 生产 MappedVoxelSource 正确性**:mmap 一个 M8a 体素 blob,经 `IVoxelSource` 取 span 逐元素 == 源;越界 region/slab 返回 invalid lease。
- [ ] **AC4 分段 Merkle 惰性 + 防篡改**:acquire 局部只校验触碰段(bytesHashed 随触碰量而非 blob 大小);篡改 blob 某段 → 触碰该段的 acquire 返回 invalid lease(完整性失败),未触碰段不报错。
- [ ] **AC5 lease 并发契约**:协作式 gating 下,后台 evictor 与前台持 lease 并发,前台数据全程有效;契约文本(spec)更新为驱逐 gating pin。M8b-1 lease/接口公共签名零改动(static_assert 仍成立)。
- [ ] **AC6 manager 全栈机制就绪**:GeometryResourceManager 经真实 `AssetRegistry`(含 voxels BufferRef)+ 真 `MappedVoxelSource`(map→verify→reinterpret)端到端服务一次 acquire→取 span→数据 == 源,证明 manager+io+core 三层贯通可用(消费者迁移到此机制是 M9a)。
- [ ] **AC7 ~~open-time 不物化~~（移交 M9a)**:原"image 体素 load 期不物化"基于假前提(image 源体素未存为 M8a blob、不走 reader)作废;真实的"reader 推迟物化 + 消费者经 manager 取"随 M9a 消费者迁移做。本里程碑不承诺。
- [ ] **AC8 分层零违反**:manager 在 services、MappedVoxelSource 在 xq_io、core 无新增 load/evict/mmap;io 无 VTK 依赖。Release 全量 ctest 绿。

## Notes
- 复杂里程碑,`task.py start` 前补 design.md(架构 + 三个决策:段表存储 / lease gating 形态 / 体素链接入点)与 implement.md。
- 依赖:M8b-1(46a939d)+ scale-probe(cabe074)已完成。探针代码(`XQ/probe/`)是弃码参考,生产实现重写在 xq_io/services,不复用 probe 目录。
- 关联 memory:`m8a-wholefile-sha-vs-streaming`、`evict-harness-pin-before-evict-race`、`xq-build-recipe`。
- 完成后即冻结门:libXQ/Source 1.0 候选接口在本里程碑验证存活后冻结,早于 M9a 消费者迁移。
