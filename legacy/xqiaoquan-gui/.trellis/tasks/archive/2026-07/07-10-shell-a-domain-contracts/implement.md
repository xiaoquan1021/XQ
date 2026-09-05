# Implementation Plan：领域契约与持久化

## Preflight

- 依赖：父任务规划已审阅；无需其它 child 完成。
- 只从 `feat/render-arch` 的获批实现分支工作；规划阶段不得执行本文件。
- 开始编辑前记录 `git status --porcelain`，保护既有 `CHECK-*`、`EXECUTE-*` 与 `XQ/xq_app_dist/`。
- 先运行现有 versioned save、payload round-trip、scene relation 和 architecture boundary 基线。

## Steps

1. **增加 ScaleSlot**
   - 在 core 定义稳定三值枚举和字符串转换。
   - 让 `XQDataNode` 权威持有，补齐构造/复制访问器。
   - 更新 scene model 或一个纯只读投影，使非渲染消费者可读取。
   - 增加 absent/三值、copy、command 与 group 回归；旧项目 absent，不静默默认 Organ。

2. **增加 image metadata payload**
   - 新增 `XQImageVolumePayload`，明确不保存真实像素或外部对象。
   - 添加 clone、domain 映射和 metadata 一致性测试。
   - 保留 `XQSourcePayload(Image)` 的 1.2 读取兼容路径。

3. **实现 VesselProfileV1 与 validator**
   - 新增值类型、stable sample id、来源/单位/frame/quality 字段。
   - 新增 domain/asset kind 和 scene group 映射。
   - 实现集中 validator 与合法/非法矩阵测试。

4. **升级持久化格式**
   - 将新 writer 版本提升为 1.3，保留 reader 1.2 分支。
   - 序列化 ScaleSlot、image metadata block 和 profile metadata/blob roles。
   - 对 profile blob type/components/count/byteCount 做严格校验。
   - 增加 1.2 -> in-memory -> 1.3 save/load 迁移测试。
   - 补齐 ContourGroup typed payload round-trip，并修正其 AssetKind 映射。

5. **实现 revision、DerivationStamp 与原子命令**
   - 增加持久化 contentRevision/DerivationStamp。
   - 增加 project-level node/asset/binding/multi-source batch command 与 AssetRegistry 可逆 API；对 asset register、node insert、binding、Scene relation、Asset relation 逐个故障注入验证全回滚和 id 单调。
   - 增加 semantic replace+invalidate command，保存/恢复 stale snapshot；保留普通 replace 给 reader/materialize。
   - 用 command 添加 Profile node 并链接 Path/Contour 多来源。
   - 修改上游后断言 Profile 传递式 stale，undo/redo 恢复 payload/revision/stale。
   - 验证失败 profile 不产生 node、asset、relation 或 undo entry。

## Focused validation

```powershell
cmake --build XQ/build_gui --config Release --target test_scale_slot test_vessel_profile test_payload_roundtrip test_project_versioned_save test_scene_relations
ctest --test-dir XQ/build_gui -C Release --output-on-failure -R "scale_slot|vessel_profile|payload_roundtrip|project_versioned_save|scene_relations|arch_boundaries"
```

## Final validation

```powershell
cmd /c XQ\build_gui_wt.bat
ctest --test-dir XQ/build_gui -C Release --output-on-failure
```

## Review gates

- 不得在缺少 1.2 兼容 fixture 的情况下合入 schema 1.3。
- 不得把半径、Flow BC/材料/结果加入 Profile。
- 不得让 image payload 持有真实像素、`IVoxelSource` 或外部库类型。
- 不得以 LOD 字段满足 ScaleSlot 验收。
- check worker 必须检查 writer/reader、payload/domain/scene model 的完整跨层映射。

## Rollback points

- Commit A：ScaleSlot core + tests。
- Commit B：image metadata payload + tests。
- Commit C：VesselProfileV1 + validator + tests。
- Commit D：schema 1.3 writer/reader + ContourGroup 修复 + migration tests。
- Commit E：revision/DerivationStamp/atomic multi-source/semantic stale undo。
- 任一阶段失败时回滚该独立 commit；在 schema gate 通过前不得启动依赖 child。
