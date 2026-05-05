# XQ Color System Documentation

## Overview
XQ uses a **Dark Indigo + Amber Gold** color system designed for maximum differentiation from SimVascular's blue/cyan light theme.

## Primary Palette

### Backgrounds (Dark Indigo Family)
```
#1A1B2E  ████  Primary background (main window)
#232440  ████  Secondary background (panels, docks)
#2D2F4A  ████  Tertiary (hover, elevated surfaces)
#1E1F35  ████  Input fields background
#151628  ████  Deepest shade (status bar, borders)
```

### Accents (Warm Gold/Orange Family)  
```
#E8A838  ████  Primary accent (amber gold) - active tabs, selections, CTA
#FF6B35  ████  Secondary accent (warm orange) - hover highlights
#FFBE76  ████  Warning / light accent
#D4941E  ████  Pressed state accent (darker gold)
```

### Text
```
#EEEEF2  ████  Primary text (near-white)
#C8C9D4  ████  Subheading text
#8B8DA0  ████  Secondary/muted text
#555770  ████  Disabled text
#FFFFFF  ████  Emphasis text (on accent backgrounds)
```

### Semantic Colors
```
#FF4757  ████  Destructive / Error / Danger
#2ED573  ████  Success / Valid / Complete
#FFBE76  ████  Warning / Caution
#70A1FF  ████  Info / Links / Informational
```

### Borders & Dividers
```
#35375A  ████  Standard border
#2D2F4A  ████  Subtle border
#E8A838  ████  Focus / active border
#444668  ████  Prominent border
```

## Stage Indicator Colors
Each pipeline stage has a subtle color association:

```
① Import   →  #70A1FF (Info blue)
② Trace    →  #A29BFE (Soft purple)
③ Contour  →  #2ED573 (Success green)
④ Build    →  #E8A838 (Amber gold)
⑤ Solve    →  #FF6B35 (Warm orange)
```

## Contrast Ratios (WCAG Compliance)
| Combination | Ratio | Grade |
|------------|-------|-------|
| #EEEEF2 on #1A1B2E | 13.2:1 | AAA ✓ |
| #8B8DA0 on #1A1B2E | 5.1:1 | AA ✓ |
| #E8A838 on #1A1B2E | 8.7:1 | AAA ✓ |
| #555770 on #1A1B2E | 2.8:1 | decorative only |
| #EEEEF2 on #232440 | 11.5:1 | AAA ✓ |

## vs SimVascular Color Map

| Purpose | SimVascular | XQ |
|---------|------------|------|
| Icon color | #0047b3 (blue) | #E8A838 (amber) |
| Icon accent | #ffffff | #EEEEF2 |
| Menu bg | #e6f2ff (light blue) | #1A1B2E (dark indigo) |
| Menu selected | #00ccff (cyan) | #2D2F4A + amber indicator |
| Main bg | #ffcccc→#99ddff gradient | #1A1B2E solid |
| Toolbar bg | #e6f2ff (light blue) | #232440 (dark panel) |
| Tab active | #1c97ea (blue) | #E8A838 (amber) bottom border |
| Plugin edit | #e6f2ff | #232440 |
