# 进度交接 — 06-28-mmg-volume-mesh-research

> 调研任务(非实现)。**任务尚未完成(未归档、未 finish-work)。** 下个会话先读本文件接上进度。
> 主产物:同目录 `findings.md`。产出的实现任务:`../06-28-volume-mesh-kernel/`。

## 已完成内容

- **调研结论文档 `findings.md`**:逐条回答 PRD §4 的 Q1~Q7,每条带出处(本地头文件 / 实跑 demo / 官方文档)。AC1/AC2/AC5 满足。
- **实证 demo(主会话亲跑核实,非只信 subagent)**:用本机 `mmg3d_O3.exe` 实跑,产物在 `../../workspace/ocean/mmg-demo/`。
  - 纯表面 `cube.mesh` 喂 mmg3d → `** MISSING DATA. ... contains points and tetrahedra. Exit program.`(Q1 铁证)。
  - 带初始体 `cube_vol`/`bend_vol`/`bend_vol2` → 正常 remesh,验证 Q3 faceId(ref)守恒条件。
- **产出后续实现任务 `06-28-volume-mesh-kernel`**(prd.md + design.md + implement.md,全基于已验证方案,无脑补)。AC3 满足。
- **交叉核对修正**:读 subagent 留下的生成脚本 diff,修正了 faceId 守恒的真因(见下「关键技术决定」),已同步到 findings/design/memory 三处。
- **经验沉淀**:memory `mmg-cannot-tetrahedralize-from-surface.md` + 更新 MEMORY.md 索引。

## 未完成内容

- **AC4(可选实证)在 findings 里标了限度**,本任务范围内已够;但实现任务需补的实测未做(属下个任务):
  - `vtkDelaunay3D` 在弯曲血管腔是否产域外 tet、`Alpha`/裁剪策略 —— **未实证**(列为实现任务阶段 0)。
  - 含翻转 tet / 自交输入喂 MMG 的行为 —— 未实证。
- **LGPL 动态链接的法务终判** —— 未做(常规理解安全,findings 已标限度)。
- **本调研任务本身**:未归档、未 finish-work(按要求保留)。
- **未提交任何改动**(等用户指示;见「当前 git 状态」)。

## 关键技术决定

1. **MMG 不能单独从闭合表面初始建体** —— 它是纯 remesher,输入必须含四面体(`MMG3D_Set_meshSize` 的 `ne` 必须 >0)。
   故采**两段式**:① VTK `vtkDelaunay3D` 初始填充(零新依赖)② MMG3D 可选 remesh 提质量。
2. **初始填充器选 VTK `vtkDelaunay3D`**:VTK 已在依赖基线(BSD,零新增),其质量缺陷由 MMG 二阶段弥补。
   排除 TetGen(AGPL,spec 已否决)、排除"MMG 配套工具"(不存在)。
3. **faceId 守恒真因(交叉核对后修正)**:不是"声明 required",而是**喂 MMG 的边界三角必须与体网格实际边界面精确吻合**
   (= 从 tet 边界面导出,被一个 tet 独占的面),否则 MMG 重建边界对不上号 → ref 丢失(实证 bend_vol v1 即便 -nosurf 仍丢;
   v2「边界三角直接从 tet 边界面导出」后守恒)。叠加 `MMG3D_IPARAM_nosurf=1` 作保险。
4. **依赖边界**:`xq_services` 经 `ITetMesher` 纯虚接口注入,**不 link kernel**(仿 M6 AiService Backend / 作废 tetgen design 方案 A);
   `adapters/{vtk,mmg}` PRIVATE link VTK/MMG;装配在 controller/app 层。星形剖分作 fallback 不删。
