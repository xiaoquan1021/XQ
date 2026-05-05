# XQ 医学影像软件框架 — 完整实施计划 (v2.0 审查修订版)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 从零搭建一个类似 SimVascular 的医学影像软件框架，名为 XQ，采用 C++ / CMake / Qt6 / MITK BlueBerry 插件架构，UI 与功能模块完整对标 SimVascular。

**Architecture:** XQ 采用三层架构：底层核心库（数学工具、几何算法）→ 中间 MITK 模块层（数据模型、渲染映射、交互器、ObjectFactory）→ 顶层 BlueBerry 插件层（UI 视图、菜单动作、透视图）。所有外部依赖来自 `~/svExternals`（只读），所有代码产出在 `~/XQ` 工作区内。

**Tech Stack:** C++17, CMake 3.18+, Qt6.7, MITK 2024.06 (BlueBerry/CTK), VTK 9.3, ITK 5.4, CppMicroServices

**v2.0 变更摘要：** 相比 v1.0，本版新增 MachineLearning/MultiPhysics/MITKSegmentation/PythonDataNodes 模块及插件；补全所有 ObjectFactory/LegacyIO/Operation 类；修复 Phase 依赖顺序；消除 QSS 重复内容；统一 MITK 数据包装命名规范。

---

## 系统红线

1. `~/Simvascular` 和 `~/svExternals` 是 **只读参考区域**，禁止写入任何文件
2. 所有代码生成和文件创建在 `~/XQ` 内进行
3. 使用 MITK BlueBerry 插件架构，不使用纯 Qt 控件
4. 不直接复制 SimVascular 代码，而是参考其架构模式独立编写
5. `rm -rf` 操作必须写明完整绝对路径（例: `rm -rf /home/xiaoquan/XQ/xq-build`）

---

## 外部依赖路径映射

以下路径来自 `~/svExternals/install`（只读引用）和 `~/svExternals/src`（MITK 构建目录）：

| 依赖        | CMake 配置路径                                                           | 用途 |
|-------------|-------------------------------------------------------------------------|------|
| Qt6         | `/home/xiaoquan/svExternals/install/qt-6.7.0/lib/cmake/Qt6`            | UI 框架 |
| VTK 9.3     | `/home/xiaoquan/svExternals/install/vtk-9.3.0/lib/cmake/vtk-9.3`      | 3D 可视化 |
| ITK 5.4     | `/home/xiaoquan/svExternals/install/itk-5.4.0/lib/cmake/ITK-5.4`      | 图像处理 |
| MITK (build)| `/home/xiaoquan/svExternals/src/MITK-2024.06/build/MITK-build`         | 编译期链接(含MITKConfig.cmake) |
| MITK (install)| `/home/xiaoquan/svExternals/install/mitk-2024.06`                     | 运行期库路径 |
| FreeType    | `/home/xiaoquan/svExternals/install/freetype-2.13.0`                   | 字体渲染 |
| GDCM        | `/home/xiaoquan/svExternals/install/gdcm-3.0.10/lib/gdcm-3.0`         | DICOM 解码 |
| HDF5        | `/home/xiaoquan/svExternals/install/hdf5-1.14.3/cmake`                 | 数据存储 |
| MMG         | `/home/xiaoquan/svExternals/install/mmg-5.3.9`                         | 网格优化 |
| OpenCASCADE | `/home/xiaoquan/svExternals/install/opencascade-7.6.0/lib/cmake/opencascade` | CAD 建模 |
| Python 3.11 | `/home/xiaoquan/svExternals/install/python-3.11.0`                     | Python 集成 |
| SWIG        | `/home/xiaoquan/svExternals/install/swig-3.0.12`                       | Python 绑定生成 |
| TinyXML2    | `/home/xiaoquan/svExternals/install/tinyxml2-8.0.0/lib/cmake/tinyxml2` | XML 读写 |

> **MITK 双路径说明:** 编译期使用 MITK build 目录（含 MITKConfig.cmake、头文件、cmake 模块）；运行期使用 MITK install 目录（含共享库 .so 文件）。`LD_LIBRARY_PATH` 应同时包含两个 lib/ 路径。

---

## UI/UX 设计系统 (蓝白配色 · 医学影像专业风格)

### 设计理念

XQ 采用 **蓝白配色** 的专业医学影像界面风格，参考 SimVascular 的实际 QSS 样式表，结合现代 UI/UX 最佳实践。风格定位：**Swiss Minimalism + Accessible & Ethical**。

**核心原则：**
- 清晰的视觉层次：蓝色为主色调表达专业与信任，白色背景保证内容可读性
- 功能优先：每个 UI 元素服务于医学影像工作流（Path → Seg → Model → Mesh → Sim）
- 高对比度：所有文本满足 WCAG AA 标准（≥4.5:1 对比度）
- 一致性：统一的间距系统（8dp 基准）、统一的圆角和阴影规则

### 色彩系统

#### 主色板

| 角色 | 色值 | 用途说明 |
|------|------|----------|
| **Primary Blue** | `#0047B3` | 图标主色、品牌色、重要操作按钮 |
| **Primary Light** | `#1C97EA` | 选中状态、工具箱高亮、活跃标签 |
| **Primary Hover** | `#00CCFF` | 菜单悬停、列表项悬停高亮 |
| **Background Blue** | `#E6F2FF` | 菜单栏/工具栏背景 |
| **Background White** | `#FFFFFF` | 卡片背景、内容区域 |
| **Background Gray** | `#F8FAFC` | 主窗口底色、禁用区域 |
| **Text Primary** | `#0F172A` | 主要文本 |
| **Text Secondary** | `#64748B` | 次要文本 |
| **Border** | `#E2E8F0` | 普通分割线 |
| **Border Active** | `#3F3F46` | 工具栏/工具箱边框 |

#### 语义色板

| 角色 | 色值 | 用途 |
|------|------|------|
| **Success** | `#16A34A` | 操作成功 |
| **Warning** | `#D97706` | 警告提示 |
| **Error** | `#DC2626` | 错误/删除 |
| **Info** | `#0891B2` | 信息提示 |

#### 渲染视口配色

| 角色 | 色值 | 用途 |
|------|------|------|
| **Path** | `#FF6600` | 路径曲线和控制点 |
| **Contour** | `#00FF00` | 分割轮廓线 |
| **Model** | `#CCCCCC` | 3D 模型表面 |
| **Mesh** | `#6699CC` | 网格线框 |
| **Selection** | `#FFFF00` | 选中高亮 |

### QSS 样式表文件说明

XQ 的完整 QSS 样式表存放在 `resources/xq.qss`。关键设计点：

- **MITK 主题注释行:** `iconColor = #0047B3`, `iconAccentColor = #FFFFFF`（由 MITK 主题引擎自动解析）
- **主窗口渐变:** `qlineargradient(#CCE0FF → #E6F2FF)`
- **菜单/工具栏背景:** `#E6F2FF`
- **悬停高亮:** `#00CCFF`
- **选中/活跃:** `#1C97EA`
- **BlueBerry TabBar:** 自定义 `berry--QCTabBar` 样式

样式表文件包括三个：
1. `xq.qss` — 主样式表（所有控件样式，约200行）
2. `xq-tab.qss` — 标签栏基础样式
3. `xq-activetab.qss` — 活跃标签高亮样式

> **注意:** 完整 QSS 内容在实现阶段 (Task 2.7) 时编写，此处仅定义设计规范，避免内容重复。

### 窗口布局规范 (DefaultPerspective)

