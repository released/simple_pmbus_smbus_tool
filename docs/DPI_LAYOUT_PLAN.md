# DPI Layout Plan

## Goal

Make the MFC GUI layout stable across common Windows display scale settings without maintaining separate hand-tuned coordinates for each scale.

Primary target scale settings:

- 100% / 96 DPI
- 125% / 120 DPI
- 150% / 144 DPI

Secondary target:

- Per-monitor DPI changes when Windows sends `WM_DPICHANGED`

## Current Risk

The current GUI uses hand-written pixel layout. Many dimensions are fixed constants such as row height, label height, tab height, margins, and button widths. Text uses point fonts, so Windows increases text pixel size at higher DPI, but many control rectangles do not grow with it. This can cause clipping, overlap, or controls moving outside their intended areas at 150% display scale.

## Design Rules

1. Use 96 DPI as the design baseline.
2. Convert design pixels to actual pixels with current window DPI.
3. Measure button and label text with the active font before assigning widths.
4. Let long fields, list views, and response panes consume remaining flexible space.
5. Avoid rows that require more fixed width than the available client width.
6. Set a reasonable minimum top-level window size so controls are not forced into impossible geometry.
7. Re-layout when the top-level window receives `WM_DPICHANGED`.

## Implementation Plan

### Shared Helper

Add DPI helpers in `src\ui\layout_utils.h`:

- get DPI for a window with a safe fallback
- scale integer dimensions from the 96 DPI baseline
- common metrics for margins, gaps, row height, label height, tab height, and group top padding
- helper to recreate Segoe UI fonts by point size and DPI

### Main Frame

Update `src\main_frame.*`:

- track current DPI
- recreate the main UI font when DPI changes
- scale fixed layout constants
- scale tab item size
- handle `WM_DPICHANGED`
- handle `WM_GETMINMAXINFO` to define a DPI-aware minimum window size

### Tabs

Update tab layout code:

- `src\ui\pmbus_tab.cpp`
- `src\ui\smbus_tab.cpp`
- `src\ui\i2c_tab.cpp`
- `src\ui\script_tab.cpp`

Each tab should:

- get DPI-aware metrics from its own window
- recreate or use DPI-scaled font metrics where needed
- scale row height, labels, group padding, margins, gaps, and fixed widths
- keep dynamic widths based on measured text and available space

### Verification

Required build verification:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_mfc.ps1 -Configuration Release -Platform x64
```

Recommended manual UI verification:

1. Run at 100% display scale.
2. Run at 125% display scale.
3. Run at 150% display scale.
4. Open PMBus, SMBus, I2C, and Script tabs.
5. Check normal window and maximized window.
6. Confirm button text, checkbox text, tab labels, path fields, progress bars, and response panes do not overlap or clip.

## Non-Goals

- No HID protocol change.
- No MCU firmware change.
- No change to PMBus, SMBus, I2C, or script execution behavior.
- No visual redesign beyond DPI-safe layout behavior.
