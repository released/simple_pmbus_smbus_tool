#include "smbus_tab.h"

#include <afxdlgs.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../core/pmbus_utils.h"
#include "../core/text_utils.h"
#include "../hid/hid_bridge_client.h"
#include "layout_utils.h"

namespace {

enum : UINT {
    IDC_SMBUS_MASTER_GROUP = 15100,
    IDC_SMBUS_PROFILE_LABEL,
    IDC_SMBUS_PROFILE_COMBO,
    IDC_SMBUS_PORT_LABEL,
    IDC_SMBUS_PORT_COMBO,
    IDC_SMBUS_PINS_LABEL,
    IDC_SMBUS_PINS_COMBO,
    IDC_SMBUS_SPEED_LABEL,
    IDC_SMBUS_SPEED_COMBO,
    IDC_SMBUS_ADDR_LABEL,
    IDC_SMBUS_ADDR_EDIT,
    IDC_SMBUS_RUN_ALL_DELAY_LABEL,
    IDC_SMBUS_RUN_ALL_DELAY_EDIT,
    IDC_SMBUS_RUN_ALL_REPEAT_LABEL,
    IDC_SMBUS_RUN_ALL_REPEAT_EDIT,
    IDC_SMBUS_PEC_CHECK,
    IDC_SMBUS_BAD_PEC_CHECK,
    IDC_SMBUS_MASTER_ENABLE,
    IDC_SMBUS_MASTER_DISABLE,
    IDC_SMBUS_PRESET_LABEL,
    IDC_SMBUS_PRESET_COMBO,
    IDC_SMBUS_CMD_LABEL,
    IDC_SMBUS_CMD_EDIT,
    IDC_SMBUS_TXN_LABEL,
    IDC_SMBUS_TXN_COMBO,
    IDC_SMBUS_TX_LABEL,
    IDC_SMBUS_TX_EDIT,
    IDC_SMBUS_READ_LEN_LABEL,
    IDC_SMBUS_READ_LEN_EDIT,
    IDC_SMBUS_COUNTER_CHECK,
    IDC_SMBUS_COUNTER_IDX_LABEL,
    IDC_SMBUS_COUNTER_IDX_EDIT,
    IDC_SMBUS_COUNTER_STEP_LABEL,
    IDC_SMBUS_COUNTER_STEP_EDIT,
    IDC_SMBUS_COUNTER_RESET,
    IDC_SMBUS_EXECUTE,
    IDC_SMBUS_RUN_ALL,
    IDC_SMBUS_STOP,
    IDC_SMBUS_PROGRESS,
    IDC_SMBUS_RAW_LABEL,
    IDC_SMBUS_RAW_EDIT,
    IDC_SMBUS_SCRIPT_LABEL,
    IDC_SMBUS_SCRIPT_PATH,
    IDC_SMBUS_SCRIPT_LOAD,
    IDC_SMBUS_SCRIPT_RUN,
    IDC_SMBUS_SCRIPT_SELECT_ALL,
    IDC_SMBUS_SCRIPT_LIST,
    IDC_SMBUS_RESULT_LABEL,
    IDC_SMBUS_RESULT_EDIT,
};

constexpr int kSmbusRetryAttempts = 3;
constexpr DWORD kSmbusRetryDelayMs = 10u;
constexpr DWORD kSmbusDefaultRunAllCommandDelayMs = 10u;
constexpr DWORD kSmbusMaxRunAllCommandDelayMs = 1000u;
constexpr int kSmbusDefaultRunAllRepeatCount = 1;
constexpr int kSmbusMaxRunAllRepeatCount = 20;
constexpr const wchar_t* kSmbusMasterOwner = L"SMBUS-M";

class UserCancelled : public std::runtime_error {
public:
    UserCancelled() : std::runtime_error("operation cancelled by user") {}
};

struct SmbusPreset {
    CSmbusTab::Profile profile;
    std::uint8_t command;
    const wchar_t* name;
    CSmbusTab::Transaction txn;
    const wchar_t* tx_hex;
    int read_len;
};

const SmbusPreset kSmbusPresets[] = {
    {CSmbusTab::Profile::Generic, 0x00u, L"SMB_EXAMPLE_QUICK_WRITE", CSmbusTab::Transaction::QuickWrite, L"", 0},
    {CSmbusTab::Profile::Generic, 0x00u, L"SMB_EXAMPLE_QUICK_READ", CSmbusTab::Transaction::QuickRead, L"", 0},
    {CSmbusTab::Profile::Generic, 0x10u, L"SMB_EXAMPLE_SEND_BYTE_A", CSmbusTab::Transaction::SendByte, L"", 0},
    {CSmbusTab::Profile::Generic, 0x11u, L"SMB_EXAMPLE_SEND_BYTE_B", CSmbusTab::Transaction::SendByte, L"", 0},
    {CSmbusTab::Profile::Generic, 0x12u, L"SMB_EXAMPLE_RECEIVE_SELECT_A", CSmbusTab::Transaction::SendByte, L"", 0},
    {CSmbusTab::Profile::Generic, 0x13u, L"SMB_EXAMPLE_RECEIVE_SELECT_B", CSmbusTab::Transaction::SendByte, L"", 0},
    {CSmbusTab::Profile::Generic, 0x00u, L"SMB_EXAMPLE_RECEIVE_BYTE", CSmbusTab::Transaction::ReceiveByte, L"", 1},
    {CSmbusTab::Profile::Generic, 0x20u, L"SMB_EXAMPLE_WRITE_BYTE_A", CSmbusTab::Transaction::WriteByte, L"00", 0},
    {CSmbusTab::Profile::Generic, 0x21u, L"SMB_EXAMPLE_WRITE_BYTE_B", CSmbusTab::Transaction::WriteByte, L"80", 0},
    {CSmbusTab::Profile::Generic, 0x22u, L"SMB_EXAMPLE_READ_BYTE_A", CSmbusTab::Transaction::ReadByte, L"", 1},
    {CSmbusTab::Profile::Generic, 0x23u, L"SMB_EXAMPLE_READ_BYTE_B", CSmbusTab::Transaction::ReadByte, L"", 1},
    {CSmbusTab::Profile::Generic, 0x30u, L"SMB_EXAMPLE_WRITE_WORD_A", CSmbusTab::Transaction::WriteWord, L"34 00", 0},
    {CSmbusTab::Profile::Generic, 0x31u, L"SMB_EXAMPLE_WRITE_WORD_B", CSmbusTab::Transaction::WriteWord, L"CD 80", 0},
    {CSmbusTab::Profile::Generic, 0x32u, L"SMB_EXAMPLE_READ_WORD_A", CSmbusTab::Transaction::ReadWord, L"", 2},
    {CSmbusTab::Profile::Generic, 0x33u, L"SMB_EXAMPLE_READ_WORD_B", CSmbusTab::Transaction::ReadWord, L"", 2},
    {CSmbusTab::Profile::Generic, 0x40u, L"SMB_EXAMPLE_BLOCK_WRITE_A", CSmbusTab::Transaction::BlockWrite, L"10 11 12 13 14 15 16 00", 0},
    {CSmbusTab::Profile::Generic, 0x41u, L"SMB_EXAMPLE_BLOCK_WRITE_B", CSmbusTab::Transaction::BlockWrite, L"20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 80", 0},
    {CSmbusTab::Profile::Generic, 0x42u, L"SMB_EXAMPLE_BLOCK_READ_A", CSmbusTab::Transaction::BlockRead, L"", 8},
    {CSmbusTab::Profile::Generic, 0x43u, L"SMB_EXAMPLE_BLOCK_READ_B", CSmbusTab::Transaction::BlockRead, L"", 16},
    {CSmbusTab::Profile::Generic, 0x50u, L"SMB_EXAMPLE_PROCESS_CALL_A", CSmbusTab::Transaction::ProcessCall, L"34 00", 2},
    {CSmbusTab::Profile::Generic, 0x51u, L"SMB_EXAMPLE_PROCESS_CALL_B", CSmbusTab::Transaction::ProcessCall, L"CD 80", 2},
    {CSmbusTab::Profile::Generic, 0x60u, L"SMB_EXAMPLE_BLOCK_PROC_CALL_A", CSmbusTab::Transaction::BlockWriteReadProcessCall, L"01 02 03 04 05 06 07 00", 8},
    {CSmbusTab::Profile::Generic, 0x61u, L"SMB_EXAMPLE_BLOCK_PROC_CALL_B", CSmbusTab::Transaction::BlockWriteReadProcessCall, L"11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 80", 16},

    {CSmbusTab::Profile::UbmController, 0x00u, L"UBM_OPERATIONAL_STATE", CSmbusTab::Transaction::UbmControllerRead, L"", 1},
    {CSmbusTab::Profile::UbmController, 0x01u, L"UBM_LAST_COMMAND_STATUS", CSmbusTab::Transaction::UbmControllerRead, L"", 1},
    {CSmbusTab::Profile::UbmController, 0x02u, L"UBM_SILICON_IDENTITY_VERSION", CSmbusTab::Transaction::UbmControllerRead, L"", 14},
    {CSmbusTab::Profile::UbmController, 0x03u, L"UBM_UPDATE_MODE_CAPABILITIES", CSmbusTab::Transaction::UbmControllerRead, L"", 1},
    {CSmbusTab::Profile::UbmController, 0x20u, L"UBM_ENTER_UPDATE_MODE", CSmbusTab::Transaction::UbmControllerWrite, L"B8 55 42 4D 01", 0},
    {CSmbusTab::Profile::UbmController, 0x21u, L"UBM_PROGRAMMABLE_MODE_DATA_TRANSFER", CSmbusTab::Transaction::UbmControllerWrite, L"00 02 AA 55", 0},
    {CSmbusTab::Profile::UbmController, 0x22u, L"UBM_EXIT_UPDATE_MODE", CSmbusTab::Transaction::UbmControllerWrite, L"55 42 4D 01", 0},
    {CSmbusTab::Profile::UbmController, 0x30u, L"UBM_HOST_FACING_CONNECTOR_INFO", CSmbusTab::Transaction::UbmControllerRead, L"", 1},
    {CSmbusTab::Profile::UbmController, 0x31u, L"UBM_BACKPLANE_INFO", CSmbusTab::Transaction::UbmControllerRead, L"", 1},
    {CSmbusTab::Profile::UbmController, 0x32u, L"UBM_STARTING_SLOT", CSmbusTab::Transaction::UbmControllerRead, L"", 1},
    {CSmbusTab::Profile::UbmController, 0x33u, L"UBM_CAPABILITIES", CSmbusTab::Transaction::UbmControllerRead, L"", 2},
    {CSmbusTab::Profile::UbmController, 0x34u, L"UBM_FEATURES", CSmbusTab::Transaction::UbmControllerRead, L"", 2},
    {CSmbusTab::Profile::UbmController, 0x34u, L"UBM_FEATURES_WRITE", CSmbusTab::Transaction::UbmControllerWrite, L"00 00", 0},
    {CSmbusTab::Profile::UbmController, 0x34u, L"UBM_BAD_CHECKSUM_WRITE", CSmbusTab::Transaction::UbmBadChecksumWrite, L"00 00", 0},
    {CSmbusTab::Profile::UbmController, 0x35u, L"UBM_CHANGE_COUNT", CSmbusTab::Transaction::UbmControllerRead, L"", 2},
    {CSmbusTab::Profile::UbmController, 0x35u, L"UBM_CHANGE_COUNT_CLEAR", CSmbusTab::Transaction::UbmControllerWrite, L"01 00", 0},
    {CSmbusTab::Profile::UbmController, 0x36u, L"UBM_DFC_INDEX_WRITE", CSmbusTab::Transaction::UbmControllerWrite, L"00", 0},
    {CSmbusTab::Profile::UbmController, 0x40u, L"UBM_DFC_STATUS_CONTROL_DESCRIPTOR", CSmbusTab::Transaction::UbmControllerRead, L"", 8},
    {CSmbusTab::Profile::UbmController, 0x37u, L"UBM_CABLE_CONTIGUOUS_CHECK", CSmbusTab::Transaction::UbmControllerWrite, L"00", 0},
    {CSmbusTab::Profile::UbmController, 0x38u, L"UBM_CCC_RESULT_INDEX", CSmbusTab::Transaction::UbmControllerWrite, L"00", 0},
    {CSmbusTab::Profile::UbmController, 0x41u, L"UBM_CCC_RESULT_DESCRIPTOR", CSmbusTab::Transaction::UbmControllerRead, L"", 35},
    {CSmbusTab::Profile::UbmController, 0x50u, L"UBM_FLEX_IO_INDEX", CSmbusTab::Transaction::UbmControllerWrite, L"00", 0},
    {CSmbusTab::Profile::UbmController, 0x51u, L"UBM_FLEX_IO_DESCRIPTOR", CSmbusTab::Transaction::UbmControllerRead, L"", 5},
    {CSmbusTab::Profile::UbmController, 0x60u, L"UBM_POWER_EVENT_DATA", CSmbusTab::Transaction::UbmControllerRead, L"", 32},
};

const SmbusPreset* FindPresetByName(CSmbusTab::Profile profile, const std::wstring& name) {
    for (const auto& preset : kSmbusPresets) {
        if (preset.profile == profile && name == preset.name) {
            return &preset;
        }
    }
    return nullptr;
}

std::wstring UpgradeLegacyPresetName(const std::wstring& name) {
    const std::wstring legacy_prefix = L"SMB_";
    const std::wstring current_prefix = L"SMB_EXAMPLE_";

    if (name.rfind(current_prefix, 0) == 0) {
        return name;
    }
    if (name.rfind(legacy_prefix, 0) == 0) {
        return current_prefix + name.substr(legacy_prefix.size());
    }
    return name;
}

std::string WideToAnsiLossy(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back((ch >= 0 && ch <= 0x7F) ? static_cast<char>(ch) : '?');
    }
    return out;
}

