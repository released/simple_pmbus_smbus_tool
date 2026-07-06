#pragma once

#include <array>
#include <string>

#include "../config/ini_manager.h"
#include "fw_version.generated.h"

namespace mfc_tool::core {

struct UiState {
    std::wstring vid = L"0x0416";
    std::wstring pid = L"0x5020";
    std::wstring timeout_ms = L"2000";
    std::wstring device_label = L"Auto Select";
    bool save_log_checked = false;
    std::wstring expected_fw_version = M032_EXPECTED_FW_VERSION;
    std::wstring last_seen_fw_version = L"-";
};

struct PmbusState {
    std::wstring master_i2c_port = L"0";
    std::wstring master_i2c_pins = L"I2C0_PB4_PB5";
    std::wstring speed_hz = L"100000";
    std::wstring master_addr = L"0x58";
    std::wstring checklist_delay_ms = L"10";
    std::wstring checklist_repeat_count = L"1";
    std::wstring pec_enable = L"0";
    std::wstring bad_pec_enable = L"0";
    std::wstring extended_enable = L"0";
    std::wstring extended_type = L"MFR_EXT";
    std::wstring command_preset = L"PAGE";
    std::wstring command_code = L"0x00";
    std::wstring transaction = L"Write Byte";
    std::wstring tx_hex = L"00 80";
    std::wstring read_len = L"16";
    std::wstring smbalert_mask_hex = L"0x0000";
    std::wstring system_policy = L"Production";
    std::wstring smbus_script_path;
};

struct SmbusState {
    std::wstring profile = L"Generic";
    std::wstring master_i2c_port = L"0";
    std::wstring master_i2c_pins = L"I2C0_PB4_PB5";
    std::wstring speed_hz = L"100000";
    std::wstring addr = L"0x5A";
    std::wstring run_all_delay_ms = L"10";
    std::wstring run_all_repeat_count = L"1";
    std::wstring pec_enable = L"1";
    std::wstring bad_pec_enable = L"0";
    std::wstring transaction = L"Read Byte";
    std::wstring command_preset = L"SMB_EXAMPLE_READ_BYTE_A";
    std::wstring command_code = L"0x22";
    std::wstring tx_hex = L"12 34";
    std::wstring read_len = L"16";
    std::wstring counter_enable = L"0";
    std::wstring counter_index = L"0";
    std::wstring counter_step = L"1";
    std::wstring smbus_script_path;
};

struct I2cState {
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

struct AppState {
    UiState ui;
    std::wstring pmbus_profile = L"PMBus Base";
    PmbusState pmbus;
    PmbusState crps;
    PmbusState ti_ucd90xxx;
    SmbusState smbus;
    std::array<I2cState, 2> i2c;
    std::wstring script_path;

    static AppState Default();

    config::IniData ToIniData(const std::wstring& ini_path) const;
    void ApplyIniData(const config::IniData& data);
};

} // namespace mfc_tool::core
