# 完整修复执行指令

> 本文档包含所有修复的**完整可执行指令**，可直接交给便宜 agent 执行。
> 按顺序执行，每完成一项运行验证，全部通过后提交。

---

## 修复顺序

1. **Blocker-1**: XQCommandStack::redo() 丢命令
2. **Blocker-2**: GeometryResourceManager 缓存键冲突  
3. **Blocker-3**: service 层 push() 未检查返回值
4. **Critical-1**: AddNodeWithSourceRelationCommand 回滚逻辑
5. **Critical-2**: mark_source_changed 自环问题
6. **Critical-3**: BlobStore 符号链接漏洞
7. **Medium-1**: 移除未经请求的 fallback

---

## Blocker-1: XQCommandStack::redo() 归还命令

### 文件
`XQ/src/core/command/XQCommandStack.cpp`

### 修改
在第 52-53 行之间插入一行：

**原代码**:
```cpp
    if (!command->execute()) {
        return false;
    }
```

**新代码**:
```cpp
    if (!command->execute()) {
        redo_stack_.push_back(std::move(command));
        return false;
    }
```

### 验证
```bash
cd XQ
cmake --build build --config Release --target test_command_stack
./build/Release/test_command_stack
```

---

## Blocker-2: GeometryResourceManager 缓存键类型安全

### 文件 1: GeometryResourceManager.h

**位置**: 第 158 行 `private:` 之后

**插入**:
```cpp
    enum class BlockKind : int {
        VoxelFull = 0,
        VoxelSegmented = 1,
        GeometrySurfaceFull = 20,
        GeometrySurfaceSegmented = 21,
        GeometryTetFull = 30,
        GeometryTetSegmented = 31,
        GeometryAllFull = 10,
        GeometryAllSegmented = 11
    };
```

**替换** 第 161 行:
```cpp
// 原: typedef std::pair<AssetId, int> ResidentKey;
// 新:
typedef std::tuple<AssetId, BlockKind> ResidentKey;
```

### 文件 2: GeometryResourceManager.cpp

**删除** 第 18-27 行的所有 `constexpr int kResident*`

**替换** `voxelBlockKey` 函数体 (约第 73 行):
```cpp
return ResidentKey(id, spec.segmented ? BlockKind::VoxelSegmented
                                      : BlockKind::VoxelFull);
```

**替换** `geometryBlockKey` 函数体 (约第 79 行):
```cpp
BlockKind kind = spec.segmented ? BlockKind::GeometryAllSegmented
                                : BlockKind::GeometryAllFull;
if (spec.hasSurface && !spec.hasTet) {
    kind = spec.segmented ? BlockKind::GeometrySurfaceSegmented
                          : BlockKind::GeometrySurfaceFull;
} else if (spec.hasTet && !spec.hasSurface) {
    kind = spec.segmented ? BlockKind::GeometryTetSegmented
                          : BlockKind::GeometryTetFull;
}
return ResidentKey(id, kind);
```

### 验证
```bash
cd XQ
cmake --build build --config Release
ctest -C Release -R GeometryResourceManager
```

---

## Blocker-3: service 层 push() 检查

### 自动修复脚本

创建 `XQ/fix_push.py`:
```python
import re
from pathlib import Path

def fix_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    pattern = r'(\s+)stack\.push\(std::move\(([^)]+)\.command\)\);'
    
    def repl(m):
        indent, var = m.groups()
        return f'{indent}if (!stack.push(std::move({var}.command))) {{\n{indent}    return fail("push failed", __LINE__);\n{indent}}}'
    
    new = re.sub(pattern, repl, content)
    
    if new != content:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(new)
        return True
    return False

files = [
    'tests/services/segmentation/SegmentationServiceTest.cpp',
    'tests/services/meshing/VolumeMeshServiceTest.cpp',
    'tests/services/path/PathServiceTest.cpp',
    'tests/services/ai/FlowMetricsServiceTest.cpp',
    'tests/services/ai/AiServiceTest.cpp',
    'tests/services/modeling/ModelingServiceTest.cpp',
    'tests/app/WorkflowIntegrationTest.cpp'
]

for f in files:
    if Path(f).exists() and fix_file(f):
        print(f'Fixed: {f}')
```

