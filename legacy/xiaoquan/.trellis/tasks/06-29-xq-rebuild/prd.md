# XQ 重建主线(路径→分割→建模→网格→流体→AI)

> 父任务 `06-29-xq-rebuild`。多交付物聚合,**每条里程碑落地为独立 trellis 子任务**;父子树非依赖系统,依赖写在子任务 prd。
> 本 prd 是父任务**总纲真相源**;里程碑详版与 brainstorm 可追溯见同目录 `ROADMAP.md`。

## Goal

血管影像建模 + 血流分析桌面应用的重建。M0~M7 主线已端到端跑通并归档(详见各归档子任务 / PROGRESS-archive)。
当前主轴:**从首版前端 → 大规模可视化前处理 + 可复用函数库(libXQ)** 的架构演进。

## Requirements

### 架构主轴(不可动摇的串行顺序)

> 先让**资产身份与存档**正确 → 再让**加载与释放**正确 → 然后让**渲染器依赖**正确 → 最后做**大规模可视化**。

严格串行:**M8a → M8b-1 → M8b-2 → M9a → M9b**。

- **接口先行、驻留后置**:M8b-1 只改"消费者依赖什么"(接口 + 兼容适配器,驻留行为不变);M8b-2 才改"数据由谁持有/何时加载释放"。这是降返工的关键切分。
- 依赖方向不变:`app → services → adapters → io → core`;库优先 / 无治理层 / 外部库当 kernel。
- 目标量级:单 Surface 2000 万三角 / 单 Tet Mesh 1000 万 tet / 单活动资产。

### 里程碑(各落独立子任务)

- **M8a — 资产身份 + 存档持久化**(= 子任务 `06-28-payload-persistence`,**✅ 已完成并归档 2026-06-30**):AssetId / 最小 AssetRegistry / 资产血缘 / Node↔assetId 绑定 / 版本化主档(schema 1.2,**minReader 严格 1.2**)/ 内容寻址二进制 sidecar(`<stem>.assets/blobs/<2>/<sha256>.bin`)/ External、Derived 资产 / 实体级 round-trip。**运行时 payload 与全量 vector 暂不变。**
- **M8b-1 — Source 接口定义**(= 子任务 `06-30-source-interface`,**实现中**):`IVoxelSource` / `IGeometrySource` / `ReadLease` + **批量范围访问**(`acquire_region/slab/points/triangles/tetrahedra`,**不逐元素虚调用**;`acquire_triangles` **必带逐三角 faceId 并行通道**);先给基于现有 vector 的 Memory/Resident 实现 + Handle 兼容适配器。**只解决"消费者依赖什么",不改驻留行为;头文件标 candidate-not-frozen,不冻结 lease 失败语义。**
- **M8b-2 — 运行时内存模型**(未落子任务):`GeometryResourceManager`(放 services)/ 按需整块驻留 / 内存预算 / pin·lease 生命周期(**lease = 对抗并发驱逐的读锁**)/ **SceneNode 去大型 vector 化** / `MappedVoxelSource`(放 io,磁盘后备只读 mmap + **map+verify+span 零拷贝读路径**)。**必须显式决策完整性(整文件 SHA)vs 流式懒映射的取舍。**
- **M9a — 渲染器/服务迁移到 Source**(未落子任务):renderer / 网格 services 从直接收 `XQTriangleSurfaceGeometryHandle` / `XQTetVolumeMeshHandle` 迁移为经 Source + 带生命周期视图取数据。**重写上传核(批量 SoA→VTK/GPU),不是逐元素等价搬运。**(注:flow/solver 不碰几何 handle,**不在迁移范围**。)
- **M9b — 大规模可视化前处理**(未落子任务):LOD / 降采样 / 分块 / 渐进式 GPU 上传 / 裁剪 / 显存预算 / 大网格拾取加速。最终交付。
- **scale-probe — 真规模 + 真最难存储探针**(= 子任务 `06-30-scale-probe`,**冻结门前置,见准则修订**):20M 三角/10M tet 合成负载 + 真 `MappedVoxelSource`(mmap+verify+并发 evict harness)喂过 M8b-1 接口,在最难场景打穿接口形状。**位于 M8b-1 收尾后、libXQ/Source 冻结门之前、M9a 迁移消费者之前。**

### UI 并行工作(M8a~M9 全程可做)

允许并行:MPR 三视图联动、Scene 交互、面板易用性、任务进度、取消、错误提示、现有规模拾取。
**硬约束**:新增 UI **不得**直接依赖 payload vector 或具体 `GeometryHandle`;UI 只经 **application commands + `NodeId` + `AssetId`** 操作状态;大规模渲染消费者**必须等 M8b-1/M9a** 接口冻结后再写。

### libXQ 对外 API 排期

| 阶段 | libXQ 动作 |
|---|---|
| M8a 后 | 只整理内部模块边界,**不冻结**公共 API |
| M8b-1 后 | 形成 Asset / Source API 候选,XQ 主程序内部先行自用验证,仍 **candidate-not-frozen** |
| **冻结门(scale-probe 后、M9a 前)** | 真规模 + 真 `MappedVoxelSource` 探针喂过接口且接口存活后,renderer/services/persistence 均能经该边界工作 → **才冻 libXQ 1.0**;**早于 M9a 迁移消费者** |

> 铁律:**内存所有权与 Source 抽象在「真规模 + 真最难存储」探针下验证前,不提前承诺稳定公共接口。**

## Acceptance Criteria

- [x] M8a 子任务 `06-28-payload-persistence` 完成并归档(实体级 round-trip + 零-VTK + 全量绿,逐条见该子任务 prd §5)。**已完成 2026-06-30。**
- [ ] M8b-1 落为独立子任务并完成(`06-30-source-interface`:Source 接口候选 + 兼容适配器 + faceId 一等通道,驻留行为不变,candidate-not-frozen)。
- [ ] scale-probe 子任务完成(`06-30-scale-probe`:真规模负载 + 真 mmap 探针打穿接口,产出接口形状结论)。
- [ ] libXQ/Source 冻结门通过(探针存活、接口定型),且早于 M9a 迁移消费者。
- [ ] M8b-2 落为独立子任务并完成(ResourceManager 入 services + 去 vector 化 + mmap 入 io + 完整性 vs 流式取舍已决)。
- [ ] M9a 落为独立子任务并完成(renderer/网格 services 迁移 Source,**重写上传核**,渲染能力不退化;flow 不在范围)。
- [ ] M9b 落为独立子任务并完成(大规模可视化前处理交付)。
- [ ] 全程依赖方向 `app→services→adapters→io→core` 不被破坏;io 零 VTK 不回归;Source 契约永不出现 `vtk*`。

## Notes

- M0~M7 已归档,不补建历史子任务。
- 已完成子任务:`06-28-volume-mesh-kernel`(体网格生产 kernel,TetGen PLC + MMG 两段式,可选 adapter 默认 OFF)。
- 详版里程碑说明、现状耦合点、与 brainstorm 决策的可追溯映射见 `ROADMAP.md`。
