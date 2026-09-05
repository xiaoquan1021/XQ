# Canonical Shell A build and run entry

Status: implemented for review; **pending manual acceptance**.

Automated green output is evidence only. It does not by itself close A1, A15,
or Shell A. Final acceptance requires source review, evidence inspection, and a
real-machine GUI review against `D:\XQ\规划-GoogleEarth全路线与壳子阶段.md`.

## What this entry owns

```text
canonical_shell_env.bat
  -> configure_canonical_shell.bat ON|OFF
       -> build_gui_wt.bat / build_shell_noflow_wt.bat
  -> run_xq.bat [project-path]
  -> verify_canonical_shell_a.bat
```

This child addresses only the canonical-entry parts of A1 and A15:

- isolated QtBase 6.7.0 from `windows-x64-vascular`;
- exact VTK 9.3.0, ITK 5.4.0, GDCM 3.0.10, HDF5 1.14.3 and tinyxml2 8.0.0 roots;
- Python 3.11 locked for CMake package metadata only, then excluded from the
  product link graph, PE closure, and GUI runtime path;
- distinct Flow ON/OFF trees, `cmake --fresh`, clean-first binary rebuilds, and
  one fixed, sequential evidence script.

It does not implement or accept the Path/module/centerline gates A5, A8, or A13.

## Public commands

Run from a normal command prompt; the scripts load MSVC themselves:

```bat
XQ\build_gui_wt.bat
XQ\build_shell_noflow_wt.bat
XQ\verify_canonical_shell_a.bat
XQ\run_xq.bat "D:\path\to\project.xqproj"
```

The build wrappers use these new trees and do not overwrite the older evidence
trees:

- `XQ\build_shell_a_on`
- `XQ\build_shell_a_off`

The repository `.gitignore` keeps generic machine-local `build_*.bat` and
`run_*.bat` files ignored, but explicitly versions these three public entries.

`run_xq.bat` launches only `build_shell_a_on\xq_app.exe`, forwards every command
line argument, uses the isolated Qt plugin directory, and replaces the inherited
runtime `PATH` with locked dependency bins plus Windows system directories. It
does not add Python, MMG, the legacy Qt bin, or host Anaconda.

## Configuration overrides

All values are owned by `canonical_shell_env.bat` and may be set before calling
an entry. Common overrides are:

```bat
set "XQ_CANONICAL_TEST_DATA_ROOT=D:\data\0007_H_AO_H"
set "XQ_CANONICAL_BUILD_ON=D:\builds\xq-shell-a-on"
set "XQ_CANONICAL_BUILD_OFF=D:\builds\xq-shell-a-off"
set "XQ_CANONICAL_LOG_ROOT=D:\evidence\xq-shell-a"
set "XQ_CANONICAL_SAMPLE_ID=0007_H_AO_H"
```

Repo, test, build, and log locations are portable overrides. The current ITK
5.4 install metadata embeds absolute GDCM/HDF5 package directories, so an
existing Externals install **cannot be copied or moved** and made valid merely by
setting `XQ_CANONICAL_EXTERNALS_ROOT`. That override is valid only after the
complete dependency install has been rebuilt at the target path and its ITK
metadata points at the matching GDCM/HDF5 roots. The checker rejects mismatched
embedded paths rather than silently consuming the old installation.

Exact package/tool roots have `XQ_CANONICAL_*` overrides under the same rule.
Missing package/version configs, tools, sample root, the isolated Qt cache, or
equal ON/OFF directories fail before configure. `CMAKE_PREFIX_PATH` is rebuilt
from the six exact roots and never contains the aggregate
`install\windows-x64` directory. The caller's `CMAKE_PREFIX_PATH` is cleared,
and both CMake package registries are disabled before discovery.

## Why each verification step exists

`verify_canonical_shell_a.bat` is deliberately sequential:

1. source guard: proves the public wrappers still use the shared canonical entry;
2. Flow ON configure/build: resets the CMake cache, clean-builds every binary,
   and proves exact discovery plus build-graph/PE closure;
3. Flow ON focused tests: exercises dependency boundaries and real app startup;
4. Flow ON full suite: checks that the canonical recipe did not break the product;
5. repeat the same evidence for the distinct Flow OFF tree;
6. negative probes: prove bad/empty/wrong-version package inputs fail instead of
   silently falling back to another host package.

The focused/full suites are not acceptance substitutes. They are run because
they exercise the binary produced by the exact entry being reviewed.

Each run creates:

```text
.trellis\workspace\ocean\shell-a-canonical-logs\<yyyyMMdd-HHmmss>\
  metadata.txt
  00-source-contract.log
  01-flow-on-build.log
  02-flow-on-focused.log
  03-flow-on-full.log
  04-flow-off-build.log
  05-flow-off-focused.log
  06-flow-off-full.log
  07-negative-probes.log
```

The script exits at the first failed step and prints the exact evidence file.

## Required manual review

Do not sign off from pass totals alone. The reviewer must still:

1. inspect both `CMakeCache.txt` files and confirm exact Qt/VTK/ITK/tinyxml2
   package dirs, the requested `XQ_ENABLE_FLOW` value, and configure-only Python;
2. inspect graph/PE log output and confirm Python, legacy Qt, zstd, MMG,
   MITK/Slicer/CTK/BlueBerry and unresolved non-system DLL hits are zero;
3. run `run_xq.bat` with the intended project path on the review machine and
   verify the real project opens without demo data or silent empty fallback;
4. check the visible GUI behavior required by the Shell A review, record the
   machine, sample/project ID, screenshots and any limitation next to the logs;
5. compare the result directly with A1 and A15 in the `D:\XQ` acceptance source.

Until that review is recorded, the correct conclusion is: **canonical entry
implemented; automated evidence may be green; manual acceptance pending**.

## Failure interpretation

- missing path/config: fix or explicitly override the locked root; do not widen
  package discovery;
- Qt isolation failure: repair/rebuild the isolated QtBase prefix; do not copy a
  host DLL into it;
- graph or PE failure: treat it as dependency ingress, even if tests happen to run;
- ON/OFF CTest failure: keep the two suites sequential and diagnose the named tree;
- GUI launch failure: keep the log and project path; do not substitute a different
  build tree or claim success from headless tests.
