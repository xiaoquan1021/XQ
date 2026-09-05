# Design — GUI 接真实影像管线 (`06-30-gui-real-pipeline`)

> 配套 `prd.md`。本设计只覆盖**接线 + 两个壳 bug**,不改算法、不改渲染器内核、不碰大规模 handle。
> 依赖方向不变:`app → services → adapters → io → core`。

## 1. 架构边界与改动面

只动 **app 层**(主窗口、stage 面板、MPR 视图)+ **资源**(字体)。services/adapters/io/core **只调用、不修改**。

| 模块 | 文件 | 改动性质 |
|---|---|---|
| 打开图像/项目 | `src/app/XQMainWindow.{h,cpp}` | 新增 File 菜单 action + 文件对话框 + 调 reader + seed 节点 |
| MPR 真实图像 | `src/app/XQMainWindow.cpp`(`loadDemoVolume`/`showImage` 邻域) | 加载成功后用真实 volume 替换合成 demo |
| 切片种子拾取 | `src/visualization/XQMprView.{h,cpp}` | 新增 picking 模式 + 点击→体素信号 |
| stage 接真数据 | `src/ui/panels/XQStageWidgets.cpp` + `XQMainWindow` | 从选中节点取 image/buffer/轮廓,填 Intent,解灰 |
| 字体根治 | `src/app/main.cpp` + `resources/`(可能嵌字体) | 显式 addApplicationFont |
| 第四格复核 | `src/visualization/XQMprView.cpp` | 已加黑底兜底,真机复核 |

**契约红线**(评审必查):新增代码不得 `#include` 或持有大规模 `GeometryHandle`/payload vector;取数据只经 `XQDataNode::payload()` 的现有只读访问 + `NodeId`;不改 `XQSceneRenderer` 公有签名;保留所有现有 objectName/公有 API。

## 2. 数据流

### 2.1 打开图像(R1)
```
File▸打开图像(.vti)
  → QFileDialog 选 .vti
  → VtkImageAdapter::loadVtiWithBuffer(path, &image, &buffer)   [adapters]
  → 失败: LoadStatus → 可读消息 → QMessageBox,return(不崩不静默)
  → 成功:
      - 经 command 把 Image 节点插入场景(可撤销;payload 携带 image+buffer 的持有者)
      - mprView_->setImage(image, buffer)  用真实体替换 demo
      - 导航器滑条 range 重设为真实各轴维度
      - 状态栏提示已加载 + 维度
```
所有权:`image`/`buffer` 必须有稳定持有者(MPR 只借指针,见 XQMprView 注释「owner 须保活到下次 setImage」)。持有者放主窗口成员(如 `std::shared_ptr` / 专用结构),**不放局部变量**。优先复用现有 `XQSourcePayload` + 一个驻留 buffer 持有者;若现有节点 payload 不持有解码 buffer,则主窗口加一个 `activeImage_` 成员持有解码后的 volume+buffer(类比现有 `demoVolume_`)。

### 2.2 打开 SV 项目(R1)
```
File▸打开 SimVascular 项目
  → QFileDialog 选项目目录/.sv 文件
  → SvProjectReader::read(...)  [io]  → 图像路径 + 路径(.pth)+ 轮廓(.ctgr)+ 波形(.flow)
  → 经 command 批量 seed:Image / Path / ContourGroup / (波形挂到算例)节点入树
  → 若含图像,顺带走 2.1 的 setImage
```

### 2.3 stage 接真数据(R2)
```
当前选中节点(onSceneSelectionChanged 已知 selected NodeId/domain)
  → stage 面板执行时,从主窗口取「当前活动图像 image+buffer」「选中轮廓组」「选中算例」
  → 填入对应 Intent(ThresholdIntent.image/buffer、LoftIntent 的轮廓、SolveIntent 的算例)
  → controller→service→命令栈(现有)
  → 结果节点入树(可撤销)
```
取数据的桥:主窗口持有「当前活动图像」(2.1 的 `activeImage_`)。stage 面板需要它,但**面板不能直接 reach 进主窗口内部**。设计:`populateStagePanels` 已有 controller 注入模式;新增一个**回调/提供者接口**(如 `std::function<ActiveImage()> imageProvider`)注入面板,面板执行时调用它拿当前 image+buffer。轮廓组/算例同理用「当前选中节点提供者」。这样面板仍只经注入的提供者,不持有大规模数据、不依赖主窗口具体类型。

### 2.4 分割两模式(R2b)
- **阈值**:选中图像 + 表单上/下阈值 → `SegmentationController::threshold`(image/buffer 来自 provider)。
- **区域生长**:需 `int seed[3]`。流程:
  ```
  分割页选「区域生长」→ 进入「点选种子」模式
    → MPR 某切片上点击
    → XQMprView 发 signal seedPicked(int i, int j, int k)
    → 分割页填 RegionGrowIntent.params.seed = {i,j,k}
    → 执行 regionGrow;SeedOutOfRange/SeedNotInThreshold → 明确提示
  ```

## 3. 切片→体素拾取(R2b 最高风险点)

