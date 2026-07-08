#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../core/app_state.h"
#include "../core/board_i2c_catalog.h"
#include "../core/bridge_service.h"
#include "../core/pin_usage_registry.h"

class CI2cTab : public CWnd {
public:
    BOOL Create(CWnd* parent, const RECT& rect, UINT id);

    void Bind(mfc_tool::core::BridgeService* service,
              std::function<void(const std::wstring&)> logger,
              mfc_tool::core::PinUsageRegistry* pin_usage);
    void SetConnected(bool connected);
    void OnDisconnected();
    void RefreshDpiLayout();

    void LoadState(const mfc_tool::core::AppState& state);
    void SaveState(mfc_tool::core::AppState* state) const;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnPortChanged();
    afx_msg void OnPinsChanged();
    afx_msg void OnMasterEnable();
    afx_msg void OnSlaveEnable();
    afx_msg void OnDisablePort();
    afx_msg void OnMasterWrite();
    afx_msg void OnMasterRead();
    afx_msg void OnWriteThenRead();
    afx_msg void OnSlaveSetTx();
    afx_msg void OnSlaveGetRx();
    afx_msg void OnMonitorStart();
    afx_msg void OnMonitorStop();
    afx_msg void OnIntervalStart();
    afx_msg void OnIntervalStop();
    afx_msg void OnGenerateData();
    afx_msg void OnCounterChanged();
    afx_msg void OnCounterReset();
    afx_msg void OnMasterTxChanged();
    afx_msg void OnSlaveTxChanged();

    DECLARE_MESSAGE_MAP()

private:
    enum class Role {
        None,
        Master,
        Slave,
    };

    struct PortRuntime {
        Role role = Role::None;
        bool monitoring = false;
        bool interval_sending = false;
        int interval_error_streak = 0;
        bool counter_seeded = false;
        unsigned int counter_value = 0;
    };

    struct PortState {
        std::wstring pin_pair;
        std::wstring baud = L"100000";
        std::wstring addr = L"0x50";
        std::wstring repeated_start = L"1";
        std::wstring master_tx_hex = L"00 11 22";
        std::wstring master_read_len = L"16";
        std::wstring slave_tx_hex = L"DE AD BE EF";
        std::wstring slave_rx_max = L"32";
        std::wstring monitor_interval_ms = L"100";
        std::wstring interval_send_100ms = L"1";
        std::wstring tx_target = L"master";
        std::wstring gen_len = L"16";
        std::wstring gen_start = L"0x00";
        std::wstring gen_step = L"1";
        std::wstring counter_enable = L"0";
        std::wstring counter_index = L"0";
        std::wstring counter_step = L"1";
    };

    void SaveVisibleToState();
    void LoadVisibleFromState();
    void PopulatePortCombo();
    void PopulatePinCombo(const std::wstring& preferred_ini_name = L"");
    void UpdateEnableState();
    void UpdateTxHexLabels();
    void RefreshPinUsage();
    void SetPortRole(int port, Role role);
    void StopPortTimers(int port);
    void StopAllTimers();
    void DisableOtherPort(int port);
    void MasterWriteInternal(bool show_message);
    void MasterReadInternal(bool show_message);
    void WriteThenReadInternal(bool show_message);
    bool SlaveSetTxInternal(bool show_message, bool advance_counter);
    void SlaveGetRxInternal(bool show_message);
    void MonitorPoll();
    void IntervalTick();
    void SetStatusText(const std::wstring& text);
    void AppendStatusText(const std::wstring& text);
    std::vector<std::uint8_t> BuildTxPayload(bool for_master, bool update_ui, bool advance_counter);
    std::vector<std::uint8_t> ParseTxHex(const CEdit& edit) const;
    int CurrentPort() const;
    int ParseEditInt(const CEdit& edit) const;
    int CurrentAddress() const;
    int CurrentBaud() const;
    std::wstring GetEditText(const CEdit& edit) const;
    const mfc_tool::core::board_i2c::PinPair& CurrentPinPair() const;
    const mfc_tool::core::board_i2c::PinPair& PinPairForPort(int port) const;
    static std::wstring OwnerId(int port, Role role);
    static std::vector<std::wstring> SharedOwnerIds();
    static std::wstring AnsiToWide(const char* text);

private:
    mfc_tool::core::BridgeService* service_ = nullptr;
    mfc_tool::core::PinUsageRegistry* pin_usage_ = nullptr;
    std::function<void(const std::wstring&)> log_;
    bool connected_ = false;
    bool loading_ = false;
    int visible_port_ = 0;

    CFont ui_font_;
    CButton config_group_;
    CStatic port_label_;
    CComboBox port_combo_;
    CStatic pins_label_;
    CComboBox pins_combo_;
    CStatic addr_label_;
    CEdit addr_edit_;
    CStatic baud_label_;
    CEdit baud_edit_;
    CButton repeated_start_check_;
    CButton master_enable_btn_;
    CButton slave_enable_btn_;
    CButton disable_btn_;

    CButton master_group_;
    CStatic master_tx_label_;
    CEdit master_tx_edit_;
    CStatic master_read_label_;
    CEdit master_read_edit_;
    CButton master_write_btn_;
    CButton master_read_btn_;
    CButton write_read_btn_;
    CStatic master_rx_label_;
    CEdit master_rx_edit_;

    CButton slave_group_;
    CStatic slave_tx_label_;
    CEdit slave_tx_edit_;
    CStatic slave_rx_max_label_;
    CEdit slave_rx_max_edit_;
    CButton slave_set_tx_btn_;
    CButton slave_get_rx_btn_;
    CStatic slave_rx_label_;
    CEdit slave_rx_edit_;

    CButton tools_group_;
    CStatic monitor_label_;
    CEdit monitor_edit_;
    CButton monitor_start_btn_;
    CButton monitor_stop_btn_;
    CStatic interval_label_;
    CEdit interval_edit_;
    CButton interval_start_btn_;
    CButton interval_stop_btn_;
    CStatic target_label_;
    CComboBox target_combo_;
    CStatic gen_len_label_;
    CEdit gen_len_edit_;
    CStatic gen_start_label_;
    CEdit gen_start_edit_;
    CStatic gen_step_label_;
    CEdit gen_step_edit_;
    CButton gen_btn_;
    CButton counter_check_;
    CStatic counter_idx_label_;
    CEdit counter_idx_edit_;
    CStatic counter_step_label_;
    CEdit counter_step_edit_;
    CButton counter_reset_btn_;

    CButton status_group_;
    CEdit status_edit_;

    std::array<PortState, 2> port_state_;
    std::array<PortRuntime, 2> runtime_;

    static constexpr UINT_PTR kMonitorTimer = 3700;
    static constexpr UINT_PTR kIntervalTimer = 3701;
};
