# Design: Real App Entry And Workflow Wiring

## Scope

This task introduces a small, testable startup boundary in the app layer. The boundary owns:

- parsing the minimal startup inputs used by `main`;
- opening an empty project when no native project path is provided;
- loading a native `.xqproj` when a path is provided;
- attaching `XQMainWindow` to the mutable scene and `XQCommandStack`.

It does not move domain logic into `app`, and it does not make `xq_app_shell` responsible for file IO.

## Proposed Structure

Add an app-layer startup helper next to `main.cpp`, for example:

- `src/app/XQAppStartup.h`
- `src/app/XQAppStartup.cpp`

The helper should expose a small API that is testable without entering `QApplication::exec()`:

```cpp
namespace xq {

enum class XQAppStartupStatus {
    Ok,
    ProjectOpenFailed,
    ProjectLoadFailed
};

struct XQAppStartupConfig {
    std::string projectPath;
};

struct XQAppStartupState {
    XQProject project;
    XQCommandStack commandStack;
};

XQAppStartupConfig parseAppStartupArguments(int argc, char** argv);
XQAppStartupStatus initializeAppStartup(const XQAppStartupConfig& config,
                                        XQAppStartupState* state);
void attachMainWindowWorkflow(XQMainWindow* window, XQAppStartupState* state);

}
```

Names can be adjusted to match local style, but the boundary must keep the load/attach logic outside `main()` so tests can call it directly.

## Data Flow

### Empty startup

`main(argc, argv)` -> parse args -> `project.open()` -> create `XQMainWindow` -> `attachWorkflow(&project.scene(), &commandStack)` -> `show()` -> `app.exec()`.

No demo nodes are inserted.

### Native project startup

`main(argc, argv)` -> parse first positional project path -> `XQProjectReader::load(path, &result)` -> move `result.project` into startup state -> create `XQMainWindow` -> `attachWorkflow(&project.scene(), &commandStack)` -> `show()` -> `app.exec()`.

Reader diagnostics stay available to the startup result if the implementation chooses to expose them, but the minimal requirement is failing closed with a non-zero process exit when the load status is not Ok.

## Dependency Boundary

- `xq_app` may link `xq_io` privately because the executable entry point reads project files.
- `xq_app_shell` should keep its current dependency shape: Qt + UI shell + controllers, without project reader IO.
- The new startup unit can be compiled into a small `xq_app_startup` static library linked by both `xq_app` and the startup test, or compiled directly into those two targets. Prefer a small library if it keeps CMake cleaner.

## Error Handling

- `initializeAppStartup` returns explicit status values instead of throwing through Qt startup.
- No load failure may fall back to demo data.
- `main()` maps non-Ok startup status to a non-zero return code before showing a window.

## Compatibility

- Preserve `XQMainWindow(const XQScene*)` and `setScene(const XQScene*)` for existing tests and read-only display use.
- Preserve `test_main_window` behavior; this task adds coverage for the actual entry wiring rather than replacing the existing window unit test.
