# 2026-07-01 XQ 全程序审计摘要

## 最大风险

命令栈、项目反序列化、几何资源生命周期和渲染输入校验都存在“失败状态继续推进”的问题。这些问题会导致数据被静默删除、损坏项目被当成有效项目加载、句柄悬空访问或渲染越界。

## 子任务拆分

### P0: fix-command-stack

- 位置：`XQ/src/core/command/XQCommand.h`、`XQ/src/core/command/XQCommandStack.cpp`、`XQ/src/core/command/XQSceneCommands.cpp`、`XQ/src/core/XQScene.cpp`。
- 问题：`XQCommand::execute()` 返回 `void`，`XQCommandStack::push()` 执行后无条件入栈；`AddNodeCommand` 和 `AddNodeWithSourceRelationCommand` 忽略 `insert/link_derived` 失败。
- 后果：重复 id add 失败后 undo 会删除原有节点；关系失败后 controller 仍返回成功。
- 必须测试：重复 id、缺失 source、重复 relation、controller 失败传播、失败命令不增加 undo 栈。

### P0: fix-blob-schema-validation

- 位置：`XQ/src/io/project/XQProjectReader.cpp`、`XQ/src/io/blob/BlobStore.cpp`、`XQ/src/io/source/MappedGeometrySource.h`、`XQ/src/services/resource/GeometryResourceManager.cpp`。
- 问题：blob line 只检查数值范围，不按 role 校验 `elementType/components/count`；大小乘法未做溢出检查；`relPath` 没有限制在资产根内。
- 后果：损坏项目可静默恢复错误几何，甚至绕过截断检查。
- 必须测试：错误 type/components、faceId 数量不平行、byteCount 与逻辑长度不一致、乘法溢出、路径穿越。

### P0: fix-geometry-resource-lifetime

- 位置：`XQ/src/services/resource/GeometryResourceManager.h`、`XQ/src/services/resource/GeometryResourceManager.cpp`。
- 问题：source handle 保存裸指针，`pin_` 只阻止 eviction，不拥有 source。
- 后果：handle 越过 manager 生命周期后 use-after-free。
- 必须测试：manager 析构后 handle 访问必须安全失败或保持对象存活。

### P0: fix-renderer-connectivity-validation

- 位置：`XQ/src/visualization/XQSceneRenderer.cpp`、`XQ/src/io/project/XQProjectReader.cpp`、`XQ/src/core/XQTriangleSurfaceGeometryHandle.cpp`。
- 问题：`build_cell_chunk()` 对非法索引仍访问 `allPoints[static_cast<size_t>(g)]`；reader 重建 geometry 后未强制 `is_valid()`。
- 后果：负索引或越界索引导致渲染越界读或崩溃。
- 必须测试：负索引、超过点数索引、lazy source 非法 connectivity。

### P1: fix-stale-propagation

- 位置：`XQ/src/core/XQScene.cpp`、`XQ/tests/core/test_scene_relations.cpp`。
- 问题：`mark_source_changed()` 只标记直接 derived。
- 后果：下游 surface/mesh 可能保持 fresh。

### P1: fix-real-app-entry

- 位置：`XQ/src/app/main.cpp`、`XQ/src/app/XQMainWindow.cpp`。
- 问题：程序入口只插入 demo 节点，未加载真实项目、未接命令栈和 workflow。

### P1: fix-lazy-geometry-gui-path

- 位置：`XQ/src/io/project/XQProjectReader.cpp`、`XQ/src/services/resource/GeometrySourceResolver.cpp`、`XQ/src/app/XQMainWindow.cpp`。
- 问题：reader/resolver 支持 lazy geometry，但 GUI 选择路径不解析 `geometryAssetId`。

### P1: fix-project-close-state-reset

- 位置：`XQ/src/core/XQProject.cpp`、`XQ/tests/core/test_project_lifecycle.cpp`。
- 问题：`close()` 只清 scene，不清 `AssetRegistry`。

### P2: fix-portable-test-data-root

- 位置：`XQ/CMakeLists.txt`。
- 问题：测试样例路径硬编码到 `C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/...`。

### P2: fix-geometry-sidecar-resolver

- 位置：`XQ/src/io/project/XQProjectWriter.cpp`、`XQ/src/services/resource/GeometrySourceResolver.cpp`。
- 问题：writer 尝试写 `.merkle`，resolver 仍强制 `FullVerify` 且注释过期。

## 父任务验证原则

- 每个子任务必须记录 targeted test。
- P0 子任务完成后优先运行相关核心/IO/resource/visualization 测试组合。
- 父任务最终运行一个全量 `ctest`，如果 build 目录变更则记录使用的 build 目录和原因。
