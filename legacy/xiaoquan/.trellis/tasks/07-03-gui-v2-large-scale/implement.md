# implement.md — 执行计划(批次/工作流/验收纪律)

> 模式:复用 07-02-xq-global-audit 的「主审拍板 → EXECUTE 文档 → 便宜 agent 照做 → 主审独立复验」。
> worktree:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/gui-v2`。
> 构建:`build_gui_wt.bat`(已带 XQ_TEST_DATA_ROOT);ctest 用 `ctest_merge.bat`。TETGEN+MMG ON 档复验用主仓 `build_audit_p01.bat` 配方另起 build 目录。

## 批次

- [x] **B1 core/services 热点 + redo 修复**(EXECUTE-B1.md)
  - S0 redo 失败归还;S5a visited 位图;S5b path 单遍 frame/resample 游标;S5c flow 抽帧。
  - 全部先红后绿;等价对拍(S5b 新旧 1e-12;S5c 抽帧=0 档旧行为)。
  - 假绿抽查:S5a 位移故意写错→红;S0 注释掉归还行→红。
- [x] **B2 死代码清零 + 打开/保存工作区**(EXECUTE-B2.md)
  - S1a 删除清单 7 项(rg 零残留验收);S1b openWorkspaceFromPath/saveWorkspaceFile(path) + 对话框转发槽 + roundtrip 测试。
  - 注意:test_app_startup 的 imageLabel 翻页断言同步改(design S1a#3)。
- [x] **B3 XQTaskRunner + 重活迁移**(EXECUTE-B3.md)
  - 新增 XQTaskRunner{h,cpp} + test_task_runner;六类 run lambda 迁移;忙态禁用矩阵;线程纪律(work 零 scene/widget/VTK)。
- [x] **B4 Path 阶段真实化**(EXECUTE-B4.md)
  - buildPathPage 重写;pathPicking 分流;nextNodeId() 统一;test_path_stage.cpp;i18n 新串。
- [x] **B5 内存接通**(EXECUTE-B5.md)
  - 预算 QSettings+Preferences;LOD 默认开;状态栏 psapi 真实内存 + 驻留字节;manager 加 geometryBudgetBytes() 访问器。
- [x] **B6 视觉打磨 + 收尾**(EXECUTE-B6.md)
  - qss token 归一/视图身份色/工具栏与状态栏细化/空场景提示;ts 补翻 + lrelease;真机目视 + 全量绿收口。

## 每批固定纪律(写进每份 EXECUTE 头部)

1. 环境:Windows + Git Bash;别信 darwin 注入;构建/测试只用给定 bat。
2. 只做本批;不加兜底/降级;不改验收外代码;测试先红后绿,红的证据(输出片段)留在 result 文件。
3. commit 精确列文件,中文信息 `类型: 描述`;绝不 `git add -A`。
4. work()/UI 线程纪律、Source 1.0 冻结、services 零 Qt/VTK 铁律。
5. 行号会漂移:改前 grep 符号定位;段头 peek 用行首 token(memory 坑)。

## 主审复验(每批)

- 全新目录完整重建(防 ninja 单目标假绿)+ 全量 ctest;B1/B5 另跑 TETGEN+MMG ON 档。
- 每批 ≥1 处假绿抽查(篡改→红→还原→绿)。
- B6 后:真机 run_xq.bat 启动目视 + 用户过目截图。

## 完成定义

AC0~AC7 全勾;spec 回填(core/command-and-scene.md 补 redo 语义;visualization/lod-and-upload.md 补 GUI 默认 LOD;新增 app 层线程纪律条目);task finish + archive。
