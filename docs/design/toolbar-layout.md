# XQ Toolbar Layout Specification
## Stage-Driven Pipeline Navigation (Inspired by CRIMSON)

## Design Rationale

CRIMSON uses a clear phased workflow: Presolver → Config → Control → Solve → Postsolver. 
SimVascular uses traditional dropdown menus for workflow navigation.
XQ adopts a **visual stage indicator** as the primary navigation paradigm — a horizontal pipeline bar showing the user's position in the analysis workflow.

## Layout Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│ [XQ]  File  Edit  View  Window  Help              [🔍] [⚙️] [?]      │  ← Menu Bar (dark)
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ① Import  ─── ② Trace  ─── ③ Contour  ─── ④ Build  ─── ⑤ Solve    │  ← Stage Bar
│   [active]                                                              │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│  📂 Open  📁 DICOM  📥 Import  │  🔄 Undo  🔁 Redo  │  📸 Screenshot │  ← Stage Tools
├──────────┬──────────────────────────────────────────┬───────────────────┤
│          │                                          │                   │
│ Explorer │         Main Viewport                    │  Properties       │
│ (Data +  │     (4-view / 3D / 2D)                  │  Panel            │
│ Workspace│                                          │                   │
│ merged)  │                                          │                   │
│          │                                          │                   │
│          │                                          │                   │
│          │                                          │                   │
├──────────┴──────────────────────────────────────────┴───────────────────┤
│ Status Bar: [Stage: Import] [Memory: XX MB] [Ready]                     │
└─────────────────────────────────────────────────────────────────────────┘
```

## Stage Bar Details

### Visual Design
- Horizontal bar with 5 stage buttons connected by lines/arrows
- Active stage: Amber gold (#E8A838) text + bottom indicator line
- Completed stages: Subtle green checkmark, lighter text
- Future stages: Muted text (#555770)
- Connecting lines between stages: #35375A (border color)

### Stage → Tool Mapping

#### ① Import (Data Acquisition)
```
Tools: [Open Project] [Open DICOM] [Import Image] [Browse Files]
View:  Standard 4-view layout
Panel: File browser + recent projects
```

#### ② Trace (Vessel Centerline Planning)
```
Tools: [New Path] [Edit Path] [Smooth Path] [Auto-Trace] [Delete Point]
View:  3-view (Axial + Sagittal + 3D) for centerline tracing
Panel: Path properties (interpolation, smoothing params)
```

#### ③ Contour (Lumen Segmentation)
```
Tools: [New Profile Group] [Lumen Contour] [3D Segment] [Loft Preview] [Edit Contour]
View:  2D cross-section + 3D lofted surface preview
Panel: Profile type selector, segmentation parameters
```

#### ④ Build (Modeling & Meshing)
```
Tools: [New Model] [Cap Faces] [Blend] [Extract CL] | [New Mesh] [Local Size] [Refine]
View:  3D model view with face selection
Panel: Model/mesh properties, quality metrics
```

#### ⑤ Solve (Hemodynamics Simulation)
```
Tools: [New Job] [Set BC] [Configure] [Run Solver] [Stop] [View Results]
View:  3D results visualization
Panel: Solver parameters, BC editor, job status
```

## Menu Structure

### Simplified Menu Bar (vs SimVascular)

| Menu | Items | Difference from SV |
|------|-------|-------------------|
| **File** | New Project, Open, Save, Save As, Import DICOM, Recent, Exit | Similar but no Python shell |
| **Edit** | Undo, Redo, Preferences | Simplified |
| **View** | Screenshot, Toggle Volume Rendering, Toggle Crosshair, Reset View, Measurements | Consolidated |
| **Window** | Perspectives (Default/Viewer/Analysis), Show/Hide panels | Renamed perspectives |
| **Help** | Welcome, Documentation, About XQ | Simplified |

**Removed**: Pipeline menu (replaced by Stage Bar)
**Removed**: Separate Tools menu (integrated into Stage Bar)

## Key Differences from SimVascular

| Aspect | SimVascular | XQ (New) |
|--------|-------------|----------|
| Workflow nav | Dropdown menus | Visual stage indicator bar |
| Tool discovery | Menu → submenu | Context-aware stage toolbar |
| Data manager | Separate left dock | Merged Explorer (data + workspace) |
| Properties | Various locations | Dedicated right panel |
| Stage awareness | None | Visual progress indication |
| Toolbar count | Multiple static bars | Dynamic per-stage bars |
| Status bar | Basic | Stage-aware with context |
| Perspectives | Standard/Viewer/Visualization | Default/Viewer/Analysis |

## Implementation Notes

### WorkbenchWindowAdvisor Changes
1. Remove `Pipeline` menu entirely
2. Add Stage Bar widget as a custom QToolBar with QToolButton items
3. Connect stage selection to:
   - Sub-toolbar visibility switching
   - Perspective/view layout hints
   - Properties panel context
4. Stage Bar should be non-closable and always visible

### CSS/QSS for Stage Bar
```css
/* Stage bar container */
#xqStageBar {
    background-color: #232440;
    border-bottom: 1px solid #35375A;
    padding: 8px 16px;
}

/* Stage button - normal */
#xqStageBar QToolButton {
    color: #555770;
    background: transparent;
    border: none;
    padding: 8px 16px;
    font-size: 13px;
}

/* Stage button - active */
#xqStageBar QToolButton:checked {
    color: #E8A838;
    border-bottom: 3px solid #E8A838;
    font-weight: bold;
}

/* Stage button - completed */
#xqStageBar QToolButton[completed="true"] {
    color: #8B8DA0;
}

/* Stage connector line */
#xqStageConnector {
    background-color: #35375A;
    min-height: 2px;
    max-height: 2px;
}
```
