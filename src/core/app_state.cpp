#include "app_state.h"

#include <cwctype>

namespace mfc_tool::core {
namespace {

bool IsAbsolutePath(const std::wstring& path) {
    if (path.size() >= 2u && path[1] == L':') {
        return true;
    }
    return path.size() >= 2u &&
           ((path[0] == L'\\' && path[1] == L'\\') ||
            (path[0] == L'/' && path[1] == L'/'));
}

std::wstring LowerPath(std::wstring path) {
    for (wchar_t& ch : path) {
        if (ch == L'/') {
            ch = L'\\';
        } else {
            ch = static_cast<wchar_t>(towlower(ch));
        }
    }
    return path;
}

std::wstring ExeRootPath() {
    return config::IniManager::DefaultIniPath(L"");
}

std::wstring ToExeRelativePath(const std::wstring& path) {
    if (path.empty() || !IsAbsolutePath(path)) {
        return path;
    }

    const std::wstring root = ExeRootPath();
    const std::wstring lower_path = LowerPath(path);
    const std::wstring lower_root = LowerPath(root);
    if (lower_root.empty() || lower_path.find(lower_root) != 0u) {
        return path;
    }
    return path.substr(root.size());
}

std::wstring FromExeRelativePath(const std::wstring& path) {
    if (path.empty() || IsAbsolutePath(path)) {
        return path;
    }
    return config::IniManager::DefaultIniPath(path);
}

std::wstring GetValue(
    const config::IniData& data,
    const std::wstring& section,
    const std::wstring& key,
    const std::wstring& fallback
) {
    auto sec_it = data.find(section);
    if (sec_it == data.end()) {
        return fallback;
    }
    auto key_it = sec_it->second.find(key);
    if (key_it == sec_it->second.end()) {
        return fallback;
    }
    return key_it->second;
}

void AddPmbusSection(config::IniData* out, const std::wstring& section, const PmbusState& state) {
    (*out)[section] = {
        {L"master_i2c_port", state.master_i2c_port},
        {L"master_i2c_pins", state.master_i2c_pins},
        {L"speed_hz", state.speed_hz},
        {L"master_addr", state.master_addr},
        {L"checklist_delay_ms", state.checklist_delay_ms},
        {L"checklist_repeat_count", state.checklist_repeat_count},
        {L"pec_enable", state.pec_enable},
        {L"bad_pec_enable", state.bad_pec_enable},
        {L"extended_enable", state.extended_enable},
        {L"extended_type", state.extended_type},
        {L"command_preset", state.command_preset},
        {L"command_code", state.command_code},
        {L"transaction", state.transaction},
        {L"tx_hex", state.tx_hex},
        {L"read_len", state.read_len},
        {L"smbalert_mask_hex", state.smbalert_mask_hex},
        {L"system_policy", state.system_policy},
        {L"smbus_script_path", ToExeRelativePath(state.smbus_script_path)}
    };
}

void ApplyPmbusSection(const config::IniData& data, const std::wstring& section, PmbusState* state) {
    state->speed_hz = GetValue(data, section, L"speed_hz", state->speed_hz);
    state->master_i2c_port = GetValue(data, section, L"master_i2c_port", state->master_i2c_port);
    state->master_i2c_pins = GetValue(data, section, L"master_i2c_pins", state->master_i2c_pins);
    state->master_addr = GetValue(data, section, L"master_addr", state->master_addr);
    state->checklist_delay_ms = GetValue(data, section, L"checklist_delay_ms", state->checklist_delay_ms);
    state->checklist_repeat_count = GetValue(data, section, L"checklist_repeat_count", state->checklist_repeat_count);
    state->pec_enable = GetValue(data, section, L"pec_enable", state->pec_enable);
    state->bad_pec_enable = GetValue(data, section, L"bad_pec_enable", state->bad_pec_enable);
    state->extended_enable = GetValue(data, section, L"extended_enable", state->extended_enable);
    state->extended_type = GetValue(data, section, L"extended_type", state->extended_type);
    state->command_preset = GetValue(data, section, L"command_preset", state->command_preset);
    state->command_code = GetValue(data, section, L"command_code", state->command_code);
    state->transaction = GetValue(data, section, L"transaction", state->transaction);
    state->tx_hex = GetValue(data, section, L"tx_hex", state->tx_hex);
    state->read_len = GetValue(data, section, L"read_len", state->read_len);
    state->smbalert_mask_hex = GetValue(data, section, L"smbalert_mask_hex", state->smbalert_mask_hex);
    state->system_policy = GetValue(data, section, L"system_policy", state->system_policy);
    state->smbus_script_path = FromExeRelativePath(GetValue(data, section, L"smbus_script_path", state->smbus_script_path));
}

std::wstring DefaultPresetScriptPath(const std::wstring& file_name) {
    return config::IniManager::DefaultIniPath(L"script_presets\\" + file_name);
}

} // namespace

AppState AppState::Default() {
    AppState s;

    s.i2c[0].pin_pair = L"I2C0_PB4_PB5";
    s.i2c[1].pin_pair = L"I2C1_PB2_PB3";

    s.crps.speed_hz = L"400000";
    s.crps.master_addr = L"0x5A";
    s.crps.pec_enable = L"1";
    s.crps.bad_pec_enable = L"0";
    s.crps.extended_enable = L"0";
    s.crps.extended_type = L"MFR_EXT";
    s.crps.command_preset = L"PMBUS_REVISION";
    s.crps.command_code = L"0x98";
    s.crps.transaction = L"Read Byte";
    s.crps.tx_hex = L"00 80";
    s.crps.read_len = L"1";

    s.ti_ucd90xxx.speed_hz = L"400000";
    s.ti_ucd90xxx.master_addr = L"0x5A";
    s.ti_ucd90xxx.pec_enable = L"1";
    s.ti_ucd90xxx.bad_pec_enable = L"0";
    s.ti_ucd90xxx.extended_enable = L"0";
    s.ti_ucd90xxx.extended_type = L"MFR_EXT";
    s.ti_ucd90xxx.command_preset = L"PMBUS_REVISION";
    s.ti_ucd90xxx.command_code = L"0x98";
    s.ti_ucd90xxx.transaction = L"Read Byte";
    s.ti_ucd90xxx.tx_hex = L"00 80";
    s.ti_ucd90xxx.read_len = L"1";

    s.script_path = DefaultPresetScriptPath(L"PMBus_Base_Full.csv");
    s.pmbus.smbus_script_path = DefaultPresetScriptPath(L"PMBus_Base_Full.csv");
    s.crps.smbus_script_path = DefaultPresetScriptPath(L"PMBus_M_CRPS_Full.csv");
    s.ti_ucd90xxx.smbus_script_path = DefaultPresetScriptPath(L"PMBus_TI_UCD90xxx_Full.csv");
    s.smbus.smbus_script_path = DefaultPresetScriptPath(L"SMBus_Generic_RunAll.csv");

    s.pmbus_profile = L"PMBus Base";
    return s;
}

config::IniData AppState::ToIniData(const std::wstring& ini_path) const {
    config::IniData out;
    out[L"APP"] = {
        {L"ini_path", ToExeRelativePath(ini_path)},
        {L"pmbus_profile", pmbus_profile},
        {L"script_path", ToExeRelativePath(script_path)}
    };

    out[L"UI"] = {
        {L"vid", ui.vid},
        {L"pid", ui.pid},
        {L"timeout_ms", ui.timeout_ms},
        {L"device_label", ui.device_label},
        {L"save_log_checked", ui.save_log_checked ? L"1" : L"0"},
        {L"expected_fw_version", ui.expected_fw_version},
        {L"last_seen_fw_version", ui.last_seen_fw_version}
    };

    AddPmbusSection(&out, L"PMBUS", pmbus);
    AddPmbusSection(&out, L"CRPS", crps);
    AddPmbusSection(&out, L"TI_UCD90XXX", ti_ucd90xxx);

    out[L"SMBUS"] = {
        {L"profile", smbus.profile},
        {L"master_i2c_port", smbus.master_i2c_port},
        {L"master_i2c_pins", smbus.master_i2c_pins},
        {L"speed_hz", smbus.speed_hz},
        {L"addr", smbus.addr},
        {L"run_all_delay_ms", smbus.run_all_delay_ms},
        {L"run_all_repeat_count", smbus.run_all_repeat_count},
        {L"pec_enable", smbus.pec_enable},
        {L"bad_pec_enable", smbus.bad_pec_enable},
        {L"transaction", smbus.transaction},
        {L"command_preset", smbus.command_preset},
        {L"command_code", smbus.command_code},
        {L"tx_hex", smbus.tx_hex},
        {L"read_len", smbus.read_len},
        {L"counter_enable", smbus.counter_enable},
        {L"counter_index", smbus.counter_index},
        {L"counter_step", smbus.counter_step},
        {L"smbus_script_path", ToExeRelativePath(smbus.smbus_script_path)}
    };

    for (int i = 0; i < 2; ++i) {
        const auto& t = i2c[i];
        out[L"I2C" + std::to_wstring(i)] = {
            {L"pin_pair", t.pin_pair},
            {L"baud", t.baud},
            {L"addr", t.addr},
            {L"repeated_start", t.repeated_start},
            {L"master_tx_hex", t.master_tx_hex},
            {L"master_read_len", t.master_read_len},
            {L"slave_tx_hex", t.slave_tx_hex},
            {L"slave_rx_max", t.slave_rx_max},
            {L"monitor_interval_ms", t.monitor_interval_ms},
            {L"interval_send_100ms", t.interval_send_100ms},
            {L"tx_target", t.tx_target},
            {L"gen_len", t.gen_len},
            {L"gen_start", t.gen_start},
            {L"gen_step", t.gen_step},
            {L"counter_enable", t.counter_enable},
            {L"counter_index", t.counter_index},
            {L"counter_step", t.counter_step}
        };
    }

    return out;
}

void AppState::ApplyIniData(const config::IniData& data) {
    pmbus_profile = GetValue(data, L"APP", L"pmbus_profile", pmbus_profile);
    script_path = FromExeRelativePath(GetValue(data, L"APP", L"script_path", script_path));

    ui.vid = GetValue(data, L"UI", L"vid", ui.vid);
    ui.pid = GetValue(data, L"UI", L"pid", ui.pid);
    ui.timeout_ms = GetValue(data, L"UI", L"timeout_ms", ui.timeout_ms);
    ui.device_label = GetValue(data, L"UI", L"device_label", ui.device_label);
    ui.save_log_checked = GetValue(data, L"UI", L"save_log_checked", ui.save_log_checked ? L"1" : L"0") == L"1";
    ui.last_seen_fw_version = GetValue(data, L"UI", L"last_seen_fw_version", ui.last_seen_fw_version);

    ApplyPmbusSection(data, L"PMBUS", &pmbus);
    ApplyPmbusSection(data, L"CRPS", &crps);
    ApplyPmbusSection(data, L"TI_UCD90XXX", &ti_ucd90xxx);

    smbus.profile = GetValue(data, L"SMBUS", L"profile", smbus.profile);
    smbus.master_i2c_port = GetValue(data, L"SMBUS", L"master_i2c_port", smbus.master_i2c_port);
    smbus.master_i2c_pins = GetValue(data, L"SMBUS", L"master_i2c_pins", smbus.master_i2c_pins);
    smbus.speed_hz = GetValue(data, L"SMBUS", L"speed_hz", smbus.speed_hz);
    smbus.addr = GetValue(data, L"SMBUS", L"addr", smbus.addr);
    smbus.run_all_delay_ms = GetValue(data, L"SMBUS", L"run_all_delay_ms", smbus.run_all_delay_ms);
    smbus.run_all_repeat_count = GetValue(data, L"SMBUS", L"run_all_repeat_count", smbus.run_all_repeat_count);
    smbus.pec_enable = GetValue(data, L"SMBUS", L"pec_enable", smbus.pec_enable);
    smbus.bad_pec_enable = GetValue(data, L"SMBUS", L"bad_pec_enable", smbus.bad_pec_enable);
    smbus.transaction = GetValue(data, L"SMBUS", L"transaction", smbus.transaction);
    smbus.command_preset = GetValue(data, L"SMBUS", L"command_preset", smbus.command_preset);
    smbus.command_code = GetValue(data, L"SMBUS", L"command_code", smbus.command_code);
    smbus.tx_hex = GetValue(data, L"SMBUS", L"tx_hex", smbus.tx_hex);
    smbus.read_len = GetValue(data, L"SMBUS", L"read_len", smbus.read_len);
    smbus.counter_enable = GetValue(data, L"SMBUS", L"counter_enable", smbus.counter_enable);
    smbus.counter_index = GetValue(data, L"SMBUS", L"counter_index", smbus.counter_index);
    smbus.counter_step = GetValue(data, L"SMBUS", L"counter_step", smbus.counter_step);
    smbus.smbus_script_path = FromExeRelativePath(GetValue(data, L"SMBUS", L"smbus_script_path", smbus.smbus_script_path));

    for (int i = 0; i < 2; ++i) {
        const std::wstring sec = L"I2C" + std::to_wstring(i);
        i2c[i].pin_pair = GetValue(data, sec, L"pin_pair", i2c[i].pin_pair);
        i2c[i].baud = GetValue(data, sec, L"baud", i2c[i].baud);
        i2c[i].addr = GetValue(data, sec, L"addr", i2c[i].addr);
        i2c[i].repeated_start = GetValue(data, sec, L"repeated_start", i2c[i].repeated_start);
        i2c[i].master_tx_hex = GetValue(data, sec, L"master_tx_hex", i2c[i].master_tx_hex);
        i2c[i].master_read_len = GetValue(data, sec, L"master_read_len", i2c[i].master_read_len);
        i2c[i].slave_tx_hex = GetValue(data, sec, L"slave_tx_hex", i2c[i].slave_tx_hex);
        i2c[i].slave_rx_max = GetValue(data, sec, L"slave_rx_max", i2c[i].slave_rx_max);
        i2c[i].monitor_interval_ms = GetValue(data, sec, L"monitor_interval_ms", i2c[i].monitor_interval_ms);
        i2c[i].interval_send_100ms = GetValue(data, sec, L"interval_send_100ms", i2c[i].interval_send_100ms);
        i2c[i].tx_target = GetValue(data, sec, L"tx_target", i2c[i].tx_target);
        i2c[i].gen_len = GetValue(data, sec, L"gen_len", i2c[i].gen_len);
        i2c[i].gen_start = GetValue(data, sec, L"gen_start", i2c[i].gen_start);
        i2c[i].gen_step = GetValue(data, sec, L"gen_step", i2c[i].gen_step);
        i2c[i].counter_enable = GetValue(data, sec, L"counter_enable", i2c[i].counter_enable);
        i2c[i].counter_index = GetValue(data, sec, L"counter_index", i2c[i].counter_index);
        i2c[i].counter_step = GetValue(data, sec, L"counter_step", i2c[i].counter_step);
    }

}

} // namespace mfc_tool::core
