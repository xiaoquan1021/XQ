# 07-02 XQ 全局架构审计与修复(面向整个 XQ,非单分支)

> 来源:2026-07-02 对 XQ 全部 163 个源文件的 10 维度并行审计(分层/依赖逆流/库泄漏/安全/测试覆盖/设计债/性能/CLAUDE.md/文档对齐/综合),
> 所有写入本任务的发现都经过主审逐条对源码人工验证;被证伪的误报单独列在 `audit-report.md` 末尾,**执行 agent 不得按误报改代码**。
>
> 区别于 `07-02-m8b2-critical-fixes`(只针对 feat/m8b2-runtime-memory 分支未提交改动),本任务面向 XQ 整体、跨所有层。

---

## 结论一览

XQ 整体健康度良好:**分层违规为零**(core 无 Qt/VTK/ITK include),外部库类型未泄漏进公开业务 API,
坐标系/单位约定未见偏离。真实问题集中在四类:

| 优先级 | 问题 | 性质 |
|---|---|---|
| **P0-1** | 3 个 adapter PUBLIC 链接 xq_services + include services 头文件(依赖逆流,违背 spec「依赖只能自上而下」) | 架构 |
| **P0-2** | 路径 confinement 纯词法、三处复制,NTFS junction 可把 blob 读/映射重定向到资产目录外(CWE-59);voxel 链路连词法检查都缺(实测 `is_symlink` 挡不住 junction,须 canonical 封闭校验) | 安全 |
| **P0-3** | 恶意 .xqproj 的 path payload 可让 `XQPath::resample` 死循环/内存耗尽(CWE-834/400),且 reader 忽略 resample 返回值 | 安全 |
| **P1** | AssetRegistry next_id_ 溢出回绕(id 复用)、mmap reinterpret_cast 对齐无断言 | 健壮性 |
| **P2** | isConfined/checkedMul 三处重复、Reader 2790 行 monolith、ResidentKey 裸 pair+magic int、命令类手动状态标志 | 设计债 |
| **P3** | 渲染器两处逐 voxel 反模式(GetScalarPointer 每 voxel 一次虚调用、InsertNextPoint 无 reserve) | 性能 |
| **P4** | 测试覆盖缺口:失败路径/返回值枚举/并发/GeometrySourceResolver 无专属测试 | 测试 |

综合风险分:5/100(MONITOR)。无 Blocker/Critical。

## 文档结构

- `audit-report.md` — 全部发现 + 证据(文件:行号)+ **已证伪误报清单(必读)**
- `EXECUTE-P0.md` — P0 三项的可执行指令(便宜 agent 按此执行,含 before/after 与验收命令)
- `EXECUTE-P1-P4.md` — P1~P4 执行指令
- `checklist.md` — 勾选清单

## 执行前置硬门(便宜 agent 第一步必做,不满足就停)

> ⚠️ 审计要改的 `BlobStore.cpp` / `GeometryResourceManager.cpp` / `XQProjectReader.cpp` / `CMakeLists.txt`
> 与另一分支 `feat/m8b2-runtime-memory` 的未提交改动**高度重叠**。若在脏工作树上直接 `git add <file>`,
> 会把别人未提交的改动一起打进审计 commit,违反「别动我没提交的改动」。故:

1. 执行前先 `git status --porcelain`。**若工作树非干净**(有任何 `M`/`??` 属于审计范围文件),**立即停,报告用户**,
   不要自行 stash / commit / 丢弃别人的改动。由用户决定基线。
2. 确认执行基线分支(默认应是 m8b2 收尾提交后的 HEAD;由用户指定)。在其上开独立分支
   `fix/xq-global-audit` 再动手。**不在别人的功能分支上直接改**。
3. 每个 commit 严格 `git add <精确文件列表>`(见各 EXECUTE 的 commit 划分),**绝不 `git add -A` / `git add .`**;
   commit 后 `git show --stat` 核对没有夹带范围外文件。

## 执行顺序与边界

1. **P0-2 与 P2 的重复代码项合并执行**(统一 confinement 工具本身就消除三处复制)。
2. P0-1(接口下沉 core)是结构性移动,单独一个 commit,不与其它修复混。
3. P2 的「Reader/Writer 拆分」「XQMainWindow 拆分」**本任务不执行**,只立档;工程量大,须单独开任务。
4. 每个 P 级一个或多个独立 commit,格式 `类型: 简短描述`(中文)。
5. **git add 一律精确列文件路径,绝不 `git add -A` / `git add .`**。

## 全局验收(每个 phase 完成后必须)

```bash
# 构建配方(已实证,见 memory xq-build-recipe / spec core/build-and-test.md):
# vcvars64 + CMAKE_PREFIX_PATH(非单 Qt6_DIR)+ offscreen ctest
# P0-1 涉及 adapter,必须额外用 -DXQ_ENABLE_TETGEN=ON -DXQ_ENABLE_MMG=ON 构建
# (adapter 默认 OFF,默认构建根本不编译它们,只跑默认 ctest = 假绿)
# MMG 需要 Externals/install/windows-x64/mmg-5.3.9 在 CMAKE_PREFIX_PATH,
# 跑 test_mmg_volume_mesh 需要 mmg3d.dll 在 PATH(CMake 已注入,见 memory ctest-environment-overrides-path)
```

- Release 全量 ctest 全绿(当前基线 24/24;新增测试后按新总数)。
- 增量构建后**核对被测 exe 时间戳真的变了**(memory ninja-target-incremental-fakegreen-trap)。
- 安全修复必须带「篡改后真转红」的负面测试,先验证测试在修复前会红(或修复后注入篡改会红),再算通过。
