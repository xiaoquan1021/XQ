# XQ UI Design Specification
## Design System: Dark Scientific Dashboard

### Design Philosophy
XQ adopts a **Dark Indigo Scientific** theme — a professional, modern dark interface optimized for long research sessions. The warm amber-gold accent creates visual distinction from SimVascular's cool blue palette.

### Core Principles
1. **Dark-first**: Deep indigo backgrounds reduce eye strain during extended use
2. **Warm accents**: Amber gold highlights convey precision and premium quality  
3. **Stage-driven workflow**: CRIMSON-inspired phased pipeline navigation
4. **Data prominence**: Dark backgrounds make VTK 3D renderings and medical images pop
5. **Minimal decoration**: Clean lines, subtle borders, no excessive shadows

### Color Tokens

| Token | Hex | Usage |
|-------|-----|-------|
| `--xq-bg-primary` | `#1A1B2E` | Main window background |
| `--xq-bg-secondary` | `#232440` | Panels, dock widgets, cards |
| `--xq-bg-tertiary` | `#2D2F4A` | Hover states, elevated surfaces |
| `--xq-bg-input` | `#1E1F35` | Input field backgrounds |
| `--xq-accent-primary` | `#E8A838` | Primary actions, active tabs, selections |
| `--xq-accent-secondary` | `#FF6B35` | Secondary highlights, hover accents |
| `--xq-accent-subtle` | `#3D3F5C` | Subtle highlights, active menu items |
| `--xq-text-primary` | `#EEEEF2` | Primary text (white-ish) |
| `--xq-text-secondary` | `#8B8DA0` | Secondary/muted text |
| `--xq-text-disabled` | `#555770` | Disabled text |
| `--xq-border` | `#35375A` | Borders, dividers |
| `--xq-border-focus` | `#E8A838` | Focused input borders |
| `--xq-destructive` | `#FF4757` | Error, delete, danger |
| `--xq-success` | `#2ED573` | Success, complete, valid |
| `--xq-warning` | `#FFBE76` | Warning states |
| `--xq-info` | `#70A1FF` | Information, links |

### Typography
- **Primary text**: White (#EEEEF2) on dark backgrounds
- **Secondary text**: Muted gray-blue (#8B8DA0)
- **Font**: System default (Qt/MITK framework fonts)
- **Body size**: 13px (standard for desktop scientific apps)
- **Heading size**: 16-18px bold

### Spacing System
- Base unit: 4px
- Component padding: 8px (compact), 12px (standard), 16px (spacious)
- Section gaps: 16px, 24px
- Toolbar item spacing: 4px between icons, 12px between groups

### Interaction States
| State | Visual Treatment |
|-------|-----------------|
| Normal | Default colors |
| Hover | Background lightens to `--xq-bg-tertiary`, text stays white |
| Pressed | Amber accent background `--xq-accent-primary` with dark text |
| Selected/Active | Amber bottom border or left border indicator |
| Disabled | 40% opacity, `--xq-text-disabled` color |
| Focus | `--xq-border-focus` (amber) ring around element |

### Comparison with SimVascular

| Element | SimVascular | XQ (New) |
|---------|-------------|----------|
| Background | Light blue #e6f2ff | Deep indigo #1A1B2E |
| Accent | Cyan #00ccff | Amber gold #E8A838 |
| Text | Black #000000 | White #EEEEF2 |
| Menu bar | Light blue solid | Dark gradient |
| Selected state | Cyan highlight | Amber highlight |
| Stylesheet LOC | 137 lines | ~500+ lines (comprehensive) |
| Theme mood | Clinical/cool | Scientific/warm-premium |
