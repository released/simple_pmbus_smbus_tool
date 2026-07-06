#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>

namespace mfc_tool::ui {

inline int MeasureTextWidth(CWnd& wnd, const std::wstring& text, int padding = 8) {
    if (!::IsWindow(wnd.GetSafeHwnd())) {
        return static_cast<int>(text.size()) * 8 + padding;
    }
    CClientDC dc(&wnd);
    CFont* font = wnd.GetFont();
    CFont* old_font = nullptr;
    if (font != nullptr) {
        old_font = dc.SelectObject(font);
    }
    const CSize sz = dc.GetTextExtent(text.c_str(), static_cast<int>(text.size()));
    if (font != nullptr && old_font != nullptr) {
        dc.SelectObject(old_font);
    }
    return sz.cx + padding;
}

inline int MeasureControlTextWidth(CWnd& wnd, int padding = 8) {
    if (!::IsWindow(wnd.GetSafeHwnd())) {
        return padding;
    }
    CString text;
    wnd.GetWindowTextW(text);
    return MeasureTextWidth(wnd, std::wstring(text.GetString()), padding);
}

inline int MeasureButtonMinWidth(CWnd& wnd, int padding = 20) {
    int width = MeasureControlTextWidth(wnd, padding);
    const LONG style = static_cast<LONG>(::GetWindowLongW(wnd.GetSafeHwnd(), GWL_STYLE));
    if ((style & BS_AUTOCHECKBOX) == BS_AUTOCHECKBOX ||
        (style & BS_CHECKBOX) == BS_CHECKBOX ||
        (style & BS_AUTORADIOBUTTON) == BS_AUTORADIOBUTTON ||
        (style & BS_RADIOBUTTON) == BS_RADIOBUTTON) {
        width += 18;
    }
    return width;
}

inline int LabelHeight() {
    return 18;
}

inline void SafeEnableWindow(CWnd& control, BOOL enable) {
    if (::IsWindow(control.GetSafeHwnd())) {
        control.EnableWindow(enable);
    }
}

inline void SetCounterControlsEnabled(CButton& counter_check,
                                      CEdit& counter_idx_edit,
                                      CEdit& counter_step_edit,
                                      CButton& counter_reset_btn,
                                      BOOL base_enable) {
    const BOOL detail_enable = (base_enable && counter_check.GetCheck() == BST_CHECKED) ? TRUE : FALSE;
    SafeEnableWindow(counter_check, base_enable);
    SafeEnableWindow(counter_idx_edit, detail_enable);
    SafeEnableWindow(counter_step_edit, detail_enable);
    SafeEnableWindow(counter_reset_btn, detail_enable);
}

class ScopedBusyState {
public:
    ScopedBusyState(bool& busy, std::function<void()> on_change)
        : busy_(&busy), on_change_(std::move(on_change)) {
        *busy_ = true;
        NotifyNoexcept();
    }

    ScopedBusyState(const ScopedBusyState&) = delete;
    ScopedBusyState& operator=(const ScopedBusyState&) = delete;

    ~ScopedBusyState() noexcept {
        Reset();
    }

    void Reset() noexcept {
        if (busy_ == nullptr) {
            return;
        }
        *busy_ = false;
        busy_ = nullptr;
        NotifyNoexcept();
    }

private:
    void NotifyNoexcept() noexcept {
        try {
            if (on_change_) {
                on_change_();
            }
        } catch (...) {
        }
    }

private:
    bool* busy_ = nullptr;
    std::function<void()> on_change_;
};

inline void SafeMoveWindow(CWnd& control, int x, int y, int width, int height, BOOL repaint = TRUE) {
    if (::IsWindow(control.GetSafeHwnd())) {
        control.MoveWindow(x, y, width, height, repaint);
    }
}

inline void SafeMoveWindow(CWnd& control, const RECT& rect, BOOL repaint = TRUE) {
    if (::IsWindow(control.GetSafeHwnd())) {
        control.MoveWindow(&rect, repaint);
    }
}

inline void SafeResetProgress(CProgressCtrl& progress, int total, BOOL show = TRUE) {
    if (!::IsWindow(progress.GetSafeHwnd())) {
        return;
    }
    const int safe_total = total > 0 ? total : 1;
    if (show) {
        progress.ShowWindow(SW_SHOW);
    }
    progress.SetRange32(0, safe_total);
    progress.SetPos(0);
}

inline void SafeSetProgressRange(CProgressCtrl& progress, int total) {
    if (::IsWindow(progress.GetSafeHwnd())) {
        progress.SetRange32(0, total > 0 ? total : 1);
    }
}

inline void SafeSetProgressPos(CProgressCtrl& progress, int pos) {
    if (::IsWindow(progress.GetSafeHwnd())) {
        progress.SetPos(pos > 0 ? pos : 0);
    }
}

inline void AutoSizeListColumns(CListCtrl& list, int column_count, std::initializer_list<int> minimum_widths = {}) {
    if (!::IsWindow(list.GetSafeHwnd())) {
        return;
    }
    for (int i = 0; i < column_count; ++i) {
        const int min_width = i < static_cast<int>(minimum_widths.size()) ? *(minimum_widths.begin() + i) : 0;
        list.SetColumnWidth(i, LVSCW_AUTOSIZE);
        const int content_width = list.GetColumnWidth(i);
        list.SetColumnWidth(i, LVSCW_AUTOSIZE_USEHEADER);
        const int header_width = list.GetColumnWidth(i);
        list.SetColumnWidth(i, (std::max)((std::max)(content_width, header_width), min_width));
    }
}

inline void UpdateWindowsAndPumpPaint(std::initializer_list<CWnd*> controls) {
    MSG msg;
    for (CWnd* control : controls) {
        if (control != nullptr && ::IsWindow(control->GetSafeHwnd())) {
            control->UpdateWindow();
        }
    }
    while (::PeekMessageW(&msg, nullptr, WM_PAINT, WM_PAINT, PM_REMOVE) != FALSE) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

inline void PumpControlMessages(CWnd& control) {
    if (!::IsWindow(control.GetSafeHwnd())) {
        return;
    }
    MSG msg;
    while (::PeekMessageW(&msg, control.GetSafeHwnd(), 0, 0, PM_REMOVE) != FALSE) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

inline int PlaceLabel(CWnd& label, int x, int y, int padding = 8, int height = 18) {
    const int width = MeasureControlTextWidth(label, padding);
    SafeMoveWindow(label, x, y, width, height);
    return width;
}

inline int PlaceControl(CWnd& control, int x, int y, int width, int height) {
    SafeMoveWindow(control, x, y, width, height);
    return x + width;
}

inline int PlaceLabelAndControl(CWnd& label,
                                CWnd& control,
                                int x,
                                int label_y,
                                int control_y,
                                int control_width,
                                int control_height,
                                int gap = 6,
                                int label_padding = 8,
                                int label_height = 18) {
    const int label_width = PlaceLabel(label, x, label_y, label_padding, label_height);
    const int control_x = x + label_width + gap;
    SafeMoveWindow(control, control_x, control_y, control_width, control_height);
    return control_x + control_width;
}

} // namespace mfc_tool::ui