```
┌──────────────────────────────────────────────────────────────────┐
│  MenuBar: File | Edit | Tools | Window | Help                    │
├──────────────────────────────────────────────────────────────────┤
│  ToolBar: [Save][Undo][Redo] | [ImgNav][ViewNav] | [Ax][Sg][Co] │
├─────────────┬────────────────────────────────────────────────────┤
│ Data Manager│                                                    │
│ (树视图)     │              3D/2D 编辑器区域                       │
│ 宽度: 20%   │              (berry::IPageLayout::EditorArea)       │
│ 不可关闭    │                                                    │
│             │              渲染窗口 (4格: 3D + 轴位 + 矢状 + 冠状) │
├─────────────┤                                                    │
│ Image       │                                                    │
│ Navigator   │                                                    │
│ (图像导航)   │                                                    │
│ 高度: 50%   │                                                    │
│ 左侧下方    │                                                    │
├─────────────┴────────────────────────────────────────────────────┤
│  Bottom Folder (折叠): Log View | Modules                        │
├──────────────────────────────────────────────────────────────────┤
│  StatusBar: [状态信息] [内存使用] [坐标显示]                       │
└──────────────────────────────────────────────────────────────────┘
```

### 菜单结构规范

```
File (文件)
├── Create XQ Project         Ctrl+N       图标: CreateXQ.png
├── Open XQ Project           Ctrl+O       图标: OpenXQ.png
├── Save Project              Ctrl+S       图标: SaveAllXQ.png
├── Save Project As...        Ctrl+Shift+S
├── Close Project                          图标: edit-delete.svg
├── ──────────────
├── Open File...                           图标: document-open.svg
├── Save MITK Scene                        图标: document-save.svg
├── ──────────────
└── Exit                      Ctrl+Q       图标: system-log-out.svg

Edit (编辑)
├── Undo                      Ctrl+Z       图标: edit-undo.svg
└── Redo                      Ctrl+Y       图标: edit-redo.svg

Tools (工具) — 按工作流顺序排列
├── Path Planning              idx=1  org.xq.views.pathplanning
├── 2D Segmentation            idx=2  org.xq.views.segmentation2d
├── 3D Segmentation            idx=3  org.xq.views.segmentation3d
├── MITK Segmentation          idx=4  org.xq.views.mitksegmentation
├── Modeling                   idx=5  org.xq.views.modeling
├── Meshing                    idx=6  org.xq.views.meshing
├── Simulation                 idx=7  org.xq.views.simulation
├── ROM Simulation             idx=8  org.xq.views.romsimulation
├── MultiPhysics               idx=9  org.xq.views.multiphysics
├── ──────────────
├── Image Processing           idx=20+
└── [其他 MITK 视图]

Window (窗口)
├── New Window
├── Open Perspective ►
│   ├── XQ Default Perspective
│   └── [其他 Perspective]
├── Reset Perspective
├── Close Perspective
├── ──────────────
└── Preferences...             Ctrl+P

Help (帮助)
├── Welcome
└── About XQ
```

### 工具栏规范

| 工具栏 | ObjectName | 内容 | 样式 |
|--------|------------|------|------|
| **主操作栏** | `mainActionsToolBar` | Save, Undo, Redo, Image Navigator, View Navigator, Axial, Sagittal, Coronal | `ToolButtonTextBesideIcon` |
| **透视图栏** | `perspectiveToolbar` | 透视图切换按钮组 | `ToolButtonTextBesideIcon` |
| **XQ 视图栏** | `xqViewToolbar` | PathPlanning, Seg2D, Seg3D, MITKSeg, Modeling, Meshing, Sim, ROMSim, MultiPhysics | `ToolButtonTextBesideIcon` |
| **其他视图栏** | `otherViewToolbar` | 其他 MITK 视图按钮 | `ToolButtonTextBesideIcon` |

### 图标资源规范

所有图标文件存放在 `Plugins/org.xq.gui.qt.application/resources/`：

| 图标文件 | 尺寸 | 用途 |
|----------|------|------|
| `icon.png` | 256×256 | 应用图标（蓝白配色 XQ logo） |
| `CreateXQ.png` | 24×24 | 创建项目 |
| `OpenXQ.png` | 24×24 | 打开项目 |
| `SaveAllXQ.png` | 24×24 | 保存项目 |
| `axial.png` | 24×24 | 轴位面 |
| `sagittal.png` | 24×24 | 矢状面 |
| `coronal.png` | 24×24 | 冠状面 |
| `ImageNavigator.png` | 24×24 | 图像导航器 |

**图标设计:** 主色 `#0047B3`，强调色 `#FFFFFF`，扁平化线条，2px 线宽，PNG 24×24 或 SVG。

### 对话框设计规范

#### About 对话框 (664×465 px)
XQ Logo + 版本号 + MITK/Qt/VTK/ITK 版本 + 简介 + [OK]

#### Create Project 对话框
项目名称输入 + 路径选择 + 默认文件夹复选框 (Paths/Segmentations/Models/Meshes/Simulations/ROMSimulations/MultiPhysics) + [Cancel][Create]

### UX 交互规范

| 交互 | 规范 |
|------|------|
| 菜单悬停 | 150ms 延迟高亮 |
| 按钮反馈 | 立即 pressed 状态 |
| 删除确认 | 必须二次确认 |
| 长操作 | 进度条 + 可取消 |
| 成功反馈 | StatusBar 3s |
| 工具提示 | 500ms 延迟，所有按钮必备 |
| 双击跳转 | Data Manager 双击节点打开对应视图 |

### 可访问性 (WCAG AA)
- 对比度 ≥ 4.5:1
- Tab 键盘导航
- 所有图标按钮有 tooltip + accessibleName
- 焦点 2px solid `#1C97EA` ring

---

## 项目目录结构总览

```
~/XQ/
├── CMakeLists.txt                          # 顶层 CMake（ExternalProject 模式）
├── build.sh                                # 构建脚本
├── run-xq.sh                               # 运行脚本
├── docs/
│   └── specs/
│       └── xq-implementation-plan.md       # 本计划文件
│
└── Code/
    ├── CMakeLists.txt                      # Code 层 CMake 主配置
    ├── CMake/                              # CMake 工具模块
    │   ├── XQMacros.cmake                  # xq_create_module / xq_create_plugin 宏
    │   ├── XQOptions.cmake                 # 构建选项定义
    │   ├── XQExternals.cmake               # 外部依赖发现
    │   ├── FindMITK.cmake                  # MITK 库查找模块
    │   └── CppMicroServices/               # usFunctionXxx cmake 工具
    │       ├── usFunctionGenerateModuleInit.cmake
    │       ├── usFunctionGetResourceSource.cmake
    │       ├── usFunctionAddResources.cmake
    │       ├── usFunctionEmbedResources.cmake
    │       └── usFunctionCheckResourceLinking.cmake
    │
    ├── Source/
    │   ├── Application/
    │   │   ├── CMakeLists.txt
    │   │   ├── main.cxx                    # XQ 可执行入口
    │   │   ├── xq.ini                      # MITK 应用配置文件
    │   │   └── target_libraries.cmake      # Application 链接目标
    │   │
    │   ├── xq4gui/
    │   │   ├── Modules/                    # MITK 模块层 (数据/渲染/交互)
    │   │   │   ├── Common/                 # 公共工具模块
    │   │   │   ├── Path/                   # 路径数据模块
    │   │   │   ├── Segmentation/           # 分割数据模块
    │   │   │   ├── Model/
    │   │   │   │   ├── Common/             # 通用模型模块
    │   │   │   │   └── OCCT/               # OpenCASCADE 模型模块
    │   │   │   ├── Mesh/
    │   │   │   │   └── Common/             # 网格模块 (含 TetGen 适配)
    │   │   │   ├── Simulation/             # 仿真模块
    │   │   │   ├── ROMSimulation/          # ROM 仿真模块
    │   │   │   ├── MultiPhysics/           # 多物理场模块 ★新增
    │   │   │   ├── MachineLearning/        # 机器学习工具模块 ★新增
    │   │   │   ├── ImageProcessing/        # 图像处理模块
    │   │   │   ├── ProjectManagement/      # 项目管理模块
    │   │   │   └── QtWidgets/              # Qt 共享控件模块
    │   │   │
    │   │   └── Plugins/                    # BlueBerry 插件层 (UI 视图)
    │   │       ├── PluginList.cmake         # ★移至此处(原在Phase5)
    │   │       ├── org.xq.gui.qt.application/
    │   │       ├── org.xq.projectdatanodes/
    │   │       ├── org.xq.pythondatanodes/          ★新增
    │   │       ├── org.xq.gui.qt.datamanager/
    │   │       ├── org.xq.gui.qt.projectmanager/
    │   │       ├── org.xq.gui.qt.pathplanning/
    │   │       ├── org.xq.gui.qt.segmentation/
    │   │       ├── org.xq.gui.qt.mitksegmentation/  ★新增
    │   │       ├── org.xq.gui.qt.modeling/
    │   │       ├── org.xq.gui.qt.meshing/
    │   │       ├── org.xq.gui.qt.simulation/
    │   │       ├── org.xq.gui.qt.romsimulation/
    │   │       ├── org.xq.gui.qt.multiphysics/      ★新增
    │   │       └── org.xq.gui.qt.imageprocessing/
    │   │
    │   └── ThirdParty/                     # XQ 特有的第三方集成（预留）
    │
    └── Testing/                            # 测试目录（预留）
```

