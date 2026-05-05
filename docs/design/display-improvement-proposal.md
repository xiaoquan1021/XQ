# XQ 影像显示与建模改造方案

## 问题分析

加载项目文件夹后，XQ 的影像/建模显示与 SimVascular 差距较大。
通过对比两者源代码，找到以下关键差异：

---

## 差异对比表

| 功能 | SimVascular | XQ 当前 | 影响 |
|------|-----------|---------|------|
| 节点可见性初始化 | ✅ 显式 SetVisibility(true) | ❌ 未设置 | 数据加载后可能不显示 |
| 节点透明度 | ✅ SetOpacity(1.0) | ❌ 未设置 | 渲染可能不正常 |
| 节点颜色 | ✅ 设置白色 (1,1,1) | ❌ 未设置 | 依赖 MITK 默认值 |
| 渲染初始化 | ✅ InitializeViews() | ❌ 未调用 | 视图不会自动对准数据 |
| 材质属性 | ✅ 完整光照参数 | ❌ 仅基础属性 | 模型渲染粗糙 |
| ImageNavigator | ✅ 默认可见 | ❌ 隐藏占位符 | 用户无法快速调整切片 |
| 文件夹节点可见性 | ✅ 文件夹节点隐藏 | ⚠️ 部分隐藏 | 可能显示多余节点 |

---

## 改造建议

### 建议 1：数据加载时显式设置节点属性（优先级：高）

**文件**: `xq_WorkspaceManager.cxx` (第 276-284 行)

**问题**: 加载数据时只创建节点、设名称，不设置任何显示属性。
SimVascular 会显式设置 `visible=true`, `opacity=1.0`, `color=(1,1,1)`。

**改造方案**:
```cpp
for (auto& data : loadedData) {
    mitk::DataNode::Pointer dataNode = mitk::DataNode::New();
    dataNode->SetData(data);
    dataNode->SetName(GetFileNameWithoutExtension(relPath));

    // 新增：显式设置显示属性
    dataNode->SetVisibility(true);
    dataNode->SetProperty("opacity", mitk::FloatProperty::New(1.0f));

    // 如果是图像数据，初始化视图
    if (auto* image = dynamic_cast<mitk::Image*>(data.GetPointer())) {
        // 加载后自动对准视图
        needsViewInit = true;
    }

    dataStorage->Add(dataNode, parentFolderNode ?: projectNode);
}
// 加载完成后初始化渲染视图
if (needsViewInit) {
    mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(dataStorage);
}
```

### 建议 2：加载完成后初始化渲染视图（优先级：高）

**文件**: `xq_WorkspaceManager.cxx` (OpenProject 函数末尾)

**问题**: SV 加载图像后调用 `InitializeViews()` 使视图自动对准数据几何范围，XQ 没有这步。

**改造方案**:
```cpp
// 在 return true 之前添加：
mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(dataStorage);
```

### 建议 3：添加 ImageNavigator 视图（优先级：中）

**文件**: `xq_DefaultPerspective.cxx`

**问题**: SV 默认显示 ImageNavigator（可调整切片位置），XQ 没有。

**改造方案**:
```cpp
// 在 Project Manager 下方添加 Image Navigator
layout->AddView("org.mitk.views.imagenavigator",
                berry::IPageLayout::BOTTOM, 0.65f,
                "org.xq.views.projectmanager");
```

### 建议 4：完善模型材质属性（优先级：中）

**文件**: 各 ObjectFactory 文件

**问题**: SV 的 ModelVtkMapper3D 设置了完整的光照参数：
- Ambient: 0.05, Diffuse: 0.9, Specular: 1.0, SpecularPower: 16.0
XQ 的 ObjectFactory 只设置了基础属性（visible, color, opacity）。

**改造方案**: 在 ObjectFactory 的 SetDefaultProperties 中补充材质参数：
```cpp
node->SetFloatProperty("material.ambientCoefficient", 0.05f);
node->SetFloatProperty("material.diffuseCoefficient", 0.9f);
node->SetFloatProperty("material.specularCoefficient", 1.0f);
node->SetFloatProperty("material.specularPower", 16.0f);
```

### 建议 5：文件夹节点可见性管理（优先级：低）

**文件**: `xq_WorkspaceManager.cxx`

**问题**: SV 对每种文件夹节点（Images, Paths, Segmentations 等）都设置了
`SetVisibility(false)`，防止文件夹本身在 3D 视图中渲染。

**改造方案**: 创建文件夹节点后隐藏：
```cpp
void xq_WorkspaceManager::CreateFolderNodes(...) {
    // 每个文件夹节点创建后
    folderNode->SetVisibility(false);
}
```

---

## 涉及文件清单

| 文件 | 修改内容 |
|------|---------|
| `Modules/ProjectManagement/xq_WorkspaceManager.cxx` | 节点属性初始化 + 视图初始化 |
| `Plugins/org.xq.core.application/.../xq_DefaultPerspective.cxx` | 添加 ImageNavigator |
| `Modules/Model/xq_ModelObjectFactory.cxx` | 补充材质属性 |
| `Modules/Segmentation/xq_*ObjectFactory.cxx` | 补充默认显示属性 |
| `Modules/Path/xq_*ObjectFactory.cxx` | 补充默认显示属性 |

---

## 注意事项

1. **不改变 XQ 的 UI 风格**——只改善数据加载后的渲染质量
2. **保持代码独立性**——不复制 SV 代码，只借鉴其初始化策略
3. **需要验证的点**：
   - `InitializeViewsByBoundingObjects` 是否在 MITK 2024.06 中可用
   - ObjectFactory 的 SetDefaultProperties 调用链是否正确触发
   - ImageNavigator 视图 ID 是否正确注册