5. **CMake**:`option(XQ_ENABLE_MMG OFF)` 仿 `XQ_ENABLE_ONNX`;MMG 无 CMake config → `find_library(mmg3d)` + include 手动。
6. **合规**:MMG = LGPL,用 **dll 动态链接**(本机有 mmg3d.dll);install 未带 LICENSE,实现任务需补 LGPL 文本入库。
7. **推翻 PRD §6 旧事实**:MMG **已装** `Externals/install/windows-x64/mmg-5.3.9`(头/lib/dll/exe 齐);
   注意:目录名 5.3.9 但二进制自报 **5.3.8**,写 lock 按实际核。

## 修改过的文件(本次会话)

新增(未跟踪):
- `.trellis/tasks/06-28-mmg-volume-mesh-research/findings.md`(主产物)
- `.trellis/tasks/06-28-mmg-volume-mesh-research/PROGRESS.md`(本文件)
- `.trellis/tasks/06-28-volume-mesh-kernel/`(新实现任务:task.json + prd.md + design.md + implement.md + check/implement.jsonl)
- `.trellis/workspace/ocean/mmg-demo/`(实证产物:cube/cube_vol/bend_vol/bend_vol2 的 .mesh 输入输出 + gen_*.py + check_refs.py)

memory(项目 memory 目录,Trellis 外):
- 新增 `mmg-cannot-tetrahedralize-from-surface.md`;更新 `MEMORY.md` 索引。

**未改动任何 C++ 源码 / CMake / 测试**(纯调研任务)。

## 当前 git 状态

- 分支:`master`,顶层 `C:/Users/OCEAN/Desktop/XIAOQUAN`。
- `git status --short`:
  - `?? .trellis/tasks/06-28-mmg-volume-mesh-research/findings.md`
  - `?? .trellis/tasks/06-28-volume-mesh-kernel/`
  - `?? .trellis/workspace/ocean/mmg-demo/`
  - `?? CLAUDE.md`(根目录,**上次会话遗留的未跟踪文件,非本次产生**,提交前先确认归属)
- 全部**未提交**,等用户指示。
- 最近 commit:`4903bc3 docs: 规划 A4 存档持久化 + MMG 体网格调研任务,作废 TetGen 任务`。

## 测试结果

- **未运行 ctest**:本次是纯调研任务,未改动任何 C++ 源码/CMake/测试,无可测对象。基线仍为已知的 44/44(上次会话状态),本次未触碰。
- **实证 demo 结果(等价"测试")**:mmg3d 命令行实跑,结论见 findings.md 各 Q + `../../workspace/ocean/mmg-demo/`。可复跑:
  把 `Externals/install/windows-x64/mmg-5.3.9/bin` 加 PATH → `mmg3d_O3.exe <in>.mesh <out>.mesh`。

## 下一步具体动作

1. **(用户决定)归档本调研任务 + 提交**:`task.py archive` 会触发自动 git commit;当前在 master,未擅自动。
   若提交,findings/新任务/demo 一起;`CLAUDE.md` 归属先确认。建议提交信息:`docs: MMG 体网格调研结论 + 产出实现任务`。
2. **(实现任务阶段 0,降风险)实测 vtkDelaunay3D**:拿一个质心落体外的弯曲管段闭合表面喂 `vtkDelaunay3D`,
   看是否产域外 tet、`Alpha`/`BoundingTriangulation` 能否裁掉、边界三角能否对回输入 → 定裁剪策略,写进 design §2a。
   这是 findings 标的「未实证」点,不实测就开工会返工。
3. 之后按 `../06-28-volume-mesh-kernel/implement.md` 的阶段 1→4 执行(接口→VTK 填充→MMG remesh→装配+验收)。

## 相关文件索引

- 主产物:`findings.md`(同目录)
- 实现任务:`../06-28-volume-mesh-kernel/{prd,design,implement}.md`
- 实证产物:`../../workspace/ocean/mmg-demo/`
- spec:`.trellis/spec/XQ/architecture/external-libs.md`(MMG 选型)
- memory:`mmg-cannot-tetrahedralize-from-surface`、`xq-surface-winding-not-consistent`、`xq-build-recipe`、`no-side-effect-in-assert`、`ninja-target-incremental-fakegreen-trap`