---

## 模块依赖关系图

```
            ┌──────────────────────────────────────────────────────────────────────────────────┐
 Plugins    │ application  datamanager  projectmgr  pathplan  seg  mitkseg  model  mesh  sim  │
            │ multiphysics  romsim  imgproc  pythondatanodes  projectdatanodes                 │
            └─────┬───────────────┬──────────────────┬──────────────────┬───────────────────────┘
                  │               │                  │                  │
            ┌─────┴───────────────┴──────────────────┴──────────────────┴───────────────────────┐
 Modules    │ Common  Path  Segmentation  Model/Common  Model/OCCT  Mesh/Common                │
            │ Simulation  ROMSimulation  MultiPhysics  MachineLearning  ImageProcessing         │
            │ ProjectManagement  QtWidgets                                                      │
            └─────┬───────────────┬──────────────────┬──────────────────┬───────────────────────┘
                  │               │                  │                  │
            ┌─────┴───────────────┴──────────────────┴──────────────────┴───────────────────────┐
 External   │ MITK (BlueBerry/CTK)  VTK  ITK  Qt6  CppMicroServices  TinyXML2  OpenCASCADE    │
            └──────────────────────────────────────────────────────────────────────────────────────┘
```

### 模块间内部依赖

| 模块 | 依赖 |
|------|------|
| `Common` | VTK, ITK, MITK (无内部依赖) |
| `Path` | Common |
| `Segmentation` | Common, Path |
| `Model/Common` | Common, Segmentation |
| `Model/OCCT` | Model/Common, OpenCASCADE |
| `Mesh/Common` | Common, Model/Common |
| `Simulation` | Common, Mesh/Common |
| `ROMSimulation` | Common |
| `MultiPhysics` | Common |
| `MachineLearning` | Common |
| `ImageProcessing` | Common |
| `ProjectManagement` | Common + 所有数据模块 |
| `QtWidgets` | Common, MitkQtWidgets |

---

## Phase 1: CMake 构建系统骨架

### Task 1.1: 顶层 CMakeLists.txt

**文件**: `~/XQ/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.21)
project(XQ VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 进入 Code/ 子目录
add_subdirectory(Code)
```

### Task 1.2: Code/CMakeLists.txt — 外部依赖发现

**文件**: `~/XQ/Code/CMakeLists.txt`

**职责**: 发现所有外部依赖，设置全局变量，加载自定义宏，添加子目录。

**关键步骤**:
1. `include(CMake/XQOptions.cmake)` — 构建选项
2. `include(CMake/XQExternals.cmake)` — 外部库路径发现
3. `find_package()` 调用顺序（顺序重要）:
   - `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml WebEngineWidgets Network OpenGL)`
   - `find_package(VTK REQUIRED)` → 保存 `set(VTK_DIR_SAVE ${VTK_DIR})`
   - `find_package(ITK REQUIRED)` → 之后 `set(VTK_DIR ${VTK_DIR_SAVE})` ⚠️ ITK 会覆盖 VTK_DIR
   - `find_package(MITK REQUIRED)` → 设置 MITK cmake module path
   - `find_package(OpenCASCADE REQUIRED)`
   - `find_package(TinyXML2 REQUIRED)`
4. `include(CMake/XQMacros.cmake)` — 自定义模块/插件宏
5. `include(CppMicroServices/usFunctionGenerateModuleInit.cmake)` 等
6. `add_subdirectory(Source/Application)`
7. `add_subdirectory(Source/xq4gui/Modules)` — 各子模块
8. `add_subdirectory(Source/xq4gui/Plugins)` — 各子插件

**SV参考**: `Simvascular/Code/CMakeLists.txt`

### Task 1.3: XQMacros.cmake — 自定义构建宏

**文件**: `~/XQ/Code/CMake/XQMacros.cmake`

**功能**: 定义 `xq_create_module()` 和 `xq_create_plugin()` 宏。

**`xq_create_module(NAME module_name ...)`**:
- 参数: `NAME`, `SRCS`, `HDRS`, `MOC_HDRS`, `UI_FILES`, `RESOURCE_FILES`, `DEPENDS`, `PACKAGE_DEPENDS`
- 行为:
  1. 调用 `usFunctionGenerateModuleInit()` 生成 CppMicroServices 初始化代码
  2. `qt6_wrap_cpp()` 处理 MOC
  3. `qt6_wrap_ui()` 处理 UI 文件
  4. `add_library(${module_name} SHARED ...)` 构建共享库
  5. 设置 `US_MODULE_NAME` 编译定义
  6. 链接 `DEPENDS` 指定的模块和 `PACKAGE_DEPENDS` 外部库
  7. 生成并嵌入 CppMicroServices 资源 (manifest.json)

**`xq_create_plugin(NAME plugin_name ...)`**:
- 参数: `NAME`, `EXPORT_DIRECTIVE`, `SRCS`, `HDRS`, `MOC_HDRS`, `UI_FILES`, `CACHED_RESOURCE_FILES`, `DEPENDS`, `PACKAGE_DEPENDS`, `MODULE_DEPENDS`
- 行为:
  1. 处理 MOC / UI
  2. 构建 `SHARED` 库，输出到 `${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugins/`
  3. 链接 BlueBerry 框架 (`berryCore`, `berryUi`)
  4. 链接 `MODULE_DEPENDS` 指定的 XQ 模块
  5. 设置插件元数据 (plugin.xml 路径)

**SV参考**: `Simvascular/Code/CMake/SimVascularMacros.cmake` 中的 `simvascular_create_module()`

### Task 1.4: XQExternals.cmake — 外部路径配置

**文件**: `~/XQ/Code/CMake/XQExternals.cmake`

**功能**: 设定 `svExternals/install` 下各库的路径变量。

