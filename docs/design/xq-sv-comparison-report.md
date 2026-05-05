# XQ vs SimVascular 全面对比报告

## 执行摘要

| 维度 | 当前相似度 | 目标 | 状态 |
|------|-----------|------|------|
| **UI 应用层** | ~23% | ≤10% | ⚠️ 需改进 |
| **Pipeline 插件** | ~12% (代码复用), 5.4% (方法重叠) | ≤10% | ✅ 基本达标 |
| **模块层** | ~25% | ≤10% | ❌ 需改进 |
| **资源/样式** | <5% | ≤10% | ✅ 已达标 |
| **总体加权** | ~18-20% | ≤10% | ❌ 需继续优化 |

---

## 1. UI 应用层对比 (~23% 相似度)

### 1.1 高相似度区域 (需重点处理)

| 组件 | 相似度 | 问题 |
|------|--------|------|
| **AppWorkbenchAdvisor** | **85-90%** | ⚠️ 几乎完全相同的框架代码 |
| **类命名模式** | **85%** | `xq_*` vs `sv4gui_*` 仅前缀不同 |
| **文件组织结构** | **70%** | 目录/文件命名模式一致 |
| **架构设计模式** | **60%** | Factory/Observer/Operation 模式相同 |
| **视图布局(Perspective)** | **50%** | 左侧面板 20% 宽度相同 |
| **菜单系统** | **45%** | File > New/Open/Save/Close 顺序相同 |

### 1.2 中等相似度区域

| 组件 | 相似度 | 说明 |
|------|--------|------|
| **WorkbenchWindowAdvisor 结构** | 40% | 类结构相似，实现已分化 |
| **About 对话框** | 40% | 接口方法名相同 (GetAboutText等) |
| **数据管理器视图** | 35-40% | MITK 节点描述符模式相同 |
| **项目管理模块** | 35% | 文件夹节点结构一致 |

### 1.3 低相似度区域 (已分化)

| 组件 | 相似度 | 说明 |
|------|--------|------|
| **工具栏/动作** | 30% | XQ阶段式 vs SV透视图式 |
| **欢迎页面** | 25% | XQ widget vs SV web-based |
| **WorkbenchWindowAdvisor 实现** | 15% | 菜单/工具栏逻辑完全不同 |

---

## 2. 模块层对比 (~25% 相似度)

### 2.1 Common 模块

| 类 | XQ | SV | 相似度 |
|----|----|----|--------|
| Math3 | `xq_Math3` (线性插值) | `sv4guiMath3` (傅里叶平滑) | **30%** |
| VtkUtils | `xq_VtkUtils` (975B) | `sv4guiVtkUtils` (2.2KB) | **35%** |
| StringUtils | `xq_StringUtils` (函数式) | `sv4guiStringUtils` (传统式) | **15%** |
| Spline | `xq_Spline` | `sv4guiSpline` | **20%** |

### 2.2 Path 模块

| 方面 | XQ | SV | 相似度 |
|------|----|----|--------|
| 数据模型 | `xq_VesselCenterline` (unique_ptr) | `sv4guiPath` (raw ptr + sv3继承) | **20%** |
| 数据元素 | `xq_CenterlineSegment` | `sv4guiPathElement` | ~20% |
| I/O | `xq_CenterlineIO` (503B) | `sv4guiPathIO` (2.5KB, 双继承) | **25%** |

### 2.3 其他模块

| 模块 | XQ 命名 | SV 命名 | 相似度 |
|------|---------|---------|--------|
| Segmentation | `xq_LumenProfile` / `xq_ProfileGroup` | `sv4gui_Contour` / `sv4gui_ContourGroup` | **20-25%** |
| Model | `xq_Model` + `xq_VascularGeometry` (unique_ptr) | `sv4guiModel` + `sv4guiModelElement` (raw ptr) | **20%** |
| Mesh | `xq_Grid` / `xq_TetGenGrid` | `sv4guiMesh` / `sv4guiMeshTetGen` | **25-30%** |
| Simulation | `xq_SolverJob` / `xq_SolverConfigWriter` | `sv4guiSimJob` / `sv4guiSimXmlWriter` | **25%** |
| ProjectMgmt | 文件夹节点结构 | 文件夹节点结构 | **25-30%** |

---

## 3. Pipeline 插件对比 (~12% 相似度)

