# XQ Core 层规范(core layer)

> 来源:`plan/00-architecture.md`、`plan/01-core-and-io.md`、`plan/README.md`。
> `core` 是零外部依赖的数据内核 + 命令栈,所有上层服务的地基。写 core/io 代码前先读本层。

---

## 本层规范

- [command-and-scene.md](./command-and-scene.md) — 所有权、命令/undo 模型、source/derived/stale 关系、线程与错误。
- [source-interface.md](./source-interface.md) — 消费侧只读 Source 接口(M8b-1):每块一次虚调用 + 整块连续视图 + 借用/物化二分 + RAII keepalive 租约。写消费 voxel/triangle/tet 数据的代码前先读。
- [acceptance.md](./acceptance.md) — 验收原则(ctest 全绿 + 0007 端到端;数据进 XQ 自有对象才算)。
- [build-and-test.md](./build-and-test.md) — 构建/测试环境配方(已实证):vcvars64 + CMAKE_PREFIX_PATH + offscreen ctest。

> 架构铁律、分层、坐标系/单位、外部库规则见 `architecture` layer。
