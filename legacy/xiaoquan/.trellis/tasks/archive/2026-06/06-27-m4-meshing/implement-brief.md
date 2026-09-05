# M4 实现 brief(给 implement worker)

你实现 M4 网格里程碑。完整规格见同目录 `design.md` + `implement.md`,**严格照做**。下面是你动手必需的事实。

## 铁律(违反=返工)
- 零外部依赖:core 与 services/meshing 的 .h/.cpp **绝不 include VTK/ITK/GDCM/Qt**;四面体化/质量全用 `core/GeometryTypes.h` 纯 C++。
- service 只读 const、只产命令,**绝不碰 scene mutator**;xq_services 只 link xq_core。
- 命令**复用 M0** 的 `AddNodeWithSourceRelationCommand`(见 src/core/command/XQSceneCommands.h),不新建命令类型。
- 测试**必须用 CHECK 宏**(`if(!(cond)) return fail(#cond,__LINE__);`),副作用调用先取变量再判断,**绝不进 assert**(Release /DNDEBUG 会删 assert = 假绿)。
- 算法/语义抉择一律参考 **SimVascular/MITK(及底层 VTK/TetGen/MMG)**;**XQ1 全面作废,任何场景不参考**。
- 遇到小抉择(质量公式、Params 字段等)**自己定合理默认,别停下问**,做到门槛达成再汇报。

## 关键接口事实(已核查,直接用)
- `XQDomainType::Mesh` 已存在,domainTypeToString 已含 "mesh"。无需改枚举。
- `GeometryTypes.h`:add/sub/scale/dot/cross/norm/normalized/distance。四面体有符号体积 = `dot(cross(sub(b,a),sub(c,a)), sub(d,a)) / 6.0`。
- `XQMesh`(src/core/XQMesh.h)已有:VolumeMeshHandle/SurfaceMeshHandle(只 count 句柄)、MeshBoundaryFace{faceId,name,kind,capId,cellIds}、MeshQualitySummary{minQuality,maxQuality,meanQuality,elementCount}、addBoundaryFace/boundaryFaceById、setSourceModelNode、setQuality。**句柄路径保留不动**(test_mesh 依赖)。
- `XQTriangleSurfaceGeometryHandle`(src/core/,M3):points + 三索引 triangles、addPoint/addTriangle、pointCount/triangleCount、point(i)/triangle(i)、is_valid。**你要给它加 per-triangle faceId 标签**(见 implement.md 步骤 0)。
- `XQSurfaceModel`(M3):triangleGeometry()、faces()(ModelFace{faceId,name,kind,capId})、hasSourceContourGroupNode/sourceContourGroupNode。
- `ModelingService`(M3):loftSurface / capModel / wallFaceId()==1;cap 默认 inletFaceId=2/outletFaceId=3。你要在 loft/cap 里给三角带 faceId 标签。
- payload 范式:看 src/core/XQSurfaceModelPayload.h(照抄结构做 XQMeshPayload)。
- service 范式:看 src/services/modeling/ModelingService.{h,cpp}(照抄 Result/CommandResult/状态枚举/命令复用模式)。
- integration 测试范式 + CTGR fixture:看 tests/services/modeling/ModelingIntegrationTest.cpp(用 XQ_CTGR_DIR + aorta_final.ctgr);CMake 里 modeling_integration 的 XQ_CTGR_DIR 定义与 offscreen 属性照抄。

## 构建 / 测试配方(已实测,照用)
- 用**全新 build 目录**(别复用别人的)。git-bash 调 .bat 用绝对路径:`cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\<脚本>.bat"`。
- 构建 .bat 内容:
  ```
  call "C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
  cd /d "C:\Users\OCEAN\Desktop\XIAOQUAN\XQ"
  set CM="C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  set PREFIX=C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64;.../tinyxml2-8.0.0;.../qt-6.7.0;.../vtk-9.3.0
  %CM% -S . -B <build目录> -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%PREFIX%"
  %CM% --build <build目录> --config Release
  ```
  (PREFIX 各段用 Externals/install/windows-x64 根 + 各 dep 子目录,见 XQ/build_verify.bat 参考——但你自建独立目录。注意 build_*.bat / build_*/ 已被 .gitignore 忽略。)
- ctest .bat:`cd /d "<build目录>"` → `set QT_QPA_PLATFORM=offscreen` → `ctest.exe -C Release --output-on-failure`。

## 完成门槛(达成才汇报)
1. 全新构建零错误;全量 ctest 真绿(M0~M3 的 32 + M4 新增,预期 ≥35)。
2. 自做假绿抽查(篡改 VolumeMeshService 闭合校验或 tet 体积统计 → Release 下相关测试 FAIL → 恢复 → PASS),把篡改点+结果写进汇报。
3. grep 确认 core/services 无 VTK/ITK include。
4. 汇报:新增/改动文件清单、ctest 结果数字、假绿抽查证据、用到的关键算法决策(质量公式等)、留下的技术债(若有)。
</content>