void AppendWritePec(std::vector<std::uint8_t>* tx, std::uint8_t addr, bool force_bad_pec) {
    std::vector<std::uint8_t> frame;
    frame.push_back(static_cast<std::uint8_t>(addr << 1));
    frame.insert(frame.end(), tx->begin(), tx->end());
    tx->push_back(static_cast<std::uint8_t>(mfc_tool::core::PmbusComputePec(frame) ^ (force_bad_pec ? 0xFFu : 0x00u)));
}

std::wstring PecText(const CSmbusTab::ExecResult& result) {
    std::wstringstream ss;
    if (!result.pec_checked) {
        return L"";
    }
    ss << L" | PEC " << (result.pec_ok ? L"OK" : L"FAIL")
       << L" rx=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
       << static_cast<unsigned int>(result.pec_rx)
       << L" calc=0x" << std::setw(2) << static_cast<unsigned int>(result.pec_calc);
    return ss.str();
}

std::uint8_t UbmChecksum(const std::vector<std::uint8_t>& bytes) {
    std::uint8_t sum = 0xA5u;
    for (std::uint8_t byte : bytes) {
        sum = static_cast<std::uint8_t>(sum + byte);
    }
    return static_cast<std::uint8_t>(0u - sum);
}

std::wstring UbmChecksumText(const CSmbusTab::ExecResult& result) {
    std::wstringstream ss;
    if (!result.ubm_checksum_checked) {
        return L"";
    }
    ss << L" | UBM checksum " << (result.ubm_checksum_ok ? L"OK" : L"FAIL")
       << L" rx=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
       << static_cast<unsigned int>(result.ubm_checksum_rx)
       << L" calc=0x" << std::setw(2) << static_cast<unsigned int>(result.ubm_checksum_calc);
    return ss.str();
}

bool IsExpectedBadPecBridgeStatus(std::uint8_t status) {
    return status == 0x04u || status == 0x05u;
}

} // namespace

BEGIN_MESSAGE_MAP(CSmbusTab, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_SMBUS_MASTER_ENABLE, &CSmbusTab::OnMasterEnable)
    ON_BN_CLICKED(IDC_SMBUS_MASTER_DISABLE, &CSmbusTab::OnMasterDisable)
    ON_CBN_SELCHANGE(IDC_SMBUS_PORT_COMBO, &CSmbusTab::OnMasterPortChanged)
    ON_CBN_SELCHANGE(IDC_SMBUS_PROFILE_COMBO, &CSmbusTab::OnProfileChanged)
    ON_CBN_SELCHANGE(IDC_SMBUS_PRESET_COMBO, &CSmbusTab::OnPresetChanged)
    ON_CBN_SELCHANGE(IDC_SMBUS_TXN_COMBO, &CSmbusTab::OnUiSettingChanged)
    ON_BN_CLICKED(IDC_SMBUS_PEC_CHECK, &CSmbusTab::OnUiSettingChanged)
    ON_BN_CLICKED(IDC_SMBUS_BAD_PEC_CHECK, &CSmbusTab::OnUiSettingChanged)
    ON_BN_CLICKED(IDC_SMBUS_COUNTER_CHECK, &CSmbusTab::OnUiSettingChanged)
    ON_BN_CLICKED(IDC_SMBUS_COUNTER_RESET, &CSmbusTab::OnCounterReset)
    ON_BN_CLICKED(IDC_SMBUS_EXECUTE, &CSmbusTab::OnExecute)
    ON_BN_CLICKED(IDC_SMBUS_RUN_ALL, &CSmbusTab::OnRunAll)
    ON_BN_CLICKED(IDC_SMBUS_STOP, &CSmbusTab::OnStop)
    ON_BN_CLICKED(IDC_SMBUS_SCRIPT_LOAD, &CSmbusTab::OnScriptLoad)
    ON_BN_CLICKED(IDC_SMBUS_SCRIPT_RUN, &CSmbusTab::OnScriptRun)
    ON_BN_CLICKED(IDC_SMBUS_SCRIPT_SELECT_ALL, &CSmbusTab::OnScriptSelectAll)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_SMBUS_SCRIPT_LIST, &CSmbusTab::OnScriptListChanged)
END_MESSAGE_MAP()

BOOL CSmbusTab::Create(CWnd* parent, const RECT& rect, UINT id) {
    return CWnd::CreateEx(0, AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                                                 reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr),
                          L"SMBusTab", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          rect, parent, id);
}

void CSmbusTab::Bind(mfc_tool::core::BridgeService* service,
                     std::function<void(const std::wstring&)> logger,
                     mfc_tool::core::PinUsageRegistry* pin_usage,
                     std::function<void()> persist_settings) {
    service_ = service;
    log_ = std::move(logger);
    pin_usage_ = pin_usage;
    persist_settings_ = std::move(persist_settings);
    RefreshPinUsage();
    UpdateEnableState();
}

int CSmbusTab::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    ui_font_.CreatePointFont(85, L"Segoe UI");
    master_group_.Create(L"SMBus Transaction Layer", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(0, 0, 0, 0), this, IDC_SMBUS_MASTER_GROUP);
    profile_label_.Create(L"Profile", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_PROFILE_LABEL);
    profile_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_SMBUS_PROFILE_COMBO);
    port_label_.Create(L"I2C", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_PORT_LABEL);
    port_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_SMBUS_PORT_COMBO);
    pins_label_.Create(L"Pins", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_PINS_LABEL);
    pins_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_SMBUS_PINS_COMBO);
    speed_label_.Create(L"Speed", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_SPEED_LABEL);
    speed_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_SMBUS_SPEED_COMBO);
    addr_label_.Create(L"Addr", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_ADDR_LABEL);
    addr_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, IDC_SMBUS_ADDR_EDIT);
    run_all_delay_label_.Create(L"Delay ms", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_RUN_ALL_DELAY_LABEL);
    run_all_delay_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, IDC_SMBUS_RUN_ALL_DELAY_EDIT);
    run_all_repeat_label_.Create(L"Repeat", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_RUN_ALL_REPEAT_LABEL);
    run_all_repeat_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, IDC_SMBUS_RUN_ALL_REPEAT_EDIT);
    pec_check_.Create(L"PEC", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 0, 0), this, IDC_SMBUS_PEC_CHECK);
    bad_pec_check_.Create(L"Bad PEC", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 0, 0), this, IDC_SMBUS_BAD_PEC_CHECK);
    master_enable_btn_.Create(L"Enable", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_SMBUS_MASTER_ENABLE);
    master_disable_btn_.Create(L"Disable", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_SMBUS_MASTER_DISABLE);
    preset_label_.Create(L"Example", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_PRESET_LABEL);
    preset_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_SMBUS_PRESET_COMBO);
    command_label_.Create(L"Code", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_CMD_LABEL);
    command_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, IDC_SMBUS_CMD_EDIT);
    txn_label_.Create(L"Transaction", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_TXN_LABEL);
    txn_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_SMBUS_TXN_COMBO);
    tx_label_.Create(L"TX HEX", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_TX_LABEL);
    tx_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, IDC_SMBUS_TX_EDIT);
    read_len_label_.Create(L"Read Len", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_READ_LEN_LABEL);
    read_len_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, IDC_SMBUS_READ_LEN_EDIT);
    counter_check_.Create(L"Counter", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 0, 0), this, IDC_SMBUS_COUNTER_CHECK);
    counter_idx_label_.Create(L"Idx", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_COUNTER_IDX_LABEL);
    counter_idx_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, IDC_SMBUS_COUNTER_IDX_EDIT);
    counter_step_label_.Create(L"Step", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_COUNTER_STEP_LABEL);
    counter_step_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, IDC_SMBUS_COUNTER_STEP_EDIT);
    counter_reset_btn_.Create(L"Reset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_SMBUS_COUNTER_RESET);
    execute_btn_.Create(L"Execute", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_SMBUS_EXECUTE);
    run_all_btn_.Create(L"Run All", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_SMBUS_RUN_ALL);
    stop_btn_.Create(L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_SMBUS_STOP);
    progress_.Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_PROGRESS);
    raw_label_.Create(L"Raw RX", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_RAW_LABEL);
    raw_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(0, 0, 0, 0), this, IDC_SMBUS_RAW_EDIT);
    script_label_.Create(L"Script", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_SCRIPT_LABEL);
    script_path_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(0, 0, 0, 0), this, IDC_SMBUS_SCRIPT_PATH);
    script_load_btn_.Create(L"Load", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_SMBUS_SCRIPT_LOAD);
    script_run_btn_.Create(L"Run", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, IDC_SMBUS_SCRIPT_RUN);
    script_select_all_check_.Create(L"Select All", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 0, 0), this, IDC_SMBUS_SCRIPT_SELECT_ALL);
    script_select_all_check_.SetCheck(BST_CHECKED);
    script_list_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL, CRect(0, 0, 0, 0), this, IDC_SMBUS_SCRIPT_LIST);
    script_list_.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
    script_list_.InsertColumn(0, L"#", LVCFMT_RIGHT, 42);
    script_list_.InsertColumn(1, L"Type", LVCFMT_LEFT, 126);
    script_list_.InsertColumn(2, L"Addr", LVCFMT_LEFT, 62);
    script_list_.InsertColumn(3, L"Reg", LVCFMT_LEFT, 62);
    script_list_.InsertColumn(4, L"Data", LVCFMT_LEFT, 150);
    script_list_.InsertColumn(5, L"Read", LVCFMT_RIGHT, 50);
    script_list_.InsertColumn(6, L"Summary", LVCFMT_LEFT, 360);
    result_label_.Create(L"Response", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SMBUS_RESULT_LABEL);
    result_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, CRect(0, 0, 0, 0), this, IDC_SMBUS_RESULT_EDIT);

    CWnd* controls[] = {
        &master_group_, &profile_label_, &profile_combo_, &port_label_, &port_combo_,
        &pins_label_, &pins_combo_, &speed_label_, &speed_combo_, &addr_label_, &addr_edit_,
        &run_all_delay_label_, &run_all_delay_edit_, &run_all_repeat_label_, &run_all_repeat_edit_, &pec_check_, &bad_pec_check_, &master_enable_btn_, &master_disable_btn_, &preset_label_, &preset_combo_,
        &command_label_, &command_edit_, &txn_label_, &txn_combo_, &tx_label_, &tx_edit_, &read_len_label_,
        &read_len_edit_, &counter_check_, &counter_idx_label_, &counter_idx_edit_, &counter_step_label_,
        &counter_step_edit_, &counter_reset_btn_, &execute_btn_, &run_all_btn_, &stop_btn_, &progress_, &raw_label_,
        &raw_edit_, &script_label_, &script_path_edit_, &script_load_btn_, &script_run_btn_,
        &script_select_all_check_, &script_list_, &result_label_, &result_edit_
    };
    for (CWnd* control : controls) {
        control->SetFont(&ui_font_);
    }

    speed_combo_.AddString(L"100000");
    speed_combo_.AddString(L"400000");
    speed_combo_.SetCurSel(0);
    PopulatePortCombo();
    PopulateProfileCombo();
    PopulateTransactionCombo();
    PopulatePresetCombo();
    addr_edit_.SetWindowTextW(L"0x5A");
    run_all_delay_edit_.SetWindowTextW(L"10");
    run_all_repeat_edit_.SetWindowTextW(L"1");
    pec_check_.SetCheck(BST_CHECKED);
    command_edit_.SetWindowTextW(L"0x22");
    tx_edit_.SetWindowTextW(L"12 34");
    read_len_edit_.SetWindowTextW(L"16");
    counter_idx_edit_.SetWindowTextW(L"0");
    counter_step_edit_.SetWindowTextW(L"1");
    mfc_tool::ui::SafeResetProgress(progress_, 1);
    stop_btn_.EnableWindow(FALSE);
    UpdateEnableState();
    return 0;
}

void CSmbusTab::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(CRect(0, 0, cx, cy));
}

