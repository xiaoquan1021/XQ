# PRD — 调研 MMG 体网格能力边界与接入方案

> 任务 id:`06-28-mmg-volume-mesh-research` | 优先级 P1 | package XQ
> **类型:调研(research),非实现**。产出是结论文档 + 选型决策 + 接入方案,不写实现代码。
> 取代作废的 `06-28-tetgen-volume-mesh`(那个误把 TetGen 当 kernel,与 spec 不符)。

## 1. 背景与问题

`VolumeMeshService` 现用质心星形剖分,仅星形域正确,弯曲血管段产翻转/退化四面体。需换生产级体网格 kernel。

**选型已由 spec 框定(不是开放问题):** `architecture/external-libs.md` 明确列 **MMG 5.3.9**(LGPL)
作"重网格"kernel,置于 `adapters/mmg`,与 OCCT 并列"暂留待引入"。**TetGen(AGPL)从未在依赖基线出现**,
不予采用(除非调研证明 MMG 完全不可行且有人拍板改选)。

**但有个未确认的关键前提:** MMG 的主业是**重网格 / 优化 / 各向异性自适应 / level-set 离散**,
不一定能"单独从一个闭合三角表面**初始**生成填充的四面体体网格"。remesher 通常需要一个**已有体网格**作输入。
若如此,光引入 MMG 不够,还要解决"初始体网格从哪来"。本任务就是把这个前提查实,给出可落地的接入方案。

## 2. 范围

### In scope(调研 + 产出文档)

1. **查实 MMG 能力边界**(去官方文档/源码,不凭记忆):见 §4 核查清单。
2. **产出选型 + 接入方案结论文档**(`findings.md`):回答 §4 每个问题,给明确方案(含数据流图)。
3. **据结论建后续实现任务**:把验证过的方案落成新的实现任务的 prd/design/implement。
4. **(可选)最小可行验证**:若能快速装 MMG + 跑一个 demo(命令行,拿一个闭合表面试),
   用实证替代纯文档推断——优先实证。

### Out of scope

- 写 `adapters/mmg` 实现代码(留给结论产出的实现任务)。
- 改 `VolumeMeshService`。
- MMG 的各向异性 / 自适应高级特性(先解决"能不能初始生成 + 保 faceId")。

## 3. 铁律约束(贯穿调研结论,方案必须满足)

1. **kernel 只进 adapter 私有实现**:MMG 类型/头只能在 `adapters/mmg/*.cpp`;
   公开 API 仍只暴露 XQ 类型(`XQTriangleSurfaceGeometryHandle` / `XQTetVolumeMeshHandle` / `ModelFace`)。
2. **service 不直接 link kernel**:方案须保 `xq_services` 纯 C++(走接口注入,仿 M6 AiService Backend 范式),
   或把装配挪到上层。结论文档要明确依赖边界。
3. **faceId 全链路**:边界面 wall/inlet/outlet 标记不能丢——方案必须说明 MMG 流程里 ref/标记怎么保留传回。
4. **降级保留**:星形剖分作 fallback 不删(kernel 不可用时)。
5. **许可合规**:MMG 5.3.9 LGPL,确认链接方式(动态/静态)与项目分发兼容,结论文档记录。

## 4. 核查清单(调研必须逐条回答,带官方出处)

> 每条结论标注来源(MMG 官方文档 URL / 源码文件 / 实测 demo),**不接受"凭印象"**。

- [ ] **Q1 初始生成**:MMG3D 能否直接吃一个**闭合三角表面**(无已有体网格)输出**填充四面体体网格**?
      还是必须先有初始体网格才能 remesh?(查 mmg3d 的输入模式:`-ls` level-set?纯 remesh?有无 "tetrahedralize from surface"?)
- [ ] **Q2 初始填充**:若 Q1=否,初始体网格从哪来?选项:(a) MMG 配套工具;(b) VTK 的 Delaunay3D
      (项目已有 VTK,但质量差);(c) 别的轻量填充器。各自质量/许可/复杂度对比。
- [ ] **Q3 faceId 保留**:MMG 的三角/四面体 **ref(reference marker)**能否承载 faceId,
      经 remesh 后传回输出?边界面标记在重网格中是否守恒?
- [ ] **Q4 API 形态**:MMG 的 C API(`MMG3D_xxx` + `MMG5_pMesh`/`MMG5_pSol`)怎么喂点/面/体、怎么取结果?
      够不够包成"XQ surface → XQ tet mesh"一个函数?
- [ ] **Q5 输入要求**:MMG 对输入表面的要求(闭合?定向一致?无自交?)——
      对照 M3 表面 winding 全局不一致 + 可能自交的现状,要不要预处理?
- [ ] **Q6 装进 Externals**:MMG 5.3.9 怎么编进 `Externals/install/windows-x64/mmg-5.3.9`(MSVC/Ninja),
      进 CMAKE_PREFIX_PATH;有无 Windows 构建坑。
- [ ] **Q7 vs 替代**:综合 Q1~Q6,MMG 是否真比"VTK Delaunay3D 初始 + 简单优化"或其他方案更优?
      给一个带依据的推荐(可能结论是 "MMG 重网格 + X 初始填充" 的组合)。

## 5. 验收标准

- [ ] **AC1 findings.md**:逐条回答 Q1~Q7,每条带官方出处(URL/源码/demo 结果),无"凭印象"。
- [ ] **AC2 方案**:给出可落地的体网格生成数据流(从 XQ 闭合表面到 XQTetVolumeMeshHandle),
      标明 MMG 在其中的角色、初始填充方案、faceId 传递路径、依赖边界(谁 link MMG)。
- [ ] **AC3 实现任务**:据 AC2 建好后续实现任务(prd/design/implement),前提全部已验证、无脑补。
- [ ] **AC4(可选)实证**:若做了 demo,附命令 + 输入输出 + 质量数据。
- [ ] **AC5 合规**:MMG 许可与链接方式结论明确。

## 6. 已知现状(供调研接手)

- spec:`architecture/external-libs.md` — MMG 5.3.9 列"暂留/重网格",`adapters/mmg`;TetGen 不在基线。
- `VolumeMeshService`(`src/services/meshing/`):星形剖分,`Params` 注释预留 "kernel selection";
  `Status` Ok/NotClosed/InvalidSurface/NullScene;要求闭合 2-流形;`transferBoundaryFaces` 搬 faceId 元数据。
- `XQTetVolumeMeshHandle`:`addPoint`/`addTet`/`points()`/`tets()`;`XQTriangleSurfaceGeometryHandle`:
  `points()`/`triangles()`/`triangleFaceId(i)`。
- 项目已有 VTK 9.3.0(含 `vtkDelaunay3D`,Q2 候选初始填充器,但已知质量一般)。
- MMG 当前**未装** Externals。
- 技术债来源:父任务 prd 滚动清单 `[M5+ kernel] TetGen/MMG 未引入`(措辞泛指,非选 TetGen)。

## 7. 方法 / 注意

- **不凭训练记忆下 MMG API 结论**(协作准则:别凭训练数据猜第三方库)。每条去官方文档/源码核。
- 优先实证:能装能跑 demo 就别纯推断。
- 若调研结论是"MMG 不适合单独做体网格生成,需配初始填充器",这是**有价值的负结论**,如实写,不强行选 MMG。
- 调研产物放本任务目录 `findings.md`;实现任务另建,不在本任务写实现代码。
