# M0 骨架 — 执行计划(implement)

> 验证以 `.trellis/spec/XQ/core/acceptance.md` 为准:Release + 全量 `ctest` 绿 + 0007 读成 scene。

## 执行步骤(建议顺序)

1. **建新工程基底**
   - 从 XQrebuild 取 `CMakeLists.txt` + `src/` + `tests/`(排除 `build_*` 临时目录、`.git`、`.codex_tasks`)。
   - 确认 `find_package(Qt6/VTK9/tinyxml2)` 可解析;先跑通现有 24 个 `ctest`(基线绿)。
   - 回滚点:此步完成 = "干净搬入基线",后续改动可对照。

2. **payload 机制**
   - 加 `XQDomainType` 枚举、`XQPayload` 基类/handle。
   - `XQDataNode` 增持 `std::shared_ptr<XQPayload>` + `domainType()` 访问器;迁移所有 `domain_type` 调用点。
   - `XQScene` 分组 + `groupForDomain()`。
   - 新增 payload 单测;**回归跑 24 旧测试必须仍绿**。

3. **命令栈**
   - 新建 `src/core/command/XQCommand.h`、`XQCommandStack.h/.cpp`。
   - 实现 4 个预置命令(Add / AddWithSourceRelation / ReplacePayload / Remove)。
   - 新增命令栈单测(execute/undo/redo/清空 + payload 复制保留 id 与来源)。
   - 把新测试 `add_test` 进 CMake。

4. **SvProjectReader → expected-scene**
   - 让 `SvProjectReader` 把 0007 读成 scene(payload 节点 + source/derived 关系)。
   - 从读取结果固化 `expected-scene.json`(节点数/分组/关系),人工核对后作为集成测基准。
   - 新增/补强 `test_svproject_reader` 对照 expected-scene。

## 验证命令(Release + 全量)

```bash
# 配置 + 构建(Release)
cmake -S <new-project> -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
# 全量 ctest
ctest --test-dir build -C Release --output-on-failure
```

- 期望:24 旧测试 + 新增 payload/命令栈/集成测全绿。
- 0007 集成:`test_svproject_reader` 对 `0007_H_AO_H` 断言节点数/分组/关系符合 expected-scene.json。

## Review 闸

- payload 改造后**先回归 24 旧测试**再继续,任何一个红就停下修。
- 集成测断言**不得**把有副作用的 `reader.load()` 包进 `assert`(Release `/DNDEBUG` 会删 → 假绿段错误)。
- 命令栈接口定稿前确认 M1 PathService 的命令签名能落在这套预置命令上(向前兼容)。

## 回滚点

- step1 完成(基线绿)/ step2 完成(payload 绿)/ step3 完成(命令栈绿)各为一个可回退快照。
- payload 改造若大面积破坏旧测试且短期难收敛 → 回退到 step1,缩小改造批次(先枚举+访问器,再挂 shared_ptr)。

## 不做(本里程碑)

- 不实现任何 service(path/seg/...);命令栈只建基础,service 在 M1+ 接入。
- `XQProjectReader/Writer` 的各 payload round-trip 不在 M0 完成(M1+ 增量)。
- 不引入 ITK/GDCM/ONNX/OCCT/MMG。
