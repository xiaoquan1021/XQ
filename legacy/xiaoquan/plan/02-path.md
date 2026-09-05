# 02 路径(M1)

## 目标

血管路径的创建、编辑、加载、重采样,以及沿路径的稳定局部标架(供 contour 放置、斜切、放样)。
纯领域服务,无 UI / VTK / 插件依赖。

## 所有权

```
src/services/path/PathService.h / .cpp
src/services/path/CenterlineFrameService.h / .cpp
tests/services/path/PathServiceTest.cpp
tests/services/path/CenterlineFrameServiceTest.cpp
```

## 输入 / 输出

- 输入:`XQImageVolume`(来源影像)、世界坐标 `Point3` 控制点(来自 MPR 点击,UI 负责转换)、
  采样间距。
- 输出:新建/编辑后的 `XQPath` 节点(链接到来源影像),以及改 scene 的命令。

## 公开 API(返回命令,不直接改 scene)

```cpp
createPathCommand(name, sourceImageNodeId, controlPoints, spacing)
    -> AddNodeWithSourceRelationCommand   // 建 path 节点 + ImageToPath 关系(同一可撤销操作)
moveControlPointCommand(node, index, newPos, spacing)   -> ReplacePayloadCommand
insertControlPointCommand(node, index, point, spacing)  -> ReplacePayloadCommand
deleteControlPointCommand(node, index, spacing)         -> ReplacePayloadCommand
resamplePathCommand(node, spacing)                      -> ReplacePayloadCommand
```

编辑命令复制旧 `XQPath` payload,保留原 path id 与 sourceImageNode。

## 局部标架(CenterlineFrameService)

沿 `XQPath` 计算 origin / tangent / normal / binormal / arcLength 的标架序列。复用
`XQPath::resample` + `frameAtArcLength`。要求:**避免法向沿路径突然翻转**(用平行传输 /
旋转最小化标架),为 contour 编辑与放样提供足够元数据。不在 contour、viewer、放样里各算一套。

## 校验

- 创建路径至少 2 个控制点;采样间距必须为正。
- 编辑要求节点为 `XQDomainType::Path` 且带 `XQPath` payload。
- move/delete 索引必须存在;insert 索引可等于控制点数;delete 后至少剩 2 点。

## 禁止

- 不引入 Qt / VTK / ITK / 插件依赖;不从 service 直接改 `XQScene`;不把路径几何存进 UI 状态。
- SimVascular 仅作血管路径语义的功能参考,**不作为迁移源码**。

## 验收

- 能从影像创建路径;`0007/Paths/*.pth` 读入的路径与新建路径共用同一 payload 类型。
- 控制点编辑 / 重采样 / undo / redo 测试通过。
- contour 编辑可直接消费路径,无需读 UI 状态。
