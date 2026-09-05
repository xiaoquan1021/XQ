# M3 建模 — 执行计划(implement)

> 构建/测试配方见 spec/XQ/core/build-and-test.md。验收:Release 全量 ctest 真绿(M0~M2 的 30 + M3 新增),
> 0007 contour 放样出闭合表面,ModelFace 稳定,undo 通。

## 关键事实(已核查)
- `XQSurfaceModel` 有 ModelFace/source/PreservedVtpArrays,但 `SurfaceGeometryHandle` 是句柄无真实三角面。
- `XQContourGroup` 放样输入齐全(XQContour+frame+orderedByPathPosition+projectToFrame)。
- 命令复用 M0,不新建命令类型;service 不碰 scene mutator(M1/M2 范式)。

## 执行步骤
1. **XQTriangleSurfaceGeometryHandle(core)**:真三角面(points + triangles 索引),校验索引范围。单测。
2. **XQSurfaceModel 扩展**:增持可选真三角面 handle(生成路径),保留句柄式字段(读入路径)。
   **M0 的 test_surface_model 必须仍绿**。
3. **XQSurfaceModelPayload(core)**:domainType==SurfaceModel,clone 深拷贝(含三角面+faces)。单测。
4. **ContourLoftInputBuilder + XQContourLoftInput(services/modeling)**:
   排序 + 每 contour 重采样到统一点数 + 起点对齐(防扭转,参考 SimVascular loft)。校验≥2 contour、有 frame。
5. **ModelingService**:
   - loftSurfaceCommand → 相邻环放样三角带 + Wall face + AddNodeWithSourceRelationCommand(source=contour group)。
   - capModel → 开口端封口三角化 + Inlet/Outlet cap face(capId);封口后闭合。
   - ModelFace 稳定 id;不碰 scene mutator。
6. **测试**:
   - ModelingServiceTest(合成 contour:已知圆环序列→放样三角形数可推算、封口后闭合性、ModelFace id 稳定、undo/redo)。
   - 集成测:0007 contour group 放样出 XQSurfaceModel,验证非空闭合 + ModelFace;可选与 MDLModelReader 读入的原模型对照量级。
7. **CMake**:新增 core/service 源 + 测试 add_test;xq_services(modeling)只 link xq_core。

## 验证(照 build-and-test.md)
- 配置+构建+全量 ctest(Release,offscreen)。期望:30 旧 + M3 新增全绿。
- 闭合性测试:封口后每条边恰被 2 个三角形共享(流形闭合),或 Euler V-E+F=2。

## Review 闸
- 防假绿:新测试 CHECK 风格,副作用先求值;抽查至少 1 个(篡改→Release FAIL→还原→PASS)。
- service 零外部依赖(grep 无 Q*/vtk/itk/occt);不碰 mutator;不新建命令类型。
- 改完先回归 M0~M2 的 30 测试。
- 放样正确性亲自核:三角带索引不自交、封口扇形/质心三角化正确。

## 回滚点
- step1(三角面 handle)/ step3(payload)/ step5(service)/ step6(0007 集成)各为快照。
- 若 0007 真实 contour 放样因点数不齐/扭转难收敛 → 先用合成规则圆环序列跑通放样+封口+闭合性(满足单测),
  0007 集成作为本步收尾尽力而为;不削弱算法本身,真实数据问题记技术债。

## 不做
- 不引入 VTK/OCCT 到放样(首版 XQ 自有三角化);不做网格(M4);不做 AI 生成表面(plan 09 评估项)。
- 不动 XQProjectReader/Writer round-trip;清理项按需不扩面。