void CSmbusTab::LayoutControls(const CRect& r) {
    const int margin = 6;
    const int gap = 6;
    const int row_h = 24;
    const int label_pad = 8;
    const int combo_h = 300;
    const int content_w = (std::max)(320, r.Width() - margin * 2);
    const int inner_left = r.left + margin + 8;
    const int inner_right = r.left + margin + content_w - 10;
    int x = inner_left;
    int y = r.top + margin + 16;

    mfc_tool::ui::SafeMoveWindow(master_group_, r);

    {
        const int enable_w = (std::max)(76, mfc_tool::ui::MeasureButtonMinWidth(master_enable_btn_));
        const int disable_w = (std::max)(76, mfc_tool::ui::MeasureButtonMinWidth(master_disable_btn_));
        const int actions_w = enable_w + gap + disable_w;
        const int actions_x = inner_right - actions_w;
        const int profile_w = 140;
        const int port_w = 66;
        const int pins_x = inner_left +
                           mfc_tool::ui::MeasureControlTextWidth(profile_label_, label_pad) + gap + profile_w + gap +
                           mfc_tool::ui::MeasureControlTextWidth(port_label_, label_pad) + gap + port_w + gap +
                           mfc_tool::ui::MeasureControlTextWidth(pins_label_, label_pad) + gap;
        const int pins_w = (std::max)(160, actions_x - gap - pins_x);

        x = inner_left;
        x = mfc_tool::ui::PlaceLabelAndControl(profile_label_, profile_combo_, x, y + 4, y, profile_w, combo_h, gap, label_pad) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(port_label_, port_combo_, x, y + 4, y, port_w, combo_h, gap, label_pad) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(pins_label_, pins_combo_, x, y + 4, y, pins_w, combo_h, gap, label_pad) + gap;

        x = actions_x;
        mfc_tool::ui::SafeMoveWindow(master_enable_btn_, x, y, enable_w, row_h);
        x += enable_w + gap;
        mfc_tool::ui::SafeMoveWindow(master_disable_btn_, x, y, disable_w, row_h);
    }

    y += row_h + gap;
    x = inner_left;
    x = mfc_tool::ui::PlaceLabelAndControl(speed_label_, speed_combo_, x, y + 4, y, 100, combo_h, gap, label_pad) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(addr_label_, addr_edit_, x, y + 4, y, 76, row_h, gap, label_pad) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(run_all_delay_label_, run_all_delay_edit_, x, y + 4, y, 46, row_h, gap, label_pad) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(run_all_repeat_label_, run_all_repeat_edit_, x, y + 4, y, 38, row_h, gap, label_pad) + gap;
    mfc_tool::ui::SafeMoveWindow(pec_check_, x, y + 2, 70, row_h);
    x += 76;
    mfc_tool::ui::SafeMoveWindow(bad_pec_check_, x, y + 2, 90, row_h);

    y += row_h + gap;
    x = inner_left;
    x = mfc_tool::ui::PlaceLabelAndControl(preset_label_, preset_combo_, x, y + 4, y, 230, combo_h, gap, label_pad) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(command_label_, command_edit_, x, y + 4, y, 70, row_h, gap, label_pad) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(txn_label_, txn_combo_, x, y + 4, y, 170, combo_h, gap, label_pad) + gap;

    y += row_h + gap;
    x = inner_left;
    x = mfc_tool::ui::PlaceLabelAndControl(tx_label_, tx_edit_, x, y + 4, y, 280, row_h, gap, label_pad) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(read_len_label_, read_len_edit_, x, y + 4, y, 64, row_h, gap, label_pad) + gap;
    mfc_tool::ui::SafeMoveWindow(execute_btn_, x, y, 88, row_h);
    x += 94;
    mfc_tool::ui::SafeMoveWindow(run_all_btn_, x, y, 88, row_h);
    x += 94;
    mfc_tool::ui::SafeMoveWindow(stop_btn_, x, y, 72, row_h);
    x += 78;
    mfc_tool::ui::SafeMoveWindow(counter_check_, x, y + 2, 82, row_h);
    x += 88;
    x = mfc_tool::ui::PlaceLabelAndControl(counter_idx_label_, counter_idx_edit_, x, y + 4, y, 46, row_h, gap, label_pad) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(counter_step_label_, counter_step_edit_, x, y + 4, y, 46, row_h, gap, label_pad) + gap;
    mfc_tool::ui::SafeMoveWindow(counter_reset_btn_, x, y, 64, row_h);

    y += row_h + gap;
    x = inner_left;
    mfc_tool::ui::SafeMoveWindow(progress_, x, y + 3, inner_right - inner_left, 18);

    y += row_h + gap;
    x = inner_left;
    x = mfc_tool::ui::PlaceLabelAndControl(raw_label_, raw_edit_, x, y + 4, y,
                                           inner_right - inner_left - 76, row_h, gap, label_pad);

    y += row_h + gap;
    x = inner_left;
    {
        const int load_w = (std::max)(64, mfc_tool::ui::MeasureButtonMinWidth(script_load_btn_));
        const int run_w = (std::max)(58, mfc_tool::ui::MeasureButtonMinWidth(script_run_btn_));
        const int buttons_w = load_w + gap + run_w;
        const int buttons_x = inner_right - buttons_w;
        const int label_w = mfc_tool::ui::MeasureControlTextWidth(script_label_, 8);
        const int path_x = inner_left + label_w + gap;
        const int path_w = (std::max)(160, buttons_x - gap - path_x);

        mfc_tool::ui::SafeMoveWindow(script_label_, inner_left, y + 4, label_w, 18);
        mfc_tool::ui::SafeMoveWindow(script_path_edit_, path_x, y, path_w, row_h);
        mfc_tool::ui::SafeMoveWindow(script_load_btn_, buttons_x, y, load_w, row_h);
        mfc_tool::ui::SafeMoveWindow(script_run_btn_, buttons_x + load_w + gap, y, run_w, row_h);
    }

    y += row_h + gap;
    {
        const int pane_bottom = r.bottom - margin - 8;
        const int pane_h = (std::max)(70, pane_bottom - y);
        const int left_w = (std::max)(260, ((inner_right - inner_left) - gap) / 2);
        const int right_x = inner_left + left_w + gap;
        const int right_w = (std::max)(220, inner_right - right_x);
        const int header_h = row_h;

        mfc_tool::ui::SafeMoveWindow(script_select_all_check_, inner_left, y + 2, 98, header_h);
        mfc_tool::ui::SafeMoveWindow(result_label_, right_x, y + 4, right_w, 18);
        mfc_tool::ui::SafeMoveWindow(script_list_, inner_left, y + header_h, left_w, (std::max)(42, pane_h - header_h));
        mfc_tool::ui::SafeMoveWindow(result_edit_, right_x, y + header_h, right_w, (std::max)(42, pane_h - header_h));
    }
}

void CSmbusTab::SetConnected(bool connected) {
    connected_ = connected;
    if (!connected_) {
        master_enabled_ = false;
    }
    RefreshPinUsage();
    UpdateEnableState();
}

void CSmbusTab::OnDisconnected() {
    master_enabled_ = false;
    run_all_running_ = false;
    script_running_ = false;
    cancel_requested_ = false;
    counter_seeded_ = false;
    RefreshPinUsage();
    UpdateEnableState();
}

void CSmbusTab::LoadState(const mfc_tool::core::AppState& state) {
    if (profile_combo_.SelectString(-1, state.smbus.profile.c_str()) == CB_ERR) {
        profile_combo_.SelectString(-1, ProfileText(Profile::Generic));
    }
    port_combo_.SetCurSel(mfc_tool::core::ParseInt(state.smbus.master_i2c_port) == 1 ? 1 : 0);
    PopulatePinCombo(state.smbus.master_i2c_pins);
    PopulateTransactionCombo();
    PopulatePresetCombo();
    speed_combo_.SelectString(-1, state.smbus.speed_hz.c_str());
    addr_edit_.SetWindowTextW(state.smbus.addr.c_str());
    run_all_delay_edit_.SetWindowTextW(state.smbus.run_all_delay_ms.c_str());
    run_all_repeat_edit_.SetWindowTextW(state.smbus.run_all_repeat_count.c_str());
    pec_check_.SetCheck(state.smbus.pec_enable == L"1" ? BST_CHECKED : BST_UNCHECKED);
    bad_pec_check_.SetCheck(state.smbus.bad_pec_enable == L"1" ? BST_CHECKED : BST_UNCHECKED);
    {
        const std::wstring preset_name = UpgradeLegacyPresetName(state.smbus.command_preset);
        if (preset_combo_.SelectString(-1, preset_name.c_str()) == CB_ERR) {
            PopulatePresetCombo();
        }
    }
    if (txn_combo_.SelectString(-1, state.smbus.transaction.c_str()) == CB_ERR && txn_combo_.GetCount() > 0) {
        txn_combo_.SetCurSel(0);
    }
    command_edit_.SetWindowTextW(state.smbus.command_code.c_str());
    tx_edit_.SetWindowTextW(state.smbus.tx_hex.c_str());
    read_len_edit_.SetWindowTextW(state.smbus.read_len.c_str());
    counter_check_.SetCheck(state.smbus.counter_enable == L"1" ? BST_CHECKED : BST_UNCHECKED);
    counter_idx_edit_.SetWindowTextW(state.smbus.counter_index.c_str());
    counter_step_edit_.SetWindowTextW(state.smbus.counter_step.c_str());
    script_doc_.path = state.smbus.smbus_script_path;
    script_doc_.rows.clear();
    SetScriptPathText();
    PopulateScriptList();
    SetScriptResponseText(L"");
    counter_seeded_ = false;
    ApplyProfileDefaults(false);
    UpdateEnableState();
}

void CSmbusTab::SaveState(mfc_tool::core::AppState* state) const {
    CString text;
    if (state == nullptr) {
        return;
    }
    profile_combo_.GetWindowTextW(text);
    state->smbus.profile = text.GetString();
    state->smbus.master_i2c_port = std::to_wstring(const_cast<CSmbusTab*>(this)->CurrentMasterPort());
    state->smbus.master_i2c_pins = const_cast<CSmbusTab*>(this)->CurrentMasterPinPair().ini_name;
    speed_combo_.GetWindowTextW(text);
    state->smbus.speed_hz = text.GetString();
    state->smbus.addr = const_cast<CSmbusTab*>(this)->GetEditText(addr_edit_);
    state->smbus.run_all_delay_ms = std::to_wstring(const_cast<CSmbusTab*>(this)->RunAllCommandDelayMs());
    state->smbus.run_all_repeat_count = std::to_wstring(const_cast<CSmbusTab*>(this)->RunAllRepeatCount());
    state->smbus.pec_enable = pec_check_.GetCheck() == BST_CHECKED ? L"1" : L"0";
    state->smbus.bad_pec_enable = bad_pec_check_.GetCheck() == BST_CHECKED ? L"1" : L"0";
    txn_combo_.GetWindowTextW(text);
    state->smbus.transaction = text.GetString();
    preset_combo_.GetWindowTextW(text);
    state->smbus.command_preset = text.GetString();
    state->smbus.command_code = const_cast<CSmbusTab*>(this)->GetEditText(command_edit_);
    state->smbus.tx_hex = const_cast<CSmbusTab*>(this)->GetEditText(tx_edit_);
    state->smbus.read_len = const_cast<CSmbusTab*>(this)->GetEditText(read_len_edit_);
    state->smbus.counter_enable = counter_check_.GetCheck() == BST_CHECKED ? L"1" : L"0";
    state->smbus.counter_index = const_cast<CSmbusTab*>(this)->GetEditText(counter_idx_edit_);
    state->smbus.counter_step = const_cast<CSmbusTab*>(this)->GetEditText(counter_step_edit_);
    state->smbus.smbus_script_path = script_doc_.path;
}

void CSmbusTab::PopulateTransactionCombo() {
    txn_combo_.ResetContent();
    if (CurrentProfile() == Profile::UbmController) {
        txn_combo_.AddString(TransactionText(Transaction::UbmControllerRead));
        txn_combo_.AddString(TransactionText(Transaction::UbmControllerWrite));
        txn_combo_.AddString(TransactionText(Transaction::UbmBadChecksumWrite));
        txn_combo_.AddString(TransactionText(Transaction::BusRecover));
        txn_combo_.SelectString(-1, TransactionText(Transaction::UbmControllerRead));
        return;
    }
    for (int i = static_cast<int>(Transaction::QuickWrite); i <= static_cast<int>(Transaction::BlockWriteReadProcessCall); ++i) {
        txn_combo_.AddString(TransactionText(static_cast<Transaction>(i)));
    }
    txn_combo_.AddString(TransactionText(Transaction::BusRecover));
    txn_combo_.SelectString(-1, TransactionText(Transaction::ReadByte));
}

void CSmbusTab::PopulateProfileCombo() {
    profile_combo_.ResetContent();
    profile_combo_.AddString(ProfileText(Profile::Generic));
    profile_combo_.AddString(ProfileText(Profile::UbmController));
    profile_combo_.SelectString(-1, ProfileText(Profile::Generic));
}

void CSmbusTab::PopulatePortCombo() {
    port_combo_.ResetContent();
    int idx = port_combo_.AddString(L"I2C0");
    port_combo_.SetItemData(idx, 0);
    idx = port_combo_.AddString(L"I2C1");
    port_combo_.SetItemData(idx, 1);
    port_combo_.SetCurSel(0);
    PopulatePinCombo();
}

void CSmbusTab::PopulatePinCombo(const std::wstring& preferred_ini_name) {
    const int port = CurrentMasterPort();
    int selected = 0;
    int row = 0;

    pins_combo_.ResetContent();
    const auto& pairs = mfc_tool::core::board_i2c::AllPinPairs();
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (pairs[i].i2c_port != port) {
            continue;
        }
        const int idx = pins_combo_.AddString(pairs[i].label);
        pins_combo_.SetItemData(idx, static_cast<DWORD_PTR>(i));
        if (preferred_ini_name == pairs[i].ini_name) {
            selected = row;
        }
        ++row;
    }
    if (pins_combo_.GetCount() > 0) {
        pins_combo_.SetCurSel(selected);
    }
}

void CSmbusTab::PopulatePresetCombo() {
    const Profile profile = CurrentProfile();
    preset_combo_.ResetContent();
    for (const auto& preset : kSmbusPresets) {
        if (preset.profile == profile) {
            preset_combo_.AddString(preset.name);
        }
    }
    if (preset_combo_.GetCount() > 0) {
        preset_combo_.SetCurSel(0);
        ApplyPresetToCommandUi();
    }
}

void CSmbusTab::OnProfileChanged() {
    ApplyProfileDefaults(true);
    PopulateTransactionCombo();
    PopulatePresetCombo();
    UpdateEnableState();
    if (log_) {
        log_(L"SMBus profile selected: " + std::wstring(ProfileText(CurrentProfile())));
    }
    if (persist_settings_) {
        persist_settings_();
    }
}

void CSmbusTab::OnMasterPortChanged() {
    PopulatePinCombo();
    RefreshPinUsage();
    if (persist_settings_) {
        persist_settings_();
    }
}

void CSmbusTab::OnPresetChanged() {
    ApplyPresetToCommandUi();
}

void CSmbusTab::ApplyPresetToCommandUi() {
    CString text;
    const SmbusPreset* preset = nullptr;
    wchar_t cmd_buf[16] = {};

    preset_combo_.GetWindowTextW(text);
    preset = FindPresetByName(CurrentProfile(), text.GetString());
    if (preset == nullptr) {
        return;
    }

    swprintf_s(cmd_buf, L"0x%02X", static_cast<unsigned int>(preset->command));
    command_edit_.SetWindowTextW(cmd_buf);
    txn_combo_.SelectString(-1, TransactionText(preset->txn));
    tx_edit_.SetWindowTextW(preset->tx_hex);
    read_len_edit_.SetWindowTextW(std::to_wstring(preset->read_len).c_str());
}