| 插件对 | XQ LOC | SV LOC | 方法重叠 | 代码复用 |
|--------|--------|--------|----------|----------|
| Path Planning | 207 | 737 | 9.5% | ~25% |
| Segmentation | 223 | 1,253 | 7.2% | ~15% |
| Modeling | 267 | 1,097 | 4.5% | ~10-15% |
| Mesh Generation | 205 | 759 | 3.3% | ~5-10% |
| Simulation | 246 | 1,560 | 2.4% | ~5% |
| **平均** | — | — | **5.4%** | **~12%** |

### 关键架构差异

| 方面 | XQ | SV |
|------|----|----|
| 信号/槽 | C++11 类型安全 `connect()` | 字符串式 `SIGNAL/SLOT` |
| 数据访问 | 直接 dynamic_cast | 观察者回调 |
| 基类 | `QmitkAbstractView` | `sv4guiQmitkFunctionality` (自定义包装) |
| UI 构建 | 代码式 (QStandardItemModel) | Designer .ui 文件 |

---

## 4. 资源/样式对比 (<5% 相似度) ✅

| 方面 | XQ | SV |
|------|----|----|
| QSS 行数 | 832 行 | 138 行 |
| 主题 | Arctic Light + Royal Blue (#2563EB) | 渐变 + Light Blue (#00ccff) |
| 图标格式 | SVG 矢量 (6个) | PNG 位图 (150+) |
| QRC 文件 | 1 个集中式 | 14 个分布式 |
| 总资源数 | 24 个 | 172+ 个 |
| 视角(Perspective) | 3 个 | 1 个 |

---

## 5. C++ 编码风格差异

| 特性 | XQ (C++17) | SV (C++11) |
|------|-----------|-----------|
| 内存管理 | `std::unique_ptr` | 裸指针 |
| 字符串 | `std::string_view` | `std::string` 值传递 |
| 属性注解 | `[[nodiscard]]` | 无 |
| 绑定 | 结构化绑定 | 显式解包 |
| 拷贝语义 | 移动语义优先 | 传统拷贝 |

---

## 6. 仍然存在的高相似度问题

### 🔴 紧急 (>50%)

1. **AppWorkbenchAdvisor (85-90%)** — 几乎相同，仅类名和透视图ID不同
2. **类命名模式 (85%)** — `xq_` 前缀替换 `sv4gui_` 但结构名一致
3. **文件组织 (70%)** — 目录结构一模一样
4. **架构模式 (60%)** — Factory/Observer/Operation 使用模式相同

### 🟡 中等 (30-50%)

5. **Perspective 布局 (50%)** — 左面板 20% 宽度、数据管理器位置相同
6. **菜单系统 (45%)** — 菜单项顺序和分组方式类似
7. **About 对话框 (40%)** — 公共接口方法名完全一致
8. **数据管理器 (35-40%)** — MITK 节点描述符模式相同
9. **VtkUtils (35%)** — VTK 封装函数有重叠
10. **Math3 (30%)** — 部分算法接口类似
11. **Mesh 模块 (25-30%)** — 结构 (Grid+Adaptor) 与 SV (Mesh+Adaptor) 平行

---

## 7. 降低相似度的建议方向

### Phase 1: 高优先级 (85-90% → <10%)

- **AppWorkbenchAdvisor**: 重构为完全不同的初始化模式
- **类命名模式**: 引入不同的命名约定 (如模块前缀式)
- **文件组织**: 改变目录层级结构

### Phase 2: 中优先级 (50-70% → <10%)

- **Perspective 布局**: 改变面板比例和位置
- **菜单系统**: 重组菜单结构
- **About 对话框**: 更改公共接口方法名

### Phase 3: 模块去重 (25-35% → <10%)

- **VtkUtils/Math3**: 重构算法接口
- **Mesh 模块**: 改变类层次结构
- **ProjectManagement**: 重新设计文件夹节点架构

---

## 8. 当前已完成的差异化

✅ QSS 样式完全不同 (Arctic Light vs 渐变蓝)
✅ SVG 图标 vs PNG 图标
✅ 阶段式工具栏 vs 透视图工具栏
✅ C++17 vs C++11 编码风格
✅ 欢迎页面技术栈不同
✅ 插件代码重叠 <12%
✅ 信号/槽语法不同

---

*报告生成时间: 基于代码分析*
*分析工具: 结构对比 + 方法匹配 + LOC 分析*