```cmake
# 基础路径
set(SV_EXTERNALS_INSTALL_DIR "$ENV{HOME}/svExternals/install")

# 各库路径映射
set(Qt6_DIR     "${SV_EXTERNALS_INSTALL_DIR}/qt-6.7.0/lib/cmake/Qt6")
set(VTK_DIR     "${SV_EXTERNALS_INSTALL_DIR}/vtk-9.3.0/lib/cmake/vtk-9.3")
set(ITK_DIR     "${SV_EXTERNALS_INSTALL_DIR}/itk-5.4.0/lib/cmake/ITK-5.4")
set(OpenCASCADE_DIR "${SV_EXTERNALS_INSTALL_DIR}/opencascade-7.6.0/lib/cmake/opencascade")
set(TinyXML2_DIR "${SV_EXTERNALS_INSTALL_DIR}/tinyxml2-8.0.0/lib/cmake/tinyxml2")
set(GDCM_DIR    "${SV_EXTERNALS_INSTALL_DIR}/gdcm-3.0.10/lib/cmake/gdcm-3.0")
set(HDF5_DIR    "${SV_EXTERNALS_INSTALL_DIR}/hdf5-1.14.3")
set(MMG_DIR     "${SV_EXTERNALS_INSTALL_DIR}/mmg-5.3.9")

# MITK 双路径
set(MITK_DIR    "$ENV{HOME}/svExternals/src/MITK-2024.06/build/MITK-build")
set(MITK_INSTALL_DIR "${SV_EXTERNALS_INSTALL_DIR}/mitk-2024.06")

# Python / SWIG
set(PYTHON_DIR  "${SV_EXTERNALS_INSTALL_DIR}/python-3.11.0")
set(SWIG_DIR    "${SV_EXTERNALS_INSTALL_DIR}/swig-3.0.12")

# CMAKE_PREFIX_PATH 聚合
list(APPEND CMAKE_PREFIX_PATH
    ${Qt6_DIR} ${VTK_DIR} ${ITK_DIR} ${MITK_DIR}
    ${OpenCASCADE_DIR} ${TinyXML2_DIR} ${GDCM_DIR}
    ${PYTHON_DIR} ${SWIG_DIR}
)
```

### Task 1.5: PluginList.cmake

**文件**: `~/XQ/Code/Source/xq4gui/Plugins/PluginList.cmake`

**说明**: 必须在 Phase 1 创建，而非等到插件开发阶段。此文件被 `Code/CMakeLists.txt` 引用以决定构建哪些插件。

```cmake
set(XQ_PLUGINS
    org.xq.gui.qt.application:ON
    org.xq.projectdatanodes:ON
    org.xq.pythondatanodes:ON
    org.xq.gui.qt.datamanager:ON
    org.xq.gui.qt.projectmanager:ON
    org.xq.gui.qt.pathplanning:ON
    org.xq.gui.qt.segmentation:ON
    org.xq.gui.qt.mitksegmentation:ON
    org.xq.gui.qt.modeling:ON
    org.xq.gui.qt.meshing:ON
    org.xq.gui.qt.simulation:ON
    org.xq.gui.qt.romsimulation:ON
    org.xq.gui.qt.multiphysics:ON
    org.xq.gui.qt.imageprocessing:ON
)
```

### Task 1.6: Application 目录

**文件**:
- `~/XQ/Code/Source/Application/CMakeLists.txt` — 调用 `mitkFunctionCreateBlueBerryApplication()`
- `~/XQ/Code/Source/Application/main.cxx` — XQ 可执行入口
- `~/XQ/Code/Source/Application/xq.ini` — MITK 应用配置（插件列表、默认透视图）
- `~/XQ/Code/Source/Application/target_libraries.cmake` — 链接目标

**xq.ini 内容**:
```ini
BlueBerry/Plugin Dirs=<plugins_output_dir>
XQ/DefaultPerspective=org.xq.gui.qt.application.defaultperspective
```

**SV参考**: `sv-build/SimVascular-build/simvascular.ini`

---

## Phase 2: Application 核心插件

### Task 2.1: org.xq.gui.qt.application 插件

**目录**: `Plugins/org.xq.gui.qt.application/`

**文件清单**:
```
org.xq.gui.qt.application/
├── CMakeLists.txt
├── files.cmake
├── manifest_headers.cmake
├── plugin.xml
├── src/
│   ├── internal/
│   │   ├── xq_ApplicationPlugin.h/.cxx
│   │   ├── xq_AppWorkbenchAdvisor.h/.cxx
│   │   ├── xq_DefaultPerspective.h/.cxx
│   │   ├── xq_WorkbenchWindowAdvisor.h/.cxx
│   │   └── xq_ActionBarAdvisor.h/.cxx       # ★新增: 菜单栏/工具栏配置
│   └── (可选: xq_AppActionBarAdvisor.h/.cxx)
└── resources/
    ├── icon.png
    └── plugin.xml
```

**功能**:
- `xq_ApplicationPlugin`: 插件 Activator，注册默认透视图
- `xq_DefaultPerspective`: 定义默认布局（左侧 DataManager + ProjectManager，中央 Editor，底部 Console）
- `xq_WorkbenchWindowAdvisor`: 配置窗口标题、菜单、工具栏、状态栏
- `xq_ActionBarAdvisor`: 定义全局菜单结构

**菜单结构** (来自 UI/UX 设计):
```
File    Edit    View    Tools                     Help
│       │       │       ├─ Path Planning          │
│       │       │       ├─ Segmentation           │
│       │       │       ├─ MITK Segmentation      │
│       │       │       ├─ Modeling                │
│       │       │       ├─ Meshing                 │
│       │       │       ├─ Simulation              │
│       │       │       ├─ ROM Simulation          │
│       │       │       ├─ MultiPhysics            │
│       │       │       └─ Image Processing        │
```

**plugin.xml 示例**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<?eclipse version="3.4"?>
<plugin>
  <extension point="org.blueberry.ui.perspectives">
    <perspective id="org.xq.gui.qt.application.defaultperspective"
                 name="XQ Default"
                 class="xq_DefaultPerspective"/>
  </extension>
</plugin>
```

**SV参考**: `Simvascular/Code/Source/sv4gui/Plugins/org.sv.gui.qt.application/`

---

## Phase 3: 数据基础设施插件

### Task 3.1: org.xq.projectdatanodes 插件

**目录**: `Plugins/org.xq.projectdatanodes/`

**职责**: 在 MITK DataStorage 中注册 XQ 自定义数据节点类型（Image、Path、Segmentation、Model 等），使其可以被 DataManager 识别和显示。

**文件清单**:
```
org.xq.projectdatanodes/
├── CMakeLists.txt
├── files.cmake
├── manifest_headers.cmake
├── plugin.xml
└── src/
    └── internal/
        ├── xq_ProjectDataNodesPlugin.h/.cxx     # Activator
        └── xq_DataNodeInit.h/.cxx                # 注册各模块 ObjectFactory
```

**SV参考**: `Simvascular/Code/Source/sv4gui/Plugins/org.sv.projectdatanodes/`

### Task 3.2: org.xq.pythondatanodes 插件 ★新增

**目录**: `Plugins/org.xq.pythondatanodes/`

**职责**: Python ↔ C++ 数据节点桥接，使 Python 脚本可以操作 DataStorage 中的节点。

**文件清单**:
```
org.xq.pythondatanodes/
├── CMakeLists.txt
├── files.cmake
├── manifest_headers.cmake
├── plugin.xml
└── src/
    └── internal/
        ├── xq_PythonDataNodesPlugin.h/.cxx     # Activator
        └── xq_PythonDataNodeInterface.h/.cxx    # Python 绑定
```

**SV参考**: `Simvascular/Code/Source/sv4gui/Plugins/org.sv.pythondatanodes/`

### Task 3.3: org.xq.gui.qt.datamanager 插件

**目录**: `Plugins/org.xq.gui.qt.datamanager/`

**职责**: 左侧面板的数据管理器视图（树形节点浏览器），基于 MITK DataManager 扩展。

**文件清单**:
```
org.xq.gui.qt.datamanager/
├── CMakeLists.txt
├── files.cmake
├── manifest_headers.cmake
├── plugin.xml
└── src/
    └── internal/
        ├── xq_DataManagerView.h/.cxx
        ├── xq_DataManagerView.ui                # Qt Designer 表单
        └── xq_DataManagerPlugin.h/.cxx          # Activator
