# M0 骨架 — 技术设计(design)

> 基于对 `XQrebuild/` 实际源码与 `0007_H_AO_H/` 实际结构的核查,而非仅 plan 文字。
> 遵循 `.trellis/spec/XQ/`(架构铁律、分层、坐标系、命令/undo、验收)。

## 现状核查结论(已验证)

- **可搬入(XQrebuild,真实现)**:
  - `src/core/` 14 个类:NodeId · Diagnostics · GeometryTypes · XQImageVolume · XQPath ·
    XQContourGroup · XQSegmentation · XQSurfaceModel · XQMesh · XQSimulationCase · XQProject ·
    XQScene · XQDataNode。
  - `src/io/project/` 5 个:XQProjectReader/Writer · SvProjectReader · PTHPathReader · CTGRContourReader。
  - `src/adapters/vtk/` 3 个:VtkImageAdapter · MDLModelReader · MSHMeshReader。
  - `tests/` **24 个测试**,`CMakeLists.txt` 已用 `add_test` 注册全部 24 个,`find_package` 解析 Qt6 / VTK9 / tinyxml2。
- **确认缺口(M0 必补)**:
  1. `XQDataNode` 当前**只有** `id / domain_type(std::string) / display_name`,**不持 payload、无 `XQDomainType` 枚举**。
  2. **无 `src/core/command/`**——命令栈完全缺失,需新建。
  3. `XQProjectReader/Writer` 是骨架,round-trip 各 payload 留待 M1+ 增量。
- **0007 实际结构**:影像 `Images/OSMSC0090-cm.vti`(单个);`Paths/*.pth`×5;`Segmentations/*.ctgr`×5;
  `Models/0090_0001.{mdl,vtp}`;`Meshes/0090_0001.{msh,vtp,vtu}`;`Simulations/0090_0001.sjb`;
  `flow-files/inflow_1d.flow`、`inflow_3d.flow`。`.svproj` 各 folder 用空标签,靠目录扫描。

## 设计要点

### 1. 工程基底
- 以 XQrebuild 的 `CMakeLists.txt` + `src/` + `tests/` 为基底建新工程(不带 `build_*` 临时目录)。
- 保留 `find_package(Qt6/VTK9/tinyxml2)`;`enable_testing()` + 24 个 `add_test` 原样纳入。
- 新增 core/payload/command 源文件与对应测试追加进 CMake。

### 2. payload 机制(core,零外部依赖)
- 新增 `XQDomainType` 枚举:Image / Path / ContourGroup / SegmentationMask / SurfaceModel / Mesh / SimulationCase。
- 新增 `XQPayload` 基类/handle;`XQDataNode` 增持 `std::shared_ptr<XQPayload>`。
  - **兼容策略**:`XQDataNode` 现有 `domain_type` 是 `std::string`;改造时同步迁移已有引用,
    新增 `XQDomainType domainType()` 访问器;保证 24 个旧测试仍绿(给 interface 加方法须同步所有调用点)。
- `XQScene` 分组:Images/Paths/Segmentations/Models/Meshes/Simulations;`groupForDomain(XQDomainType)` 映射。

### 3. 命令栈(core,新建 `src/core/command/`)
- `XQCommand`:抽象 `execute() / undo() / label()`。
- `XQCommandStack`:push+execute、undo、redo、清空。
- 预置命令:`AddNodeCommand`、`AddNodeWithSourceRelationCommand`、`ReplacePayloadCommand`、`RemoveNodeCommand`。
- 所有 scene 变更经命令栈;service 不直接改 scene(本里程碑无 service,先把命令/undo 基础建好)。

### 4. SvProjectReader → expected-scene
- `SvProjectReader::load(projectDir)` 读 `0007` 成 `XQProject` + `XQScene`,各文件成对应 payload 节点,建 source/derived 关系。
- 需要一个 `expected-scene.json`(节点数/分组/关系的对照基准)作为集成测断言依据。
  - **决策**:本里程碑以 0007 实际结构(上文)生成 expected-scene 基准;具体数值在实现时从读取结果固化并人工核对。

## 风险 / 注意
- payload 改造会触及 `XQDataNode` 的所有调用点(io reader、scene、ui model),须全量改并保持 24 测试绿。
- 验收必须 Release + 全量 `ctest`;有副作用的 reader 调用别包进 `assert`(`/DNDEBUG` 会删,导致假绿段错误)。
- 命令栈是 M1+ 所有 service 的地基,接口须稳定(execute/undo 复原语义、payload 复制保留 id 与来源)。