void CSmbusTab::UpdateEnableState() {
    if (!::IsWindow(profile_combo_.GetSafeHwnd())) {
        return;
    }
    const BOOL ready = (connected_ && service_ != nullptr);
    const BOOL busy = (run_all_running_ || script_running_) ? TRUE : FALSE;
    const BOOL ubm = IsUbmProfile() ? TRUE : FALSE;
    const BOOL shared_blocked = (pin_usage_ != nullptr &&
        pin_usage_->AnyActiveExcept({L"PMBUS-M", L"CRPS-M", L"TI-UCD-M", L"SMBUS-M", L"I2C0-M", L"I2C0-S", L"I2C1-M", L"I2C1-S", L"FW-UPLOAD-M"}, {kSmbusMasterOwner})) ? TRUE : FALSE;
    const BOOL generic_edit = (ready && !busy && !ubm) ? TRUE : FALSE;
    const BOOL can_switch_profile = (!master_enabled_ && !busy) ? TRUE : FALSE;
    const BOOL can_config_master = (ready && !master_enabled_ && !busy) ? TRUE : FALSE;
    const BOOL can_edit_command = (ready && !busy) ? TRUE : FALSE;
    const BOOL can_use_master = (ready && master_enabled_ && !busy) ? TRUE : FALSE;
    if (pec_check_.GetCheck() != BST_CHECKED || ubm) {
        bad_pec_check_.SetCheck(BST_UNCHECKED);
    }
    if (ubm) {
        counter_check_.SetCheck(BST_UNCHECKED);
    }
    mfc_tool::ui::SafeEnableWindow(profile_combo_, can_switch_profile);
    mfc_tool::ui::SafeEnableWindow(port_combo_, can_config_master);
    mfc_tool::ui::SafeEnableWindow(pins_combo_, can_config_master);
    mfc_tool::ui::SafeEnableWindow(speed_combo_, can_config_master);
    mfc_tool::ui::SafeEnableWindow(addr_edit_, can_config_master);
    mfc_tool::ui::SafeEnableWindow(run_all_delay_edit_, ready && !busy);
    mfc_tool::ui::SafeEnableWindow(run_all_repeat_edit_, ready && !busy);
    mfc_tool::ui::SafeEnableWindow(master_enable_btn_, ready && !master_enabled_ && !shared_blocked && !busy);
    mfc_tool::ui::SafeEnableWindow(master_disable_btn_, ready && master_enabled_ && !busy);
    mfc_tool::ui::SafeEnableWindow(preset_combo_, can_edit_command);
    mfc_tool::ui::SafeEnableWindow(command_edit_, can_edit_command);
    mfc_tool::ui::SafeEnableWindow(txn_combo_, can_edit_command);
    mfc_tool::ui::SafeEnableWindow(tx_edit_, can_edit_command);
    mfc_tool::ui::SafeEnableWindow(read_len_edit_, can_edit_command);
    mfc_tool::ui::SafeEnableWindow(execute_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(run_all_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(stop_btn_, busy ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(pec_check_, generic_edit);
    mfc_tool::ui::SafeEnableWindow(bad_pec_check_, generic_edit && pec_check_.GetCheck() == BST_CHECKED);
    mfc_tool::ui::SetCounterControlsEnabled(counter_check_, counter_idx_edit_, counter_step_edit_, counter_reset_btn_, generic_edit);
    mfc_tool::ui::SafeEnableWindow(script_load_btn_, busy ? FALSE : TRUE);
    mfc_tool::ui::SafeEnableWindow(script_run_btn_, (can_use_master && !script_doc_.rows.empty()) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(script_select_all_check_, busy ? FALSE : TRUE);
    mfc_tool::ui::SafeEnableWindow(script_list_, busy ? FALSE : TRUE);
}

void CSmbusTab::RefreshPinUsage() {
    if (pin_usage_ == nullptr) {
        return;
    }
    const auto& pins = CurrentMasterPinPair();
    pin_usage_->SetLabel(kSmbusMasterOwner, L"SMBus master");
    pin_usage_->SetClaim(kSmbusMasterOwner, {pins.sda_pin, pins.scl_pin});
    pin_usage_->SetActive(kSmbusMasterOwner, master_enabled_);
}

void CSmbusTab::OnMasterEnable() {
    try {
        if (run_all_running_) {
            return;
        }
        if (service_ == nullptr || !connected_) {
            return;
        }
        const int master_port = CurrentMasterPort();
        const auto& pins = CurrentMasterPinPair();
        if (pin_usage_ != nullptr &&
            pin_usage_->AnyActiveExcept({L"PMBUS-M", L"CRPS-M", L"TI-UCD-M", L"SMBUS-M", L"I2C0-M", L"I2C0-S", L"I2C1-M", L"I2C1-S", L"FW-UPLOAD-M"}, {kSmbusMasterOwner})) {
            throw std::runtime_error("Another PMBus/SMBus master is already active. Disable it first.");
        }
        if (pin_usage_ != nullptr &&
            pin_usage_->AnyPinOccupied({pins.sda_pin, pins.scl_pin}, {kSmbusMasterOwner})) {
            throw std::runtime_error("SMBus master requires the selected I2C pins. Disable the conflicting active function first.");
        }
        service_->I2cMasterInit(master_port, pins.sda_pin, pins.scl_pin, CurrentSpeedHz());
        master_enabled_ = true;
        if (log_) {
            log_(L"SMBus master enabled on " + std::wstring(pins.label) + L" at " + std::to_wstring(CurrentSpeedHz()) + L"Hz");
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"SMBus Error", MB_ICONERROR | MB_OK);
    }
    RefreshPinUsage();
    UpdateEnableState();
}

void CSmbusTab::OnMasterDisable() {
    try {
        if (run_all_running_) {
            return;
        }
        if (service_ != nullptr && connected_) {
            service_->I2cDeinit(CurrentMasterPort());
        }
        master_enabled_ = false;
        if (log_) {
            log_(L"SMBus master disabled");
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"SMBus Error", MB_ICONERROR | MB_OK);
    }
    RefreshPinUsage();
    UpdateEnableState();
}

void CSmbusTab::OnExecute() {
    try {
        if (run_all_running_ || service_ == nullptr || !connected_ || !master_enabled_) {
            return;
        }
        ExecResult result = ExecuteSelected();
        SetRawRxText(mfc_tool::core::HexDump(result.raw));
        SetResultText(BuildResultText(result, CurrentTransaction()));
        if (log_) {
            log_(L"SMBus " + CurrentTransactionText() +
                 L" addr=" + GetEditText(addr_edit_) +
                 L" cmd=" + GetEditText(command_edit_) +
                 L" raw=" + mfc_tool::core::HexDump(result.raw));
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"SMBus Execute Error", MB_ICONERROR | MB_OK);
        if (log_) {
            log_(L"SMBus execute failed: " + AnsiToWide(e.what()));
        }
    }
}

CSmbusTab::ExecResult CSmbusTab::ExecuteSelected() {
    const std::uint8_t addr = static_cast<std::uint8_t>(ParseEditInt(addr_edit_) & 0x7F);
    const std::uint8_t command = ParseCommandCode();
    const bool pec = (pec_check_.GetCheck() == BST_CHECKED);
    const bool bad_pec = (bad_pec_check_.GetCheck() == BST_CHECKED);
    const std::vector<std::uint8_t> data = BuildTxPayload(true, true);
    const int read_len = (std::max)(0, ParseEditInt(read_len_edit_));
    const Transaction txn = CurrentTransaction();

    switch (txn) {
    case Transaction::QuickWrite:
        return ExecQuick(addr, false);
    case Transaction::QuickRead:
        return ExecQuick(addr, true);
    case Transaction::SendByte:
        return ExecSendByte(addr, command, pec, bad_pec);
    case Transaction::ReceiveByte:
        return ExecReceiveByte(addr, pec);
    case Transaction::WriteByte:
        if (data.size() != 1u) {
            throw std::invalid_argument("Write Byte requires exactly 1 data byte.");
        }
        return ExecWrite(addr, command, data, pec, bad_pec);
    case Transaction::ReadByte:
        return ExecRead(addr, command, 1, pec);
    case Transaction::WriteWord:
        if (data.size() != 2u) {
            throw std::invalid_argument("Write Word requires exactly 2 data bytes.");
        }
        return ExecWrite(addr, command, data, pec, bad_pec);
    case Transaction::ReadWord:
        return ExecRead(addr, command, 2, pec);
    case Transaction::BlockWrite: {
        std::vector<std::uint8_t> payload;
        if (data.size() > 32u) {
            throw std::invalid_argument("SMBus Block Write supports up to 32 data bytes.");
        }
        payload.push_back(static_cast<std::uint8_t>(data.size() & 0xFFu));
        payload.insert(payload.end(), data.begin(), data.end());
        return ExecWrite(addr, command, payload, pec, bad_pec);
    }
    case Transaction::BlockRead:
        return ExecBlockRead(addr, command, read_len, pec);
    case Transaction::ProcessCall:
        if (data.size() != 2u) {
            throw std::invalid_argument("Process Call requires exactly 2 write data bytes.");
        }
        return ExecProcessCall(addr, command, data, pec);
    case Transaction::BlockWriteReadProcessCall:
        if (data.size() > 32u) {
            throw std::invalid_argument("Block Write-Read Process Call supports up to 32 write data bytes.");
        }
        return ExecBlockWriteReadProcessCall(addr, command, data, read_len, pec);
    case Transaction::UbmControllerRead:
        return ExecUbmControllerRead(addr, command, read_len);
    case Transaction::UbmControllerWrite:
        return ExecUbmControllerWrite(addr, command, data, false);
    case Transaction::UbmBadChecksumWrite:
        return ExecUbmControllerWrite(addr, command, data, true);
    case Transaction::BusRecover: {
        ExecResult result;
        result.raw = service_->I2cMasterBusStatus(CurrentMasterPort(), true);
        result.data = result.raw;
        return result;
    }
    default:
        throw std::invalid_argument("Unsupported SMBus transaction.");
    }
}

CSmbusTab::ExecResult CSmbusTab::ExecuteScriptCommandRow(const mfc_tool::core::SmbusScriptRow& row) {
    const std::uint8_t addr = static_cast<std::uint8_t>(row.address & 0x7F);
    const std::uint8_t command = static_cast<std::uint8_t>(row.command & 0xFF);
    const bool pec = row.pec;
    const int read_len = row.read_length > 0 ? row.read_length : 32;

    if (row.kind != mfc_tool::core::SmbusScriptRowKind::Command) {
        return {};
    }
    if (row.address < 0) {
        throw std::invalid_argument("script command row requires a slave address.");
    }
    if (row.profile == mfc_tool::core::SmbusScriptProfile::SmbusUbm) {
        if (row.command < 0) {
            throw std::invalid_argument("UBM script row requires a register/command byte.");
        }
        switch (row.command_type) {
        case mfc_tool::core::SmbusScriptCommandType::ReadByte:
            return ExecUbmControllerRead(addr, command, 1);
        case mfc_tool::core::SmbusScriptCommandType::ReadWord:
            return ExecUbmControllerRead(addr, command, 2);
        case mfc_tool::core::SmbusScriptCommandType::Read32:
            return ExecUbmControllerRead(addr, command, 4);
        case mfc_tool::core::SmbusScriptCommandType::BlockRead:
            return ExecUbmControllerRead(addr, command, read_len);
        case mfc_tool::core::SmbusScriptCommandType::SendByte:
        case mfc_tool::core::SmbusScriptCommandType::WriteByte:
        case mfc_tool::core::SmbusScriptCommandType::WriteWord:
        case mfc_tool::core::SmbusScriptCommandType::BlockWrite:
            return ExecUbmControllerWrite(addr, command, row.data, false);
        case mfc_tool::core::SmbusScriptCommandType::BadChecksumWrite:
            return ExecUbmControllerWrite(addr, command, row.data, true);
        default:
            throw std::invalid_argument("unsupported UBM script command type.");
        }
    }

    switch (row.command_type) {
    case mfc_tool::core::SmbusScriptCommandType::QuickWrite:
        return ExecQuick(addr, false);
    case mfc_tool::core::SmbusScriptCommandType::QuickRead:
        return ExecQuick(addr, true);
    case mfc_tool::core::SmbusScriptCommandType::SendByte:
        if (row.command < 0) {
            throw std::invalid_argument("SendByte script row requires a register/command byte.");
        }
        return ExecSendByte(addr, command, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::ReceiveByte:
        return ExecReceiveByte(addr, pec);
    case mfc_tool::core::SmbusScriptCommandType::WriteByte:
        if (row.command < 0 || row.data.size() != 1u) {
            throw std::invalid_argument("WriteByte script row requires register and 1 data byte.");
        }
        return ExecWrite(addr, command, row.data, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::WriteWord:
        if (row.command < 0 || row.data.size() != 2u) {
            throw std::invalid_argument("WriteWord script row requires register and 2 data bytes.");
        }
        return ExecWrite(addr, command, row.data, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::ReadByte:
        if (row.command < 0) {
            throw std::invalid_argument("ReadByte script row requires a register/command byte.");
        }
        return ExecRead(addr, command, 1, pec);
    case mfc_tool::core::SmbusScriptCommandType::ReadWord:
        if (row.command < 0) {
            throw std::invalid_argument("ReadWord script row requires a register/command byte.");
        }
        return ExecRead(addr, command, 2, pec);
    case mfc_tool::core::SmbusScriptCommandType::Read32:
        if (row.command < 0) {
            throw std::invalid_argument("Read32 script row requires a register/command byte.");
        }
        return ExecRead(addr, command, 4, pec);
    case mfc_tool::core::SmbusScriptCommandType::BlockWrite:
        if (row.command < 0 || row.data.size() > 32u) {
            throw std::invalid_argument("BlockWrite script row requires register and up to 32 data bytes.");
        }
        {
            std::vector<std::uint8_t> payload = {static_cast<std::uint8_t>(row.data.size() & 0xFFu)};
            payload.insert(payload.end(), row.data.begin(), row.data.end());
            return ExecWrite(addr, command, payload, pec, false);
        }
    case mfc_tool::core::SmbusScriptCommandType::BlockRead:
        if (row.command < 0) {
            throw std::invalid_argument("BlockRead script row requires a register/command byte.");
        }
        return ExecBlockRead(addr, command, read_len, pec);
    case mfc_tool::core::SmbusScriptCommandType::ProcessCall:
        if (row.command < 0 || row.data.size() != 2u) {
            throw std::invalid_argument("ProcessCall script row requires register and 2 data bytes.");
        }
        return ExecProcessCall(addr, command, row.data, pec);
    case mfc_tool::core::SmbusScriptCommandType::BlockWriteReadProcessCall:
        if (row.command < 0 || row.data.size() > 32u) {
            throw std::invalid_argument("BlockWriteReadProcessCall script row requires register and up to 32 data bytes.");
        }
        return ExecBlockWriteReadProcessCall(addr, command, row.data, read_len, pec);
    case mfc_tool::core::SmbusScriptCommandType::BusRecover: {
        ExecResult result;
        result.raw = service_->I2cMasterBusStatus(CurrentMasterPort(), true);
        result.data = result.raw;
        return result;
    }
    case mfc_tool::core::SmbusScriptCommandType::BadPecWriteByte:
        if (row.command < 0 || row.data.size() != 1u) {
            throw std::invalid_argument("BadPecWriteByte script row requires register and 1 data byte.");
        }
        try {
            return ExecWrite(addr, command, row.data, true, true);
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (!IsExpectedBadPecBridgeStatus(e.status())) {
                throw;
            }
            ExecResult result;
            result.raw = {command};
            result.raw.insert(result.raw.end(), row.data.begin(), row.data.end());
            AppendWritePec(&result.raw, addr, true);
            result.data = {e.status()};
            return result;
        }
    default:
        throw std::invalid_argument("unsupported SMBus script command type.");
    }
}

CSmbusTab::ExecResult CSmbusTab::ExecQuick(std::uint8_t addr, bool read_bit) {
    ExecResult result;
    result.raw = service_->I2cMasterSmbusQuick(CurrentMasterPort(), addr, read_bit);
    result.data = result.raw;
    if (result.raw.size() >= 4u) {
        result.ack = (result.raw[3] != 0u);
    }
    if (!result.ack) {
        throw std::runtime_error("SMBus Quick command was NACKed");
    }
    SleepAfterRunAllCommand();
    return result;
}

CSmbusTab::ExecResult CSmbusTab::ExecReceiveByte(std::uint8_t addr, bool pec) {
    ExecResult result;
    std::vector<std::uint8_t> rx = service_->I2cMasterRead(CurrentMasterPort(), addr, pec ? 2 : 1);
    const int count = rx.empty() ? 0 : static_cast<int>(rx[0]);
    const size_t size = (std::min)(static_cast<size_t>(count), rx.size() > 1 ? rx.size() - 1 : static_cast<size_t>(0));
    if (size > 0u) {
        result.raw.assign(rx.begin() + 1, rx.begin() + 1 + size);
        result.data = result.raw;
    }
    if (pec && result.raw.size() >= 2u) {
        result.pec_checked = true;
        result.pec_rx = result.raw.back();
        result.data.pop_back();
        std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>((addr << 1) | 1u)};
        frame.insert(frame.end(), result.data.begin(), result.data.end());
        result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
        result.pec_ok = (result.pec_calc == result.pec_rx);
    }
    SleepAfterRunAllCommand();
    return result;
}

CSmbusTab::ExecResult CSmbusTab::ExecSendByte(std::uint8_t addr, std::uint8_t command, bool pec, bool force_bad_pec) {
    ExecResult result;
    int attempt = 0;

    for (attempt = 0; attempt < kSmbusRetryAttempts; ++attempt) {
        std::vector<std::uint8_t> tx = {command};
        if (pec) {
            AppendWritePec(&tx, addr, force_bad_pec);
        }
        try {
            service_->I2cMasterWrite(CurrentMasterPort(), addr, tx);
            result.raw = tx;
            SleepAfterRunAllCommand();
            return result;
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (!force_bad_pec && e.status() == 0x04u && attempt < (kSmbusRetryAttempts - 1)) {
                (void)service_->I2cMasterBusStatus(CurrentMasterPort(), true);
                ::Sleep(kSmbusRetryDelayMs);
                continue;
            }
            throw;
        }
    }
    return result;
}

CSmbusTab::ExecResult CSmbusTab::ExecWrite(std::uint8_t addr, std::uint8_t command, const std::vector<std::uint8_t>& data, bool pec, bool force_bad_pec) {
    ExecResult result;
    int attempt = 0;

    for (attempt = 0; attempt < kSmbusRetryAttempts; ++attempt) {
        std::vector<std::uint8_t> tx = {command};
        tx.insert(tx.end(), data.begin(), data.end());
        if (pec) {
            AppendWritePec(&tx, addr, force_bad_pec);
        }
        try {
            service_->I2cMasterWrite(CurrentMasterPort(), addr, tx);
            result.raw = tx;
            SleepAfterRunAllCommand();
            return result;
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (!force_bad_pec && e.status() == 0x04u && attempt < (kSmbusRetryAttempts - 1)) {
                (void)service_->I2cMasterBusStatus(CurrentMasterPort(), true);
                ::Sleep(kSmbusRetryDelayMs);
                continue;
            }
            throw;
        }
    }
    return result;
}

CSmbusTab::ExecResult CSmbusTab::ExecRead(std::uint8_t addr, std::uint8_t command, int read_len, bool pec) {
    ExecResult result;
    int attempt = 0;
    for (attempt = 0; attempt < kSmbusRetryAttempts; ++attempt) {
        try {
            result.raw = service_->I2cMasterWriteThenReadRaw(CurrentMasterPort(), addr, true, {command}, read_len + (pec ? 1 : 0));
            result.data = result.raw;
            if (pec && !result.raw.empty()) {
                result.pec_checked = true;
                result.pec_rx = result.raw.back();
                result.data.pop_back();
                std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1), command, static_cast<std::uint8_t>((addr << 1) | 1u)};
                frame.insert(frame.end(), result.data.begin(), result.data.end());
                result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
                result.pec_ok = (result.pec_calc == result.pec_rx);
            }
            if (result.pec_checked && !result.pec_ok && attempt < (kSmbusRetryAttempts - 1)) {
                ::Sleep(kSmbusRetryDelayMs);
                continue;
            }
            SleepAfterRunAllCommand();
            return result;
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (e.status() == 0x04u && attempt < (kSmbusRetryAttempts - 1)) {
                ::Sleep(kSmbusRetryDelayMs);
                continue;
            }
            throw;
        }
    }
    return result;
}

CSmbusTab::ExecResult CSmbusTab::ExecBlockRead(std::uint8_t addr, std::uint8_t command, int max_read_len, bool pec) {
    ExecResult result;
    const int capped = (std::max)(1, (std::min)(max_read_len, 32));
    result.raw = service_->I2cMasterPmbusBlockReadRaw(CurrentMasterPort(), addr, {command}, pec, capped);
    result.data = result.raw;
    if (pec && result.data.size() >= 2u) {
        result.pec_checked = true;
        result.pec_rx = result.data.back();
        result.data.pop_back();
    }
    if (!result.data.empty()) {
        const std::uint8_t count = result.data[0];
        if (result.data.size() != (1u + static_cast<size_t>(count))) {
            throw std::runtime_error("SMBus block read count mismatch");
        }
        if (pec) {
            std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1), command, static_cast<std::uint8_t>((addr << 1) | 1u)};
            frame.insert(frame.end(), result.data.begin(), result.data.end());
            result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
            result.pec_ok = (result.pec_calc == result.pec_rx);
        }
        result.data.erase(result.data.begin());
    }
    SleepAfterRunAllCommand();
    return result;
}

CSmbusTab::ExecResult CSmbusTab::ExecProcessCall(std::uint8_t addr, std::uint8_t command, const std::vector<std::uint8_t>& data, bool pec) {
    ExecResult result;
    std::vector<std::uint8_t> tx = {command, data[0], data[1]};
    result.raw = service_->I2cMasterWriteThenReadRaw(CurrentMasterPort(), addr, true, tx, 2 + (pec ? 1 : 0));
    result.data = result.raw;
    if (pec && !result.raw.empty()) {
        result.pec_checked = true;
        result.pec_rx = result.raw.back();
        result.data.pop_back();
        std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1), command, data[0], data[1], static_cast<std::uint8_t>((addr << 1) | 1u)};
        frame.insert(frame.end(), result.data.begin(), result.data.end());
        result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
        result.pec_ok = (result.pec_calc == result.pec_rx);
    }
    SleepAfterRunAllCommand();
    return result;
}

