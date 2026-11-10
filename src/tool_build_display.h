#pragma once
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <initializer_list>
#include <string>
#pragma comment(lib, "comctl32.lib")

namespace native_tool_ui {
constexpr int BuildIdentityControlId = 32145;

// A native, noninteractive field. The parent retains its controls and command routes.
class BuildIdentityLabel {
public:
    BuildIdentityLabel() = default;
    BuildIdentityLabel(const BuildIdentityLabel&) = delete;
    BuildIdentityLabel& operator=(const BuildIdentityLabel&) = delete;
    ~BuildIdentityLabel() { Destroy(); }

    bool Create(HWND parent, const wchar_t* version, const wchar_t* displayTime,
                const wchar_t* timestamp) {
        if (!::IsWindow(parent) || !version || !displayTime || !timestamp || label_ ||
            ::GetDlgItem(parent, BuildIdentityControlId)) return false;
        text_ = std::wstring(L"v") + version + L" | BUILD " + displayTime;
        detail_ = std::wstring(L"Version: ") + version + L"\r\nBuilt: " + timestamp;
        label_ = ::CreateWindowExW(0, L"STATIC", text_.c_str(),
            WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE | SS_ENDELLIPSIS | SS_NOTIFY,
            0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(BuildIdentityControlId)),
            ::GetModuleHandleW(nullptr), nullptr);
        if (!label_) return false;
        if (!::SetPropW(label_, OwnerProperty(), this)) { ::DestroyWindow(label_); label_ = nullptr; return false; }
        SetFont(reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
        INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES};
        if (!::InitCommonControlsEx(&common)) { Destroy(); return false; }
        tooltip_ = ::CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
        if (!tooltip_) { Destroy(); return false; }
        if (!::SetPropW(tooltip_, OwnerProperty(), this)) { ::DestroyWindow(tooltip_); tooltip_ = nullptr; Destroy(); return false; }
        TOOLINFOW tool{}; tool.cbSize = TTTOOLINFOW_V2_SIZE; tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        tool.hwnd = parent; tool.uId = reinterpret_cast<UINT_PTR>(label_); tool.lpszText = detail_.data();
        if (!::SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool))) { Destroy(); return false; }
        ::SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, 640);
        return true;
    }

    void SetFont(HFONT font) {
        if (::IsWindow(label_) && font) ::SendMessageW(label_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    int Height() const { return ::IsWindow(label_) ? TextSize().cy + Padding() : 0; }
    int PreferredWidth() const { return ::IsWindow(label_) ? TextSize().cx + Padding() : 0; }
    void Place(int right, int top, int availableWidth) {
        if (!::IsWindow(label_)) return;
        const int width = (std::max)(0, (std::min)(PreferredWidth(), availableWidth));
        ::SetWindowPos(label_, nullptr, (std::max)(0, right - width), top, width, Height(),
            SWP_NOACTIVATE | SWP_NOZORDER);
    }
    HWND Handle() const { return label_; }

private:
    static const wchar_t* OwnerProperty() { return L"Released.BuildIdentity.Owner"; }
    int Padding() const {
        HDC dc = ::GetDC(label_); const int dpi = dc ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
        if (dc) ::ReleaseDC(label_, dc);
        return (std::max)(4, ::MulDiv(8, dpi, 96));
    }
    SIZE TextSize() const {
        SIZE size{}; HDC dc = ::GetDC(label_); if (!dc) return size;
        HFONT font = reinterpret_cast<HFONT>(::SendMessageW(label_, WM_GETFONT, 0, 0));
        HGDIOBJ old = font ? ::SelectObject(dc, font) : nullptr;
        ::GetTextExtentPoint32W(dc, text_.c_str(), static_cast<int>(text_.size()), &size);
        if (old) ::SelectObject(dc, old);
        ::ReleaseDC(label_, dc);
        return size;
    }
    void Destroy() {
        for (HWND window : {tooltip_, label_}) {
            if (::IsWindow(window) && ::GetPropW(window, OwnerProperty()) == this) {
                ::RemovePropW(window, OwnerProperty()); ::DestroyWindow(window);
            }
        }
        tooltip_ = label_ = nullptr;
    }
    HWND label_ = nullptr, tooltip_ = nullptr;
    std::wstring text_, detail_;
};
}  // namespace native_tool_ui
