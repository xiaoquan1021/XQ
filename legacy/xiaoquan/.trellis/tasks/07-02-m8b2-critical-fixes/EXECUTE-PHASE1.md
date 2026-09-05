# Phase 1: Blocker 修复执行指令

> 本文档是**完整可执行的指令**，可直接交给便宜 agent 执行。
> 每个修复包含：确切位置、原代码、新代码、验证步骤。

---

## 任务目标

修复 3 个 Blocker 级问题，使分支可以合并。

---

## Blocker-1: 修复 XQCommandStack::redo() 丢失命令

### 文件位置
`XQ/src/core/command/XQCommandStack.cpp`

### 当前代码 (行 44-57)
```cpp
bool XQCommandStack::redo()
{
    if (redo_stack_.empty()) {
        return false;
    }

    std::unique_ptr<XQCommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!command->execute()) {
        return false;
    }
    undo_stack_.push_back(std::move(command));
    return true;
}
```

### 修改后代码
```cpp
bool XQCommandStack::redo()
{
    if (redo_stack_.empty()) {
        return false;
    }

    std::unique_ptr<XQCommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!command->execute()) {
        redo_stack_.push_back(std::move(command));  // ← 新增：归还命令
        return false;
    }
    undo_stack_.push_back(std::move(command));
    return true;
}
```

### 执行步骤

1. 打开文件 `XQ/src/core/command/XQCommandStack.cpp`
2. 定位到第 52 行 `if (!command->execute()) {`
3. 在第 53 行 `return false;` **之前**插入新行：
   ```cpp
   redo_stack_.push_back(std::move(command));
   ```
4. 保存文件

### 新增测试

**文件**: `XQ/tests/core/test_command_stack.cpp`

**在文件末尾 `return 0;` 之前插入**:

```cpp
    // redo() failure must preserve the redo stack
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        
        // 设置场景：命令入栈并 undo
        if (!stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::AddNodeCommand(&scene, make_node(1, xq::XQDomainType::Image, "img", "Images/x.vti"))))) {
            return fail("redo test fixture push failed", __LINE__);
        }
        if (!stack.undo()) {
            return fail("redo test fixture undo failed", __LINE__);
        }
        if (!stack.can_redo()) {
            return fail("redo test fixture has redo entry", __LINE__);
        }
        
        // 外部插入相同 id 的节点，使 redo 必然失败
        scene.insert(make_node(1, xq::XQDomainType::Image, "external", "Images/external.vti"));
        
        const std::size_t redo_count_before = stack.redo_count();
        
        // redo 应该失败
        if (stack.redo()) {
            return fail("redo with conflicting node should fail", __LINE__);
        }
        
        // 关键断言：redo 失败后，命令仍在 redo_stack
        if (stack.redo_count() != redo_count_before) {
            return fail("failed redo must preserve redo stack", __LINE__);
        }
        
        // 确认外部节点未被破坏
        const xq::XQDataNode* node = scene.find(xq::NodeId(1));
        if (node == nullptr || node->display_name() != "external") {
            return fail("failed redo must not corrupt external node", __LINE__);
        }
    }
```

### 验证步骤

```bash
cd XQ
cmake --build build --config Release --target test_command_stack
./build/Release/test_command_stack
# 输出应包含：All tests passed
```

如果失败，检查：
1. `redo_stack_.push_back` 是否在 `return false` 之前
2. 是否用 `std::move(command)`（不是拷贝）
3. 新测试是否正确插入到 `return 0;` 之前

---

## Blocker-2: 修复 GeometryResourceManager 缓存键冲突

### 步骤 1: 定义 BlockKind 枚举

**文件**: `XQ/src/services/resource/GeometryResourceManager.h`

**在第 158 行 `private:` 之后立即插入**:

```cpp
    // 缓存块类型标签，确保 voxel 和 geometry 键空间隔离
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

**替换第 161 行**:

原代码:
```cpp
typedef std::pair<AssetId, int> ResidentKey;
```

新代码:
```cpp
typedef std::tuple<AssetId, BlockKind> ResidentKey;
```

### 步骤 2: 更新缓存键生成函数

**文件**: `XQ/src/services/resource/GeometryResourceManager.cpp`

**删除第 18-27 行的 constexpr 常量**:
```cpp
constexpr int kResidentVoxelFull = 0;
constexpr int kResidentVoxelSegmented = 1;
constexpr int kResidentGeometryAllFull = 10;
constexpr int kResidentGeometryAllSegmented = 11;
constexpr int kResidentGeometrySurfaceFull = 20;
constexpr int kResidentGeometrySurfaceSegmented = 21;
constexpr int kResidentGeometryTetFull = 30;
constexpr int kResidentGeometryTetSegmented = 31;
```

**替换 `voxelBlockKey` 函数 (约第 71-75 行)**:

原代码:
```cpp
GeometryResourceManager::ResidentKey
GeometryResourceManager::voxelBlockKey(const AssetId& id,
                                       const VoxelSourceSpec& spec)
{
    return ResidentKey(id, spec.segmented ? kResidentVoxelSegmented
                                          : kResidentVoxelFull);
}
```

新代码:
```cpp
GeometryResourceManager::ResidentKey
GeometryResourceManager::voxelBlockKey(const AssetId& id,
                                       const VoxelSourceSpec& spec)
{
    return ResidentKey(id, spec.segmented ? BlockKind::VoxelSegmented
                                          : BlockKind::VoxelFull);
}
```

**替换 `geometryBlockKey` 函数 (约第 77-91 行)**:

原代码:
```cpp
GeometryResourceManager::ResidentKey
GeometryResourceManager::geometryBlockKey(const AssetId& id,
                                          const GeometrySourceSpec& spec)
{
    int kind = spec.segmented ? kResidentGeometryAllSegmented
                              : kResidentGeometryAllFull;
    if (spec.hasSurface && !spec.hasTet) {
        kind = spec.segmented ? kResidentGeometrySurfaceSegmented
                              : kResidentGeometrySurfaceFull;
    } else if (spec.hasTet && !spec.hasSurface) {
        kind = spec.segmented ? kResidentGeometryTetSegmented
                              : kResidentGeometryTetFull;
    }
    return ResidentKey(id, kind);
}
```

新代码:
```cpp
GeometryResourceManager::ResidentKey
GeometryResourceManager::geometryBlockKey(const AssetId& id,
                                          const GeometrySourceSpec& spec)
{
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
}
```

### 步骤 3: 添加类型安全断言

**在 `acquireVoxelSource` 函数中** (约第 180 行，缓存命中分支):

在这行之后:
```cpp
if (it != blocks_.end()) {
```

插入断言:
```cpp
    // 类型安全检查：确保缓存的是 voxel 源
    if (it->second.voxelSource == nullptr) {
        // 键冲突，不应该发生，但防御性清理
        blocks_.erase(it);
        // 继续执行缓存未命中逻辑
    } else {
```

然后把原来的 `return VoxelSourceHandle(...)` 移到 `else` 块内，并在最后加 `}`。

**在 `acquireGeometrySource` 函数中** (约第 245 行，缓存命中分支):

类似地插入:
```cpp
    if (it->second.geometrySource == nullptr) {
        blocks_.erase(it);
    } else {
```

### 步骤 4: 新增测试

**文件**: `XQ/tests/services/resource/GeometryResourceManagerTest.cpp`

**在文件末尾 `return 0;` 之前插入**:

```cpp
    // 混合缓存：同一 AssetId 的 voxel 和 geometry 不冲突
    {
        xq::AssetRegistry registry;
        const xq::AssetId voxelAssetId(100);
        const xq::AssetId geoAssetId(100);  // 相同 id
        
        // 创建 voxel asset
        const xq::AssetRecord* voxelRec = registry.createAsset(
            voxelAssetId, xq::XQDomainType::SegmentationMask);
        xq::BufferRef voxelRef = makeVoxelRef(rootDir + "/voxels_100.bin", 8000);
        registry.bindBlob(voxelAssetId, "voxels", voxelRef);
        
        // 创建 geometry asset（相同 id）
        const xq::AssetRecord* geoRec = registry.createAsset(
            geoAssetId, xq::XQDomainType::SurfaceModel);
        xq::BufferRef ptsRef = makePointsRef(rootDir + "/points_100.bin", 100);
        xq::BufferRef trisRef = makeTrianglesRef(rootDir + "/tris_100.bin", 50);
        xq::BufferRef faceRef = makeFaceIdRef(rootDir + "/faceId_100.bin", 50);
        registry.bindBlob(geoAssetId, "points", ptsRef);
        registry.bindBlob(geoAssetId, "tris", trisRef);
        registry.bindBlob(geoAssetId, "faceId", faceRef);
        
        // 写实际数据
        std::vector<std::uint8_t> voxelData(8000, 1);
        writeRawFile(rootDir + "/voxels_100.bin", voxelData);
        
        std::vector<double> points = makeTestPoints(100);
        std::vector<int> tris = makeTestTriangles(50);
        std::vector<int> faceIds = makeTestFaceIds(50);
        writeRawFile(rootDir + "/points_100.bin", 
                     reinterpret_cast<const char*>(points.data()), 
                     points.size() * sizeof(double));
        writeRawFile(rootDir + "/tris_100.bin", 
                     reinterpret_cast<const char*>(tris.data()), 
                     tris.size() * sizeof(int));
        writeRawFile(rootDir + "/faceId_100.bin", 
                     reinterpret_cast<const char*>(faceIds.data()), 
                     faceIds.size() * sizeof(int));
        
        xq::GeometryResourceManager mgr(&registry, rootDir);
        
        // 先 acquire voxel
        xq::GeometryResourceManager::VoxelSourceSpec voxelSpec;
        voxelSpec.dims[0] = 20;
        voxelSpec.dims[1] = 20;
        voxelSpec.dims[2] = 20;
        voxelSpec.type = xq::ScalarType::UInt8;
        voxelSpec.components = 1;
        voxelSpec.segmented = false;
        
        auto voxelHandle = mgr.acquireVoxelSource(voxelAssetId, voxelSpec);
        if (!voxelHandle.valid()) {
            return fail("mixed cache: voxel acquire failed", __LINE__);
        }
        
        // 再 acquire geometry（相同 AssetId）
        xq::GeometryResourceManager::GeometrySourceSpec geoSpec;
        geoSpec.pointCount = 100;
        geoSpec.triCount = 50;
        geoSpec.hasSurface = true;
        geoSpec.hasTet = false;
        geoSpec.segmented = false;
        
        auto geoHandle = mgr.acquireGeometrySource(geoAssetId, geoSpec);
        if (!geoHandle.valid()) {
            return fail("mixed cache: geometry acquire failed", __LINE__);
        }
        
        // 关键断言：两个 handle 都有效
        if (mgr.blockCount() != 2) {
            return fail("mixed cache: should have 2 distinct blocks", __LINE__);
        }
        
        // 验证不会互相干扰
        auto voxelLease = voxelHandle->acquire_whole();
        auto geoLease = geoHandle->acquire_points();
        
        if (!voxelLease.valid() || !geoLease.valid()) {
            return fail("mixed cache: leases should both be valid", __LINE__);
        }
    }
```

### 验证步骤

```bash
cd XQ
cmake --build build --config Release --target GeometryResourceManagerTest
./build/Release/GeometryResourceManagerTest
# 输出应包含：All tests passed
```

如果编译失败，检查：
1. `BlockKind` 是否在 `private:` 之后定义
2. `ResidentKey` 是否改为 `std::tuple<AssetId, BlockKind>`
3. 所有 `kResident*` 常量是否已删除
4. `BlockKind::` 前缀是否正确

---

## Blocker-3: 修复 service 层 push() 返回值未检查

### 需要修改的文件列表

1. `XQ/tests/services/segmentation/SegmentationServiceTest.cpp`
2. `XQ/tests/services/meshing/VolumeMeshServiceTest.cpp`
3. `XQ/tests/services/path/PathServiceTest.cpp`
4. `XQ/tests/services/ai/FlowMetricsServiceTest.cpp`
5. `XQ/tests/services/ai/AiServiceTest.cpp`
6. `XQ/tests/services/modeling/ModelingServiceTest.cpp`
7. `XQ/tests/services/flow/FlowSolver1DTest.cpp` (如果有 push)
8. `XQ/tests/app/WorkflowIntegrationTest.cpp`

### 统一修改模式

**查找模式**:
```cpp
stack.push(std::move(cmd.command));
```

**替换为**:
```cpp
if (!stack.push(std::move(cmd.command))) {
    return fail("command push failed", __LINE__);
}
```

**或者** (如果在 void 函数中):
```cpp
if (!stack.push(std::move(cmd.command))) {
    std::cerr << "FAIL: command push failed at line " << __LINE__ << std::endl;
    return;
}
```

### 详细修改指令

#### 文件 1: SegmentationServiceTest.cpp

**位置**: 约第 599 行和第 642 行

原代码:
```cpp
stack.push(std::move(cmd.command));
```

新代码:
```cpp
if (!stack.push(std::move(cmd.command))) {
    return fail("segmentation command push failed", __LINE__);
}
```

#### 文件 2: VolumeMeshServiceTest.cpp

**位置**: 约第 301 行

原代码:
```cpp
stack.push(std::move(cmd.command));
```

新代码:
```cpp
if (!stack.push(std::move(cmd.command))) {
    return fail("volume mesh command push failed", __LINE__);
}
```

#### 文件 3: PathServiceTest.cpp

**位置**: 约第 116, 152, 161, 190, 212, 241, 264 行

**全部替换** (使用 sed 或编辑器全局替换):

查找:
```
stack\.push\(std::move\(([^)]+)\.command\)\);
```

替换为:
```cpp
if (!stack.push(std::move($1.command))) {
    return fail("path command push failed", __LINE__);
}
```

#### 文件 4-6: 其他 service 测试

**使用相同模式**，将所有 `stack.push(std::move(...))` 改为带检查的版本。

### 自动化脚本

创建 `XQ/fix_push_checks.py`:

```python
import re
import sys
from pathlib import Path

def fix_push_calls(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 匹配 stack.push(std::move(...));
    pattern = r'(\s+)stack\.push\(std::move\(([^)]+)\.command\)\);'
    
    def replace(match):
        indent = match.group(1)
        var = match.group(2)
        return f'''{indent}if (!stack.push(std::move({var}.command))) {{
{indent}    return fail("command push failed", __LINE__);
{indent}}}'''
    
    new_content = re.sub(pattern, replace, content)
    
    if new_content != content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Fixed: {file_path}")
        return True
    return False

if __name__ == '__main__':
    test_dir = Path('tests/services')
    files = list(test_dir.rglob('*Test.cpp'))
    files.extend(Path('tests/app').rglob('*Test.cpp'))
    
    fixed = 0
    for file in files:
        if fix_push_calls(file):
            fixed += 1
    
    print(f"Total fixed: {fixed} files")
```

**运行**:
```bash
cd XQ
python fix_push_checks.py
```

### 验证步骤

```bash
cd XQ
# 编译所有 service 测试
cmake --build build --config Release --target all_tests

# 运行
ctest --output-on-failure -C Release -R "Service|Workflow"
```

如果有测试失败，检查：
1. `fail()` 函数是否存在（所有测试文件都应有）
2. 是否遗漏了某些 `push()` 调用
3. 是否在 void 函数中使用了 `return fail(...)`（应该改为打印+return）

---

## 完成 Phase 1 后的验收清单

- [ ] `XQCommandStack::redo()` 已修复，新测试通过
- [ ] `GeometryResourceManager` 缓存键已改为 `BlockKind` enum
- [ ] 混合缓存测试通过
- [ ] 所有 service 层 `push()` 调用已加检查
- [ ] 所有现有测试通过：
  ```bash
  cd XQ
  cmake --build build --config Release
  ctest -C Release --output-on-failure
  ```
- [ ] 无编译警告
- [ ] git diff 确认所有修改符合预期

---

## 如果遇到问题

### 编译错误：找不到 BlockKind

**原因**: `BlockKind` 定义位置错误或缺少作用域

**解决**:
1. 确认 `BlockKind` 在 `GeometryResourceManager.h` 的 `private:` 之后
2. 使用时必须加 `BlockKind::` 前缀

### 测试失败：assertion failed

**原因**: 测试逻辑错误或修复不完整

**解决**:
1. 运行单个测试查看详细输出
2. 检查修改的代码是否完全匹配本文档
3. 使用 gdb 调试：`gdb --args ./build/Release/test_command_stack`

### Python 脚本报错

**原因**: 文件路径或编码问题

**解决**:
1. 手动替换，不使用脚本
2. 用编辑器的"查找替换"功能（支持正则表达式）

---

## Phase 1 完成后

提交代码:
```bash
git add -A
git commit -m "fix(core): 修复 3 个 Blocker — redo 丢命令/缓存键冲突/push 未检查

- XQCommandStack::redo() 失败时归还命令到 redo_stack_
- GeometryResourceManager 缓存键改用类型安全的 BlockKind enum
- service 层所有 push() 调用加返回值检查

Fixes: #blocker-1, #blocker-2, #blocker-3
Tests: test_command_stack, GeometryResourceManagerTest, all service tests"
```

然后继续 Phase 2: Critical 修复。