CSmbusTab::ExecResult CSmbusTab::ExecBlockWriteReadProcessCall(std::uint8_t addr, std::uint8_t command, const std::vector<std::uint8_t>& data, int max_read_len, bool pec) {
    ExecResult result;
    std::vector<std::uint8_t> tx = {command, static_cast<std::uint8_t>(data.size() & 0xFFu)};
    const size_t max_payload = static_cast<size_t>((std::max)(0, max_read_len));
    const int read_request_len = 1 + static_cast<int>(max_payload) + (pec ? 1 : 0);
    tx.insert(tx.end(), data.begin(), data.end());
    result.raw = service_->I2cMasterWriteThenReadRaw(CurrentMasterPort(), addr, true, tx, read_request_len);
    result.data = result.raw;
    if (result.data.empty()) {
        throw std::runtime_error("SMBus block process response missing count byte");
    }

    const std::uint8_t count = result.data[0];
    const size_t payload_end = 1u + static_cast<size_t>(count);
    if (static_cast<size_t>(count) > max_payload) {
        throw std::runtime_error("SMBus block process response count exceeds requested length");
    }
    if (result.raw.size() < payload_end + (pec ? 1u : 0u)) {
        throw std::runtime_error("SMBus block process response shorter than count byte");
    }
    if (pec) {
        result.pec_checked = true;
        result.pec_rx = result.raw[payload_end];
        std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1), command, static_cast<std::uint8_t>(data.size() & 0xFFu)};
        frame.insert(frame.end(), data.begin(), data.end());
        frame.push_back(static_cast<std::uint8_t>((addr << 1) | 1u));
        frame.insert(frame.end(), result.raw.begin(), result.raw.begin() + payload_end);
        result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
        result.pec_ok = (result.pec_calc == result.pec_rx);
    }
    if (result.data.size() > payload_end) {
        result.data.resize(payload_end);
    }
    result.data.erase(result.data.begin());
    SleepAfterRunAllCommand();
    return result;
}

CSmbusTab::ExecResult CSmbusTab::ExecUbmControllerRead(std::uint8_t addr, std::uint8_t command, int read_len) {
    ExecResult result;
    std::vector<std::uint8_t> command_frame = {static_cast<std::uint8_t>(addr << 1), command};
    std::vector<std::uint8_t> tx = {command, UbmChecksum(command_frame)};
    const int length = (std::max)(1, read_len);

    result.raw = service_->I2cMasterWriteThenReadRaw(CurrentMasterPort(), addr, true, tx, length + 1);
    result.data = result.raw;
    if (result.raw.size() >= 1u) {
        result.ubm_checksum_checked = true;
        result.ubm_checksum_rx = result.raw.back();
        result.data.pop_back();
        result.ubm_checksum_calc = UbmChecksum(result.data);
        result.ubm_checksum_ok = (result.ubm_checksum_rx == result.ubm_checksum_calc);
    }
    SleepAfterRunAllCommand();
    return result;
}

CSmbusTab::ExecResult CSmbusTab::ExecUbmControllerWrite(std::uint8_t addr, std::uint8_t command, const std::vector<std::uint8_t>& data, bool force_bad_checksum) {
    ExecResult result;
    std::vector<std::uint8_t> tx = {command};
    std::vector<std::uint8_t> checksum_frame = {static_cast<std::uint8_t>(addr << 1), command};
    std::uint8_t checksum = 0;

    tx.insert(tx.end(), data.begin(), data.end());
    checksum_frame.insert(checksum_frame.end(), data.begin(), data.end());
    checksum = UbmChecksum(checksum_frame);
    if (force_bad_checksum) {
        checksum = static_cast<std::uint8_t>(checksum ^ 0xFFu);
    }
    tx.push_back(checksum);
    service_->I2cMasterWrite(CurrentMasterPort(), addr, tx);
    result.raw = tx;
    result.data = data;
    SleepAfterRunAllCommand();
    return result;
}

std::wstring CSmbusTab::BuildResultText(const ExecResult& result, Transaction txn) const {
    std::wstringstream ss;
    ss << TransactionText(txn);
    if (txn == Transaction::QuickWrite || txn == Transaction::QuickRead) {
        ss << L" | ACK=" << (result.ack ? L"1" : L"0");
    } else {
        ss << L" | data=" << mfc_tool::core::HexDump(result.data);
    }
    ss << PecText(result);
    ss << UbmChecksumText(result);
    return ss.str();
}