```

**SV参考**: `Simvascular/Code/Source/sv4gui/Plugins/org.sv.gui.qt.datamanager/`

### Task 3.4: org.xq.gui.qt.projectmanager 插件

**目录**: `Plugins/org.xq.gui.qt.projectmanager/`

**职责**: 项目创建、打开、保存管理器。管理 `.xqproj` 项目文件。

**文件清单**:
```
org.xq.gui.qt.projectmanager/
├── CMakeLists.txt
├── files.cmake
├── manifest_headers.cmake
├── plugin.xml
└── src/
    └── internal/
        ├── xq_ProjectManagerView.h/.cxx
        ├── xq_ProjectManagerView.ui
        ├── xq_ProjectCreate.h/.cxx             # 新建项目向导
        ├── xq_ProjectCreate.ui
        └── xq_ProjectManagerPlugin.h/.cxx      # Activator
```

**SV参考**: `Simvascular/Code/Source/sv4gui/Plugins/org.sv.gui.qt.projectmanager/`

---

## Phase 4: 核心数据模块

> **通用约定**: 每个模块都遵循 SV 的 ObjectFactory 模式，提供一个 `XxxObjectFactory` 类，在 CppMicroServices 的模块激活时自动注册到 MITK IO 框架。模块内 MITK 封装类统一使用 `MitkXxx` 前缀（如 `xq_MitkMesh`）。

### Task 4.1: Common 模块

**目录**: `Modules/Common/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_Math3.h/.cxx` | 3D 数学工具（向量、矩阵、变换） |
| `xq_Spline.h/.cxx` | ★ 样条曲线基类 |
| `xq_VtkParametricSpline.h/.cxx` | ★ VTK 参数化样条实现 |
| `xq_VtkUtils.h/.cxx` | VTK 辅助工具（polydata 操作等） |
| `xq_XmlIOUtil.h/.cxx` | TinyXML2 XML 读写工具 |
| `xq_StringUtils.h/.cxx` | 字符串处理工具 |
| `files.cmake` | 文件列表定义 |
| `CMakeLists.txt` | `xq_create_module(NAME xqCommon ...)` |

**依赖**: VTK, ITK, MITK, TinyXML2

**SV参考**: `Simvascular/Code/Source/sv4gui/Modules/Common/`

### Task 4.2: Path 模块

**目录**: `Modules/Path/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_PathElement.h/.cxx` | 路径数据元素（控制点 + 算法生成点） |
| `xq_Path.h/.cxx` | MITK BaseData 子类，路径容器 |
| `xq_PathOperation.h/.cxx` | 路径操作（添加/移除/移动控制点） |
| `xq_PathDataInteractor.h/.cxx` | 交互器（鼠标点击添加控制点） |
| `xq_PathVtkMapper3D.h/.cxx` | 3D 渲染 Mapper |
| `xq_PathVtkMapper2D.h/.cxx` | 2D 切面渲染 Mapper |
| `xq_PathIO.h/.cxx` | 文件 IO（读写 .xqpth） |
| `xq_PathLegacyIO.h/.cxx` | ★ 兼容旧版文件格式 |
| `xq_PathObjectFactory.h/.cxx` | ★ MITK ObjectFactory 注册 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqPath DEPENDS xqCommon)` |

**依赖**: xqCommon, VTK, MITK

### Task 4.3: Segmentation 模块

**目录**: `Modules/Segmentation/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_Contour.h/.cxx` | 轮廓基类 |
| `xq_ContourCircle.h/.cxx` | 圆形轮廓 |
| `xq_ContourEllipse.h/.cxx` | 椭圆轮廓 |
| `xq_ContourPolygon.h/.cxx` | 多边形轮廓 |
| `xq_ContourTensionPolygon.h/.cxx` | ★ 张力多边形轮廓 |
| `xq_ContourSplinePolygon.h/.cxx` | 样条多边形轮廓 |
| `xq_ContourModel.h/.cxx` | ★ MITK 轮廓模型 |
| `xq_ContourModelVtkMapper2D.h/.cxx` | ★ 轮廓模型 2D 渲染 |
| `xq_ContourModelThresholdInteractor.h/.cxx` | ★ 阈值交互器 |
| `xq_ContourGroup.h/.cxx` | 轮廓组（同一路径上的多个轮廓） |
| `xq_ContourGroupDataInteractor.h/.cxx` | 轮廓组交互 |
| `xq_ContourGroupVtkMapper2D.h/.cxx` | 轮廓组 2D 渲染 |
| `xq_ContourGroupVtkMapper3D.h/.cxx` | 轮廓组 3D 渲染 |
| `xq_ContourOperation.h/.cxx` | 轮廓操作命令 |
| `xq_Surface.h/.cxx` | ★ 表面数据类 |
| `xq_SurfaceVtkMapper3D.h/.cxx` | ★ 表面 3D 渲染 |
| `xq_Seg3DUtils.h/.cxx` | ★ 3D 分割工具 |
| `xq_MitkSeg3D.h/.cxx` | ★ MITK 3D 分割数据节点 |
| `xq_MitkSeg3DOperation.h/.cxx` | ★ 3D 分割操作 |
| `xq_MitkSeg3DIO.h/.cxx` | ★ 3D 分割文件 IO |
| `xq_MitkSeg3DVtkMapper3D.h/.cxx` | ★ 3D 分割渲染 |
| `xq_MitkSeg3DDataInteractor.h/.cxx` | ★ 3D 分割交互 |
| `xq_SegmentationUtils.h/.cxx` | ★ 分割通用工具 |
| `xq_SegmentationIO.h/.cxx` | 分割文件 IO |
| `xq_SegmentationLegacyIO.h/.cxx` | ★ 旧版格式兼容 |
| `xq_SegmentationObjectFactory.h/.cxx` | ★ ObjectFactory 注册 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqSegmentation DEPENDS xqCommon xqPath)` |

**依赖**: xqCommon, xqPath, VTK, MITK

### Task 4.4: Model/Common 模块

**目录**: `Modules/Model/Common/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_ModelElement.h/.cxx` | 模型数据元素基类 |
| `xq_ModelElementAnalytic.h/.cxx` | ★ 解析模型元素 |
| `xq_ModelElementFactory.h/.cxx` | ★ 模型元素工厂 |
| `xq_ModelElementPolyData.h/.cxx` | PolyData 模型元素 |
| `xq_ModelUtils.h/.cxx` | ★ 模型通用工具 |
| `xq_Model.h/.cxx` | MITK BaseData 子类，模型容器 |
| `xq_ModelOperation.h/.cxx` | ★ 模型操作命令 |
| `xq_ModelDataInteractor.h/.cxx` | ★ 模型交互器 |
| `xq_ModelVtkMapper2D.h/.cxx` | ★ 2D 渲染 |
| `xq_ModelVtkMapper3D.h/.cxx` | 3D 渲染 |
| `xq_ModelIO.h/.cxx` | 文件 IO |
| `xq_ModelLegacyIO.h/.cxx` | ★ 旧版格式 IO |
| `xq_ModelObjectFactory.h/.cxx` | ★ ObjectFactory 注册 |
| `xq_RegisterPolyDataFunction.h/.cxx` | ★ PolyData 模型函数注册 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqModelCommon DEPENDS xqCommon xqSegmentation)` |

**依赖**: xqCommon, xqSegmentation, VTK, MITK

### Task 4.5: Model/OCCT 模块 ★新增

