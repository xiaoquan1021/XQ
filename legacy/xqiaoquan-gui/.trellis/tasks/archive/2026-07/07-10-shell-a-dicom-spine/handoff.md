# Handoff: DICOM 数据脊柱

更新时间：2026-07-10（Asia/Shanghai）

## 当前判断

- 仓库：C:\Users\OCEAN\Desktop\XQIAOQUAN-gui
- 分支：feat/google-earth-shell-a
- Trellis child：.trellis/tasks/07-10-shell-a-dicom-spine
- child 完成条件已满足，等待本次 finish-work 归档。
- 当前 HEAD：d9b7575 feat: resolve persisted DICOM image resources
- Commit D、默认构建、focused tests、spec 更新、独立审查和授权去标识真实 DICOM 门均已完成。
- finish-work 只归档当前 child；归档后暂停，不进入 profile-assembly 或任何下一个 Trellis 子任务。

## 已提交

1. 0f5de07 feat: define DICOM series reader contract
2. 37e5fc1 feat: add audited DICOM series adapter
3. c0a18c0 feat: prepare atomic DICOM project imports
4. d9b7575 feat: resolve persisted DICOM image resources

## Commit D 已完成

- GeometryResourceManager 支持按项目 AssetRegistry 绑定的 resident voxel install/acquire/remove。
- resident DICOM 使用独立 cache flavor，不冒充 managed voxel blob，并遵守预算、LRU、pin 和 lease 生命周期。
- install generation 同时比较 source 与 pin，关闭同指针重装 ABA。
- asset 删除、同 AssetId 重注册、locator/UID/fingerprint/geometry 变化都会使旧 resident 失效。
- ImageResourceResolver resident-first；miss 时优先以 .xqproj 父目录解析 sourceRelPath，仅在相对目录缺失时使用 sourceAbsPath。
- Windows rooted-relative 绕过已拒绝，包括 C:series 与 \series。
- resolver 始终使用持久化 SeriesInstanceUID，并校验 UID、fingerprint、LPS geometry、scalar/component、buffer size、intensity/window。
- reader I/O 前后重校验 AssetRecord；locator-only 或 identity/fingerprint/geometry 并发变化返回 SourceChanged，不安装旧结果。
- reader diagnostics 被映射为固定安全模板，不透传 reader 自由文本或 PHI。
- image payload 保持 metadata-only；XQProjectReader 不读取或解码 DICOM。
- 新增可选 XQ_DICOM_TEST_DATA_ROOT / test_dicom_real_series。默认空时不注册测试；非空无效目录在 configure 时直接失败。
- 真实门要求恰好一个多切片 series，并覆盖显式 UID、LPS round-trip、原子导入、保存重开、headless lazy acquire 与 SHA-256 字节一致性。
- 真实门分阶段释放 initial decode、import residency 与 lazy decode，只跨阶段保留 geometry/identity、byte count 和 SHA-256，避免多份临床体数据造成约 4 倍内存峰值。
- .trellis/spec/XQ/core/source-interface.md 已加入完整 DICOM residency/lazy-reopen 契约。

## 当前验证证据

- cmd /c XQ\build_gui_wt.bat：通过；configure/generate 成功，ninja 无待构建项。
- focused gate：12/12 通过。
- 单独复验 test_image_resource_resolver：1/1 通过。
- 默认 Release 全量 CTest：83/83 通过，2026-07-10，本次用时 18.60 秒。
- XQ/build_gui/CMakeCache.txt 中 XQ_DICOM_TEST_DATA_ROOT 为空；test_dicom_real_series 默认未注册。
- git diff --cached --check 在 Commit D 前干净；staged 边界精确为 13 个文件。
- 独立只读终审：无剩余 actionable correctness、privacy 或 MITK 问题。
- 曾使用仓库内 CC0 合成 regular-oblique fixture 验证可选 gate plumbing：1/1 通过。该结果只证明 plumbing，不等于授权真实数据验收。
- 非空无效 XQ_DICOM_TEST_DATA_ROOT 的受控 configure 失败已验证。
- 用户授权候选 C:\Users\OCEAN\Desktop\XIAOQUAN\0080_H_PULM_H 已受控检查：目录内容主要为 VTP/CTGR/PTH，并包含一个 VTI；DICOM 候选为 0。test_dicom_real_series 实测发现 0 个 series，因不满足“恰好一个多切片 DICOM series”而失败；没有 DICOM 对象可做去标识 allowlist 审查。随后已把 XQ_DICOM_TEST_DATA_ROOT 恢复为空，并确认默认测试集回到 83 项。
- 公开血管向候选 TCIA CPTAC-LUAD 胸部 CTA 可发现为单 series，但当前 adapter 在 canonical decode 阶段按契约拒绝；未放宽测试，候选文件已移除。
- 最终真实样例：TCIA LIDC-IDRI-0957，CT/CHEST，65 slices，SeriesInstanceUID 1.3.6.1.4.1.14519.5.2.1.6279.6001.314917368146772872954571551463，CC BY 3.0。TCIA 官方声明公开 DICOM 经标准化去标识并满足 HIPAA Safe Harbor。
- 外部样例目录：D:\XQ\data\dicom\tcia_lidc_idri_0957_ct\series；不进入 Git。来源、许可、去标识依据和复现命令记录在同级 SOURCE.md。
- 下载 ZIP SHA-256：ca4324bb14873c64f9bd616002cf380f02f8f215105e22fb7b28b2769c6925ee；TCIA 随包 MD5 65/65 匹配，65 个 DICOM 均有 DICM preamble。
- test_dicom_real_series：1/1 通过，用时约 3.02 秒。
- 注册真实门后的 Release 全量 CTest：84/84 通过，用时 19.21 秒。
- 验收后已恢复 XQ_DICOM_TEST_DATA_ROOT 为空，并确认默认测试集回到 83 项、真实门不注册。

## 真实数据门（已通过）

最终样例来自 TCIA LIDC-IDRI 的公开临床胸部 CT。精确下载单个 SeriesInstanceUID，许可证与哈希文件放在 series 目录外，因此测试目录恰好只含一个 65 层 series。

通过命令：

    cmake -S XQ -B XQ/build_gui -DXQ_DICOM_TEST_DATA_ROOT="D:\XQ\data\dicom\tcia_lidc_idri_0957_ct\series"
    cmake --build XQ/build_gui --config Release --target test_dicom_real_series
    ctest --test-dir XQ/build_gui -C Release --output-on-failure -R "^test_dicom_real_series$"

验收日志只记录技术元数据与 pass/fail，没有输出自由文本 DICOM tag 或 PHI。

## finish-work 边界

- task.py archive 与 add_session.py 只处理当前 task/workspace 的托管文件。
- CHECK-*.md、EXECUTE-*.md 与 XQ/xq_app_dist/ 保持未跟踪且不修改。
- 不归档其他 completed/planning task。

## 禁止操作

- 不使用 MITK。
- 不升级 project schema。
- 不执行 git add .。
- 不执行 git clean、git reset --hard 或其他清理用户工作区的操作。
- 不删除、暂存或修改上述本地文件。
- 不进入 profile-assembly 或下一个 Trellis 子任务。

## 下一步

执行 trellis-finish-work：归档当前 DICOM child、记录本次提交与验收，然后暂停；不得进入下一个 child。