void CSmbusTab::OnScriptLoad() {
    CFileDialog dlg(TRUE, L"csv", nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    L"CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) {
        return;
    }
    try {
        LoadScriptFromPath(dlg.GetPathName().GetString());
        SetScriptResponseText(L"Script loaded: " + script_doc_.path);
        if (log_) {
            log_(L"SMBus script loaded: " + script_doc_.path);
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"SMBus Script", MB_ICONERROR | MB_OK);
    }
    UpdateEnableState();
}

void CSmbusTab::OnScriptSelectAll() {
    const bool checked = script_select_all_check_.GetCheck() == BST_CHECKED;
    script_list_updating_ = true;
    script_list_.SetRedraw(FALSE);
    for (size_t i = 0u; i < script_doc_.rows.size(); ++i) {
        script_doc_.rows[i].selected = checked;
        if (i < static_cast<size_t>(script_list_.GetItemCount())) {
            script_list_.SetCheck(static_cast<int>(i), checked ? TRUE : FALSE);
        }
    }
    script_list_.SetRedraw(TRUE);
    script_list_.Invalidate(FALSE);
    script_list_updating_ = false;
    UpdateScriptSummary();
}

void CSmbusTab::OnScriptListChanged(NMHDR* pNMHDR, LRESULT* pResult) {
    if (pResult != nullptr) {
        *pResult = 0;
    }
    const auto* item = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if (item == nullptr || script_running_ || script_list_updating_) {
        return;
    }
    if ((item->uChanged & LVIF_STATE) == 0u) {
        return;
    }
    const int idx = item->iItem;
    if (idx >= 0 && idx < static_cast<int>(script_doc_.rows.size())) {
        script_doc_.rows[static_cast<size_t>(idx)].selected = script_list_.GetCheck(idx) == TRUE;
        UpdateScriptSummary();
    }
}

void CSmbusTab::OnScriptRun() {
    if (script_running_ || service_ == nullptr || !connected_ || !master_enabled_) {
        return;
    }
    if (script_doc_.rows.empty() && !script_doc_.path.empty()) {
        try {
            LoadScriptFromPath(script_doc_.path);
        } catch (const std::exception& e) {
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"SMBus Script", MB_ICONERROR | MB_OK);
            return;
        }
    }
    if (script_doc_.rows.empty()) {
        ::MessageBoxW(m_hWnd, L"Load a script CSV before running.", L"SMBus Script", MB_ICONWARNING | MB_OK);
        return;
    }

    ResetCancel();
    script_running_ = true;
    UpdateEnableState();
    try {
        size_t command_count = 0u;
        size_t pause_count = 0u;
        size_t read_count = 0u;
        size_t write_count = 0u;
        size_t selected_count = 0u;
        size_t progress_done = 0u;

        for (const auto& row : script_doc_.rows) {
            if (row.selected &&
                (row.kind == mfc_tool::core::SmbusScriptRowKind::Pause ||
                 row.kind == mfc_tool::core::SmbusScriptRowKind::Command)) {
                ++selected_count;
            }
        }
        if (selected_count == 0u) {
            ::MessageBoxW(m_hWnd, L"Select at least one executable script row before running.", L"SMBus Script", MB_ICONWARNING | MB_OK);
            script_running_ = false;
            UpdateEnableState();
            return;
        }

        SetScriptResponseText(L"Script running done=0/" + std::to_wstring(selected_count) +
                              L" remaining=" + std::to_wstring(selected_count));
        mfc_tool::ui::SafeResetProgress(progress_, static_cast<int>(selected_count));
        FlushUiUpdates();
        if (log_) {
            log_(L"SMBus script run started: " + script_doc_.path);
        }
        for (size_t index = 0u; index < script_doc_.rows.size(); ++index) {
            const auto& row = script_doc_.rows[index];
            if (!row.selected) {
                continue;
            }
            bool row_executed = false;
            if (row.kind == mfc_tool::core::SmbusScriptRowKind::Pause) {
                ThrowIfCancelRequested();
                ++pause_count;
                if (row.delay_ms > 0) {
                    SleepWithCancel((std::min)(row.delay_ms, 10000));
                }
                ++progress_done;
                row_executed = true;
            } else if (row.kind == mfc_tool::core::SmbusScriptRowKind::Command) {
                ThrowIfCancelRequested();
                ExecResult result = ExecuteScriptCommandRow(row);
                ++command_count;
                if (mfc_tool::core::SmbusScriptRowIsRead(row)) {
                    ++read_count;
                    AppendScriptResponse(L"#" + std::to_wstring(index + 1u) + L" " +
                                         mfc_tool::core::SmbusScriptRowSummary(row));
                    AppendScriptResponse(L"  raw=" + mfc_tool::core::HexDump(result.raw));
                    AppendScriptResponse(L"  data=" + mfc_tool::core::HexDump(result.data) + PecText(result) + UbmChecksumText(result));
                }
                if (mfc_tool::core::SmbusScriptRowIsWrite(row)) {
                    ++write_count;
                }
                SetRawRxText(mfc_tool::core::HexDump(result.raw));
                if (row.delay_ms > 0) {
                    SleepWithCancel((std::min)(row.delay_ms, 10000));
                }
                ++progress_done;
                row_executed = true;
            }
            if (row_executed) {
                mfc_tool::ui::SafeSetProgressPos(progress_, static_cast<int>(progress_done));
                if ((progress_done % 10u) == 0u || progress_done == selected_count || index + 1u == script_doc_.rows.size()) {
                    AppendScriptResponse(L"Progress done=" + std::to_wstring(progress_done) +
                                         L"/" + std::to_wstring(selected_count) +
                                         L" remaining=" + std::to_wstring(selected_count - progress_done));
                }
                FlushUiUpdates();
            }
        }
        mfc_tool::ui::SafeSetProgressPos(progress_, static_cast<int>(selected_count));
        AppendScriptResponse(L"Complete commands=" + std::to_wstring(command_count) +
                             L" pauses=" + std::to_wstring(pause_count) +
                             L" reads=" + std::to_wstring(read_count) +
                             L" writes=" + std::to_wstring(write_count));
        if (log_) {
            log_(L"SMBus script run complete: commands=" + std::to_wstring(command_count) +
                 L", pauses=" + std::to_wstring(pause_count) +
                 L", reads=" + std::to_wstring(read_count) +
                 L", writes=" + std::to_wstring(write_count));
        }
    } catch (const UserCancelled&) {
        AppendScriptResponse(L"Stopped by user.");
        if (log_) {
            log_(L"SMBus script run stopped by user.");
        }
    } catch (const std::exception& e) {
        const std::wstring msg = AnsiToWide(e.what());
        if (log_) {
            log_(L"SMBus script run failed: " + msg);
        }
        ::MessageBoxW(m_hWnd, msg.c_str(), L"SMBus Script", MB_ICONERROR | MB_OK);
    }
    script_running_ = false;
    cancel_requested_ = false;
    UpdateEnableState();
}

void CSmbusTab::OnRunAll() {
    if (run_all_running_ || service_ == nullptr || !connected_ || !master_enabled_) {
        return;
    }
    ResetCancel();
    mfc_tool::ui::ScopedBusyState busy(run_all_running_, [this]() {
        UpdateEnableState();
        FlushUiUpdates();
    });
    try {
        switch (CurrentProfile()) {
        case Profile::UbmController:
            RunUbmControllerAll();
            break;
        case Profile::Generic:
        default:
            RunGenericAll();
            break;
        }
    } catch (const UserCancelled&) {
        SetResultText(L"SMBus Run All stopped by user.");
        if (log_) {
            log_(L"SMBus Run All stopped by user.");
        }
    } catch (const std::exception& e) {
        const std::wstring msg = AnsiToWide(e.what());
        SetResultText(L"SMBus Run All failed: " + msg);
        if (log_) {
            log_(L"SMBus Run All failed: " + msg);
        }
        busy.Reset();
        ::MessageBoxW(m_hWnd, msg.c_str(), L"SMBus Run All Error", MB_ICONERROR | MB_OK);
    }
    cancel_requested_ = false;
    busy.Reset();
}

void CSmbusTab::OnStop() {
    RequestCancel();
}

void CSmbusTab::RunGenericAll() {
    int pass = 0;
    int fail = 0;
    int step = 0;
    int current_loop = 1;
    const int repeat_count = RunAllRepeatCount();
    const int per_loop_total = 34;
    const int total = per_loop_total * repeat_count;
    std::uint8_t run_counter = 0;
    bool stop_run = false;

    mfc_tool::ui::SafeResetProgress(progress_, total);
    SetResultText(L"Running SMBus example transaction validation, loop 1/" + std::to_wstring(repeat_count) + L"...");
    FlushUiUpdates();

    auto run = [&](const wchar_t* label, Transaction txn, std::uint8_t command, std::vector<std::uint8_t> data, int read_len, bool pec, bool bad_pec) {
        std::wstring display_label = label;
        if (stop_run) {
            return;
        }
        if (repeat_count > 1) {
            display_label += L" (loop " + std::to_wstring(current_loop) + L"/" + std::to_wstring(repeat_count) + L")";
        }
        ThrowIfCancelRequested();
        if (!RunOneAllTest(display_label.c_str(), txn, command, data, read_len, pec, bad_pec, &pass, &fail)) {
            stop_run = true;
            if (log_) {
                log_(L"SMBus Run All stopped after transport failure at " + display_label);
            }
        }
        ++step;
        mfc_tool::ui::SafeSetProgressPos(progress_, step);
        FlushUiUpdates();
        ThrowIfCancelRequested();
    };

    auto counted = [&run_counter](std::initializer_list<std::uint8_t> prefix, std::uint8_t start) {
        std::vector<std::uint8_t> data(prefix.begin(), prefix.end());
        data.push_back(static_cast<std::uint8_t>(start + run_counter));
        run_counter = static_cast<std::uint8_t>(run_counter + 1u);
        return data;
    };

    for (current_loop = 1; current_loop <= repeat_count; ++current_loop) {
        if (log_) {
            log_(L"SMBus Run All loop " + std::to_wstring(current_loop) + L"/" + std::to_wstring(repeat_count) + L" started");
        }
        SetResultText(L"Running SMBus example transaction validation, loop " +
                      std::to_wstring(current_loop) + L"/" + std::to_wstring(repeat_count) + L"...");
        FlushUiUpdates();

        run(L"Example Quick Write", Transaction::QuickWrite, 0x00u, {}, 0, false, false);
        run(L"Example Quick Read", Transaction::QuickRead, 0x00u, {}, 0, false, false);
        run(L"Example Send Byte A", Transaction::SendByte, 0x10u, {}, 0, true, false);
        run(L"Example Send Byte B", Transaction::SendByte, 0x11u, {}, 0, true, false);
        run(L"Example Receive Select A", Transaction::SendByte, 0x12u, {}, 0, true, false);
        run(L"Example Receive Byte A", Transaction::ReceiveByte, 0x00u, {}, 1, true, false);
        run(L"Example Receive Select B", Transaction::SendByte, 0x13u, {}, 0, true, false);
        run(L"Example Receive Byte B", Transaction::ReceiveByte, 0x00u, {}, 1, true, false);
        run(L"Stress Back-to-back Receive Select A", Transaction::SendByte, 0x12u, {}, 0, true, false);
        run(L"Stress Back-to-back Receive Byte A", Transaction::ReceiveByte, 0x00u, {}, 1, true, false);
        run(L"Example Write Byte A", Transaction::WriteByte, 0x20u, {run_counter++}, 0, true, false);
        run(L"Example Write Byte B", Transaction::WriteByte, 0x21u, {static_cast<std::uint8_t>(0x80u + run_counter++)}, 0, true, false);
        run(L"Example Read Byte A", Transaction::ReadByte, 0x22u, {}, 1, true, false);
        run(L"Example Read Byte B", Transaction::ReadByte, 0x23u, {}, 1, true, false);
        run(L"Example Write Word A", Transaction::WriteWord, 0x30u, counted({0x34u}, 0x00u), 0, true, false);
        run(L"Example Write Word B", Transaction::WriteWord, 0x31u, counted({0xCDu}, 0x80u), 0, true, false);
        run(L"Example Read Word A", Transaction::ReadWord, 0x32u, {}, 2, true, false);
        run(L"Example Read Word B", Transaction::ReadWord, 0x33u, {}, 2, true, false);
        run(L"Example Block Write A", Transaction::BlockWrite, 0x40u, counted({0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u}, 0x00u), 0, true, false);
        run(L"Example Block Write B", Transaction::BlockWrite, 0x41u, counted({0x20u, 0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u, 0x28u, 0x29u, 0x2Au, 0x2Bu, 0x2Cu, 0x2Du, 0x2Eu}, 0x80u), 0, true, false);
        run(L"Example Block Read A", Transaction::BlockRead, 0x42u, {}, 8, true, false);
        run(L"Example Block Read B", Transaction::BlockRead, 0x43u, {}, 16, true, false);
        run(L"Stress After Block Read B Select A", Transaction::SendByte, 0x12u, {}, 0, true, false);
        run(L"Stress After Block Read B Receive A", Transaction::ReceiveByte, 0x00u, {}, 1, true, false);
        run(L"Example Process Call A", Transaction::ProcessCall, 0x50u, counted({0x34u}, 0x00u), 2, true, false);
        run(L"Example Process Call B", Transaction::ProcessCall, 0x51u, counted({0xCDu}, 0x80u), 2, true, false);
        run(L"Example Block Process A", Transaction::BlockWriteReadProcessCall, 0x60u, counted({0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u}, 0x00u), 8, true, false);
        run(L"Example Block Process B", Transaction::BlockWriteReadProcessCall, 0x61u, counted({0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u, 0x19u, 0x1Au, 0x1Bu, 0x1Cu, 0x1Du, 0x1Eu, 0x1Fu}, 0x80u), 16, true, false);
        run(L"Stress After Block Process B Select A", Transaction::SendByte, 0x12u, {}, 0, true, false);
        run(L"Stress After Block Process B Receive A", Transaction::ReceiveByte, 0x00u, {}, 1, true, false);
        run(L"Example Bad PEC Write Byte", Transaction::WriteByte, 0x20u, {static_cast<std::uint8_t>(0x40u + run_counter++)}, 0, true, true);
        run(L"Stress After Bad PEC Select A", Transaction::SendByte, 0x12u, {}, 0, true, false);
        run(L"Stress After Bad PEC Receive A", Transaction::ReceiveByte, 0x00u, {}, 1, true, false);
        run(L"Bus Recover Preflight", Transaction::BusRecover, 0x00u, {}, 0, false, false);
        if (stop_run) {
            break;
        }
    }

    {
        std::wstringstream ss;
        if (stop_run) {
            ss << L"SMBus Run All stopped: PASS=" << pass << L" FAIL=" << fail;
        } else {
            ss << L"SMBus Run All complete: loops=" << repeat_count << L" PASS=" << pass << L" FAIL=" << fail;
        }
        SetResultText(ss.str());
        if (log_) {
            log_(ss.str());
        }
    }
}

