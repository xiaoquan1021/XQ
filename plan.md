# XQ Windows Migration Plan

## Active Phase: Windows Build Convergence

1. Keep the Windows x64 / VS2022 Externals stack usable from source-built installs.
2. Build XQ with `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`.
3. For each configure/build failure:
   - Record the failure in `execution_log.md`.
   - Add the smallest practical PowerShell/CMake regression test first.
   - Fix the recipe, CMake, or source code at the root cause.
   - Rerun the targeted test and the failed command.
4. Once XQ builds, run the available PowerShell tests, CMake build checks, and `diff --check`.
5. Commit and push the completed iteration.

## Next Phase: Autonomous Research

After the current build convergence phase is verified, research comparable medical imaging workstations and open-source projects. Extract high-value features into this plan before starting the next implementation loop.
