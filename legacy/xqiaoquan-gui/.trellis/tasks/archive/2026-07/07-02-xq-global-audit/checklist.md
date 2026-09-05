# Checklist — 07-02 XQ 全局审计任务

> 所有决策已拍板(见下「已决决策」);执行 agent 逐项勾选,每个 P 级完成后跑对应验收。
> 顺序建议:P0-1 → P0-2 → P0-3 → P1 → P3 → P4。P2 全程不改代码。

## 前置硬门(第一步,不满足就停)
- [ ] `git status --porcelain` 干净;若有审计范围文件(BlobStore/GRM/XQProjectReader/CMakeLists 等)属别人未提交改动 → **停,报告用户**,不自行 stash/commit/丢弃
- [ ] 已确认执行基线(m8b2 收尾后的 HEAD,由用户指定),在其上开 `fix/xq-global-audit` 分支再动手
- [ ] 每 commit 后 `git show --stat` 核对无夹带范围外文件

## 已决决策(不得再选、不得改方向)

- [x] **P0-1 = 方案 A(port 接口下沉 core)+ 新增架构边界 ctest**,不用改 spec 折中方案。已验证 adapter .cpp 只用 core 类型。
- [x] **P0-2 = canonical 路径封闭校验(`isPathWithinRoot`)+ 合并三份词法校验到 core/io/PathSafety + voxel 链路补 confinement**。
  实测 `is_symlink` 对 junction 无效(已作废),改用 `weakly_canonical` 前缀比较;发现逃逸直接拒绝,不跟随、不兜底。
- [x] **P0-3 = resample 设 1e6 采样点上限 + reader 检查返回值**,非 Ok 报 ParseError。
- [x] **P1 = AssetRegistry 溢出拒绝发号 + mmap 加对齐 assert**(debug-only,零 release 开销)。
- [x] **P2 拆分类 = 立档不改**(monolith reader/writer、God Object 主窗口、ResidentKey 值类型、命令标志),后续单独开任务。
- [x] **P3 = 只做影像体 GetScalarPointer + 掩膜 InsertNextPoint 两处**,其余逐元素循环不改。
- [x] **P4 = GeometrySourceResolver 单测 + 服务层失败枚举必做**,金字塔倒挂等立档不做。
- [x] **存疑项 1:SurfaceLodBuilder.h 暴露 VTK = 保留不改**。理由:visualization 是 disposable 可视化产物层,spec 允许 VTK 出现在渲染层;已 grep 确认仅 XQSceneRenderer + 其测试引用,无跨层扩散。
- [x] **存疑项 2:XQMainWindow.cpp:326 / XQSceneRenderer.cpp:519 自动降级 = 保留不改**。理由:属产品既有设计(网格失败回退表面、LOD 失败回退全几何),非本次新增;CLAUDE.md「别加降级」针对 AI 擅自新增,强删历史设计会改变可见行为、反违反同一规则精神。若日后要改属新需求,单独提。

## P0-1 依赖逆流(EXECUTE-P0.md §P0-1)  ✅ commit 07be369
- [x] S1 移动 ITetMesher.h 到 core/meshing,改 guard,更新 7~8 处 include
- [x] S2 移动 XQAiSegmentationRequest.h 到 core,改 guard,更新 8 处 include
- [x] S3 抽 XQAiBackends.h 到 core,AiService.h 删旧定义+加 include,OnnxBackend.h 改 include
- [x] S4 CMake:三 adapter 去 xq_services,mmg 保留 xq_adapter_tetgen
- [x] S5 spec external-libs.md 补两段式说明
- [x] S6 新建 tests/cmake/check_arch_boundaries.cmake + 注册 test_arch_boundaries(照 test_portable_test_data_root),先红后绿
- [x] 验收:`-DXQ_ENABLE_TETGEN=ON -DXQ_ENABLE_MMG=ON` 完整构建 + ctest 全绿 65/65(含 test_arch_boundaries);`grep services/... src/adapters/` 为空;ONNX 头自洽(探针编过,依赖本机缺失)