void CSmbusTab::RunUbmControllerAll() {
    int pass = 0;
    int fail = 0;
    int step = 0;
    int current_loop = 1;
    const int repeat_count = RunAllRepeatCount();
    const int per_loop_total = 32;
    const int total = per_loop_total * repeat_count;
    std::vector<std::uint8_t> change_count;

    mfc_tool::ui::SafeResetProgress(progress_, total);
    SetResultText(L"Running UBM Controller validation, loop 1/" + std::to_wstring(repeat_count) + L"...");
    FlushUiUpdates();

    auto run = [&](const wchar_t* label, Transaction txn, std::uint8_t command, std::vector<std::uint8_t> data, int read_len, bool bad_checksum) {
        std::wstring display_label = label;
        if (repeat_count > 1) {
            display_label += L" (loop " + std::to_wstring(current_loop) + L"/" + std::to_wstring(repeat_count) + L")";
        }
        ThrowIfCancelRequested();
        RunOneUbmTest(display_label.c_str(), txn, command, data, read_len, bad_checksum, &pass, &fail);
        ++step;
        mfc_tool::ui::SafeSetProgressPos(progress_, step);
        FlushUiUpdates();
        ThrowIfCancelRequested();
    };

    auto run_expect_byte = [&](const wchar_t* label, std::uint8_t command, std::uint8_t expected) {
        std::wstring display_label = label;
        if (repeat_count > 1) {
            display_label += L" (loop " + std::to_wstring(current_loop) + L"/" + std::to_wstring(repeat_count) + L")";
        }
        ThrowIfCancelRequested();
        try {
            const std::uint8_t addr = static_cast<std::uint8_t>(ParseEditInt(addr_edit_) & 0x7F);
            ExecResult result = ExecUbmControllerRead(addr, command, 1);
            if (result.ubm_checksum_checked && !result.ubm_checksum_ok) {
                throw std::runtime_error("UBM checksum mismatch");
            }
            if (result.data.empty() || result.data[0] != expected) {
                std::ostringstream oss;
                oss << "expected 0x" << std::uppercase << std::hex << static_cast<unsigned int>(expected);
                if (!result.data.empty()) {
                    oss << ", got 0x" << static_cast<unsigned int>(result.data[0]);
                }
                throw std::runtime_error(oss.str());
            }
            ++pass;
            if (log_) {
                log_(L"SMBus PASS " + display_label + L" raw=" + mfc_tool::core::HexDump(result.raw));
            }
        } catch (const std::exception& e) {
            ++fail;
            if (log_) {
                log_(L"SMBus FAIL " + display_label + L": " + AnsiToWide(e.what()));
            }
        }
        ++step;
        mfc_tool::ui::SafeSetProgressPos(progress_, step);
        FlushUiUpdates();
        ThrowIfCancelRequested();
    };

    for (current_loop = 1; current_loop <= repeat_count; ++current_loop) {
        change_count.clear();
        if (log_) {
            log_(L"UBM Controller Run All loop " + std::to_wstring(current_loop) + L"/" +
                 std::to_wstring(repeat_count) + L" started");
        }
        SetResultText(L"Running UBM Controller validation, loop " +
                      std::to_wstring(current_loop) + L"/" + std::to_wstring(repeat_count) + L"...");
        FlushUiUpdates();

        run(L"UBM Operational State", Transaction::UbmControllerRead, 0x00u, {}, 1, false);
        run(L"UBM Last Command Status", Transaction::UbmControllerRead, 0x01u, {}, 1, false);
        run(L"UBM Silicon Identity", Transaction::UbmControllerRead, 0x02u, {}, 14, false);
        run(L"UBM Update Capabilities", Transaction::UbmControllerRead, 0x03u, {}, 1, false);
        run(L"UBM HFC Info", Transaction::UbmControllerRead, 0x30u, {}, 1, false);
        run(L"UBM Backplane Info", Transaction::UbmControllerRead, 0x31u, {}, 1, false);
        run(L"UBM Starting Slot", Transaction::UbmControllerRead, 0x32u, {}, 1, false);
        run(L"UBM Capabilities", Transaction::UbmControllerRead, 0x33u, {}, 2, false);
        run(L"UBM Features Read", Transaction::UbmControllerRead, 0x34u, {}, 2, false);
        run(L"UBM Features Write", Transaction::UbmControllerWrite, 0x34u, {0x00u, 0x00u}, 0, false);
        run(L"UBM LCS After Features", Transaction::UbmControllerRead, 0x01u, {}, 1, false);

        {
            const std::wstring change_count_label =
                (repeat_count > 1)
                    ? (L"UBM Change Count Read (loop " + std::to_wstring(current_loop) + L"/" +
                       std::to_wstring(repeat_count) + L")")
                    : L"UBM Change Count Read";
            ThrowIfCancelRequested();
            ExecResult cc = ExecUbmControllerRead(static_cast<std::uint8_t>(ParseEditInt(addr_edit_) & 0x7F), 0x35u, 2);
            change_count = cc.data;
            if (cc.ubm_checksum_checked && !cc.ubm_checksum_ok) {
                ++fail;
                if (log_) {
                    log_(L"SMBus FAIL " + change_count_label + L": checksum mismatch");
                }
            } else {
                ++pass;
                if (log_) {
                    log_(L"SMBus PASS " + change_count_label + L" raw=" + mfc_tool::core::HexDump(cc.raw));
                }
            }
            ++step;
            mfc_tool::ui::SafeSetProgressPos(progress_, step);
            FlushUiUpdates();
            ThrowIfCancelRequested();
        }

        if (change_count.size() != 2u) {
            change_count = {0x01u, 0x00u};
        }
        run(L"UBM Change Count Clear", Transaction::UbmControllerWrite, 0x35u, change_count, 0, false);
        run(L"UBM LCS After Change Clear", Transaction::UbmControllerRead, 0x01u, {}, 1, false);
        run(L"UBM DFC Index 0", Transaction::UbmControllerWrite, 0x36u, {0x00u}, 0, false);
        run(L"UBM DFC Descriptor 0", Transaction::UbmControllerRead, 0x40u, {}, 8, false);
        run(L"UBM DFC Index 1", Transaction::UbmControllerWrite, 0x36u, {0x01u}, 0, false);
        run(L"UBM DFC Descriptor 1", Transaction::UbmControllerRead, 0x40u, {}, 8, false);
        run(L"UBM Enter Update Shell Read", Transaction::UbmControllerRead, 0x20u, {}, 5, false);
        run(L"UBM Enter Update Shell Write", Transaction::UbmControllerWrite, 0x20u, {0xB8u, 0x55u, 0x42u, 0x4Du, 0x01u}, 0, false);
        run_expect_byte(L"UBM Operational State Reduced", 0x00u, 0x04u);
        run(L"UBM PMDT Shell Write", Transaction::UbmControllerWrite, 0x21u, {0x00u, 0x02u, 0xAAu, 0x55u}, 0, false);
        run(L"UBM Exit Update Shell Read", Transaction::UbmControllerRead, 0x22u, {}, 4, false);
        run(L"UBM Exit Update Shell Write", Transaction::UbmControllerWrite, 0x22u, {0x55u, 0x42u, 0x4Du, 0x01u}, 0, false);
        run_expect_byte(L"UBM Operational State Ready", 0x00u, 0x03u);
        run(L"UBM CCC Control", Transaction::UbmControllerWrite, 0x37u, {0x00u}, 0, false);
        run(L"UBM CCC Result Index", Transaction::UbmControllerWrite, 0x38u, {0x00u}, 0, false);
        run(L"UBM CCC Result Descriptor", Transaction::UbmControllerRead, 0x41u, {}, 35, false);
        run(L"UBM Flex I/O Index", Transaction::UbmControllerWrite, 0x50u, {0x00u}, 0, false);
        run(L"UBM Flex I/O Descriptor", Transaction::UbmControllerRead, 0x51u, {}, 5, false);
        run(L"UBM Power Event Data", Transaction::UbmControllerRead, 0x60u, {}, 32, false);
        run(L"UBM Bad Checksum Negative", Transaction::UbmControllerWrite, 0x34u, {0x00u, 0x00u}, 0, true);
    }

    {
        std::wstringstream ss;
        ss << L"UBM Controller Run All complete: loops=" << repeat_count << L" PASS=" << pass << L" FAIL=" << fail;
        SetResultText(ss.str());
        if (log_) {
            log_(ss.str());
        }
    }
}

void CSmbusTab::RunOneUbmTest(const wchar_t* label, Transaction txn, std::uint8_t command, const std::vector<std::uint8_t>& data, int read_len, bool bad_checksum, int* pass, int* fail) {
    try {
        const std::uint8_t addr = static_cast<std::uint8_t>(ParseEditInt(addr_edit_) & 0x7F);
        ExecResult result;
        switch (txn) {
        case Transaction::UbmControllerRead:
            result = ExecUbmControllerRead(addr, command, read_len);
            break;
        case Transaction::UbmControllerWrite:
            result = ExecUbmControllerWrite(addr, command, data, bad_checksum);
            break;
        default:
            throw std::invalid_argument("unsupported UBM Run All item");
        }
        if (result.ubm_checksum_checked && !result.ubm_checksum_ok) {
            throw std::runtime_error("UBM checksum mismatch");
        }
        if (bad_checksum) {
            ExecResult lcs = ExecUbmControllerRead(addr, 0x01u, 1);
            if (lcs.data.empty() || lcs.data[0] != 0x02u) {
                throw std::runtime_error("Bad checksum negative test did not set Last Command Status 0x02");
            }
        }
        ++(*pass);
        if (log_) {
            log_(std::wstring(L"SMBus PASS ") + label + L" raw=" + mfc_tool::core::HexDump(result.raw));
        }
    } catch (const UserCancelled&) {
        throw;
    } catch (const std::exception& e) {
        ++(*fail);
        if (log_) {
            log_(std::wstring(L"SMBus FAIL ") + label + L": " + AnsiToWide(e.what()));
        }
    }
}

bool CSmbusTab::RunOneAllTest(const wchar_t* label, Transaction txn, std::uint8_t command, const std::vector<std::uint8_t>& data, int read_len, bool pec, bool bad_pec, int* pass, int* fail) {
    try {
        const std::uint8_t addr = static_cast<std::uint8_t>(ParseEditInt(addr_edit_) & 0x7F);
        ExecResult result;
        switch (txn) {
        case Transaction::QuickWrite:
            result = ExecQuick(addr, false);
            break;
        case Transaction::QuickRead:
            result = ExecQuick(addr, true);
            break;
        case Transaction::SendByte:
            result = ExecSendByte(addr, command, pec, bad_pec);
            break;
        case Transaction::ReceiveByte:
            result = ExecReceiveByte(addr, pec);
            break;
        case Transaction::WriteByte:
        case Transaction::WriteWord:
            result = ExecWrite(addr, command, data, pec, bad_pec);
            break;
        case Transaction::ReadByte:
            result = ExecRead(addr, command, 1, pec);
            break;
        case Transaction::ReadWord:
            result = ExecRead(addr, command, 2, pec);
            break;
        case Transaction::BlockWrite: {
            std::vector<std::uint8_t> payload = {static_cast<std::uint8_t>(data.size() & 0xFFu)};
            payload.insert(payload.end(), data.begin(), data.end());
            result = ExecWrite(addr, command, payload, pec, bad_pec);
            break;
        }
        case Transaction::BlockRead:
            result = ExecBlockRead(addr, command, read_len, pec);
            break;
        case Transaction::ProcessCall:
            result = ExecProcessCall(addr, command, data, pec);
            break;
        case Transaction::BlockWriteReadProcessCall:
            result = ExecBlockWriteReadProcessCall(addr, command, data, read_len, pec);
            break;
        case Transaction::BusRecover:
            result.raw = service_->I2cMasterBusStatus(CurrentMasterPort(), true);
            result.data = result.raw;
            SleepAfterRunAllCommand();
            break;
        default:
            throw std::invalid_argument("unsupported Run All item");
        }
        if (result.pec_checked && !result.pec_ok && !bad_pec) {
            ++(*fail);
            if (log_) {
                std::wstringstream ss;
                ss << L"SMBus FAIL " << label << L": PEC mismatch raw="
                   << mfc_tool::core::HexDump(result.raw)
                   << L" rx=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
                   << static_cast<unsigned int>(result.pec_rx)
                   << L" calc=0x" << std::setw(2)
                   << static_cast<unsigned int>(result.pec_calc);
                log_(ss.str());
            }
            return true;
        }
        ++(*pass);
        if (log_) {
            log_(std::wstring(L"SMBus PASS ") + label + L" raw=" + mfc_tool::core::HexDump(result.raw));
        }
        return true;
    } catch (const UserCancelled&) {
        throw;
    } catch (const mfc_tool::hid::BridgeStatusException& e) {
        if (bad_pec && IsExpectedBadPecBridgeStatus(e.status())) {
            ++(*pass);
            if (log_) {
                log_(std::wstring(L"SMBus PASS ") + label + L" expected negative-path bridge status: " + AnsiToWide(e.what()));
            }
            return true;
        } else {
            ++(*fail);
            if (log_) {
                log_(std::wstring(L"SMBus FAIL ") + label + L": " + AnsiToWide(e.what()));
            }
            return false;
        }
    } catch (const std::exception& e) {
        if (bad_pec) {
            ++(*fail);
            if (log_) {
                log_(std::wstring(L"SMBus FAIL ") + label + L" unexpected negative-path error: " + AnsiToWide(e.what()));
            }
        } else {
            ++(*fail);
            if (log_) {
                log_(std::wstring(L"SMBus FAIL ") + label + L": " + AnsiToWide(e.what()));
            }
        }
        return false;
    }
}

