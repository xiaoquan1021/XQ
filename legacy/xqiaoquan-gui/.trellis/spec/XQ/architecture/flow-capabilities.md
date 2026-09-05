# Flow execution capability boundary

Flow capability controls whether the shell may execute the 1D Flow stack. It
does not control whether projects may contain, display, analyze, load, or save
typed historical Flow data.

## Scenario: Disable Flow execution without deleting Flow data

### 1. Scope / Trigger

- Trigger: changes to `XQ_ENABLE_FLOW`, `WorkflowCapabilities`,
  `XQWorkflowSession`, the shell's fifth page, Flow source lists, or tests that
  call a Flow execution service indirectly.
- Purpose: keep Flow a real optional typed capability without growing a plugin
  registry, a Noop implementation, or a second project-data model.

### 2. Signatures

```cmake
option(XQ_ENABLE_FLOW "Build Flow execution services and controllers" ON)
```

```cpp
struct WorkflowCapabilities {
    bool flowSolver1D;
    static constexpr bool flowCompiledIn();
    static constexpr WorkflowCapabilities compiledDefaults();
    static constexpr WorkflowCapabilities withoutFlow();
    constexpr WorkflowCapabilities constrainedToBuild() const;
};

explicit XQWorkflowSession(WorkflowCapabilities capabilities);
bool XQWorkflowSession::hasFlowCapability() const;
FlowController* XQWorkflowSession::flowController() const;
FlowSmokeController* XQWorkflowSession::flowSmokeController() const;
```

`StagePanelContext` deliberately has no Flow capability/controller/output-id
fields. Its fifth-page inputs are `VesselProfileController*`,
`PathModuleController*`, the Path-input output-id provider, and ordinary scene
providers. Widgets do not read the CMake option directly.

### 3. Contracts

- `XQ_ENABLE_FLOW` defaults to `ON`. Runtime capability requests are intersected
  with the compiled capability; an OFF binary cannot manufacture Flow by passing
  `flowSolver1D=true`.
- Runtime OFF creates neither `FlowController` nor `FlowSmokeController`.
  Path, Segmentation, Modeling, Meshing, AI, command-stack, render, and project
  IO wiring remain live.
- Build OFF excludes these execution sources:
  `BoundaryConditionService.cpp`, `FlowSolver1D.cpp`,
  `FlowInputAssembler.cpp`, `FlowGeometrySmokeService.cpp`,
  `FlowController.cpp`, and `FlowSmokeController.cpp`.
- Build OFF retains `XQSimulationCase`, `XQFlowResult`, `XQFlowSmoke` persistent
  DTOs, payloads, reader/writer code, and `FlowMetricsService`. Historical Flow
  nodes remain visible and may be analyzed without starting a solver.
- The fifth page of the six-stage shell is `Modules`, not `Flow`. It exposes
  Path input preparation plus Path-only module execution in both Flow ON and
  OFF builds. The shell never exposes Flow source/protocol/run controls or a
  Flow capability state.
- The old `xqStagePage_Flow`, `xqFlowSourceCombo`, `xqFlowSmokeProtocol`, and
  `xqFlowSmokeRun` object contracts are retired. Do not keep them hidden or
  disabled; absence is part of the replacement contract.
- Test ownership follows symbols actually used, not the test's directory or
  name. A nominal AI test that calls `FlowSolver1D` or
  `BoundaryConditionService` is a Flow-execution test and is registered only
  when `XQ_ENABLE_FLOW=ON`.
- Do not introduce `OperationRegistry`, universal `Context`, dynamic DLL
  discovery, hot loading, Python runtime, or fake providers to satisfy this
  boundary.

### 4. Validation & Error Matrix

- Runtime OFF + attach project -> `hasFlowCapability()==false`, both Flow
  controller accessors return null, non-Flow controllers remain non-null.
- Build OFF + runtime request ON -> request is clamped to unavailable; no missing
  symbol, fallback solver, or delayed crash.
- Build OFF + project containing Profile/Case/FlowResult -> load and resave
  succeed with typed payload, provenance, values, ScaleSlot, and lineage intact.
- Flow ON or OFF -> page count remains six; page 5 is `xqStagePage_Modules`,
  Path modules run, all retired Flow controls are absent, and historical
  FlowResult rows remain in the scene tree.
- A non-Flow target still referencing an excluded execution symbol -> OFF link
  must fail; classify or refactor the target instead of re-adding the solver.
- Missing Flow sources in OFF `build.ninja` -> expected. Any listed execution
  source is a boundary failure even if tests happen to pass.

### 5. Good/Base/Bad Cases

- Good: `XQ_ENABLE_FLOW=OFF` builds the real app shell, opens a historical Flow
  project, shows the result read-only, saves it again, and runs PathValidate
  from the Modules page without any Flow run entry.
- Base: default ON preserves the backend Flow smoke/service/controller and their
  independent tests, while the shell still exposes only Modules on page 5.
- Bad: keep a disabled/hidden Flow page beside Modules, retain old Flow object
  names, delete historical FlowResult nodes, or register a Noop solver that
  returns success.

### 6. Tests Required

- `test_workflow_capabilities`: compiled-default intersection, explicit runtime
  OFF, non-Flow controllers, and real writer/reader/resave of historical Flow
  payload plus lineage.
- `test_workflow_session`: default ON/OFF controller construction and command
  gateway regression.
- `test_main_window`: six-page Modules replacement in runtime OFF and compiled
  ON, old Flow controls absent, PathValidate source/summary/dump visible, and
  historical FlowResult still visible.
- `test_shell_a_gui`: real DICOM -> Path input -> PathValidate -> save/reopen,
  with zero SimulationCase/FlowResult produced by the shell and the same test
  registered in ON and OFF builds.
- ON Release: focused Flow smoke/controller tests, then full `ctest`.
- OFF Release: independent configure/build, assert the six execution sources are
  absent from generated build rules, then run focused shell gates and full
  `ctest`.

### 7. Wrong vs Correct

#### Wrong

```cpp
flow_.reset(new NoopFlowController());
return FlowController::Status::Ok;
```

```cpp
stageContext.flowSmoke = session.flowSmokeController();
panel->addWidget(buildFlowPage(...)); // leaves the superseded shell entry alive
```

```cmake
# OFF still compiles the solver; the UI merely hides its button.
target_sources(xq_services PRIVATE src/services/flow/FlowSolver1D.cpp)
```

#### Correct

```cpp
if (capabilities_.flowSolver1D) {
    flow_.reset(new FlowController(scene, stack));
} else {
    flow_.reset();
}
```

```cpp
stageContext.pathModules = session.pathModuleController();
panel->addWidget(buildModulesPage(...)); // identical shell surface in ON/OFF
```

```cmake
if(XQ_ENABLE_FLOW)
    list(APPEND _xq_service_sources
        src/services/flow/FlowSolver1D.cpp
    )
endif()
```
