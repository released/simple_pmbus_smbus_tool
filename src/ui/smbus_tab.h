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
#include "../core/smbus_script.h"

class CSmbusTab : public CWnd {
public:
    enum class Profile {
        Generic = 0,
        UbmController
    };

    enum class Transaction {
        QuickWrite = 0,
        QuickRead,
        SendByte,
        ReceiveByte,
        WriteByte,
        ReadByte,
        WriteWord,
        ReadWord,
        BlockWrite,
        BlockRead,
        ProcessCall,
        BlockWriteReadProcessCall,
        UbmControllerRead,
        UbmControllerWrite,
        UbmBadChecksumWrite,
        BusRecover
    };

    struct ExecResult {
        std::vector<std::uint8_t> raw;
        std::vector<std::uint8_t> data;
        bool pec_checked = false;
        bool pec_ok = true;
        std::uint8_t pec_rx = 0;
        std::uint8_t pec_calc = 0;
        bool ubm_checksum_checked = false;
        bool ubm_checksum_ok = true;
        std::uint8_t ubm_checksum_rx = 0;
        std::uint8_t ubm_checksum_calc = 0;
        bool ack = true;
    };

    BOOL Create(CWnd* parent, const RECT& rect, UINT id);

    void Bind(mfc_tool::core::BridgeService* service,
              std::function<void(const std::wstring&)> logger,
              mfc_tool::core::PinUsageRegistry* pin_usage,
              std::function<void()> persist_settings = nullptr);
    void SetConnected(bool connected);
    void OnDisconnected();
    void RefreshDpiLayout();

    void LoadState(const mfc_tool::core::AppState& state);
    void SaveState(mfc_tool::core::AppState* state) const;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnMasterEnable();
    afx_msg void OnMasterDisable();
    afx_msg void OnMasterPortChanged();
    afx_msg void OnProfileChanged();
    afx_msg void OnPresetChanged();
    afx_msg void OnExecute();
    afx_msg void OnRunAll();
    afx_msg void OnStop();
    afx_msg void OnScriptLoad();
    afx_msg void OnScriptRun();
    afx_msg void OnScriptSelectAll();
    afx_msg void OnScriptListChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnCounterReset();
    afx_msg void OnUiSettingChanged();
    DECLARE_MESSAGE_MAP()

private:
    int CalculateVirtualContentHeight(int client_height) const;
    void ScrollToOffset(int next_offset);
    void LayoutScrolledContent();
    void UpdateVerticalScroll(const CRect& client);
    void LayoutControls(const CRect& r);
    void UpdateEnableState();
    void RefreshPinUsage();
    void FlushUiUpdates();
    void RequestCancel();
    void ResetCancel();
    void ThrowIfCancelRequested();
    bool SleepWithCancel(int delay_ms);
    int ParseEditInt(const CEdit& edit) const;
    int CurrentSpeedHz() const;
    int CurrentMasterPort() const;
    const mfc_tool::core::board_i2c::PinPair& CurrentMasterPinPair() const;
    std::wstring GetEditText(const CEdit& edit) const;
    std::uint8_t ParseCommandCode() const;
    std::vector<std::uint8_t> ParseTxHex(const CEdit& edit) const;
    std::vector<std::uint8_t> BuildTxPayload(bool update_ui, bool advance_counter);
    Profile CurrentProfile() const;
    Transaction CurrentTransaction() const;
    std::wstring CurrentTransactionText() const;
    void PopulateProfileCombo();
    void PopulatePortCombo();
    void PopulatePinCombo(const std::wstring& preferred_ini_name = L"");
    void PopulatePresetCombo();
    void PopulateTransactionCombo();
    void ApplyPresetToCommandUi();
    void ApplyProfileDefaults(bool force_address);
    bool IsUbmProfile() const;
    void SetRawRxText(const std::wstring& text);
    void SetResultText(const std::wstring& text);
    void SetScriptPathText();
    void SetScriptResponseText(const std::wstring& text);
    void AppendScriptResponse(const std::wstring& text);
    void LoadScriptFromPath(const std::wstring& path);
    void PopulateScriptList();
    void UpdateScriptSummary();
    DWORD RunAllCommandDelayMs() const;
    int RunAllRepeatCount() const;
    void SleepAfterRunAllCommand();

