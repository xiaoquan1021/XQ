# XQ Menu Bar & Toolbar Redesign

## Problem Statement

XQ's current menu bar and toolbar structure needs a complete redesign. The previous implementation deviated from SimVascular's proven layout. This spec defines a fresh design that:

1. Follows SimVascular's menu/toolbar structure as the primary reference
2. Adapts for XQ's specific views and capabilities
3. Removes non-functional or unnecessary items
4. Maintains clean organization

## Reference: SimVascular's Structure

### SV Menu Bar: File | Edit | Tools | Window | Help

- **File**: New, Open, Save, Save As, Close, ---, File Open, Save Scene, ---, Exit
- **Edit**: Undo, Redo
- **Tools**: [Core SV views in priority order], ---, [Utility views alphabetically]
- **Window**: New Window, ---, Open Perspective→, Reset Perspective, Close Perspective, ---, Preferences
- **Help**: Welcome, ---, About

### SV Toolbars (4 total):

1. `mainActionsToolBar`: Save | Undo | Redo | [ImageNav] | [ViewNav] | Axial | Sagittal | Coronal
2. `perspectiveToolBar`: SimVascular | Viewer | Visualization
3. `svViewToolBar`: Path Planning | 2D Seg | 3D Seg | Modeling | Meshing | CFD Sim | ROM Sim | MultiPhysics
4. `viewToolBar`: [DICOM] | Data Manager | Level Set | MITKSeg | Clipping Plane | Seg Utilities

---

## Proposed Design

### Menu Bar: File | Edit | Tools | Window | Help

#### File Menu (`&File`)

| # | Label | Shortcut | Type | Handler |
|---|-------|----------|------|---------|
| 1 | &New Project | Ctrl+N | Action | `xq_FileCreateProjectAction` |
| 2 | &Open Project | Ctrl+O | Action | `xq_FileOpenProjectAction` |
| 3 | &Save Project | Ctrl+S | Action | `xq_FileSaveProjectAction` |
| 4 | Save &As... | Ctrl+Shift+S | Action | `OnSaveAs()` |
| 5 | &Close Project | Ctrl+W | Action | `OnCloseProject()` |
| - | SEPARATOR | | | |
| 6 | Open &Data File... | | Action | `OnOpenDataFile()` (QmitkFileOpenAction equivalent) |
| 7 | Save All as MITK &Scene... | | Action | `OnSaveScene()` |
| - | SEPARATOR | | | |
| 8 | &Exit | Ctrl+Q | Action | `OnExit()` |

**Design rationale**: Matches SV exactly. Removed: Import DICOM (moved to Tools), Import SV Project (moved to Tools), Recent Projects submenu (unnecessary complexity). SV doesn't have these in File menu.

#### Edit Menu (`&Edit`)

| # | Label | Shortcut | Type | Handler |
|---|-------|----------|------|---------|
| 1 | &Undo | Ctrl+Z | Action | `mitk::UndoController::Undo()` |
| 2 | &Redo | Ctrl+Y | Action | `mitk::UndoController::Redo()` |

**Design rationale**: Identical to SV.

#### Tools Menu (`&Tools`)

| # | Label | Type | Handler / View ID |
|---|-------|------|-------------------|
| 1 | &Path Planning | Action | `org.xq.views.pathplanning` |
| 2 | 2D &Segmentation | Action | `org.xq.views.segmentation` |
| 3 | 3D Se&gmentation | Action | `org.xq.views.mitksegmentation` |
| 4 | &Modeling | Action | `org.xq.views.modeling` |
| 5 | M&eshing | Action | `org.xq.views.meshing` |
| 6 | &CFD Simulation | Action | `org.xq.views.simulation` |
| 7 | &ROM Simulation | Action | `org.xq.views.romsimulation` |
| 8 | Multi&Physics | Action | `org.xq.views.multiphysics` |
| - | SEPARATOR | | |
| 9 | &Image Processing | Action | `org.xq.views.imageprocessing` |
| 10 | &Data Manager | Action | `org.xq.views.datamanager` |
| 11 | Pro&ject Manager | Action | `org.xq.views.projectmanager` |

**Design rationale**: Matches SV's approach — core pipeline views first (in workflow order), separator, then utility views. SV puts Data Manager in Tools menu as a utility view. XQ also adds Project Manager here. All items use `berry::QtShowViewAction` pattern (open/focus the view).