**目录**: `Modules/Model/OCCT/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_ModelElementOCCT.h/.cxx` | OpenCASCADE 模型元素 |
| `xq_ModelUtilsOCCT.h/.cxx` | OCCT 模型工具 |
| `xq_RegisterOCCTFunction.h/.cxx` | OCCT 模型函数注册 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqModelOCCT DEPENDS xqModelCommon PACKAGE_DEPENDS OpenCASCADE)` |

**依赖**: xqModelCommon, OpenCASCADE

**SV参考**: `Simvascular/Code/Source/sv4gui/Modules/Model/OCCT/`

### Task 4.6: Mesh/Common 模块

**目录**: `Modules/Mesh/Common/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_Mesh.h/.cxx` | 网格数据基类 |
| `xq_MeshTetGen.h/.cxx` | ★ TetGen 网格实现 |
| `xq_MeshFactory.h/.cxx` | ★ 网格工厂（TetGen/MMG 选择） |
| `xq_MeshAdaptor.h/.cxx` | ★ 网格自适应基类 |
| `xq_MeshTetGenAdaptor.h/.cxx` | ★ TetGen 自适应实现 |
| `xq_MitkMesh.h/.cxx` | MITK BaseData 封装（注意前缀） |
| `xq_MitkMeshIO.h/.cxx` | 文件 IO |
| `xq_MitkMeshMapper2D.h/.cxx` | ★ 2D 渲染 |
| `xq_MitkMeshMapper3D.h/.cxx` | 3D 渲染 |
| `xq_MitkMeshOperation.h/.cxx` | ★ 网格操作命令 |
| `xq_MitkMeshObjectFactory.h/.cxx` | ★ ObjectFactory 注册 |
| `xq_MeshLegacyIO.h/.cxx` | ★ 旧版格式 IO |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqMeshCommon DEPENDS xqCommon xqModelCommon)` |

**依赖**: xqCommon, xqModelCommon, VTK, MITK

### Task 4.7: Simulation 模块

**目录**: `Modules/Simulation/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_SimJob.h/.cxx` | 仿真任务数据 |
| `xq_MitkSimJob.h/.cxx` | MITK 封装 |
| `xq_MitkSimJobIO.h/.cxx` | 文件 IO |
| `xq_SimulationUtils.h/.cxx` | 仿真工具函数 |
| `xq_SimXmlWriter.h/.cxx` | ★ 仿真 XML 写入器 |
| `xq_MitkSimulationObjectFactory.h/.cxx` | ★ ObjectFactory 注册 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqSimulation DEPENDS xqCommon xqMeshCommon)` |

**依赖**: xqCommon, xqMeshCommon, TinyXML2

### Task 4.8: ROMSimulation 模块

**目录**: `Modules/ROMSimulation/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_ROMSimJob.h/.cxx` | ROM 仿真任务数据 |
| `xq_MitkROMSimJob.h/.cxx` | MITK 封装 |
| `xq_MitkROMSimJobIO.h/.cxx` | 文件 IO |
| `xq_ROMSimulationUtils.h/.cxx` | ★ ROM 仿真工具 |
| `xq_MitkROMSimulationObjectFactory.h/.cxx` | ★ ObjectFactory 注册 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqROMSimulation DEPENDS xqCommon)` |

**依赖**: xqCommon, TinyXML2

### Task 4.9: MultiPhysics 模块 ★新增

**目录**: `Modules/MultiPhysics/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_MPJob.h/.cxx` | 多物理场任务数据 |
| `xq_MPViscosity.h/.cxx` | 粘性模型 |
| `xq_MPBoundaryCondition.h/.cxx` | 边界条件 |
| `xq_MPEquation.h/.cxx` | 方程定义 |
| `xq_MPXmlWriter.h/.cxx` | 多物理场 XML 写入器 |
| `xq_MitkMPJob.h/.cxx` | MITK 封装 |
| `xq_MitkMPJobIO.h/.cxx` | 文件 IO |
| `xq_MitkMPObjectFactory.h/.cxx` | ObjectFactory 注册 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqMultiPhysics DEPENDS xqCommon)` |

**依赖**: xqCommon, TinyXML2

**SV参考**: `Simvascular/Code/Source/sv4gui/Modules/MultiPhysics/`

### Task 4.10: MachineLearning 模块 ★新增

**目录**: `Modules/MachineLearning/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_MachineLearningUtils.h/.cxx` | ML 工具函数 |
| `json.hxx` | JSON 头文件库 (nlohmann/json) |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqMachineLearning DEPENDS xqCommon)` |

**依赖**: xqCommon

**SV参考**: `Simvascular/Code/Source/sv4gui/Modules/MachineLearning/`

### Task 4.11: ImageProcessing 模块

**目录**: `Modules/ImageProcessing/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_ImageProcessingUtils.h/.cxx` | 图像处理工具 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqImageProcessing DEPENDS xqCommon)` |

**依赖**: xqCommon, VTK, ITK

### Task 4.12: ProjectManagement 模块

**目录**: `Modules/ProjectManagement/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_ProjectManager.h/.cxx` | 项目管理器核心 |
| `xq_DataFolder.h/.cxx` | 数据文件夹基类 |
| `xq_ImageFolder.h/.cxx` | ★ 图像文件夹 |
| `xq_PathFolder.h/.cxx` | ★ 路径文件夹 |
| `xq_SegmentationFolder.h/.cxx` | ★ 分割文件夹 |
| `xq_ModelFolder.h/.cxx` | ★ 模型文件夹 |
| `xq_MeshFolder.h/.cxx` | ★ 网格文件夹 |
| `xq_SimulationFolder.h/.cxx` | ★ 仿真文件夹 |
| `xq_ROMSimulationFolder.h/.cxx` | ★ ROM 仿真文件夹 |
| `xq_MultiPhysicsFolder.h/.cxx` | ★ 多物理场文件夹 |
| `xq_RepositoryFolder.h/.cxx` | ★ 仓库文件夹 |
| `xq_DataNodeOperation.h/.cxx` | ★ 数据节点操作 |
| `xq_DataNodeOperationInterface.h/.cxx` | ★ 数据节点操作接口 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqProjectManagement DEPENDS xqCommon xqPath xqSegmentation xqModelCommon xqMeshCommon xqSimulation xqROMSimulation xqMultiPhysics)` |

**依赖**: xqCommon + 所有数据模块

### Task 4.13: QtWidgets 模块

**目录**: `Modules/QtWidgets/`

**文件清单**:
| 文件 | 职责 |
|------|------|
| `xq_ResliceSlider.h/.cxx` | 多平面切片滑动条控件 |
| `files.cmake` | 文件列表 |
| `CMakeLists.txt` | `xq_create_module(NAME xqQtWidgets DEPENDS xqCommon PACKAGE_DEPENDS MitkQtWidgets)` |

**依赖**: xqCommon, MitkQtWidgets

---

## Phase 5: 功能插件

> **插件通用结构**: 每个功能插件都遵循 BlueBerry 插件标准结构：
> - `CMakeLists.txt` + `files.cmake` + `manifest_headers.cmake` + `plugin.xml`
> - `src/internal/` 包含 Activator、View、PreferencePage 等
> - View 类继承 `QmitkAbstractView`，提供 `CreateQtPartControl()` 和 `SetFocus()`

### Task 5.1: org.xq.gui.qt.pathplanning 插件

**目录**: `Plugins/org.xq.gui.qt.pathplanning/`

**文件清单**:
```
src/internal/
├── xq_PathPlanningPlugin.h/.cxx         # Activator
├── xq_PathPlanningView.h/.cxx           # 主视图
├── xq_PathPlanningView.ui               # UI 表单
├── xq_PathCreate.h/.cxx                 # 路径创建对话框
├── xq_PathCreate.ui
├── xq_PathSmooth.h/.cxx                 # 路径平滑对话框
├── xq_PathSmooth.ui
├── xq_PathPoint2DSizeAction.h/.cxx      # 2D 点大小调节
├── xq_PathPoint3DSizeAction.h/.cxx      # 3D 点大小调节
└── xq_PathPreferencePage.h/.cxx         # 偏好设置页
```

**模块依赖**: xqCommon, xqPath, xqProjectManagement

**SV参考**: `Plugins/org.sv.gui.qt.pathplanning/`

