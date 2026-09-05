# M5 实现 brief(给 implement worker)

你实现 M5 流体里程碑。完整规格见同目录 `design.md` + `implement.md`,**严格照做**。下面是动手必需的事实。

## 铁律(违反=返工)
- 应用内血流求解,**绝不依赖 svSolver/svFSI/外部 CFD/Python**;core/services/flow 的 .h/.cpp **绝不 include VTK/ITK/GDCM/Qt/任何求解器**;全用纯 C++ 数值 + `core/GeometryTypes.h`。
- service 只读 const、只产命令,**绝不碰 scene mutator**;xq_services 只 link xq_core。
- 命令**复用 M0**(AddNodeWithSourceRelationCommand / ReplacePayloadCommand / AddNodeCommand,见 src/core/command/XQSceneCommands.h),不新建命令类型。
- 测试**必须用 CHECK 宏**(`if(!(cond)) return fail(#cond,__LINE__);`),副作用调用先取变量再判断,**绝不进 assert**(Release /DNDEBUG 删 assert = 假绿)。
- 算法参考 **SimVascular(svOneDSolver/svZeroDSolver 的 1D/0D 血流)/MITK/教科书 1D 血流**;**XQ1 全面作废,任何场景不参考**。
- 遇小抉择(离散格式、误差阈值、插值)自定合理默认,**别停下问**,做到门槛达成再汇报。

## 关键事实(已核查,直接用)
- **单位全程 CGS**(cm,g,s):血液 density=1.06 g/cm³、viscosity=0.04 poise(来自 0090_0001.sjb)。流量 cm³/s、压力 dyn/cm²、面积 cm²。
- **SimVascular 参考(0090_0001.sjb,权威边界设置)**:出口 BC=RCR,每 cap `Values="Rp C Rd"`(如 outflow=`106.0 0.00068483 1784.0`);入口=流量波形(Period=0.984s);壁=rigid。
- **inflow_1d.flow 格式**:两列文本 `t Q`(`../0007_H_AO_H/flow-files/inflow_1d.flow`,主会话路径 `C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/flow-files/inflow_1d.flow`)。
- `XQSimulationCase`(src/core/,已有):SolverParameters、BoundaryConditionType{NoSlip,PrescribedVelocity,PrescribedPressure,Resistance}、BoundaryCondition{faceId,type,value}、boundaryConditionByFaceId、setSourceMeshNode。
  - **关键兼容**:`tests/core/test_simulation_case.cpp` 依赖现有 4 个枚举值及顺序(NoSlip=wall/PrescribedVelocity=inlet/Resistance=outlet)。扩展枚举**只能末尾追加** RCR、InletFlowWaveform;BoundaryCondition 只加字段不删。否则破 M0 测试。
- **XQDomainType 无 FlowResult**:末尾追加 `FlowResult`,同步 `XQDomainType.h` domainTypeToString switch + `XQScene.cpp` 的 domain switch(两处无 default,漏分支会编译报错,据此全覆盖)。检查 test_payload_and_groups.cpp / SvProjectReader.cpp 按需补。
- **无 XQSimulationCasePayload**(case 目前只作值对象)→ 需新建。XQFlowResult + payload 全新建。
- `XQMesh.boundaryFaces()`(M4):MeshBoundaryFace{faceId,name,kind,capId,cellIds} = 边界条件权威面列表。
- `XQPath`(M1):samplePoints/arcLength/frameAtArcLength,**无截面积**;A0(x) 从 XQContourGroup 的 contour 多边形面积(鞋带公式)取,合成校验算例用解析常/变截面。
- payload 范式:src/core/XQSurfaceModelPayload.h;service 范式:src/services/meshing/VolumeMeshService.{h,cpp}(Result/CommandResult/状态枚举/命令复用);integration+fixture 范式:tests/services/meshing/MeshingIntegrationTest.cpp + CMake 里它的 XQ_CTGR_DIR/offscreen 设法。

## 数值验收门槛(核心,防假绿,plan 硬要求)
FlowSolver1D 必须有两条**对解析解**的测试,带相对误差阈值(真断言,可证伪):
1. **稳态泊肃叶刚性管**:恒定 Q0、常截面、rigid → 稳态压降与泊肃叶解析吻合(误差 < 1–2%)。
2. **单段 0D RCR 阶跃**:恒定 Q0 入 RCR → P(t)=Q0(Rp+Rd)+(P0−Q0(Rp+Rd))·e^{−t/(Rd·C)},数值解逐点吻合(误差 < 阈值)。
CFL 超限必须发诊断(CflViolation)不静默发散。

## 构建/测试配方(已实测,照用)
- 用**全新独立 build 目录**。git-bash 调 .bat 用绝对路径:`cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\<脚本>.bat"`。
- 构建:vcvars64 → cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Externals/install/windows-x64 + tinyxml2-8.0.0 + qt-6.7.0 + vtk-9.3.0 各根>" → cmake --build。参考 XQ/build_verify.bat(build_*.bat / build_*/ 已被 .gitignore 忽略)。
- ctest:cd build 目录 → `set QT_QPA_PLATFORM=offscreen` → `ctest.exe -C Release --output-on-failure`。

## 完成门槛(全达成才汇报)
1. 全新构建零错误;全量 ctest 真绿(M0~M4 的 35 + M5 新增,预期 ≥38)。
2. 假绿抽查:篡改 RCR 时间常数(Rd·C)或泊肃叶系数 → 解析对照测试相对误差超阈 Release FAIL → 恢复 → PASS。记篡改点+误差数值。
3. grep 确认 core/services 无外部依赖 include。
4. 汇报写进 `.trellis/tasks/06-27-m5-flow/implement-report.md`:文件清单、ctest 数字、假绿证据、两个解析对照的实际相对误差数值、关键数值决策(离散格式/阈值)、技术债。写完再 idle。
</content>