    ExecResult ExecQuick(std::uint8_t addr, bool read_bit);
    ExecResult ExecReceiveByte(std::uint8_t addr, bool pec);
    ExecResult ExecSendByte(std::uint8_t addr, std::uint8_t command, bool pec, bool force_bad_pec);
    ExecResult ExecWrite(std::uint8_t addr, std::uint8_t command, const std::vector<std::uint8_t>& data, bool pec, bool force_bad_pec);
    ExecResult ExecRead(std::uint8_t addr, std::uint8_t command, int read_len, bool pec);
    ExecResult ExecBlockRead(std::uint8_t addr, std::uint8_t command, int max_read_len, bool pec);
    ExecResult ExecProcessCall(std::uint8_t addr, std::uint8_t command, const std::vector<std::uint8_t>& data, bool pec);
    ExecResult ExecBlockWriteReadProcessCall(std::uint8_t addr, std::uint8_t command, const std::vector<std::uint8_t>& data, int max_read_len, bool pec);
    ExecResult ExecUbmControllerRead(std::uint8_t addr, std::uint8_t command, int read_len);
    ExecResult ExecUbmControllerWrite(std::uint8_t addr, std::uint8_t command, const std::vector<std::uint8_t>& data, bool force_bad_checksum);
    ExecResult ExecuteSelected();
    ExecResult ExecuteScriptCommandRow(const mfc_tool::core::SmbusScriptRow& row);
    std::wstring BuildResultText(const ExecResult& result, Transaction txn) const;
    bool RunOneAllTest(const wchar_t* label, Transaction txn, std::uint8_t command, const std::vector<std::uint8_t>& data, int read_len, bool pec, bool bad_pec, int* pass, int* fail);
    void RunGenericAll();
    void RunUbmControllerAll();
    void RunOneUbmTest(const wchar_t* label, Transaction txn, std::uint8_t command, const std::vector<std::uint8_t>& data, int read_len, bool bad_checksum, int* pass, int* fail);
    static const wchar_t* ProfileText(Profile profile);
    static const wchar_t* TransactionText(Transaction txn);
    static std::wstring AnsiToWide(const char* text);

private:
    mfc_tool::core::BridgeService* service_ = nullptr;
    mfc_tool::core::PinUsageRegistry* pin_usage_ = nullptr;
    std::function<void(const std::wstring&)> log_;
    std::function<void()> persist_settings_;
    bool connected_ = false;
    bool master_enabled_ = false;
    bool run_all_running_ = false;
    bool script_running_ = false;
    bool cancel_requested_ = false;
    bool script_list_updating_ = false;
    bool counter_seeded_ = false;
    unsigned int counter_value_ = 0;
    mfc_tool::core::SmbusScriptDocument script_doc_;

    CFont ui_font_;
    int scroll_offset_ = 0;
    int virtual_content_height_ = 0;
    CButton master_group_;
    CStatic profile_label_;
    CComboBox profile_combo_;
    CStatic port_label_;
    CComboBox port_combo_;
    CStatic pins_label_;
    CComboBox pins_combo_;
    CStatic speed_label_;
    CComboBox speed_combo_;
    CStatic addr_label_;
    CEdit addr_edit_;
    CStatic run_all_delay_label_;
    CEdit run_all_delay_edit_;
    CStatic run_all_repeat_label_;
    CEdit run_all_repeat_edit_;
    CButton pec_check_;
    CButton bad_pec_check_;
    CButton master_enable_btn_;
    CButton master_disable_btn_;
    CStatic preset_label_;
    CComboBox preset_combo_;
    CStatic command_label_;
    CEdit command_edit_;
    CStatic txn_label_;
    CComboBox txn_combo_;
    CStatic tx_label_;
    CEdit tx_edit_;
    CStatic read_len_label_;
    CEdit read_len_edit_;
    CButton counter_check_;
    CStatic counter_idx_label_;
    CEdit counter_idx_edit_;
    CStatic counter_step_label_;
    CEdit counter_step_edit_;
    CButton counter_reset_btn_;
    CButton execute_btn_;
    CButton run_all_btn_;
    CButton stop_btn_;
    CProgressCtrl progress_;
    CStatic raw_label_;
    CEdit raw_edit_;
    CStatic script_label_;
    CEdit script_path_edit_;
    CButton script_load_btn_;
    CButton script_run_btn_;
    CButton script_select_all_check_;
    CListCtrl script_list_;
    CStatic result_label_;
    CEdit result_edit_;
};
