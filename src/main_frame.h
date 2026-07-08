#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <cstdint>
#include <string>
#include <vector>

#include "config/ini_manager.h"
#include "core/app_state.h"
#include "core/bridge_service.h"
#include "core/pin_usage_registry.h"
#include "log/logger.h"
#include "ui/i2c_tab.h"
#include "ui/pmbus_tab.h"
#include "ui/script_tab.h"
#include "ui/smbus_tab.h"

class CMainFrame : public CFrameWnd {
public:
    CMainFrame();

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedRefresh();
    afx_msg void OnBnClickedConnect();
    afx_msg void OnBnClickedDisconnect();
    afx_msg void OnBnClickedGetInfo();
    afx_msg void OnBnClickedPing();
    afx_msg void OnBnClickedResetMcu();
    afx_msg void OnBnClickedSaveIni();
    afx_msg void OnBnClickedLoadIni();
    afx_msg void OnBnClickedResetIni();
    afx_msg void OnBnClickedSaveLog();
    afx_msg void OnBnClickedSaveLogCheck();
    afx_msg void OnBnClickedClearLog();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnClose();

    DECLARE_MESSAGE_MAP()

private:
    void RecreateUiFont();
    void ApplyTopControlFont();
    void LayoutControls(int cx, int cy);
    void InitializeTabs();
    void ShowActiveTabPage();
    void AppendLog(const std::wstring& text);
    void TrimVisibleLogIfNeeded();
    void RefreshDevices();
    void SetConnectionUi(bool connected);
    void UpdateStatusChipColor();
    void UpdateIniPathUi();
    void SetScriptTabDirty(bool dirty);
    std::wstring SelectedDevicePath() const;
    static std::wstring DeviceLabel(const mfc_tool::hid::DeviceInfo& d, int index);
    void LogBridgeInfoPayload(const std::vector<std::uint8_t>& rx, bool from_connect);
    static std::wstring ResetReasonText(std::uint8_t reason);
    void UpdateFirmwareInfoUi();
    static std::wstring ExtractFirmwareVersion(const std::wstring& bridge_name);
    static bool FirmwareVersionMatch(const std::wstring& expected, const std::wstring& actual);
    void LoadIni();
    void SaveIni();
    void SaveIniTo(const std::wstring& path);
    bool TryLoadIniFrom(const std::wstring& path);
    void ResetIniToDefault();
    void SaveLogToFile(const std::wstring& path);
    std::wstring GetControlText(const CWnd& w) const;
    void SetControlText(CWnd& w, const std::wstring& text);
    static void ShowErrorBox(const std::wstring& title, const std::wstring& message);
    static std::wstring AnsiToWide(const char* text);

private:
    CFont ui_font_;
    UINT current_dpi_ = 96u;
    CBrush status_chip_brush_;
    COLORREF status_chip_color_ = RGB(95, 104, 117);
    COLORREF status_chip_text_color_ = RGB(255, 255, 255);

    CStatic vid_label_;
    CEdit vid_edit_;
    CStatic pid_label_;
    CEdit pid_edit_;
    CStatic timeout_label_;
    CEdit timeout_edit_;
    CButton refresh_btn_;
    CButton connect_btn_;
    CButton disconnect_btn_;
    CStatic status_title_;
    CStatic status_chip_;
    CButton save_ini_btn_;
    CButton load_ini_btn_;
    CButton reset_ini_btn_;
    CStatic ini_path_title_;
    CStatic ini_path_value_;
    CStatic build_info_title_;
    CStatic build_info_value_;
    CStatic fw_info_title_;
    CStatic fw_info_value_;
    CStatic device_label_;
    CComboBox device_combo_;
    CButton get_info_btn_;
    CButton ping_btn_;
    CButton reset_mcu_btn_;

    CTabCtrl tab_ctrl_;
    CPmbusTab pmbus_tab_;
    CSmbusTab smbus_tab_;
    CI2cTab i2c_tab_;
    CScriptTab script_tab_;
    CButton save_log_check_;
    CButton save_log_btn_;
    CButton clear_log_btn_;
    CEdit log_edit_;

    std::vector<mfc_tool::hid::DeviceInfo> scanned_devices_;
    mfc_tool::config::IniManager ini_;
    mfc_tool::core::AppState state_;
    mfc_tool::core::BridgeService service_;
    mfc_tool::core::PinUsageRegistry pin_usage_;
    mfc_tool::log::Logger logger_;
    std::wstring expected_fw_version_ = M032_EXPECTED_FW_VERSION;
    std::wstring current_fw_version_ = L"-";
    bool fw_version_match_ = true;
};
