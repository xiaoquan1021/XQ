# XQ vs SimVascular vs CRIMSONFlowsolver 对比分析

## 1. 项目定位

| 维度 | XQ | SimVascular | CRIMSONFlowsolver |
|------|-----|-------------|-------------------|
| **全称** | XQ Cardiovascular Hemodynamics Workbench | SimVascular | CRIMSON Flowsolver |
| **定位** | 心血管血流动力学仿真工作台 | 心血管建模与仿真平台 | 心血管CFD求解器 |
| **语言** | C++ / Qt | C++ / Qt / Python | Fortran / C++ |
| **GUI** | ✅ Qt/MITK/BlueBerry | ✅ Qt/MITK/BlueBerry | ❌ 无GUI（纯命令行） |
| **许可** | 自有 | BSD | 学术许可 |

## 2. 架构对比

### 2.1 技术栈

| 层级 | XQ | SimVascular | CRIMSON |
|------|-----|-------------|---------|
| **UI框架** | Qt 5 + BlueBerry | Qt 5 + BlueBerry | N/A |
| **医学影像** | MITK | MITK | N/A |
| **3D渲染** | VTK | VTK | N/A |
| **CAD内核** | OpenCASCADE | OpenCASCADE + Parasolid | N/A |
| **网格生成** | TetGen | TetGen + MeshSim | N/A |
| **CFD求解** | 外部调用 | svSolver (PHASTA fork) | PHASTA (原生) |
| **降阶模型** | 简化版 | svOneDSolver | N/A |
| **Python** | ❌ 无 | ✅ 完整绑定 | N/A |
| **边界条件** | 基础 | 完整 | 工厂模式+管理器 (最先进) |

### 2.2 模块结构

**XQ (重构后 11 个模块):**
```
Common / Path / Segmentation / Model(Common+OCCT)
Mesh(Common) / Simulation / ROMSimulation
MultiPhysics / ImageProcessing / ProjectManagement
```

**SimVascular (14 个模块):**
```
Common / Path / Segmentation / Model(Common+OCCT+Parasolid)
Mesh(Common+AdaptMesh+MeshSim+TetGen) / Simulation
ROMSimulation / MachineLearning / QtWidgets
ImageProcessing / ProjectManagement / PythonAPI
```

**CRIMSON (3 阶段流水线):**
```
Presolver (网格预处理) → Flowsolver (CFD求解) → Postsolver (后处理)
```

### 2.3 代码体量

| 指标 | XQ | SimVascular | 比率 |
|------|-----|-------------|------|
| **模块代码行** | 4,050 | 10,406 | 39% |
| **插件代码行** | ~15,500 | ~48,400 | 32% |
| **模块文件数** | 104 | 108 | 96% |
| **插件文件数** | ~80 | ~150 | 53% |

## 3. UI 对比

| 维度 | XQ (重构后) | SimVascular |
|------|------------|-------------|
| **色调** | Teal #0D9488 + Slate | 临床蓝 #0055cc |
| **风格** | 极简扁平，无渐变/阴影 | 渐变 + 阴影效果 |
| **主题** | 仅 Light (Flat Minimalist) | Dark / Light / Default |
| **工具栏** | 4个（待重组为3组） | 4个线性排列 |
| **透视** | 3个 (Default/Viewer/Visualization) | 1个 (Default) |
| **欢迎页** | 白底 + teal色带 + 卡片 | 蓝色渐变横幅 |

## 4. 相似之处

1. **相同上游依赖**: MITK + VTK + ITK + Qt + BlueBerry + CTK
2. **相同工作流**: Path → Segmentation → Modeling → Meshing → Simulation
3. **相同插件架构**: BlueBerry OSGi-like plugin framework
4. **相同文件命名模式**: `xq_ClassName` ↔ `sv4guiClassName` (1:1对应)
5. **相同模块划分**: Path/Segmentation/Model/Mesh/Simulation

## 5. 关键差异

### XQ 优势
- 多透视布局 (3个 vs SV的1个)
- 更精简的代码 (SV的32-39%)
- SV项目导入功能
- 独立的UI色调和风格

### SV 优势
- 完整的编辑功能 (MeshEdit/ModelEdit/SegEdit)
- Python脚本绑定
- MPI并行支持
- ROM仿真完整流程
- 更多网格生成选项 (MeshSim)

### CRIMSON 优势
- 工厂模式边界条件管理 (最先进)
- 分层配置系统 (模板+覆盖)
- 三阶段流水线组织
- 纯HPC优化的求解器

## 6. 当前问题：XQ-SV 相似度仍然过高

### 6.1 结构相似度
- 文件命名 1:1 对应 (xq_ ↔ sv4gui)
- 类名模式完全一致
- 模块/插件目录结构对应
- 函数签名大量重合

### 6.2 需要解决
- 目标: 代码相似度 < 10%
- 方法: 重命名 + 重写 + 重组 + 精简
