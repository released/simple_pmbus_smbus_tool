# MFC DPI Scaling Guide

This note documents the DPI/layout mitigation used by `PmbusSmbusHidTool` so the same approach can be copied into other simple-series MFC tools.

## User-Facing Scale Policy

Default / recommended Windows display scale:

- `System > Display > Scale & layout`
- `系統 > 顯示器 > 縮放與配置`
- `125%`

The GUI should remain usable at:

- `100%`
- `125%`
- `150%`

The goal is not pixel-identical layout at every scale. The goal is that controls remain readable, reachable, and non-overlapping.

## Problem Seen Before The Fix

The original MFC pages used fixed pixel coordinates and fixed row heights. At 150%, Windows increased text and control rendering size, but the layout rectangles did not grow consistently. On dense tabs such as PMBus and SMBus, lower controls such as `Select All`, script command lists, and response panes were clipped or overlapped.

When a scrollbar was first added by moving child controls with negative coordinates, old child-control positions were not erased reliably. This caused visible artifacts and controls appearing stacked together after scrolling.

## Current Mitigation

Shared DPI helpers are centralized in `src\ui\layout_utils.h`:

- `GetDpiForHwnd`
- `GetDpiForWnd`
- `DpiScaler`
- `MetricsForWindow`
- `CreatePointFontForWindow`
- `ApplyFontToChildWindows`

Main frame behavior:

- Track current DPI.
- Handle `WM_DPICHANGED`.
- Recreate Segoe UI fonts on DPI change.
- Scale top-level margins, rows, tab item size, and minimum window size.
- Ask each tab to refresh DPI layout.

Tab behavior:

- Use `MetricsForWindow(*this)` instead of fixed `24`, `26`, `18`, `6`, and `8` pixel constants.
- Measure text before deciding button and label widths.
- Keep list views, path fields, and response panes flexible.
- Scale initial list column widths.

Dense-tab scrolling:

- PMBus and SMBus tabs use `WS_VSCROLL`.
- The tab calculates a virtual content height.
- Scrollbar range is updated with `SetScrollInfo`.
- Mouse wheel and scrollbar events update `scroll_offset_`.
- Scroll movement uses `ScrollWindowEx(..., SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE)` so child windows scroll with the page and old regions are erased.
- Full layout refresh uses `SetRedraw(FALSE)`, moves controls once, then calls `RedrawWindow` with erase and child redraw flags.

## Porting Checklist For Other Simple Tools

1. Add or reuse the shared DPI helpers from `src\ui\layout_utils.h`.
2. Replace direct `CreatePointFont(...)` calls with `CreatePointFontForWindow(...)`.
3. Replace fixed layout constants with `DpiScaler::Scale(...)` or `MetricsForWindow(...)`.
4. Use measured widths for buttons, checkboxes, and labels.
5. Scale list view default column widths.
6. Add `WM_DPICHANGED` handling in the main frame.
7. Add a DPI-aware minimum window size in `WM_GETMINMAXINFO`.
8. For dense tab pages, add vertical scrolling instead of compressing controls until panes become unusable.
9. When scrolling child-window controls, use `ScrollWindowEx` with `SW_SCROLLCHILDREN | SW_ERASE` rather than re-laying out every child on each wheel event.
10. Verify at 100%, 125%, and 150%.

## Verification Matrix

For each simple-series tool:

| Scale | Required checks |
| --- | --- |
| 100% | Normal and maximized windows, all tabs visible, no clipped buttons. |
| 125% | Default validation scale, all primary workflows visible. |
| 150% | Dense tabs show scrollbars when needed; lower panes remain reachable; no overlap after mouse-wheel scrolling. |

Also check:

- Tab labels remain centered and readable.
- Button text is not truncated.
- Checkbox labels are not hidden behind adjacent fields.
- Path fields and response panes resize with the window.
- Scrollbar movement does not leave ghost controls or stacked old drawings.

## Non-Goals

- No HID protocol change.
- No MCU firmware change.
- No PMBus/SMBus/I2C transaction behavior change.
- No visual redesign beyond scale-safe layout behavior.
