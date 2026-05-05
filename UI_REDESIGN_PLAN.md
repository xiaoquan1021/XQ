# XQ UI Redesign Plan — Toolbar Reorganization & Visual Differentiation

## 1. Current State Comparison

### SimVascular (SV) Toolbar Layout
```
┌──────────────────────────────────────────────────────────────────────────┐
│ [Save] [Undo] [Redo] [ImageNav] [ViewNav] [Axial] [Sag] [Cor]         │  ← mainActionsToolBar (single row, everything mixed)
├──────────────────────────────────────────────────────────────────────────┤
│ [Default] [SVToolbar views...]                                          │  ← perspectiveToolBar + svViewToolBar
├──────────────────────────────────────────────────────────────────────────┤
│ [DICOM] [other views...]                                                │  ← viewToolBar
└──────────────────────────────────────────────────────────────────────────┘
```
- **Style**: All tools crammed into horizontal bars, flat icon-beside-text style
- **Color**: Light blue (#e6f2ff) + cyan (#00ccff) accents
- **Navigation**: Perspective tabs + flat tool lists, no guided workflow
- **Layout**: Left panel (DataManager), center (Editor), no explicit right panel

### XQ Current Toolbar Layout
```
┌──────────────────────────────────────────────────────────────────────────┐
│ [New] [Open] [Save] │ [Undo] [Redo] │ [Screenshot]                     │  ← workspaceToolBar (icon-only)
├──────────────────────────────────────────────────────────────────────────┤
│ ① Import  │  ② Trace  │  ③ Contour  │  ④ Build  │  ⑤ Solve            │  ← xqStageBar (bottom-border tabs)
├──────────────────────────────────────────────────────────────────────────┤
│ [Import DICOM] [Open Data File] │ [Data Explorer]                       │  ← xqStageToolsBar (context-sensitive)
├──────────────────────────────────────────────────────────────────────────┤
│ [Axial] [Sagittal] [Coronal] │ [Perspective ▾]                          │  ← sliceControlToolBar
└──────────────────────────────────────────────────────────────────────────┘
```
- **Style**: 4 separate bars, icon-only top bar, pipeline stages unique
- **Color**: Arctic Light (#F8FAFC) + Royal Blue (#2563EB) — already distinct from SV
- **Navigation**: Stage-based guided workflow (CRIMSON-inspired)

### CRIMSON Workflow Inspiration
CRIMSON uses a solver-centric pipeline: **Geometry → Meshing → Boundary Conditions → Solve → Post-process**. This guided, sequential workflow inspired XQ's stage bar but CRIMSON is a Fortran/C++ solver without a Qt GUI.

---

## 2. Redesign Goals

1. **Maximize visual and structural distance from SimVascular** — SV uses horizontal flat bars with everything mixed; XQ should use a **vertical/sidebar** approach or a clearly different spatial arrangement
2. **Reduce toolbar count** from 4 to 2-3 by consolidating related functions
3. **Strengthen the pipeline metaphor** — make the stage flow more prominent and visually connected
4. **Ensure WCAG AA compliance** — ≥4.5:1 contrast, ≥44px touch targets
5. **Keep the Arctic Light + Royal Blue theme** but enhance it with better hierarchy

---

## 3. Proposed New Layout

### Option A: "Ribbon + Pipeline" Layout (RECOMMENDED)

Inspired by ribbon UIs (Microsoft Office / engineering tools like ANSYS), **NOT** by SV's flat toolbar style.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ XQ │ File ▾ │ Edit ▾ │ View ▾ │ Help ▾     (compact menu bar)              │
├─────────────┬────────────────────────────────────────────────────────────────┤
│             │                                                                │
│  ┌────────┐ │  ╔══════════════════════════════════════════════════════════╗  │
│  │①Import │ │  ║  [Import DICOM]  [Open File]  │  [Data Explorer]       ║  │
│  │        │ │  ║                                                         ║  │
│  ├────────┤ │  ╚══════════════════════════════════════════════════════════╝  │
│  │②Trace  │ │                                                                │
│  │        │ │  ┌────────────────────────────────────────────────────────┐    │
│  ├────────┤ │  │              3D / 2D Render Views                     │    │
│  │③Contour│ │  │                                                       │    │
│  │        │ │  │  [Axial] [Sagittal] [Coronal] [3D]                    │    │
│  ├────────┤ │  │                                                       │    │
│  │④ Build │ │  └────────────────────────────────────────────────────────┘    │
│  │        │ │                                                                │
│  ├────────┤ │                                                                │
│  │⑤ Solve │ │                                                                │
│  │        │ │                                                                │
│  └────────┘ │                                                                │
│             │                                                                │
│ [Save] [↩] │                                                                │
│ [↪] [📷]   │                                                                │
└─────────────┴────────────────────────────────────────────────────────────────┘
```

**Key differences from SV:**
- **Left sidebar pipeline nav** (vertical) vs SV's horizontal tabs
- **Context ribbon** at top changes per stage (vs SV's static toolbar)
- **Workspace actions at bottom of sidebar** (vs SV's mixed-in top toolbar)
- **Slice controls embedded in render area** (vs SV's separate toolbar)

### Option B: "Compact Header + Side Rail" Layout

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ XQ  [New] [Open] [Save] [Undo] [Redo]  │  [Axial] [Sagittal] [Coronal]    │
├──────────────────────────────────────────────────────────────────────────────┤
│ │①│②│③│④│⑤│  [Import DICOM] [Open File] [Data Explorer]                    │
├──────────────┬───────────────────────────────────────────────────────────────┤
│  DataManager │              Render Views                                     │
│  ProjectMgr  │                                                               │
│  Tool Panel  │                                                               │
└──────────────┴───────────────────────────────────────────────────────────────┘
```

**This is closer to current layout — less disruptive but also less differentiated from SV.**

---

## 4. Detailed Implementation Plan (Option A)

### Todo 1: Convert Stage Bar from Horizontal to Vertical Left Sidebar

**File**: `xq_WorkbenchWindowAdvisor.cxx`

**Changes**:
- Replace horizontal `QToolBar("Stage Bar")` with a fixed-width `QDockWidget` on the left
- The dock widget contains a custom `QWidget` with vertical `QVBoxLayout`
- Each stage is a tall `QPushButton` (or QToolButton) stacked vertically
- The sidebar has a fixed width of ~80px
- Stage icons use SVG icons (no emoji circled numbers)
- Active stage has a left-edge indicator bar (3px Royal Blue)
- Bottom of sidebar: Save/Undo/Redo/Screenshot as small icon buttons

**Implementation approach**:
```cpp
// Replace xqStageBar QToolBar with:
auto* stageSidebar = new QDockWidget("Pipeline", mainWindow);
stageSidebar->setObjectName("xqStageSidebar");
stageSidebar->setFeatures(QDockWidget::NoDockWidgetFeatures);
stageSidebar->setFixedWidth(80);
stageSidebar->setTitleBarWidget(new QWidget()); // hide title bar

auto* sidebarContent = new QWidget();
auto* sidebarLayout = new QVBoxLayout(sidebarContent);
sidebarLayout->setContentsMargins(0, 8, 0, 8);
sidebarLayout->setSpacing(4);

// Stage buttons
struct StageEntry { const char* label; const char* icon; const char* id; };
StageEntry stages[] = {
    {"Import",  ":/xq/stage-import.svg",  "import"},
    {"Trace",   ":/xq/stage-trace.svg",   "trace"},
    {"Contour", ":/xq/stage-contour.svg", "contour"},
    {"Build",   ":/xq/stage-build.svg",   "build"},
    {"Solve",   ":/xq/stage-solve.svg",   "solve"},
};

m_StageGroup = new QButtonGroup(sidebarContent);
m_StageGroup->setExclusive(true);

for (const auto& s : stages) {
    auto* btn = new QToolButton();
    btn->setText(s.label);
    btn->setIcon(QIcon(s.icon));
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setCheckable(true);
    btn->setFixedSize(72, 56);
    btn->setProperty("stageId", QString(s.id));
    m_StageGroup->addButton(btn);
    sidebarLayout->addWidget(btn, 0, Qt::AlignCenter);
}

sidebarLayout->addStretch();

// Workspace actions at bottom
auto* saveBtn = new QToolButton();
saveBtn->setIcon(QIcon(":/xq/save.svg"));
// ... undo, redo, screenshot similarly
sidebarLayout->addWidget(saveBtn, 0, Qt::AlignCenter);

stageSidebar->setWidget(sidebarContent);
mainWindow->addDockWidget(Qt::LeftDockWidgetArea, stageSidebar);
```

### Todo 2: Merge Stage Tools Bar and Workspace Toolbar into Single Context Ribbon

**File**: `xq_WorkbenchWindowAdvisor.cxx`

**Changes**:
- Remove separate `workspaceToolBar` (New/Open/Save etc. move to sidebar bottom + File menu)
- Keep `xqStageToolsBar` but restyle it as a **context ribbon** — wider, with grouped sections
- Remove `sliceControlToolBar` — move Axial/Sagittal/Coronal controls into the render area as floating overlay buttons (or into the context ribbon under a "View" group)

**Result**: Only 1 horizontal toolbar remains (the context ribbon), plus the vertical sidebar.

**Toolbar count**: 4 → 2 (sidebar + ribbon), a clear structural departure from SV's 3-4 horizontal bars.

### Todo 3: QSS Styling for Sidebar Pipeline

**File**: `xq.qss`

**New styles**:
```css
/* ========== Stage Sidebar ========== */
QDockWidget#xqStageSidebar {
    background-color: #1E293B;  /* Dark slate — contrasts with Arctic Light content */
    border-right: 1px solid #334155;
}
QDockWidget#xqStageSidebar QToolButton {
    color: #94A3B8;
    background: transparent;
    border: none;
    border-left: 3px solid transparent;
    border-radius: 0;
    padding: 8px 4px;
    font-size: 11px;
    font-weight: 500;
}
QDockWidget#xqStageSidebar QToolButton:hover {
    color: #E2E8F0;
    background: rgba(255, 255, 255, 0.08);
}
QDockWidget#xqStageSidebar QToolButton:checked {
    color: #FFFFFF;
    background: rgba(37, 99, 235, 0.15);
    border-left: 3px solid #2563EB;
    font-weight: bold;
}

/* Sidebar bottom utility buttons */
QDockWidget#xqStageSidebar QToolButton[utility="true"] {
    color: #64748B;
    padding: 6px;
}
QDockWidget#xqStageSidebar QToolButton[utility="true"]:hover {
    color: #E2E8F0;
    background: rgba(255, 255, 255, 0.06);
}
```

**Key visual distinction**: SV has no sidebar; XQ's dark sidebar creates a strong two-tone layout (dark nav + light content). This is a fundamentally different UI pattern from SV's all-light horizontal approach.

### Todo 4: Relocate Slice Controls

**File**: `xq_WorkbenchWindowAdvisor.cxx`

**Changes**:
- Remove `sliceControlToolBar` as a separate toolbar
- Add Axial/Sagittal/Coronal toggles into the context ribbon under a "Display" section
- Move the Perspective dropdown into the File menu or a sidebar gear icon
- This eliminates an entire toolbar row, making the UI more compact

**Ribbon layout per stage**:
```
Stage: Import
┌──────────────────────────────────────────────────────────────────────────┐
│ ┌─ Data ─────────┐  ┌─ Display ──────────────────┐  ┌─ View ────────┐ │
│ │ Import  │ Open  │  │ [Axial] [Sag] [Cor] [3D]  │  │ [Layout ▾]   │ │
│ │ DICOM   │ File  │  │                            │  │              │ │
│ └─────────────────┘  └────────────────────────────┘  └──────────────┘ │
└──────────────────────────────────────────────────────────────────────────┘

Stage: Trace
┌──────────────────────────────────────────────────────────────────────────┐
│ ┌─ Path ──────────┐  ┌─ Display ──────────────────┐  ┌─ View ────────┐ │
│ │ Vessel Planning │  │ [Axial] [Sag] [Cor] [3D]  │  │ [Layout ▾]   │ │
│ └─────────────────┘  └────────────────────────────┘  └──────────────┘ │
└──────────────────────────────────────────────────────────────────────────┘
```

### Todo 5: Update Default Perspective Layout

**File**: `xq_DefaultPerspective.cxx`

**Changes**:
- Left column (DataManager + ProjectManager) remains, but **narrower** (18% vs 22%) since the sidebar takes ~80px
- Right tool stack placement changes — opens as **bottom panel** (BOTTOM, 0.65f) instead of RIGHT, to differentiate from SV which uses right-side panels
- Add a thin property inspector placeholder on the right (14% width)

**New layout**:
```
┌────┬──────────────┬──────────────────────────────────┬─────────────┐
│Side│ DataManager   │                                  │  Properties │
│bar │ (18%)        │       Editor / Render Views       │  (14%)      │
│    ├──────────────┤         (68%)                     │             │
│    │ ProjectMgr   │                                    │             │
│ 80 ├──────────────┤──────────────────────────────────┼─────────────┤
│ px │              │   Tool View (opened on demand)    │             │
│    │              │   + Log Console (15% height)      │             │
└────┴──────────────┴──────────────────────────────────┴─────────────┘
```

### Todo 6: Create SVG Stage Icons

**Directory**: `xq4gui/Plugins/org.xq.core.application/resources/`

Create 5 simple SVG icons for the sidebar stages:
- `stage-import.svg` — arrow-down-to-bracket (data import)
- `stage-trace.svg` — bezier-curve / pen-path (vessel tracing)
- `stage-contour.svg` — circle-dashed / crosshair-circle (contouring)
- `stage-build.svg` — cube / box-3d (model building)
- `stage-solve.svg` — chart-line-up / wave (simulation/solving)

All icons: 24×24px, stroke-only, 2px stroke, #94A3B8 default color (handled via QSS currentColor).

### Todo 7: Remove Unused workspaceToolBar & sliceControlToolBar

**File**: `xq_WorkbenchWindowAdvisor.cxx`

Remove the creation of `workspaceToolBar` and `sliceControlToolBar`. Their actions are redistributed:
- New/Open/Save → File menu (already exists) + sidebar bottom icons
- Undo/Redo → sidebar bottom icons (and Ctrl+Z/Ctrl+Y still work)
- Screenshot → sidebar bottom or View menu
- Axial/Sagittal/Coronal → context ribbon "Display" section
- Perspective dropdown → View menu submenu

---

## 5. Visual Comparison: SV vs XQ After Redesign

| Aspect | SimVascular | XQ (Redesigned) |
|--------|-------------|-----------------|
| **Toolbar orientation** | All horizontal | Vertical sidebar + single horizontal ribbon |
| **Toolbar count** | 3-4 horizontal bars | 1 sidebar + 1 ribbon |
| **Navigation pattern** | Perspective tabs + flat button lists | Pipeline sidebar with progressive stages |
| **Background scheme** | White #e6f2ff everywhere | Two-tone: Dark sidebar #1E293B + Light content #F8FAFC |
| **Accent color** | Cyan #00ccff | Royal Blue #2563EB |
| **Active indicator** | Blue highlight on button | Left-edge bar on sidebar button |
| **Slice controls** | In top toolbar, mixed with other actions | In context ribbon under "Display" group |
| **Workspace actions** | First items in top toolbar | Bottom of sidebar + File menu |
| **Tool views** | Right-side stacked tabs | Bottom panel |
| **Overall feel** | Engineering/academic (MITK default) | Modern engineering IDE (VS Code-like sidebar) |

---

## 6. QSS Theme Adjustments

### Sidebar + Ribbon color harmony:
- Sidebar background: **#1E293B** (Slate 800) — dark, distinct from SV's all-white
- Sidebar text: **#94A3B8** (Slate 400) inactive, **#FFFFFF** active
- Sidebar active indicator: **#2563EB** (Royal Blue) left border
- Ribbon background: **#FFFFFF** with subtle bottom border **#E2E8F0**
- Ribbon group labels: **#64748B** (Slate 500), 10px font
- Ribbon group separators: **1px #CBD5E1** vertical line

### Remove from current QSS:
- `QToolBar#xqStageBar` styles (replaced by sidebar)
- `QToolBar#workspaceToolBar` styles (removed)
- `QToolBar#sliceControlToolBar` styles (removed)

### Add to current QSS:
- `QDockWidget#xqStageSidebar` styles (see Todo 3)
- `QToolBar#xqStageToolsBar` enhanced with ribbon group styling
- Ribbon group visual separators

---

## 7. Implementation Priority

| # | Todo | Effort | Impact |
|---|------|--------|--------|
| 1 | Convert Stage Bar → vertical sidebar | Medium | HIGH — biggest visual change |
| 2 | Merge toolbars into context ribbon | Medium | HIGH — reduces toolbar count |
| 3 | QSS sidebar styling | Low | HIGH — establishes two-tone look |
| 4 | Relocate slice controls to ribbon | Low | MEDIUM — removes a toolbar |
| 5 | Update default perspective | Low | MEDIUM — improves panel layout |
| 6 | Create SVG stage icons | Low | MEDIUM — replaces emoji numbers |
| 7 | Remove unused toolbars | Low | LOW — cleanup |

---

## 8. Constraints & Risk Mitigation

- ✅ Only modify files under `~/XQ/`
- ✅ `rm -rf` must specify full target path
- ✅ Build must pass after each phase
- ⚠️ QDockWidget on left may interfere with DataManager dock — **test dock nesting**
- ⚠️ Sidebar needs to coexist with BlueBerry's perspective system — **test perspective switches**
- ⚠️ Icon SVGs need to be registered in the Qt resource system (`resources.qrc`)

---

## 9. Alternative: Minimal-Disruption Variant

If the full sidebar conversion is too risky for the build system:

1. Keep horizontal Stage Bar but restyle it dramatically:
   - Move it **below the render area** (bottom-anchored) instead of top
   - Use pill-shaped buttons instead of tab style
   - Add progress connector lines between stages

2. Consolidate workspaceToolBar + sliceControlToolBar into a single compact bar

3. Still achieves visual differentiation from SV through:
   - Bottom-mounted pipeline bar (unique placement)
   - Pill-button style (unique shape)
   - Fewer toolbars overall

---

*Created: UI Toolbar Redesign Plan for XQ*
*Theme: Arctic Light (#F8FAFC) + Royal Blue (#2563EB) + Dark Sidebar (#1E293B)*
*Target: Maximize visual distance from SimVascular while maintaining engineering workflow clarity*
