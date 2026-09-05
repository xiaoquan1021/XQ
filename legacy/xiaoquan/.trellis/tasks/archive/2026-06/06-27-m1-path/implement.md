# M1 路径 — 执行计划(implement)

> 构建/测试配方见 spec/XQ/core/build-and-test.md(vcvars64 + CMAKE_PREFIX_PATH + offscreen ctest)。
> 验收:Release + 全量 ctest 真绿(M0 已 26 个,M1 后更多),0007 path 读成 XQPath 节点。

## 执行步骤

1. **XQPathPayload(core)**
   - 新增 `src/core/XQPathPayload.h`:`class XQPathPayload : public XQPayload`,持 `XQPath`,
     实现 `domainType()==XQDomainType::Path`、`clone()` 深拷贝。
   - 单测:payload 的 domain/clone/取还 XQPath 往返。

2. **W1 收紧 scene 可变性(前置)**
   - 按 design 方案收紧 `XQScene` mutator 的可达性(friend 命令类优先;牵动过大则退到"service 只读 + 命令改"底线)。
   - **改完立即全量 ctest,M0 的 26 个必须仍绿**。这步是回滚点。

3. **PathService(services/path)**
   - `src/services/path/PathService.h/.cpp`:5 个公开 API,全部返回 M0 既有命令
     (createPathCommand→AddNodeWithSourceRelation;move/insert/delete/resample→ReplacePayload)。
   - 校验齐全(≥2 点、spacing>0、索引、domain==Path);非法返回带诊断的失败,不抛异常。
   - 单测 `tests/services/path/PathServiceTest.cpp`:创建/编辑/重采样 + execute/undo/redo
     + 复制 payload 保留 path id 与 sourceImageNode。

4. **CenterlineFrameService(services/path)**
   - `src/services/path/CenterlineFrameService.h/.cpp`:RMF 标架序列。
   - 单测 `CenterlineFrameServiceTest.cpp`:法向连续(相邻标架法向不突变)、tangent⊥normal、单位向量、arcLength 单调。

5. **SvProjectReader 接真实 path 几何**
   - path 节点改挂 `XQPathPayload`(调 PTHPathReader 填充)。
   - 更新 `test_svproject_reader.cpp`:path 节点 payload 类型断言为 path-typed;节点数/关系不变。

6. **CMake**:新增源文件 + 2 个 service 测试 add_test;service 库依赖 core(不依赖 Qt/VTK)。

## 验证命令(照 build-and-test.md)
```
配置+构建: cmd //c <build.bat>   (vcvars64 + cmake Ninja Release + CMAKE_PREFIX_PATH)
全量 ctest: cmd //c <test.bat>    (cd build & set QT_QPA_PLATFORM=offscreen & ctest -C Release --output-on-failure)
```
期望:M0 的 26 + M1 新增(payload/PathService/CenterlineFrame + 更新的集成测)全绿。

## Review 闸
- W1 收紧后**先回归 M0 26 测试**再继续,红就停。
- 防假绿:所有新测试用 CHECK 风格(`if(!cond){fprintf;return 1;}`),副作用调用先求值到变量,绝不进条件表达式。
- service 层确认零外部依赖(只 include core)、不直接调 scene mutator。
- 抽查:对至少一个新测试做"篡改期望→Release FAIL→还原→PASS"证明非假绿。

## 回滚点
- step1(payload 绿)/ step2(W1 收紧后 26 仍绿)/ step3-4(service 绿)各为快照。
- W1 改造若大面积破坏 M0 测试且难收敛 → 退到 design 的"底线方案"(service 只读 + 命令改,mutator 暂留 public),W1 完整收紧记为 risk 留后。

## 不做(本里程碑)
- 不做 contour/建模(M2/M3);不引入 ITK/VTK 到 path 几何;不碰 XQProjectReader/Writer round-trip(M1+ 增量,按需)。
- fixture 绝对路径 / expected-scene 双真值(N1/N2 清理项):**可选**,若顺手且不扩面则一并改,否则留记录。