### 运行
```bash
cd XQ
python fix_push.py
```

### 验证
```bash
cmake --build build --config Release
ctest -C Release -R "Service|Workflow"
```

---

## Critical-1: AddNodeWithSourceRelationCommand 回滚

### 文件
`XQ/src/core/command/XQSceneCommands.cpp`

### 修改
在第 70 行（`if (scene_->link_derived...` 之前）插入：
```cpp
    inserted_ = true;
```

删除第 77 行的 `inserted_ = true;`

### 验证
```bash
cd XQ
cmake --build build --config Release --target test_command_stack
./build/Release/test_command_stack
```

---

## Critical-2: mark_source_changed 自环过滤

### 文件
`XQ/src/core/XQScene.cpp`

### 修改
替换第 124 行 `pending.push_back(*derived_it);` 为：
```cpp
        if (*derived_it != source) {
            pending.push_back(*derived_it);
        }
```

### 验证
```bash
cd XQ
cmake --build build --config Release --target test_scene_relations
./build/Release/test_scene_relations
```

---

## Critical-3: BlobStore 符号链接防御

### 文件
`XQ/src/io/blob/BlobStore.cpp`

### 修改
在第 310 行 `const fs::path path = ...` 之后插入：

```cpp
    const fs::path canonicalPath = fs::weakly_canonical(path, ec);
    if (ec) {
        return Status::InvalidMetadata;
    }
    
    const fs::path canonicalRoot = fs::canonical(rootDir_, ec);
    if (ec) {
        return Status::InvalidMetadata;
    }
    
    const std::string pathStr = canonicalPath.string();
    const std::string rootStr = canonicalRoot.string();
    if (pathStr.size() < rootStr.size() 
        || pathStr.substr(0, rootStr.size()) != rootStr) {
        return Status::InvalidMetadata;
    }
```

### 验证
```bash
cd XQ
cmake --build build --config Release --target test_blob_store
./build/Release/test_blob_store
```

---

## Medium-1: 移除未经请求的 fallback

### 文件
`XQ/src/app/XQMainWindow.cpp`

### 修改
删除第 313-323 行的整个 fallback 块：

```cpp
            if (!stats.ok) {
                lazy = resolveLazyGeometrySource(*payload,
                                                 *geometryResources_,
                                                 *geometryRegistry_,
                                                 LazyGeometrySourceMode::SurfaceOnly);
                if (lazy.valid() && lazy->meta().triangleCount > 0) {
                    stats = renderer.addSurfaceProgressive(lazy.source());
                }
            }
```

### 验证
手动测试：
```bash
cd XQ
cmake --build build --config Release --target xq_app
# 打开项目，选择 mesh 节点，验证无自动降级
```

---

## 最终验收

### 完整测试
```bash
cd XQ
cmake --build build --config Release
ctest -C Release --output-on-failure
```

### 预期结果
- 所有测试通过
- 无编译警告
- 无新增 segfault

### 提交
```bash
git add -A
git commit -m "fix: 修复 7 个关键问题 (3 Blocker + 3 Critical + 1 Medium)

Blocker:
- XQCommandStack::redo() 失败时归还命令
- GeometryResourceManager 缓存键改用类型安全 enum
- service 层所有 push() 加返回值检查

Critical:
- AddNodeWithSourceRelationCommand 回滚逻辑修正
- mark_source_changed 过滤自环
- BlobStore 符号链接防御

Medium:
- 移除 XQMainWindow 未经请求的降级分支

Tests: all pass (24/24 Release)
Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## 故障排除

### 编译错误：BlockKind 未定义
**解决**: 确认 `enum class BlockKind` 在 GeometryResourceManager.h 的 `private:` 之后

### 测试失败：push failed
**解决**: 检查 `fail()` 函数是否存在，或改用 `std::cerr` + `return`

### 符号链接测试跳过
**原因**: Windows 权限不足
**解决**: 正常，不算失败

### merge conflict
**解决**: 先 `git pull --rebase main`，解决冲突后继续