### Task 5.2: org.xq.gui.qt.segmentation 插件

**目录**: `Plugins/org.xq.gui.qt.segmentation/`

**文件清单**:
```
src/internal/
├── xq_SegmentationPlugin.h/.cxx         # Activator
├── xq_SegmentationView.h/.cxx           # 主视图 (2D 轮廓)
├── xq_SegmentationView.ui
├── xq_Seg3DCreateAction.h/.cxx          # 3D 分割创建
├── xq_LoftParamWidget.h/.cxx            # Lofting 参数控件
├── xq_LoftParamWidget.ui
├── xq_ContourGroupCreate.h/.cxx         # 轮廓组创建
├── xq_ContourGroupCreate.ui
└── xq_SegmentationPreferencePage.h/.cxx # 偏好设置
```

**模块依赖**: xqCommon, xqPath, xqSegmentation, xqProjectManagement

### Task 5.3: org.xq.gui.qt.mitksegmentation 插件 ★新增

**目录**: `Plugins/org.xq.gui.qt.mitksegmentation/`

**职责**: 包装 MITK 原生分割工具（阈值、区域生长、Level Set 等），提供 SV 风格的 UI 面板。

**文件清单**:
```
src/internal/
├── xq_MitkSegmentationPlugin.h/.cxx     # Activator
├── xq_MitkSegmentationView.h/.cxx       # 主视图
└── xq_MitkSegmentationView.ui
```

**模块依赖**: xqCommon, MITK Segmentation 模块

**SV参考**: `Plugins/org.sv.gui.qt.mitksegmentation/`

### Task 5.4: org.xq.gui.qt.modeling 插件

**目录**: `Plugins/org.xq.gui.qt.modeling/`

**文件清单**:
```
src/internal/
├── xq_ModelingPlugin.h/.cxx             # Activator
├── xq_ModelingView.h/.cxx               # 主视图
├── xq_ModelingView.ui
├── xq_ModelCreate.h/.cxx                # 模型创建
├── xq_ModelCreate.ui
├── xq_ModelExtractPathsAction.h/.cxx    # 从模型提取路径
├── xq_ModelFaceSelectionWidget.h/.cxx   # 面选择控件
└── xq_ModelPreferencePage.h/.cxx        # 偏好设置
```

**模块依赖**: xqCommon, xqPath, xqSegmentation, xqModelCommon, xqModelOCCT, xqProjectManagement

### Task 5.5: org.xq.gui.qt.meshing 插件

**目录**: `Plugins/org.xq.gui.qt.meshing/`

**文件清单**:
```
src/internal/
├── xq_MeshingPlugin.h/.cxx              # Activator
├── xq_MeshingView.h/.cxx                # 主视图
├── xq_MeshingView.ui
├── xq_MeshCreate.h/.cxx                 # 网格创建
├── xq_MeshCreate.ui
├── xq_LocalMeshSizeWidget.h/.cxx        # 局部尺寸控件
└── xq_MeshPreferencePage.h/.cxx         # 偏好设置
```

**模块依赖**: xqCommon, xqModelCommon, xqMeshCommon, xqProjectManagement

### Task 5.6: org.xq.gui.qt.simulation 插件

**目录**: `Plugins/org.xq.gui.qt.simulation/`

**文件清单**:
```
src/internal/
├── xq_SimulationPlugin.h/.cxx           # Activator
├── xq_SimulationView.h/.cxx             # 主视图
├── xq_SimulationView.ui
├── xq_SimJobCreate.h/.cxx               # 仿真任务创建
├── xq_SimJobCreate.ui
├── xq_CapBCWidget.h/.cxx                # 边界条件控件
├── xq_SolverProcessHandler.h/.cxx       # 求解器进程管理
├── xq_SimulationPreferencePage.h/.cxx   # 偏好设置
└── xq_SimulationPreferencePage.ui
```

**模块依赖**: xqCommon, xqMeshCommon, xqSimulation, xqProjectManagement

### Task 5.7: org.xq.gui.qt.romsimulation 插件

**目录**: `Plugins/org.xq.gui.qt.romsimulation/`

**文件清单**:
```
src/internal/
├── xq_ROMSimulationPlugin.h/.cxx        # Activator
├── xq_ROMSimulationView.h/.cxx          # 主视图
├── xq_ROMSimulationView.ui
└── xq_ROMSimulationPreferencePage.h/.cxx
```

**模块依赖**: xqCommon, xqROMSimulation, xqProjectManagement

### Task 5.8: org.xq.gui.qt.multiphysics 插件 ★新增

**目录**: `Plugins/org.xq.gui.qt.multiphysics/`

**文件清单**:
```
src/internal/
├── xq_MultiPhysicsPlugin.h/.cxx         # Activator
├── xq_MultiPhysicsView.h/.cxx           # 主视图
├── xq_MultiPhysicsView.ui
├── xq_MPJobCreate.h/.cxx                # 任务创建
├── xq_MPJobCreate.ui
├── xq_MPBCWidget.h/.cxx                 # 边界条件控件
├── xq_MPBCWidget.ui
└── xq_MultiPhysicsPreferencePage.h/.cxx # 偏好设置
```

**模块依赖**: xqCommon, xqMultiPhysics, xqProjectManagement

**SV参考**: `Plugins/org.sv.gui.qt.multiphysics/`

### Task 5.9: org.xq.gui.qt.imageprocessing 插件

**目录**: `Plugins/org.xq.gui.qt.imageprocessing/`

**文件清单**:
```
src/internal/
├── xq_ImageProcessingPlugin.h/.cxx      # Activator
├── xq_ImageProcessingView.h/.cxx        # 主视图
├── xq_ImageProcessingView.ui
├── xq_ImageSeedInteractor.h/.cxx        # 种子点交互
└── xq_ImagePreferencePage.h/.cxx        # 偏好设置
```

**模块依赖**: xqCommon, xqImageProcessing, xqProjectManagement

---

## Phase 6: 集成测试与运行

### Task 6.1: 构建验证 ✅ COMPLETE

**检查清单**:
1. ✅ `cmake` 配置无错误完成
2. ✅ 所有模块 `.so` 生成到 `lib/` (13个)
3. ✅ 所有插件 `.so` 生成到 `lib/plugins/` (14个)
4. ✅ Application 可执行文件生成 (`build/bin/XQ`)

**构建过程中修复的主要问题**:
- Python3 版本 (系统 3.8 → svExternals 3.11)
- OpenMP 必须在 MITK 之前 find_package
- Export 宏命名 (TOUPPER + _EXPORT 后缀)
- AUTOMOC/AUTOUIC/AUTORCC 替代手动 qt6_wrap_cpp
- Berry→MITK API 迁移 (IPreferences, CoreServices 等)
- MITK 2024.06 API 变更 (DoRead, Write, SetReaderDescription 等)
- HDF5 1.12 vs 1.14 ABI 不匹配 (--allow-shlib-undefined)
- QString ↔ std::string 转换 (mitk::IPreferences API)
- PerformOk() 返回类型 void→bool
- UI 控件名称与 .ui 文件不匹配

### Task 6.2: 运行时验证

**检查清单**:
1. 启动 XQ 可执行文件，BlueBerry 框架正确加载
2. 默认透视图布局正确显示（DataManager + ProjectManager + Editor + Console）
3. 所有插件在 MITK Plugin 框架中注册成功
4. DataStorage 中可创建和管理各类型数据节点
5. 项目创建/打开/保存功能正常

### Task 6.3: 运行脚本 ✅ COMPLETE

**文件**: `~/XQ/run-xq.sh`

