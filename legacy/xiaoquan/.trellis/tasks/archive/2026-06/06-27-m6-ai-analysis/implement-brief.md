# M6 实现 brief(给 implement worker)

你实现 M6 AI 分析里程碑。完整规格见同目录 `design.md` + `implement.md`,**严格照做**。下面是动手必需的事实。

## 铁律(违反=返工)
- **ONNX Runtime 对象绝不进公开 API**;只在 `src/adapters/onnx/OnnxBackend.cpp` 私有、且包在 `#ifdef XQ_ENABLE_ONNX` / CMake option(默认 OFF)内。services/ai 与所有 .h **绝不 include onnxruntime**。
- **无 Python 运行时**。
- service 只读 const、只产命令,**绝不碰 scene mutator**;xq_services 只 link xq_core。
- 命令**复用 M0**(AddNodeWithSourceRelationCommand 等),不新建命令类型。
- 测试**必须用 CHECK 宏**(`if(!(cond)) return fail(#cond,__LINE__);`),副作用调用先取变量再判断,**绝不进 assert**。
- 参考血流力学标准公式(WSS/FFR/OSI,CGS)+ SimVascular/MITK;**XQ1 全面作废不参考**。
- 小抉择自定合理默认,**别停下问**。

## 关键环境约束(重要)
- **ONNX Runtime 未安装**(Externals 没有 onnxruntime,plan/09 说需新增 manifest)。所以**本里程碑不验收真实 .onnx 推理**——做:
  1. **FlowMetricsService 纯算**(FFR/WSS/OSI,零 ONNX)→ 完整落地 + 解析对照验收(M6 验收硬项)。
  2. **AiService + mock backend** → 接口契约 + 链路验收(复用 M2 已建的纯虚 Backend 模式)。
  3. **OnnxBackend 接口层**(option XQ_ENABLE_ONNX 默认 OFF,真实实现/find onnxruntime 仅 ON 时编译)→ 接口定义,真实推理记技术债后挂。
- 默认构建(OFF)必须零错误、不 find onnxruntime、不编 onnx adapter/OnnxBackendTest。

## 关键事实(已核查,直接用)
- **M2 已建 AI 分割契约**(`src/services/segmentation/XQAiSegmentationRequest.h`):`XQAiSegmentationRequest{modelId,hasRoi,roi[6],targetLabels}` + 纯虚 `XQAiSegmentationBackend::segment(image,buffer,request)->shared_ptr<XQSegmentationMask>`。**M6 直接复用,不另造分割契约**。
- **M5 XQFlowResult**(`src/core/XQFlowResult.h`):times()、segments()、flowQ()/pressureP()/areaA()(`[segment][time]`,CGS:Q cm³/s、P dyn/cm²、A cm²)、isConsistent()、sourceCaseNode()。FFR/WSS/OSI 输入齐全。
- **XQDomainType 无 AiAnalysis**:末尾追加,同步 XQDomainType.h domainTypeToString + XQScene.cpp 两处 switch(无 default 漏分支会编译报错,据此全覆盖)。
- 血流力学公式(CGS):WSS 圆管 τ_w=4μQ/(πR³),R=√(A/π);FFR=Pd/Pa(远端/近端时均压);OSI=0.5(1−|∫τdt|/∫|τ|dt)∈[0,0.5];ΔP=inlet−outlet 时均。
- payload 范式:src/core/XQFlowResultPayload.h;service 范式:src/services/flow/FlowSolver1D.{h,cpp};integration+fixture:tests/services/flow/FlowIntegrationTest.cpp + CMake 设法。

## 数值验收门槛(核心,防假绿)
FlowMetricsService 必须有**对解析解的真断言**测试(相对误差阈值,可证伪):
- 合成 XQFlowResult(已知恒定 Q/A/P)→ WSS=4μQ/(πR³) 闭式对照、FFR=已知 Pd/Pa 对照、OSI 用已知振荡波形对照。
代理预测结果 provenance 必须标 SurrogatePredicted(不得伪装成求解结果)。

## 构建/测试配方(已实测,照用)
- 全新独立 build 目录;git-bash 调 .bat 用绝对路径 `cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\<脚本>.bat"`。
- 构建:vcvars64 → cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Externals/install/windows-x64 + tinyxml2-8.0.0 + qt-6.7.0 + vtk-9.3.0 各根>"(**不加 onnxruntime**,XQ_ENABLE_ONNX 默认 OFF)→ cmake --build。参考 XQ/build_verify.bat。
- ctest:cd build → `set QT_QPA_PLATFORM=offscreen` → ctest -C Release --output-on-failure。

## 完成门槛(全达成才汇报)
1. 默认构建(XQ_ENABLE_ONNX OFF)零错误;全量 ctest 真绿(M0~M5 的 38 + M6 新增,预期 ≥41)。
2. 假绿抽查:篡改 WSS 公式(R³→R²)或 FFR 比值方向 → FlowMetrics 解析对照 Release FAIL → 恢复 → PASS。记篡改点+误差。
3. grep 确认 services/ai 与公开 .h 无 onnxruntime include。
4. 汇报写 `.trellis/tasks/06-27-m6-ai-analysis/implement-report.md`:文件清单、ctest 数字、假绿证据、指标解析对照误差数值、ONNX 后挂说明、技术债。写完再 idle。

只有遇到真正无法从参考推断、且会改变可见行为的死结,才写进 report 并停下。否则一路做到门槛达成。
</content>