## P0-2 符号链接逃逸(EXECUTE-P0.md §P0-2)  ✅ commit 9fb4a09
- [x] S1 新建 core/io/PathSafety.{h,cpp}(isConfinedRelativePath + isPathWithinRoot)+ 进 xq_core 源列表
- [x] S2 三处删本地词法定义、改调用共享工具(BlobStore:301 / XQProjectReader:1011 / GRM:103)
- [x] S3 MmapBlob 加 FILE_FLAG_OPEN_REPARSE_POINT + 拒 reparse(MappingControl 已确认 RAII,直接 return)
- [x] S4 BlobStore::readVerified + GRM geometry 链路 + GRM voxel 链路(新缺口)各加 isPathWithinRoot 封闭校验
- [x] S5 负面测试:BlobStore junction 逃逸被拒 + GRM voxel 词法(`../`)与 junction 两用例(逃逸目标须放尺寸正确的真实 blob,否则先红不可达=假绿);自制 check 框架,`cmd /c mklink /J`,建链接失败不得静默当过;先红后绿本地验证
- [x] 验收:默认档 ctest 全绿 65/65 + 两负面用例绿;confinement 全仓库仅一份定义(PathSafety);isPathWithinRoot 出现在 PathSafety + BlobStore + GRM(geometry+voxel)

## P0-3 resample 死循环(EXECUTE-P0.md §P0-3)  ✅ commit c983670
- [x] S1 XQPath::resample 加 kMaxSamples=1e6 上限保护
- [x] S2 reader rebuild_path 检查 resample 返回值,非 Ok → TextError
- [x] S3 负面测试:恶意 spacing 加载返回 ParseError 且不 hang;core 单测 resample 边界
- [x] 验收:ctest 全绿 65/65 + 恶意工程限时返回错误(篡改上限/篡改 reader 返回值均先红)

## P1 健壮性(EXECUTE-P1-P4.md §P1)  ✅ commit e92adb2
- [x] P1-1 AssetRegistry createAsset/registerAsset 溢出拒绝 + 测试
- [x] P1-2 MappedGeometrySource.cpp 的 4 处 reinterpret_cast(182/196/197/215)前加 alignof assert(MappedVoxelSource 无 reinterpret_cast,不动)
- [x] 验收:Release ctest 全绿 65/65 + Debug 档(assert 生效)mmap/payload 相关 6/6 绿;溢出守卫先红后绿(完整脚本重编,exe 真重链)

## P3 性能(EXECUTE-P1-P4.md §P3)  ✅ commit 6d02793
- [x] P3-1 影像体基址+步进(已确认单组件 float/extent 0-based)
- [x] P3-2 掩膜先数后填(SetNumberOfPoints + SetPoint)
- [x] 验收:test_scene_renderer* + test_image_viewer 全绿,ctest 65/65

## P4 测试(EXECUTE-P1-P4.md §P4)  ✅ commit 1d6047d
- [x] P4-1 GeometrySourceResolver 补 TetOnly 等价 + 未知 assetId 不崩用例(文件已存在,补用例;CMake 已注册无需改)
- [x] P4-2 FlowController(SolveFailed/Rejected/NullScene)+ ModelingService(InvalidModel);AiService null→InvalidInput/InvalidFlow/NullScene 现有已覆盖,identify 返回值类型无 null 路径故不改
- [x] 验收:新用例纳入 ctest 全绿 65/65;四测试均先红后绿(篡改守卫真转红)

## 全局收尾
- [x] 每 commit 精确列文件(禁 `git add -A`),中文 message,类型前缀
- [x] 最终 `-DXQ_ENABLE_TETGEN=ON -DXQ_ENABLE_MMG=ON` 全量 Release ctest 全绿 65/65,exe 时间戳真更新(每轮完整脚本重编)
- [x] 三条铁律复检:core 无 Qt/VTK/ITK(rg 空);adapter 不 include/link services(rg 空,P0-1 已治理 + test_arch_boundaries 守护);公开 API 只 XQ 类型(架构边界测试覆盖)