#### Window Menu (`&Window`)

| # | Label | Shortcut | Type | Handler |
|---|-------|----------|------|---------|
| 1 | Open &Perspective | | Submenu | (see below) |
| 2 | &Reset Perspective | | Action | `OnResetPerspective()` |
| - | SEPARATOR | | | |
| 3 | &Preferences... | Ctrl+P | Action | `OpenPreferencesDialog()` |

**Open Perspective Submenu:**

| # | Label | Perspective ID |
|---|-------|---------------|
| 1 | XQ Default | `org.xq.defaultperspective` |
| 2 | Viewer | `org.xq.viewerperspective` |
| 3 | Visualization | `org.xq.visualizationperspective` |

**Design rationale**: Matches SV closely. Removed: Show View submenu (views are in Tools menu, matching SV), Toggle Axial/Sagittal/Coronal (toolbar-only, matching SV), Reinitialize Views (not in SV's Window menu), Volume Rendering toggle, Crosshair toggle, Fullscreen toggle. SV keeps Window menu minimal — just perspective management and preferences. Changed shortcut from Ctrl+, to Ctrl+P to match SV.

#### Help Menu (`&Help`)

| # | Label | Type | Handler |
|---|-------|------|---------|
| 1 | &Welcome | Action | `OnWelcome()` |
| - | SEPARATOR | | |
| 2 | &About XQ | Action | `OnAbout()` |

**Design rationale**: Identical to SV. Removed: Keyboard Shortcuts (Ctrl+K) — SV doesn't have this.

---

### Toolbar Structure (4 toolbars, matching SV)

#### Toolbar 1: `mainActionsToolBar` — "Main Actions"

| # | Button | Icon | Handler |
|---|--------|------|---------|
| 1 | Save Project | `SaveAllSV.png` equivalent | `m_SaveProjectAction` |
| 2 | Undo | `edit-undo.svg` | `m_UndoAction` |
| 3 | Redo | `edit-redo.svg` | `m_RedoAction` |
| - | SEPARATOR | | |
| 4 | Axial (checkable) | `axial.png` | `OnToggleAxialPlane()` |
| 5 | Sagittal (checkable) | `sagittal.png` | `OnToggleSagittalPlane()` |
| 6 | Coronal (checkable) | `coronal.png` | `OnToggleCoronalPlane()` |

**Properties:**
- ObjectName: `mainActionsToolBar`
- Movable: false
- ContextMenuPolicy: PreventContextMenu
- ToolButtonStyle: TextBesideIcon (non-macOS)

**Design rationale**: Matches SV. Removed: Image Navigator and View Navigator buttons (XQ doesn't have these views). Removed: Reinit button (not in SV's main toolbar).

#### Toolbar 2: `perspectiveToolBar` — "Perspectives"

| # | Button | Perspective ID |
|---|--------|---------------|
| 1 | XQ Default | `org.xq.defaultperspective` |
| 2 | Viewer | `org.xq.viewerperspective` |
| 3 | Visualization | `org.xq.visualizationperspective` |

**Properties:**
- ObjectName: `perspectiveToolBar`
- Uses `berry::QtOpenPerspectiveAction` instances

**Design rationale**: Matches SV's perspectiveToolBar exactly. Quick perspective switching without going through Window menu.

#### Toolbar 3: `xqViewToolBar` — "XQ Views"

| # | Button | View ID |
|---|--------|---------|
| 1 | Path Planning | `org.xq.views.pathplanning` |
| 2 | 2D Segmentation | `org.xq.views.segmentation` |
| 3 | 3D Segmentation | `org.xq.views.mitksegmentation` |
| 4 | Modeling | `org.xq.views.modeling` |
| 5 | Meshing | `org.xq.views.meshing` |
| 6 | CFD Simulation | `org.xq.views.simulation` |
| 7 | ROM Simulation | `org.xq.views.romsimulation` |
| 8 | MultiPhysics | `org.xq.views.multiphysics` |

**Properties:**
- ObjectName: `xqViewToolBar`
- Uses `berry::QtShowViewAction` instances
- ToolButtonStyle: TextBesideIcon (non-macOS)

**Design rationale**: Matches SV's `svViewToolBar` exactly — core pipeline views in workflow order.

#### Toolbar 4: `viewToolBar` — "Views"

| # | Button | View ID |
|---|--------|---------|
| 1 | Image Processing | `org.xq.views.imageprocessing` |
| 2 | Data Manager | `org.xq.views.datamanager` |
| 3 | Project Manager | `org.xq.views.projectmanager` |

**Properties:**
- ObjectName: `viewToolBar`
- Uses `berry::QtShowViewAction` instances

**Design rationale**: Matches SV's `viewToolBar` — utility/secondary views. XQ has Image Processing, Data Manager, Project Manager as utility views.

---

### Status Bar (unchanged from current)

| Component | Purpose |
|-----------|---------|
| QmitkProgressBar | Progress indicator (hidden by default) |
| QmitkMemoryUsageIndicatorView | Memory indicator |
| Selection Info Label | Current selection status |
| Coordinate Display Label | Mouse position |
| Memory/Node Count Label | Memory and data node count |

---

## What Gets Removed (vs. current XQ)

| Removed Item | Previous Location | Reason |
|-------------|-------------------|--------|
| Import DICOM | File menu | Not in SV's File menu; use Open Data File instead |
| Import SV Project | File menu | Not in SV's File menu; specialized import |
| Recent Projects submenu | File menu | Not in SV; adds complexity |
| Measurements submenu | Tools menu | Not in SV; functionality not essential |
| Volume Rendering Presets submenu | Tools menu | Not in SV; can use Preferences |
| Reset Window/Level | Tools menu | Not in SV |
| Clipping Plane toggle | Tools menu | Not in SV |
| DICOM Info | Tools menu | Not in SV |
| Screenshot (F12) | Tools menu | Not in SV |
| Show View submenu | Window menu | Not in SV; views are in Tools |
| Toggle Axial/Sagittal/Coronal | Window menu | Toolbar-only (matches SV) |
| Reinitialize Views | Window menu | Not in SV's Window menu |
| Volume Rendering toggle | Window menu | Not in SV |
| Crosshair toggle | Window menu | Not in SV |
| Fullscreen (F11) | Window menu | Not in SV |
| Keyboard Shortcuts (Ctrl+K) | Help menu | Not in SV |

## What Gets Added (vs. current XQ)

| Added Item | Location | Reason |
|-----------|----------|--------|
| Perspective toolbar | New toolbar | Matches SV's `perspectiveToolBar` |
| Utility views toolbar | New toolbar | Matches SV's `viewToolBar` (split from single XQ toolbar) |
| Data Manager in Tools | Tools menu | Matches SV — all views accessible from Tools |
| Project Manager in Tools | Tools menu | XQ-specific utility view |

## Slots That Can Be Removed from Header

The following member variables and slots become unused:
- `m_ImportDicomAction` — no longer in menu
- `m_RecentProjectsMenu` — no longer in menu
- `m_ToggleAxialAction`, `m_ToggleSagittalAction`, `m_ToggleCoronalAction` — still needed for toolbar, but remove from menu
- `m_VolumeRenderingAction` — no longer in menu
- `m_CrosshairAction` — no longer in menu

**Note**: Keep all slot implementations intact — they still work and may be re-added later. Only remove menu/toolbar references.

## Implementation Approach

### Action Pattern

Match SV's approach: use `berry::QtShowViewAction` for all view-opening actions in Tools menu and toolbars. These actions are created once and shared between menu items and toolbar buttons (same action object added to both). This replaces the current pattern of custom QAction + manual slot connections (OnShowPathPlanning, etc.).

For File/Edit/Window/Help menu items, continue using standard QAction with slot connections (same as SV).

### Scope

Modify only the `PostWindowCreate()` method in `xq_WorkbenchWindowAdvisor.cxx` to rebuild all menus and toolbars. The existing slot implementations (lines ~892-2111) remain completely unchanged — they're still valid for File, Edit, Window, and Help actions.

### Files to Modify

1. `xq_WorkbenchWindowAdvisor.cxx` — Rewrite PostWindowCreate() menu/toolbar sections
2. `xq_WorkbenchWindowAdvisor.h` — Clean up unused member variables (optional, non-breaking)
3. `xq.qss` — Update toolbar styles for 4 toolbars
4. Delete stale workbench state after build (perspective layout change)

### Files NOT Modified

- `xq_DefaultPerspective.cxx` — Keep current layout
- `xq_WelcomePart.cxx` — Keep current design
- All view plugins — Unchanged
- All slot implementations in WorkbenchWindowAdvisor — Unchanged
