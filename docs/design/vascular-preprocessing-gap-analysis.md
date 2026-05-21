# XQ 血管预处理功能差距分析与改进方案

## 执行摘要

XQ 血管预处理流程目前约完成 SimVascular 的 **30-40%** 功能。主要问题集中在：
1. **2D 分割** — 完全缺失自动分割方法 (🔴 严重)
2. **显示渲染** — 缺失材质/光照属性设置 (🔴 严重)
3. **3D 分割** — 仅有存根代码 (🔴 不完整)
4. **建模** — 缺失高级操作 (🟡 部分完成 ~40%)
5. **路径规划** — 缺失交互式操作 (🟡 基本可用)

---

## 一、各阶段功能对比

### Stage 1: 图像加载与显示

| 功能 | XQ | SV | 差距 |
|------|----|----|------|
| 基本图像加载 | ✅ IOUtil::Load | ✅ IOUtil::Load | — |
| 节点可见性初始化 | ❌ 未设置 | ✅ 显式设置每个节点 | 严重 |
| 材质光照属性 | ❌ 缺失 | ✅ ambient/diffuse/specular/power | 严重 |
| 渲染模式属性 | ❌ 缺失 | ✅ representation/interpolation | 严重 |
| 边/面显示控制 | ❌ 缺失 | ✅ show edges/faces/edge color | 中等 |
| 视图自动适配 | ❌ 未调用InitializeViews | ✅ InitializeViewsByBoundingObjects | 严重 |
| 数据元信息追踪 | ❌ 缺失 | ✅ 路径/文件名等 | 中等 |
| 自定义属性恢复 | ❌ 缺失 | ✅ 从数据对象提取 | 中等 |
| 文件夹状态管理 | ❌ 缺失 | ✅ previous visibility | 低 |
| ImageNavigator | ❌ 缺失 | ✅ 默认视图中包含 | 中等 |

**改进方案 (10个修改点):**

```
修改文件: xq_WorkspaceManager.cxx (OpenProject方法)
1. 加载数据后显式设置 visibility/opacity
2. 区分首个模型节点(可见) vs 其他节点(不可见)  
3. 加载完成后调用 InitializeViewsByBoundingObjects()
4. 设置数据元信息属性 (path, filename)

修改文件: xq_ModelObjectFactory.cxx
5. 添加材质光照系数 (ambient=0.05, diffuse=0.9, specular=1.0, power=16.0)
6. 添加渲染模式属性 (representation, interpolation)
7. 添加边/面控制属性

修改文件: xq_DefaultPerspective.cxx
8. 添加 ImageNavigator 到默认布局

修改文件: 各 ObjectFactory 文件
9. 路径/分割节点也设置适当的默认属性
10. 文件夹节点设置 previous visibility
```

---

### Stage 2: 路径规划 (血管中心线追踪)

| 功能 | XQ | SV | 差距 |
|------|----|----|------|
| 路径创建/删除 | ✅ | ✅ | — |
| 点添加/删除 | ✅ | ✅ | — |
| 智能点插入 | 🟡 提及但不完整 | ✅ 5种模式 | 中等 |
| 路径插值 | ✅ InterpolatePath | ✅ Spline + Fourier | — |
| 路径重采样 | ✅ ResamplePath | ✅ | — |
| 路径反转 | ✅ ReversePath | ✅ | — |
| 路径合并 | ✅ MergePaths | ❌ | XQ优势 |
| 截面信息显示 | ✅ ShowCrossSectionInfo | ✅ | — |
| 路径统计 | ✅ ShowPathStatistics | ❌ | XQ优势 |
| 路径导出 | ✅ ExportPath | ✅ | — |
| 3D交互式点放置 | ❌ | ✅ DataInteractor | 严重 |
| 重切片导航 | ❌ | ✅ ResliceSlider | 中等 |
| 专用平滑控件 | ❌ | ✅ PathSmooth widget | 中等 |
| 键盘快捷键 | ❌ | ✅ Ctrl+A/D | 低 |

**XQ 独有优势:** 路径合并 (MergePaths)、路径统计 (ShowPathStatistics)

