#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../core/app_state.h"
#include "../core/board_i2c_catalog.h"
#include "../core/bridge_service.h"
#include "../core/pin_usage_registry.h"
#include "../core/pmbus_utils.h"
#include "../core/smbus_script.h"

class CPmbusTab : public CWnd {
public:
    enum class Profile {
        BasePmbus = 0,
        Crps,
        TiUcd90xxx
    };

    void SetProfile(Profile profile);
    BOOL Create(CWnd* parent, const RECT& rect, UINT id);

    void Bind(mfc_tool::core::BridgeService* service,
              std::function<void(const std::wstring&)> logger,
              mfc_tool::core::PinUsageRegistry* pin_usage,
              std::function<void()> persist_settings = {});
    void SetConnected(bool connected);
    void OnDisconnected();

    void LoadState(const mfc_tool::core::AppState& state);
    void SaveState(mfc_tool::core::AppState* state) const;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnMasterEnable();
    afx_msg void OnMasterDisable();
    afx_msg void OnExecute();
    afx_msg void OnScan();
    afx_msg void OnAra();
    afx_msg void OnSmbalertRead();
    afx_msg void OnSmbalertWrite();
    afx_msg void OnUiSettingChanged();
    afx_msg void OnProfileChanged();
    afx_msg void OnMasterPortChanged();
    afx_msg void OnSystemPolicyChanged();
    afx_msg void OnCommandPresetChanged();
    afx_msg void OnIllegalQuickTest();
    afx_msg void OnChecklistBasic();
    afx_msg void OnChecklistPec();
    afx_msg void OnChecklistError();
    afx_msg void OnChecklistTelemetry();
    afx_msg void OnChecklistMfr();
    afx_msg void OnChecklistFull();
    afx_msg void OnStop();
    afx_msg void OnScriptLoad();
    afx_msg void OnScriptRun();
    afx_msg void OnScriptSelectAll();
    afx_msg void OnScriptListChanged(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(const CRect& r);
    struct ExecResult {
        std::vector<std::uint8_t> raw;
        std::vector<std::uint8_t> data;
        bool pec_checked = false;
        bool pec_ok = true;
        std::uint8_t pec_rx = 0;
        std::uint8_t pec_calc = 0;
    };

    void UpdateEnableState();
    void RefreshPinUsage();
    bool IsCrpsProfile() const;
    bool IsTiProfile() const;
    bool IsLabValidationPolicy() const;
    const wchar_t* ProfileDisplayName() const;
    const wchar_t* ProfileErrorTitle() const;
    std::wstring MasterOwnerId() const;
    const wchar_t* ProfileIniName() const;
    void RefreshProfileUi();
    bool OtherSharedProfileActive() const;
    bool IsPresetVisibleForProfile(const mfc_tool::core::PmbusCommandPreset& preset) const;
    const mfc_tool::core::PmbusState& SelectState(const mfc_tool::core::AppState& state) const;
    mfc_tool::core::PmbusState& SelectState(mfc_tool::core::AppState* state) const;
    void SetDecodedText(const std::wstring& text);
    void SetRawRxText(const std::wstring& text);
    void SetScanSummaryText(const std::wstring& text);
    void SetIllegalTestResultText(const std::wstring& text);
    void UpdatePmbusSummaryLine(const std::wstring& text);
    void FlushUiUpdates();
    void RequestCancel();
    void ResetCancel();
    void ThrowIfCancelRequested();
    bool SleepWithCancel(int delay_ms);
    int ParseEditInt(const CEdit& edit) const;
    DWORD ChecklistCommandDelayMs() const;
    int ChecklistRepeatCount() const;
    void SleepAfterChecklistCommand();
    int CurrentSpeedHz() const;
    int CurrentMasterPort() const;
    const mfc_tool::core::board_i2c::PinPair& CurrentMasterPinPair() const;
    std::wstring GetEditText(const CEdit& edit) const;
    std::uint8_t ParseCommandCode() const;
    std::vector<std::uint8_t> ParseTxHex(const CEdit& edit) const;
    mfc_tool::core::PmbusTransactionType CurrentTransactionType() const;
    void PopulatePresetCombo();
    void ApplyPresetToCommandUi();

    ExecResult ExecReceiveByte(std::uint8_t addr, bool pec);
    ExecResult ExecSendByte(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes, bool pec, bool force_bad_pec);
    ExecResult ExecReadByCommand(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes, int read_len, bool pec, bool force_bad_pec);
    ExecResult ExecBlockReadCommand(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes, int max_read_len, bool pec, bool force_bad_pec);
    ExecResult ExecWriteByCommand(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes, const std::vector<std::uint8_t>& data, bool pec, bool force_bad_pec);
    ExecResult ExecProcessCall(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes, const std::vector<std::uint8_t>& data,
                               bool pec, bool force_bad_pec);
    ExecResult ExecBlockWriteReadProcessCall(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes, const std::vector<std::uint8_t>& data,
                                             int max_read_len, bool pec, bool force_bad_pec);
    std::wstring BuildDecodedText(std::uint8_t command, bool extended_mode, const ExecResult& result);
    void ExecuteAraHelper(bool from_recovery);
    std::vector<std::uint8_t> CurrentCommandBytes() const;
    bool ExtendedModeEnabled() const;
    bool ForceBadPecEnabled() const;
    void RunChecklistSuite(int mode);
    int EstimateChecklistProgressTotal(int mode) const;
    std::wstring CurrentExtendedTypeText() const;
    bool ValidateCurrentTransaction(std::uint8_t command, mfc_tool::core::PmbusTransactionType txn) const;
    void ReinitMasterBusForRetry();
    void PopulatePortCombos();
    void PopulateMasterPinCombo(const std::wstring& preferred_ini_name = L"");
    void SetScriptPathText();
    void SetScriptResponseText(const std::wstring& text);
    void AppendScriptResponse(const std::wstring& text);
    void UpdateScriptSummary();
    void LoadScriptFromPath(const std::wstring& path);
    void PopulateScriptList();
    ExecResult ExecuteScriptCommandRow(const mfc_tool::core::SmbusScriptRow& row, bool pec);
    bool ScriptMetadataPecEnabled() const;
    static std::wstring AnsiToWide(const char* text);

private:
    mfc_tool::core::BridgeService* service_ = nullptr;
    mfc_tool::core::PinUsageRegistry* pin_usage_ = nullptr;
    std::function<void(const std::wstring&)> log_;
    std::function<void()> persist_settings_;
    Profile profile_ = Profile::BasePmbus;
    bool connected_ = false;
    bool master_enabled_ = false;
    bool checklist_running_ = false;
    bool script_running_ = false;
    bool cancel_requested_ = false;
    bool script_list_updating_ = false;
    std::uint8_t cached_vout_mode_ = 0x17;
    std::int8_t cached_vout_mode_exponent_ = -9;
    mfc_tool::core::SmbusScriptDocument script_doc_;

    CFont ui_font_;

    CButton master_group_;
    CStatic profile_label_;
    CComboBox profile_combo_;
    CStatic system_policy_label_;
    CComboBox system_policy_combo_;
    CStatic master_port_label_;
    CComboBox master_port_combo_;
    CStatic master_pins_label_;
    CComboBox master_pins_combo_;
    CStatic speed_label_;
    CComboBox speed_combo_;
    CStatic master_addr_label_;
    CEdit master_addr_edit_;
    CStatic checklist_delay_label_;
    CEdit checklist_delay_edit_;
    CStatic checklist_repeat_label_;
    CEdit checklist_repeat_edit_;
    CButton pec_check_;
    CButton bad_pec_check_;
    CButton master_enable_btn_;
    CButton master_disable_btn_;
    CButton scan_btn_;
    CButton ara_btn_;
    CStatic command_preset_label_;
    CComboBox command_preset_combo_;
    CStatic command_code_label_;
    CEdit command_code_edit_;
    CButton ext_check_;
    CComboBox ext_type_combo_;
    CStatic transaction_label_;
    CComboBox transaction_combo_;
    CStatic tx_hex_label_;
    CEdit tx_hex_edit_;
    CStatic read_len_label_;
    CEdit read_len_edit_;
    CButton execute_btn_;
    CStatic raw_rx_label_;
    CEdit raw_rx_edit_;
    CStatic decoded_label_;
    CEdit decoded_edit_;
    CButton scan_summary_group_;
    CStatic scan_summary_label_;
    CEdit scan_summary_edit_;
    CStatic illegal_test_label_;
    CButton illegal_test_btn_;
    CStatic illegal_result_label_;
    CEdit illegal_result_edit_;
    CProgressCtrl checklist_progress_;
    CButton checklist_basic_btn_;
    CButton checklist_pec_btn_;
    CButton checklist_error_btn_;
    CButton checklist_telemetry_btn_;
    CButton checklist_mfr_btn_;
    CButton checklist_full_btn_;
    CButton stop_btn_;
    CStatic smbalert_label_;
    CEdit smbalert_edit_;
    CButton smbalert_read_btn_;
    CButton smbalert_write_btn_;
    CStatic script_label_;
    CEdit script_path_edit_;
    CButton script_load_btn_;
    CButton script_run_btn_;
    CButton script_select_all_check_;
    CListCtrl script_list_;
    CStatic script_response_label_;
    CEdit script_response_edit_;
};
