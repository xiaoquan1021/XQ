# XQ Lightweight Tools UI Redesign

## Goal

XQ is a lightweight medical imaging application. The UI should make existing tools easy to find and use without turning the app into a heavy workstation. The redesign should remove the fragile "category button + underline dropdown" interaction and replace it with stable, direct access to the real tools that already exist.

This document summarizes the proposed direction only. It does not add new product features or new workflow entities.

## Current Problem

The previous toolbar used broad category buttons such as:

```text
Import | Trace | Contour | Build | Solve | Axial | Sagittal | Coronal | Logging
```

Some buttons opened an underline dropdown before showing the actual tool. This created several user experience and implementation problems:

- The visible buttons were not the real tools.
- Users had to click through an extra layer to reach a tool.
- Floating dropdown state was fragile: focus loss, positioning, closing, overlap, and synchronization could break.
- Non-tool actions were mixed into the tools area, such as file import, panel display, view layout, and logging.

## Design Principle

Use a lightweight desktop medical imaging layout:

```text
Menu Bar
Global Toolbar
Left Panels
Central Viewports
Status Bar
```

Do not add new heavyweight UI regions such as an activity rail, command palette, right inspector, report workspace, or review workspace unless those features already exist and are approved separately.

The key rule is:

```text
File operations belong in File.
Panel visibility belongs in View or Window.
Viewport layout belongs in View/Layout.
Real processing tools belong in Tools and the main toolbar.
```

## Menu Classification

### File

File should contain actions directly related to data/project loading, saving, and exporting:

```text
Import DICOM...
Open Data File...
Open Recent
Save
Export
```

These should not be placed under Tools because they are data entry operations, not analysis tools.

### View

View should contain existing windows, panels, and viewport display controls:

```text
Data Manager
Image Navigator
Data Explorer
Workspace Explorer
Logging
Axial
Sagittal
Coronal
3D
Layout presets
```

Panel-like items such as Data Manager belong here because users interpret them as visible UI regions, not processing tools.

### Tools

Tools should contain only real processing or analysis tools:

```text
Image Processing
Path Planning
2D Segmentation
3D Segmentation
Solid Modeling
Mesh Generation
Flow Simulation
```

These are the same core tools that should replace the old broad toolbar buttons.

### Window

Window should contain layout-management actions if available:

```text
Reset Layout
Save Layout
Restore Default Layout
Toggle Panels
```

If these actions do not currently exist, do not invent them during the lightweight toolbar redesign.

## Toolbar Redesign

The top toolbar should show direct access to the real tools, not category wrappers.

Avoid this:

```text
Trace
  Vessel Planning
```

Prefer this:

```text
Path
```

Because the full tool names are long, the toolbar should use icon plus short label, with the complete name in tooltip and menu text.

Recommended toolbar labels:

```text
Process | Path | 2D Seg | 3D Seg | Model | Mesh | Flow
```

Recommended full-name mapping:

```text
Process -> Image Processing
Path    -> Path Planning
2D Seg  -> 2D Segmentation
3D Seg  -> 3D Segmentation
Model   -> Solid Modeling
Mesh    -> Mesh Generation
Flow    -> Flow Simulation
```

This keeps the toolbar compact without scroll buttons, left/right arrows, or overflow controls.

## Toolbar Layout

The toolbar should stay simple:

```text
Open | Save | Undo | Redo || Process | Path | 2D Seg | 3D Seg | Model | Mesh | Flow || Layout
```

Notes:

- `Open` is acceptable as a file shortcut, but it should semantically map to File actions.
- The tools section should use fixed-size tool buttons to avoid layout jitter.
- The active tool should be visually highlighted.
- Full names should remain available via tooltip and the Tools menu.
- The status bar can show the active full tool name, for example `Current Tool: Path Planning`.

## Icons

Existing XQ had minimalist SVG stage icons for the old broad categories:

```text
stage-import.svg
stage-trace.svg
stage-contour.svg
stage-build.svg
stage-solve.svg
```

Those icons match the visual style but not the new tool semantics. A new lightweight icon set was added under:

```text
Code/Source/ImagingWorkbench/Plugins/org.xq.core.application/resources/
```

New icon mapping:

```text
tool-process.svg -> Image Processing
tool-path.svg    -> Path Planning
tool-seg-2d.svg  -> 2D Segmentation
tool-seg-3d.svg  -> 3D Segmentation
tool-model.svg   -> Solid Modeling
tool-mesh.svg    -> Mesh Generation
tool-flow.svg    -> Flow Simulation
```

All icons use:

```text
24px viewBox
2px rounded strokes
XQ blue stroke color
simple shape language
no emoji
no gradients
no new feature concepts
```

Usage example:

```cpp
QIcon(":/xq/tool-path.svg")
```

## User Experience Rules

- The user should see the real tool directly.
- The user should not need to open a dropdown to discover the actual tool.
- The toolbar should not require horizontal scrolling, arrow navigation, or hidden overflow for the core tools.
- Short labels should be understandable after one use.
- Tooltips should preserve full names for clarity.
- Menus should remain complete and explicit for discoverability.
- Do not add tools, panels, or workflows that do not currently exist.

## Implementation Scope

Recommended first implementation pass:

1. Keep the existing menu bar.
2. Move file actions out of Tools and into File where applicable.
3. Move panel/window visibility actions into View or Window where applicable.
4. Replace old toolbar category buttons with the real tool buttons.
5. Use the new `tool-*.svg` icons with short labels.
6. Remove the underline dropdown behavior for top-level tool access.

Success criteria:

```text
The toolbar displays existing real tools directly.
No top-level tool button opens an underline dropdown.
All core tool buttons fit in one row at the target desktop width.
File, View, Window, and Tools menus have clear responsibilities.
No new non-existing product feature is introduced.
```