```bash
#!/bin/bash
export LD_LIBRARY_PATH="${HOME}/svExternals/install/mitk-2024.06/lib:${HOME}/svExternals/install/vtk-9.3.0/lib:${HOME}/svExternals/install/itk-5.4.0/lib:${HOME}/svExternals/install/qt-6.7.0/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${HOME}/svExternals/install/qt-6.7.0/plugins"
./build/bin/XQ "$@"
```

---

## 实施优先级策略

### 第一梯队（最小可运行应用）
> 目标: 空窗口能编译并启动

1. Phase 1 全部 (CMake 骨架)
2. Task 2.1 (Application 插件)
3. Task 4.1 (Common 模块 — 被所有模块依赖)

### 第二梯队（数据基础设施）
> 目标: 项目管理 + DataManager 可用

4. Task 3.1 (ProjectDataNodes)
5. Task 3.3 (DataManager 插件)
6. Task 3.4 (ProjectManager 插件)

### 第三梯队（核心工作流：Path → Seg → Model → Mesh）
> 目标: 血管建模流水线可运行

7. Task 4.2 (Path 模块) + Task 5.1 (PathPlanning 插件)
8. Task 4.3 (Segmentation 模块) + Task 5.2 (Segmentation 插件)
9. Task 4.4 (Model/Common 模块) + Task 5.4 (Modeling 插件)
10. Task 4.5 (Model/OCCT 模块)
11. Task 4.6 (Mesh 模块) + Task 5.5 (Meshing 插件)
12. Task 4.13 (QtWidgets 模块)
13. Task 4.12 (ProjectManagement 模块)

### 第四梯队（仿真 + 扩展）
> 目标: 完整 SV 功能对等

14. Task 4.7 (Simulation) + Task 5.6 (Simulation 插件)
15. Task 4.8 (ROMSimulation) + Task 5.7 (ROM 插件)
16. Task 4.9 (MultiPhysics 模块) + Task 5.8 (MultiPhysics 插件)
17. Task 4.10 (MachineLearning 模块)
18. Task 4.11 (ImageProcessing 模块) + Task 5.9 (ImageProcessing 插件)
19. Task 5.3 (MITK Segmentation 插件)
20. Task 3.2 (PythonDataNodes 插件)

### 第五梯队（测试与优化）
> 目标: 质量保证

21. Phase 6 (集成测试)
22. UI 美化和 QSS 样式应用

---

## 技术备注

### 1. ITK 覆盖 VTK_DIR 问题

ITK 的 CMake 配置会设置自己的 `VTK_DIR`。解决方案：

```cmake
# 保存
set(VTK_DIR_SAVE "${VTK_DIR}")
find_package(ITK REQUIRED)
# 恢复
set(VTK_DIR "${VTK_DIR_SAVE}")
```

### 2. MITK 双路径系统

- **编译时**: 使用 `${MITK_DIR}` (build 目录) 获取 headers + cmake modules
- **运行时**: 使用 `${MITK_INSTALL_DIR}` (install 目录) 获取 `.so` 文件

两者缺一不可。`LD_LIBRARY_PATH` 必须包含 install 目录。

### 3. CppMicroServices 模块初始化

每个模块都需要 `usFunctionGenerateModuleInit()` 生成的 `.cpp` 初始化文件。此文件定义了模块的元数据和激活函数。在 `xq_create_module()` 宏中自动处理。

### 4. ObjectFactory 模式

SV 中每个数据模块都有一个 `ObjectFactory` 类，在 CppMicroServices 模块加载时通过 `RegisterObjectFactory` 注册到 MITK IO 框架。这使得 MITK 可以自动发现和使用各数据类型的 IO/Mapper。

```cpp
class xq_PathObjectFactory : public mitk::CoreObjectFactoryBase {
public:
    void CreateFileExtensions(...) override;
    mitk::Mapper::Pointer CreateMapper(mitk::DataNode*, MapperSlotId) override;
    void RegisterIOFactories() override;
};
```

### 5. plugin.xml 与 BlueBerry 扩展点

每个插件的 `plugin.xml` 定义了它向 BlueBerry 框架注册的扩展点：
- `org.blueberry.ui.views` — 注册视图
- `org.blueberry.ui.perspectives` — 注册透视图
- `org.blueberry.ui.preferencePages` — 注册偏好设置页

### 6. 命名约定

| 类型 | 前缀 | 示例 |
|------|------|------|
| 普通数据类 | `xq_` | `xq_PathElement`, `xq_Contour` |
| MITK 封装类 | `xq_Mitk` | `xq_MitkMesh`, `xq_MitkSimJob` |
| VTK Mapper | `xq_XxxVtkMapper` | `xq_PathVtkMapper3D` |
| 插件 Activator | `xq_XxxPlugin` | `xq_PathPlanningPlugin` |
| 视图类 | `xq_XxxView` | `xq_PathPlanningView` |
| ObjectFactory | `xq_XxxObjectFactory` | `xq_PathObjectFactory` |

### 7. files.cmake 模式

每个模块/插件都使用 `files.cmake` 替代直接在 CMakeLists.txt 中列出文件，提高可维护性：

```cmake
# files.cmake
set(H_FILES
    xq_PathElement.h
    xq_Path.h
    ...
)
set(CPP_FILES
    xq_PathElement.cxx
    xq_Path.cxx
    ...
)
set(MOC_H_FILES
    xq_PathDataInteractor.h
    ...
)
set(RESOURCE_FILES
    Interactions/xq_PathConfig.xml
    ...
)
```

---

## v1.0 → v2.0 变更日志

| 问题类型 | 描述 | 修复 |
|----------|------|------|
| 缺失模块 | MultiPhysics 模块未包含 | 新增 Task 4.9 |
| 缺失模块 | MachineLearning 模块未包含 | 新增 Task 4.10 |
| 缺失模块 | Model/OCCT 子模块未包含 | 新增 Task 4.5 |
| 缺失插件 | MultiPhysics 插件未包含 | 新增 Task 5.8 |
| 缺失插件 | MITKSegmentation 插件未包含 | 新增 Task 5.3 |
| 缺失插件 | PythonDataNodes 插件未包含 | 新增 Task 3.2 |
| 缺失文件 | Common: Spline, VtkParametricSpline | 补充到 Task 4.1 |
| 缺失文件 | Path: ObjectFactory, LegacyIO | 补充到 Task 4.2 |
| 缺失文件 | Segmentation: 大量类缺失 (~15 个) | 补充到 Task 4.3 |
| 缺失文件 | Model: Utils, Factory, Operation 等 | 补充到 Task 4.4 |
| 缺失文件 | Mesh: TetGen, Factory, Adaptor 等 | 补充到 Task 4.6 |
| 缺失文件 | Simulation: XmlWriter, ObjectFactory | 补充到 Task 4.7 |
| 缺失文件 | ROMSimulation: Utils, ObjectFactory | 补充到 Task 4.8 |
| 缺失文件 | ProjectManagement: 各 Folder 类, DataNodeOp | 补充到 Task 4.12 |
| 结构问题 | QSS 内容重复(内联+任务) | 移除内联代码，仅保留设计规范 |
| 结构问题 | xq_AboutDialog 重复列出 | 去重 |
| 结构问题 | PluginList.cmake 位于 Phase 5 | 移到 Phase 1 (Task 1.5) |
| 缺失配置 | xq.ini MITK 应用配置文件 | 新增到 Task 1.6 |
| 缺失配置 | target_libraries.cmake | 新增到 Task 1.6 |
| 命名不一致 | MITK 封装类前缀不统一 | 统一为 `xq_MitkXxx` 模式 |
| 缺失模式 | ObjectFactory 注册模式未说明 | 每个数据模块添加 ObjectFactory |
| 依赖顺序 | ITK 覆盖 VTK_DIR 未说明 | 技术备注中详细记录 |
| 新增 | SWIG 依赖未列入 | 依赖表新增 SWIG |
| 新增 | MITK 双路径解释 | 依赖表和技术备注中说明 |