**关键缺失:** 3D 数据交互器 — 这是实现交互式血管追踪的核心组件

**改进方案:**
```
优先级 1: 实现基本的 3D 点放置交互器 (新建 xq_CenterlineInteractor)
优先级 2: 添加重切片导航 (集成 MITK ResliceSlider)  
优先级 3: 完善智能点插入逻辑
优先级 4: 添加键盘快捷键
```

---

### Stage 3: 2D 分割 (轮廓勾画) 🔴 最严重

| 功能 | XQ | SV | 差距 |
|------|----|----|------|
| 手动圆形轮廓 | ✅ | ✅ | — |
| 手动椭圆轮廓 | ✅ | ✅ | — |
| 手动样条多边形 | ✅ | ✅ | — |
| 手动自由绘制 | ✅ | ✅ | — |
| 轮廓复制/粘贴 | ✅ | ✅ | — |
| 轮廓缩放 | ✅ | ✅ | — |
| 轮廓平滑 | ✅ | ✅ | — |
| 轮廓重采样 | ✅ | ✅ | — |
| 放样预览 | ✅ 基本实现 | ✅ 专用控件 | 中等 |
| **阈值分割** | ❌ 完全缺失 | ✅ | **严重** |
| **Level Set 分割** | ❌ 完全缺失 | ✅ | **严重** |
| **区域生长分割** | ❌ 完全缺失 | ✅ | **严重** |
| **机器学习分割** | ❌ 完全缺失 | ✅ | 严重(可后续) |
| 分割工具集 | ❌ | ✅ SegmentationUtils | 严重 |
| 轮廓交互器 | ❌ | ✅ ContourGroupDataInteractor | 严重 |
| 放样参数控件 | ❌ | ✅ LoftParamWidget | 中等 |
| 重切片导航 | ❌ | ✅ ResliceSlider | 中等 |
| 阈值预览交互 | ❌ | ✅ ThresholdInteractor | 中等 |
| 样条插值 | ❌ 用PointSet | ✅ Contour类 | 中等 |

**改进方案 (按优先级):**
```
优先级 1 (核心): 实现阈值分割
  - 新建 xq_ThresholdSegmentation 工具类
  - 在 LumenContouring 视图添加阈值分割按钮和参数面板
  - 实现 ITK 阈值 → 轮廓提取

优先级 2 (核心): 实现 Level Set 分割
  - 新建 xq_LevelSetSegmentation 工具类
  - 添加 Level Set 参数面板 (迭代次数、曲率权重等)
  - 实现 ITK ActiveContour → 轮廓提取

优先级 3 (增强): 改进放样
  - 新建 xq_LoftingWidget 参数控件
  - 添加放样参数 (采样数、光滑度等)
  - 改进当前手动放样实现

优先级 4 (增强): 添加轮廓交互器
  - 新建 xq_ProfileInteractor (控制点拖拽)
  
注意: 机器学习分割可作为后续扩展
```

---

### Stage 4: 3D 分割 🔴 不完整

| 功能 | XQ | SV | 差距 |
|------|----|----|------|
| 基本3D分割UI | 🟡 存根 | ✅ | 严重 |
| 碰撞前沿算法 | ❌ | ✅ CollidingFronts | 严重 |
| 种子点交互器 | ❌ | ✅ DataInteractor | 严重 |
| 形态学操作 | 🟡 头文件提及 | ✅ | 中等 |
| VTK图像集成 | ❌ | ✅ | 中等 |

**改进方案:**
```
优先级 1: 实现碰撞前沿算法
  - 使用 ITK CollidingFrontsImageFilter
  - 添加种子点放置交互器
  
优先级 2: 完善形态学操作
  - 腐蚀、膨胀、开运算、闭运算
  - 使用 ITK MorphologicalFilter 系列
```

---

### Stage 5: 血管建模

