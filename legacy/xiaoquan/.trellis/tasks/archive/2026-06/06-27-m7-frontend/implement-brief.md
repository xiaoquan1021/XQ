# M7 实现 brief(给 implement worker)

你实现 M7 前端里程碑(收尾)。完整规格见同目录 `design.md` + `implement.md`,**严格照做**。下面是动手必需的事实。

## 铁律(违反=返工)
- 壳薄:UI 只调 service、读 scene、把意图经命令栈提交;**不实现任何领域算法**。
- **UI 不绕过 service/命令直接改 scene**:Controller 只经 `XQCommandStack::push` 提交命令,**绝不直接调 scene.insert/remove**(scene mutator)。
- **Controller 纯 C++**(只 include core + services,**不 include Qt/VTK**)→ 无头可测;Qt/VTK 只在 ui widget / app / visualization。
- 命令复用 M0~M6 各 service 已有的命令产出;不新建命令类型。
- 测试**必须用 CHECK 宏**(`if(!(cond)) return fail(#cond,__LINE__);`),绝不进 assert。
- 参考 plan/08 + SimVascular UI 工作流;**XQ1 全面作废不参考**。小抉择自定合理默认,别停下问。

## 关键事实(已核查,直接用)
- **现有 Qt 壳骨架**(M0):`src/ui/XQSceneModel.{h,cpp}`(树模型)、`src/app/XQMainWindow.{h,cpp}`(`XQMainWindow()` 默认构造 + `setScene(const XQScene*)`,test_main_window 这样用)、`src/visualization/XQImageViewer.{h,cpp}`(VTK 离屏)。
  - **保留 XQMainWindow 现有签名**(默认构造 + setScene const);可变 scene + 命令栈用新增方法/构造接入,别改坏 test_main_window。
- **命令**:`XQCommandStack`(src/core/command/XQCommandStack.h):push(执行+入 undo 栈)/undo/redo/can_undo/can_redo。命令 AddNodeCommand(scene,node,label) / AddNodeWithSourceRelationCommand 等在 XQSceneCommands.h。
- **各 service 命令产出方法(动手前 rg 各 .h 确认实际签名,别臆造)**:
  - M3 ModelingService::loftSurfaceCommand / createModelNodeCommand;M4 SurfaceMeshService/VolumeMeshService 的 build*MeshCommand;M5 FlowSolver1D::buildFlowResultCommand + BoundaryConditionService;M6 FlowMetricsService::analyzeFlowCommand + AiService::segment/identify/predictFlow(纯虚 backend,用 mock)。
  - M1 PathService / M2 SegmentationService 的命令方法名按其 .h 实际(可能非 *Command 命名)。
- **存档模型(关键)**:XQProjectWriter 只序列化节点**结构**(id/domain_type/display_name/派生关系/stale),**不序列化 payload 实体数据**。新 domain(FlowResult/AiAnalysis)domainTypeToString 已支持 → 结构 round-trip 自动可用。M7"scene 一致"= 结构一致;payload 实体持久化是技术债不做。
- **XQScene**:insert 返回 XQScene::InsertResult::Inserted;visit_nodes / visit_derived_relations 遍历;find(id)。
- 范式参考:service Result/CommandResult 模式见 src/services/flow/FlowSolver1D.{h,cpp};测试见 tests/services/flow/FlowSolver1DTest.cpp(CHECK 宏);现有 UI 测试 tests/app/test_main_window.cpp(offscreen Qt 构造)。

## 范围(收尾,切清楚)
- **做**:6 个 Controller(Path/Segmentation/Modeling/Meshing/Flow/Ai,纯 C++,意图→service→命令→stack→scene)+ XQMainWindow 集成(右侧阶段面板 + Edit undo/redo)+ Controller 无头链路测试 + 0007 端到端工作流测试(AI 用 mock backend)+ 新 domain 结构 round-trip 验证。
- **不做(记技术债)**:W1 scene 可变性收紧(高风险,plan 非强制;守"只经命令栈"纪律即可);payload 实体数据持久化(现有存档只存结构)。

## 构建/测试配方(已实测,照用)
- 全新独立 build 目录;git-bash 调 .bat 用绝对路径 `cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\<脚本>.bat"`。
- 构建:vcvars64 → cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Externals/install/windows-x64 + tinyxml2-8.0.0 + qt-6.7.0 + vtk-9.3.0 各根>" → cmake --build。参考 XQ/build_verify.bat。
- ctest:cd build → `set QT_QPA_PLATFORM=offscreen`(Qt 测试必需) → ctest -C Release --output-on-failure。

## 完成门槛(全达成才汇报)
1. 全新构建零错误;全量 ctest 真绿(M0~M6 的 41 + M7 新增,预期 ≥43)。
2. 假绿抽查:篡改某 Controller(不 push 命令 / push 错 domain) → WorkflowControllerTest Release FAIL → 恢复 → PASS。记篡改点+结果。
3. grep 确认 Controller 源 + core/services 无 Qt/VTK include(Controller 纯 C++)。
4. 确认 UI 只经命令栈改 scene(Controller 不直接调 scene mutator)。
5. 汇报写 `.trellis/tasks/06-27-m7-frontend/implement-report.md`:文件清单、ctest 数字、假绿证据、端到端工作流验证(各阶段节点 + undo/redo 全程)、技术债。写完再 idle。

只有遇到真正无法从参考推断、且会改变可见行为的死结,才写进 report 并停下。否则一路做到门槛达成——这是最后一个里程碑,务必端到端跑通。
</content>