**约束(已查实)**:切片不是 voxel→pixel 直接 blit,而是 `vtkImageSliceMapper` + `renderer_->ResetCamera()` 离屏渲染到 RGBA 再贴 QLabel(`setScaledContents(false)`,`AlignCenter`)。因此屏幕点击→体素**不能用朴素像素比例**,必须走 VTK 的 display→world→voxel 变换。

### 3.1 方案 A(推荐):VTK 坐标反投影
- 在切片渲染时记录每格的 `vtkRenderer*`(切片用的离屏 renderer)与当前 axis/slice。
- 点击像素 (px,py) on QLabel:
  1. QLabel 像素 → 渲染图像像素:扣除 AlignCenter 的居中偏移(pixmap 比 label 小则有黑边)、扣除 pixmap 与离屏 buffer 的尺寸关系(本设计里 pixmap == 离屏 buffer 尺寸,1:1)。
  2. 渲染图像像素 → 世界坐标:VTK 离屏 renderer 是平行投影的切片视图;用 `vtkCoordinate`(SetCoordinateSystemToDisplay,SetValue(x, height-1-py, 0))→ `GetComputedWorldValue(renderer)` 拿 world (x,y,z)。注意 VTK display 原点在左下,Qt 在左上,y 要翻转。
  3. 世界坐标 → 体素 (i,j,k):用 `XQImageVolume` 的 geometry(origin/spacing/direction)逆变换:`vox = direction^{-1} · (world - origin) / spacing`,四舍五入并夹紧到 extent。被切的那一轴用当前 slice index 直接给定(不从 world 反推,避免数值误差)。
- 产出 `int seed[3]`,发 `seedPicked`。

### 3.2 方案 B(回退,若 A 的相机变换在 offscreen 下不稳)
- 改用「不经 VTK 相机」的直接切片提取做拾取层:已知 axis、slice、in-plane 两轴映射(代码注释 line 226-229 已列)。若离屏渲染严格按 image extent 等比铺满且无额外缩放,则可用 in-plane 维度 + pixmap 尺寸建立线性映射。**风险**:ResetCamera 会留边距,等比关系需实测标定。
- 决策点:实现时先试 A;A 在 offscreen 下经程序化往返校验(给定体素→世界→display→反推体素,误差≤1)即采用 A,否则退 B 并在 implement 标注。

### 3.3 校验
单元/集成可测:构造已知 geometry 的小体,模拟 display 坐标,断言反推体素正确(往返误差≤1 体素)。这条进 check 清单。

## 4. 中文字体根治(R4)

**根因假设**(待真机确认):`QFont("Microsoft YaHei")` 靠 family 名匹配,offscreen/精简字体后端可能不扫系统字体 → tofu。

**方案**:
1. 把 `C:/Windows/Fonts/msyh.ttc` 作为应用字体显式加载:`QFontDatabase::addApplicationFont(...)`(优先嵌进 qrc `:/fonts/msyh.ttc` 保证可移植;ttc 19MB 较大,先评估——若不嵌则从系统路径加载,记录环境依赖)。
2. 取加载后返回的 family 名设 `app.setFont`,并保持 qss `font-family` 与之一致。
3. **验证不靠直方图**:写一个最小程序化检查——用 `QRawFont::fromFont` + `glyphIndexesForChars` 对一个汉字(如「轴」U+8F74)断言 glyphIndex≠0(0=无字形=tofu)。作为 check 项;另需真机目视(用户确认)。

> 历史教训(memory `harness-cannot-read-local-images`):本环境 Read 看不到本地图,字体是否 tofu **不能靠离屏抓图判断**;必须用 glyph 可用性程序检查 + 用户真机目视。

## 5. 第四格(R5)

已加 `QFrame#xqMprVolumeFrame { background:#000000 }` 兜底,像素确认纯黑。真机复核:Quad 模式四格是否都可见、3D 格 VTK 渲染区是否正常。若用户之前看的是改字体前旧图(`xq_ui_zh.png`),则本项可能已无缺陷,确认即收尾。

## 6. 兼容 / 回归

- `test_main_window` 依赖 `showImage`/`lastRgbaByteCount`/objectName:不得破坏。新增加载入口走新 action,不改 `showImage` 签名。
- demo volume 仍保留(无文件加载时的默认),只是被真实加载覆盖。
- 全量 ctest(现 49 绿)不退化;新增拾取映射 + 字体 glyph 检查补测试。

## 7. 风险与回退

| 风险 | 缓解 |
|---|---|
| 切片拾取坐标映射(VTK offscreen 相机变换) | 方案 A 先行 + 往返校验;不达标退方案 B,implement 标注 |
| ttc 19MB 嵌 qrc 膨胀 | 先评估嵌入 vs 系统路径加载;默认系统路径 + 记录依赖,真机若不命中再嵌 |
| stage provider 注入改动面 | 用 std::function 提供者,面板不依赖主窗口具体类型,契约不破 |
| 活动图像所有权悬空 | 主窗口成员持有(类比 demoVolume_),MPR 只借指针 |