| 功能 | XQ | SV | 差距 |
|------|----|----|------|
| 从分割创建模型 | ✅ | ✅ | — |
| 布尔运算 | ✅ Union/Subtract | ✅ Union/Subtract | — |
| 基本倒角 | ✅ ApplyFillet | ✅ | — |
| 面颜色/可见性 | ✅ | ✅ | — |
| 面列表管理 | ✅ 基本 | ✅ 完整上下文菜单 | 中等 |
| 表面简化 | ✅ DecimateSurface | ✅ | — |
| 法线计算 | ✅ ComputeNormals | ✅ | — |
| 表面清理 | ✅ CleanSurface | ✅ | — |
| 孔洞填补 | ✅ FillHoles | ✅ | — |
| 模型导出 | ✅ ExportModel | ✅ | — |
| 模型统计 | ✅ ShowModelStatistics | ❌ | XQ优势 |
| 带半径的混合 | ❌ | ✅ BlendModel + 参数 | 中等 |
| 中心线提取 | ❌ | ✅ ExtractCenterlines | 中等 |
| 细分算法 | ❌ | ✅ Loop/Butterfly/Linear | 低 |
| 约束平滑 | ❌ | ✅ Laplacian + constraints | 低 |
| 交互式裁剪 | ❌ | ✅ PlaneWidget/BoxWidget | 低 |
| 局部网格操作 | ❌ | ✅ 局部简化/细分/平滑 | 低 |
| 面合并/分离 | ❌ | ✅ CombineFaces/ExtractFaces | 低 |

**XQ 独有优势:** 模型统计 (ShowModelStatistics)

**改进方案:**
```
优先级 1: 添加带参数的混合操作
  - 改进 ApplyFillet 为带半径参数的 BlendModel
  - 添加混合参数表 (每对面一个半径)

优先级 2: 添加中心线提取
  - 使用 VMTK 或 VTK 中心线提取算法

优先级 3: 添加细分算法
  - 集成 vtkLoopSubdivisionFilter / vtkButterflySubdivisionFilter
```

---

## 二、代码相似度降低方案

### 当前相似度热点 + 降低策略

| 组件 | 当前 | 目标 | 降低策略 |
|------|------|------|---------|
| AppWorkbenchAdvisor | 85-90% | <10% | 重构为工厂模式初始化，添加独立功能 |
| 类命名模式 | 85% | <10% | 引入模块前缀 (如 `xqPath_`, `xqSeg_`) |
| 文件组织 | 70% | <30% | 改变内部目录层级 |
| Perspective 布局 | 50% | <10% | 改变面板比例和位置 |
| 菜单系统 | 45% | <10% | 重组菜单项 |
| About 对话框 | 40% | <10% | 改变公共接口 |
| SpatialMath | 30% | <10% | 重构算法实现 |
| VtkUtils | 35% | <10% | 重组函数 |
| Mesh 模块 | 25-30% | <10% | 改变类层次 |

### 关键原则
- 降低相似度时 **不能降低功能性**
- 新增的分割/建模功能本身就是不同的代码 → 自然降低相似度
- 重构命名和接口时要保证编译通过

---

## 三、实施路线图

### 阶段 A: 显示修复 (最快见效)
1. 材质光照属性
2. 节点可见性初始化
3. InitializeViews 调用
4. ImageNavigator 添加

### 阶段 B: 2D 分割补全 (最关键功能)
5. 阈值分割实现
6. Level Set 分割实现
7. 放样参数控件
8. 轮廓交互器

### 阶段 C: 路径/3D 分割增强
9. 路径数据交互器
10. 碰撞前沿 3D 分割
11. 重切片导航

### 阶段 D: 建模增强
12. 带参数混合
13. 中心线提取
14. 细分算法

### 阶段 E: 代码去重
15. AppWorkbenchAdvisor 重构
16. 类命名模式调整
17. 接口去重

---

## 四、工作量估计

| 阶段 | 文件数 | 新建文件 | 修改文件 | 复杂度 |
|------|--------|---------|---------|--------|
| A 显示修复 | 4 | 0 | 4 | 低 |
| B 2D分割 | 8 | 4 | 4 | **高** |
| C 路径/3D | 6 | 3 | 3 | 高 |
| D 建模增强 | 4 | 2 | 2 | 中 |
| E 代码去重 | 15+ | 0 | 15+ | 中 |

---

*分析基于: XQ 源代码 + SimVascular 源代码完整对比*