void CSmbusTab::FlushUiUpdates() {
    mfc_tool::ui::UpdateWindowsAndPumpPaint({
        &raw_edit_,
        &script_path_edit_,
        &script_list_,
        &result_edit_,
        &stop_btn_,
        &progress_
    });
    mfc_tool::ui::PumpControlMessages(stop_btn_);
}

void CSmbusTab::RequestCancel() {
    if (!run_all_running_ && !script_running_) {
        return;
    }
    cancel_requested_ = true;
    mfc_tool::ui::SafeEnableWindow(stop_btn_, FALSE);
    if (run_all_running_) {
        SetResultText(L"Stopping after current SMBus transaction...");
    } else {
        AppendScriptResponse(L"Stopping after current script transaction...");
    }
    FlushUiUpdates();
}

void CSmbusTab::ResetCancel() {
    cancel_requested_ = false;
}

void CSmbusTab::ThrowIfCancelRequested() {
    FlushUiUpdates();
    if (cancel_requested_) {
        throw UserCancelled();
    }
}

bool CSmbusTab::SleepWithCancel(int delay_ms) {
    int remaining = (std::max)(0, delay_ms);
    while (remaining > 0) {
        const int chunk = (std::min)(remaining, 25);
        ::Sleep(static_cast<DWORD>(chunk));
        remaining -= chunk;
        ThrowIfCancelRequested();
    }
    return !cancel_requested_;
}

void CSmbusTab::SetRawRxText(const std::wstring& text) {
    raw_edit_.SetWindowTextW(text.c_str());
}

void CSmbusTab::SetResultText(const std::wstring& text) {
    result_edit_.SetWindowTextW(text.c_str());
}

DWORD CSmbusTab::RunAllCommandDelayMs() const {
    try {
        const int delay_ms = ParseEditInt(run_all_delay_edit_);
        if (delay_ms < 0) {
            return kSmbusDefaultRunAllCommandDelayMs;
        }
        return static_cast<DWORD>((std::min)(delay_ms, static_cast<int>(kSmbusMaxRunAllCommandDelayMs)));
    } catch (const std::exception&) {
        return kSmbusDefaultRunAllCommandDelayMs;
    }
}

int CSmbusTab::RunAllRepeatCount() const {
    try {
        const int repeat_count = ParseEditInt(run_all_repeat_edit_);
        if (repeat_count <= 0) {
            return kSmbusDefaultRunAllRepeatCount;
        }
        return (std::min)(repeat_count, kSmbusMaxRunAllRepeatCount);
    } catch (const std::exception&) {
        return kSmbusDefaultRunAllRepeatCount;
    }
}

void CSmbusTab::SleepAfterRunAllCommand() {
    if (run_all_running_) {
        SleepWithCancel(static_cast<int>(RunAllCommandDelayMs()));
    }
}

void CSmbusTab::SetScriptPathText() {
    if (::IsWindow(script_path_edit_.GetSafeHwnd())) {
        script_path_edit_.SetWindowTextW(script_doc_.path.c_str());
    }
}

void CSmbusTab::SetScriptResponseText(const std::wstring& text) {
    SetResultText(text);
}

void CSmbusTab::AppendScriptResponse(const std::wstring& text) {
    if (!::IsWindow(result_edit_.GetSafeHwnd())) {
        return;
    }
    int len = result_edit_.GetWindowTextLengthW();
    result_edit_.SetSel(len, len);
    result_edit_.ReplaceSel((text + L"\r\n").c_str());
}

void CSmbusTab::LoadScriptFromPath(const std::wstring& path) {
    mfc_tool::core::SmbusScriptDocument loaded;
    std::wstring error;

    if (!mfc_tool::core::LoadSmbusScriptCsv(path, &loaded, &error)) {
        throw std::runtime_error(WideToAnsiLossy(error));
    }
    script_doc_ = std::move(loaded);
    SetScriptPathText();
    PopulateScriptList();
    UpdateScriptSummary();
    if (persist_settings_) {
        persist_settings_();
    }
}

void CSmbusTab::PopulateScriptList() {
    if (!::IsWindow(script_list_.GetSafeHwnd())) {
        return;
    }
    script_list_updating_ = true;
    script_list_.SetRedraw(FALSE);
    script_list_.DeleteAllItems();
    for (size_t i = 0u; i < script_doc_.rows.size(); ++i) {
        const auto& row = script_doc_.rows[i];
        const int item = script_list_.InsertItem(static_cast<int>(i), std::to_wstring(i + 1u).c_str());
        script_list_.SetItemData(item, static_cast<DWORD_PTR>(i));
        script_list_.SetCheck(item, row.selected ? TRUE : FALSE);
        script_list_.SetItemText(item, 1, mfc_tool::core::SmbusScriptRowTypeText(row).c_str());
        script_list_.SetItemText(item, 2, row.address >= 0 ? mfc_tool::core::FormatSmbusScriptHexByte(row.address).c_str() : L"");
        script_list_.SetItemText(item, 3, row.command >= 0 ? mfc_tool::core::FormatSmbusScriptHexByte(row.command).c_str() : L"");
        script_list_.SetItemText(item, 4, mfc_tool::core::FormatSmbusScriptData(row.data).c_str());
        script_list_.SetItemText(item, 5, row.read_length > 0 ? std::to_wstring(row.read_length).c_str() : L"");
        script_list_.SetItemText(item, 6, mfc_tool::core::SmbusScriptRowSummary(row).c_str());
    }
    mfc_tool::ui::AutoSizeListColumns(script_list_, 7, {74, 126, 62, 62, 150, 50, 360});
    script_list_.SetRedraw(TRUE);
    script_list_.Invalidate(FALSE);
    script_list_updating_ = false;
}

void CSmbusTab::UpdateScriptSummary() {
    size_t command_count = 0u;
    size_t selected_count = 0u;
    size_t read_count = 0u;

    for (const auto& row : script_doc_.rows) {
        if (row.kind != mfc_tool::core::SmbusScriptRowKind::Command) {
            continue;
        }
        ++command_count;
        if (row.selected) {
            ++selected_count;
        }
        if (mfc_tool::core::SmbusScriptRowIsRead(row)) {
            ++read_count;
        }
    }
    SetScriptResponseText(L"Script rows=" + std::to_wstring(script_doc_.rows.size()) +
                          L" commands=" + std::to_wstring(command_count) +
                          L" selected=" + std::to_wstring(selected_count) +
                          L" reads=" + std::to_wstring(read_count));
}

int CSmbusTab::ParseEditInt(const CEdit& edit) const {
    return mfc_tool::core::ParseInt(GetEditText(edit));
}

int CSmbusTab::CurrentSpeedHz() const {
    CString text;
    speed_combo_.GetWindowTextW(text);
    return mfc_tool::core::ParseInt(text.GetString());
}

int CSmbusTab::CurrentMasterPort() const {
    const int sel = port_combo_.GetCurSel();
    if (sel != CB_ERR) {
        return static_cast<int>(port_combo_.GetItemData(sel));
    }
    return 0;
}

const mfc_tool::core::board_i2c::PinPair& CSmbusTab::CurrentMasterPinPair() const {
    const auto& pairs = mfc_tool::core::board_i2c::AllPinPairs();
    const int sel = pins_combo_.GetCurSel();
    if (sel != CB_ERR) {
        const auto idx = static_cast<size_t>(pins_combo_.GetItemData(sel));
        if (idx < pairs.size()) {
            return pairs[idx];
        }
    }
    const auto* fallback = mfc_tool::core::board_i2c::DefaultPinPair(CurrentMasterPort());
    return fallback != nullptr ? *fallback : pairs.front();
}

std::wstring CSmbusTab::GetEditText(const CEdit& edit) const {
    CString s;
    const_cast<CEdit&>(edit).GetWindowTextW(s);
    return s.GetString();
}

std::uint8_t CSmbusTab::ParseCommandCode() const {
    return static_cast<std::uint8_t>(ParseEditInt(command_edit_) & 0xFF);
}

std::vector<std::uint8_t> CSmbusTab::ParseTxHex(const CEdit& edit) const {
    return mfc_tool::core::ParseHexBytes(GetEditText(edit));
}

std::vector<std::uint8_t> CSmbusTab::BuildTxPayload(bool update_ui, bool advance_counter) {
    std::vector<std::uint8_t> data = ParseTxHex(tx_edit_);
    if (!data.empty() && counter_check_.GetCheck() == BST_CHECKED) {
        int idx = 0;
        int step = 1;
        try {
            idx = ParseEditInt(counter_idx_edit_);
            step = ParseEditInt(counter_step_edit_);
        } catch (...) {
            idx = 0;
            step = 1;
        }
        if (step <= 0) {
            step = 1;
        }
        if (idx >= 0 && static_cast<size_t>(idx) < data.size()) {
            if (!counter_seeded_) {
                counter_value_ = data[static_cast<size_t>(idx)];
                counter_seeded_ = true;
            }
            data[static_cast<size_t>(idx)] = static_cast<std::uint8_t>(counter_value_ & 0xFFu);
            if (advance_counter) {
                counter_value_ = (counter_value_ + static_cast<unsigned int>(step)) & 0xFFu;
            }
        }
    }
    if (update_ui) {
        tx_edit_.SetWindowTextW(mfc_tool::core::HexDump(data).c_str());
    }
    return data;
}

CSmbusTab::Profile CSmbusTab::CurrentProfile() const {
    CString text;
    profile_combo_.GetWindowTextW(text);
    for (int i = 0; i <= static_cast<int>(Profile::UbmController); ++i) {
        const Profile profile = static_cast<Profile>(i);
        if (text == ProfileText(profile)) {
            return profile;
        }
    }
    return Profile::Generic;
}

bool CSmbusTab::IsUbmProfile() const {
    return CurrentProfile() == Profile::UbmController;
}

void CSmbusTab::ApplyProfileDefaults(bool force_address) {
    const Profile profile = CurrentProfile();
    if (profile == Profile::Generic) {
        if (force_address) {
            addr_edit_.SetWindowTextW(L"0x5A");
            pec_check_.SetCheck(BST_CHECKED);
            bad_pec_check_.SetCheck(BST_UNCHECKED);
        }
        return;
    }
    if (force_address) {
        addr_edit_.SetWindowTextW(L"0x5A");
    }
    pec_check_.SetCheck(BST_UNCHECKED);
    bad_pec_check_.SetCheck(BST_UNCHECKED);
    counter_check_.SetCheck(BST_UNCHECKED);
}

CSmbusTab::Transaction CSmbusTab::CurrentTransaction() const {
    CString text;
    txn_combo_.GetWindowTextW(text);
    for (int i = 0; i <= static_cast<int>(Transaction::BusRecover); ++i) {
        const Transaction txn = static_cast<Transaction>(i);
        if (text == TransactionText(txn)) {
            return txn;
        }
    }
    return Transaction::ReadByte;
}

std::wstring CSmbusTab::CurrentTransactionText() const {
    return TransactionText(CurrentTransaction());
}

const wchar_t* CSmbusTab::ProfileText(Profile profile) {
    switch (profile) {
    case Profile::Generic: return L"Generic";
    case Profile::UbmController: return L"UBM Controller";
    default: return L"Generic";
    }
}

const wchar_t* CSmbusTab::TransactionText(Transaction txn) {
    switch (txn) {
    case Transaction::QuickWrite: return L"Quick Write";
    case Transaction::QuickRead: return L"Quick Read";
    case Transaction::SendByte: return L"Send Byte";
    case Transaction::ReceiveByte: return L"Receive Byte";
    case Transaction::WriteByte: return L"Write Byte";
    case Transaction::ReadByte: return L"Read Byte";
    case Transaction::WriteWord: return L"Write Word";
    case Transaction::ReadWord: return L"Read Word";
    case Transaction::BlockWrite: return L"Block Write";
    case Transaction::BlockRead: return L"Block Read";
    case Transaction::ProcessCall: return L"Process Call";
    case Transaction::BlockWriteReadProcessCall: return L"Block Wr/Rd ProcCall";
    case Transaction::UbmControllerRead: return L"UBM Ctrl Read";
    case Transaction::UbmControllerWrite: return L"UBM Ctrl Write";
    case Transaction::UbmBadChecksumWrite: return L"UBM Bad Checksum Write";
    case Transaction::BusRecover: return L"Bus Recover";
    default: return L"Unknown";
    }
}

void CSmbusTab::OnUiSettingChanged() {
    if (pec_check_.GetCheck() != BST_CHECKED || IsUbmProfile()) {
        bad_pec_check_.SetCheck(BST_UNCHECKED);
    }
    UpdateEnableState();
}

void CSmbusTab::OnCounterReset() {
    counter_seeded_ = false;
    if (log_) {
        log_(L"SMBus TX counter reset");
    }
}

std::wstring CSmbusTab::AnsiToWide(const char* text) {
    if (text == nullptr) {
        return L"";
    }
    return std::wstring(text, text + strlen(text));
}
