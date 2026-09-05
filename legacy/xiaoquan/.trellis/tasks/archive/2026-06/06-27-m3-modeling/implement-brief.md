# M3 实现任务简报（implement worker）

Active task: .trellis/tasks/06-27-m3-modeling

## 目标
在 XQ 工程(`C:/Users/OCEAN/Desktop/XIAOQUAN/XQ`,M0~M2 完成,30 ctest 全绿)实现 M3 建模:
真三角面 handle + XQSurfaceModel 扩展 + payload + ContourLoftInputBuilder + ModelingService(放样+封口)。
先读 prd.md / design.md / implement.md / jsonl 里 spec,再动手。

## 已核查的关键事实（直接用，别重造）
- `XQSurfaceModel` 已有 `ModelFace{faceId,name,FaceKind(Wall/Cap/Inlet/Outlet),capId,boundaryLoopIds}`、
  addFace/faceById、source/sourceContourGroupNode、PreservedVtpArrays;但 `SurfaceGeometryHandle` 是**句柄,无真实三角面**。
- `XQContourGroup` 放样输入齐全:`XQContour{contourId,pathArcLength,ContourFrame{origin,normal,xAxis,yAxis},type,points,closed}`、
  `orderedByPathPosition()`、`projectToFrame/unprojectFromFrame`。
- 命令(M0,复用,**别新建命令类型**):AddNodeWithSourceRelationCommand(scene*, node, source, label)、AddNodeCommand、ReplacePayloadCommand。
- 范式(M1/M2):service 只读 const 节点 + 只产命令、**不碰 scene mutator**;`xq_services` 只 link xq_core;
  payload `: public XQPayload`(domainType+clone 深拷贝);XQDataNode 非默认可构造。

## 实现内容（按 implement.md 步骤）
1. **XQTriangleSurfaceGeometryHandle**(src/core):真三角面(vector<Point3> + vector<三 int 索引>),校验索引范围。零依赖。
2. **XQSurfaceModel 扩展**:增持可选真三角面 handle(生成路径),**保留句柄式字段,M0 的 test_surface_model 必须仍绿**。
3. **XQSurfaceModelPayload**(src/core):domainType==SurfaceModel,clone 深拷贝。
4. **ContourLoftInputBuilder + XQContourLoftInput**(services/modeling):排序 + 每 contour 重采样到统一点数 +
   起点对齐(防扭转,参考 SimVascular loft 的 align)。校验≥2 contour、有 frame。
5. **ModelingService**:
   - loftSurfaceCommand → 相邻环放样三角带 + Wall face + AddNodeWithSourceRelationCommand(source=contour group)。
   - capModel → 开口端封口三角化(扇形/质心)+ Inlet/Outlet cap face(capId);封口后闭合。
   - ModelFace 稳定 id;不碰 mutator。
6. 测试:ModelingServiceTest(合成圆环→放样三角形数可推算+封口闭合性+ModelFace 稳定+undo/redo);
   集成测 0007 contour group 放样出闭合 XQSurfaceModel + ModelFace。
7. CMake:新增源 + 测试 add_test;modeling 库只 link xq_core。

## 已验证的构建配方（必须照用）
.bat 包 vcvars64+cmake，`cmd //c build.bat`:
```bat
@echo off
call "C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "C:\Users\OCEAN\Desktop\XIAOQUAN\XQ"
set CM="C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set PREFIX=C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64;C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/tinyxml2-8.0.0;C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0;C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/vtk-9.3.0
%CM% -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%PREFIX%"
%CM% --build build --config Release
```
ctest:另一 .bat,`cd build & set QT_QPA_PLATFORM=offscreen & ctest -C Release --output-on-failure`。
**辅助 .bat 用完删,别留进工程。别提交 build。**

## 验收门槛
- Release 全量 ctest **全绿**(M0~M2 的 30 + M3 新增)。
- 0007 contour group 放样出 XQSurfaceModel,封口后**闭合**(测试验证:每条边恰 2 三角形共享 / Euler V-E+F=2),ModelFace 稳定。
- 放样/封口/面元 id 稳定性/undo 单测通过。
- 防假绿:新测试 CHECK 风格、副作用先求值;抽查至少 1 个(篡改→Release FAIL→还原→PASS)。

## 禁止
- 不碰 XQrebuild/XQ1/plan 源码和 .git。**放样/封口语义参考 SimVascular/MITK,绝不参考 XQ1**。
- service 公开 API 不依赖 VTK/OCCT/Qt/插件;不新建命令类型;不直接改 scene;不 git commit。
- 有副作用调用别进 assert/CHECK 条件表达式。

## 完成请报告
改了/加了哪些文件、最终 ctest、防假绿抽查证据、合成数据怎么验放样/闭合的、0007 集成结果、剩余坑。
