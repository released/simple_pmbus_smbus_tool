#include "pmbus_tab.h"

#include <afxdlgs.h>

#include <algorithm>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../core/bridge_commands.h"
#include "../core/text_utils.h"
#include "../hid/hid_bridge_client.h"
#include "layout_utils.h"

namespace {

enum : UINT {
    IDC_PMBUS_MASTER_GROUP = 13100,
    IDC_PMBUS_PROFILE_LABEL,
    IDC_PMBUS_PROFILE_COMBO,
    IDC_PMBUS_SYSTEM_POLICY_LABEL,
    IDC_PMBUS_SYSTEM_POLICY_COMBO,
    IDC_PMBUS_MASTER_PORT_LABEL,
    IDC_PMBUS_MASTER_PORT_COMBO,
    IDC_PMBUS_MASTER_PINS_LABEL,
    IDC_PMBUS_MASTER_PINS_COMBO,
    IDC_PMBUS_SPEED_LABEL,
    IDC_PMBUS_SPEED_COMBO,
    IDC_PMBUS_MASTER_ADDR_LABEL,
    IDC_PMBUS_MASTER_ADDR_EDIT,
    IDC_PMBUS_CHECKLIST_DELAY_LABEL,
    IDC_PMBUS_CHECKLIST_DELAY_EDIT,
    IDC_PMBUS_CHECKLIST_REPEAT_LABEL,
    IDC_PMBUS_CHECKLIST_REPEAT_EDIT,
    IDC_PMBUS_PEC_CHECK,
    IDC_PMBUS_BAD_PEC_CHECK,
    IDC_PMBUS_MASTER_ENABLE,
    IDC_PMBUS_MASTER_DISABLE,
    IDC_PMBUS_SCAN,
    IDC_PMBUS_ARA,
    IDC_PMBUS_CMD_PRESET_LABEL,
    IDC_PMBUS_CMD_PRESET_COMBO,
    IDC_PMBUS_CMD_CODE_LABEL,
    IDC_PMBUS_CMD_CODE_EDIT,
    IDC_PMBUS_EXT_CHECK,
    IDC_PMBUS_EXT_TYPE_COMBO,
    IDC_PMBUS_TXN_LABEL,
    IDC_PMBUS_TXN_COMBO,
    IDC_PMBUS_TX_HEX_LABEL,
    IDC_PMBUS_TX_HEX_EDIT,
    IDC_PMBUS_READ_LEN_LABEL,
    IDC_PMBUS_READ_LEN_EDIT,
    IDC_PMBUS_EXECUTE,
    IDC_PMBUS_RAW_RX_LABEL,
    IDC_PMBUS_RAW_RX_EDIT,
    IDC_PMBUS_DECODED_LABEL,
    IDC_PMBUS_DECODED_EDIT,
    IDC_PMBUS_SCAN_SUMMARY_GROUP,
    IDC_PMBUS_SCAN_SUMMARY_LABEL,
    IDC_PMBUS_SCAN_SUMMARY_EDIT,
    IDC_PMBUS_ILLEGAL_LABEL,
    IDC_PMBUS_ILLEGAL_BTN,
    IDC_PMBUS_ILLEGAL_RESULT_LABEL,
    IDC_PMBUS_ILLEGAL_RESULT_EDIT,
    IDC_PMBUS_CHECKLIST_PROGRESS,
    IDC_PMBUS_CHECKLIST_BASIC_BTN,
    IDC_PMBUS_CHECKLIST_PEC_BTN,
    IDC_PMBUS_CHECKLIST_ERROR_BTN,
    IDC_PMBUS_CHECKLIST_TELEMETRY_BTN,
    IDC_PMBUS_CHECKLIST_MFR_BTN,
    IDC_PMBUS_CHECKLIST_FULL_BTN,
    IDC_PMBUS_STOP,
    IDC_PMBUS_SMBALERT_LABEL,
    IDC_PMBUS_SMBALERT_EDIT,
    IDC_PMBUS_SMBALERT_READ,
    IDC_PMBUS_SMBALERT_WRITE,
    IDC_PMBUS_SCRIPT_LABEL,
    IDC_PMBUS_SCRIPT_PATH,
    IDC_PMBUS_SCRIPT_LOAD,
    IDC_PMBUS_SCRIPT_EDIT,
    IDC_PMBUS_SCRIPT_SAVE,
    IDC_PMBUS_SCRIPT_RUN,
    IDC_PMBUS_SCRIPT_SELECT_ALL,
    IDC_PMBUS_SCRIPT_LIST,
    IDC_PMBUS_SCRIPT_RESPONSE_LABEL,
    IDC_PMBUS_SCRIPT_RESPONSE,
};

constexpr int kPmbusAraAddr = 0x0C;
constexpr int kChecklistModeBasic = 0;
constexpr int kChecklistModePec = 1;
constexpr int kChecklistModeError = 2;
constexpr int kChecklistModeTelemetry = 3;
constexpr int kChecklistModeMfr = 4;
constexpr int kChecklistModeFull = 5;
constexpr DWORD kPmbusRetryDelayMs = 10u;
constexpr DWORD kPmbusScanStageDelayMs = 30u;
constexpr DWORD kPmbusScanPreflightSettleMs = 20u;
constexpr DWORD kPmbusDefaultChecklistCommandDelayMs = 10u;
constexpr DWORD kPmbusMaxChecklistCommandDelayMs = 1000u;
constexpr int kPmbusDefaultChecklistRepeatCount = 1;
constexpr int kPmbusMaxChecklistRepeatCount = 20;
constexpr int kPmbusRetryAttempts = 4;
constexpr const wchar_t* kPmbusProfileBaseName = L"PMBus Base";
constexpr const wchar_t* kPmbusProfileCrpsName = L"M-CRPS";
constexpr const wchar_t* kPmbusProfileTiName = L"TI UCD90xxx";
constexpr const wchar_t* kPmbusPolicyProductionName = L"Production";
constexpr const wchar_t* kPmbusPolicyLabName = L"Lab validation";

class UserCancelled : public std::runtime_error {
public:
    UserCancelled() : std::runtime_error("operation cancelled by user") {}
};

std::string WideToAnsiLossy(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back((ch >= 0 && ch <= 0x7F) ? static_cast<char>(ch) : '?');
    }
    return out;
}

int ProfileComboIndex(CPmbusTab::Profile profile) {
    if (profile == CPmbusTab::Profile::Crps) {
        return 1;
    }
    if (profile == CPmbusTab::Profile::TiUcd90xxx) {
        return 2;
    }
    return 0;
}

CPmbusTab::Profile ProfileFromIniName(const std::wstring& name) {
    if (name == kPmbusProfileCrpsName || name == L"CRPS" || name == L"MCRPS") {
        return CPmbusTab::Profile::Crps;
    }
    if (name == kPmbusProfileTiName || name == L"TI" || name == L"UCD90XXX" || name == L"TI_UCD90XXX") {
        return CPmbusTab::Profile::TiUcd90xxx;
    }
    return CPmbusTab::Profile::BasePmbus;
}

int SystemPolicyComboIndex(const std::wstring& name) {
    if (name == kPmbusPolicyLabName || name == L"LAB" || name == L"LAB_VALIDATION" ||
        name == L"Lab" || name == L"1") {
        return 1;
    }
    return 0;
}

std::wstring FormatBaseProfilePresetName(const mfc_tool::core::PmbusCommandPreset& preset) {
    std::wstringstream ss;

    if ((preset.code >= 0xB0u) && (preset.code <= 0xBFu)) {
        ss << L"USER_DATA_" << std::setw(2) << std::setfill(L'0')
           << std::dec << static_cast<unsigned int>(preset.code - 0xB0u);
        return ss.str();
    }

    if ((preset.code >= 0xC0u) && (preset.code <= 0xFDu)) {
        ss << L"MFR_SPECIFIC_" << std::uppercase << std::hex << std::setw(2)
           << std::setfill(L'0') << static_cast<unsigned int>(preset.code);
        return ss.str();
    }

    return preset.name;
}

std::wstring TiUcd90xxxProfilePresetName(const mfc_tool::core::PmbusCommandPreset& preset) {
    switch (preset.code) {
    case 0xB5: return L"BLACK_BOX_FAULT_INFO";
    case 0xB6: return L"BLACK_BOX_FAULT_RAILS_WARNING";
    case 0xB7: return L"BLACK_BOX_LOG_RAILS_VALUE";
    case 0xB9: return L"RAIL_STATE";
    case 0xD0: return L"SEQ_TIMEOUT";
    case 0xD1: return L"VOUT_CAL_MONITOR";
    case 0xD2: return L"SYSTEM_RESET_CONFIG";
    case 0xD3: return L"SYSTEM_WATCHDOG_CONFIG";
    case 0xD4: return L"SYSTEM_WATCHDOG_RESET";
    case 0xD5: return L"MONITOR_CONFIG";
    case 0xD6: return L"NUM_PAGES";
    case 0xD7: return L"RUN_TIME_CLOCK";
    case 0xD8: return L"RUN_TIME_CLOCK_TRIM";
    case 0xD9: return L"ROM_MODE";
    case 0xDA: return L"USER_RAM_00";
    case 0xDB: return L"SOFT_RESET";
    case 0xDC: return L"RESET_COUNT";
    case 0xDD: return L"PIN_SELECTED_RAIL_STATES";
    case 0xDE: return L"RESEQUENCE";
    case 0xDF: return L"CONSTANTS";
    case 0xE0: return L"PWM_SELECT";
    case 0xE1: return L"PWM_CONFIG";
    case 0xE2: return L"PARM_INFO";
    case 0xE3: return L"PARM_VALUE";
    case 0xE4: return L"TEMPERATURE_CAL_GAIN";
    case 0xE5: return L"TEMPERATURE_CAL_OFFSET";
    case 0xE7: return L"FAN_CONFIG_INDEX";
    case 0xE8: return L"FAN_CONFIG";
    case 0xE9: return L"FAULT_RESPONSES";
    case 0xEA: return L"LOGGED_FAULTS";
    case 0xEB: return L"LOGGED_FAULT_DETAIL_INDEX";
    case 0xEC: return L"LOGGED_FAULT_DETAIL";
    case 0xED: return L"LOGGED_PAGE_PEAKS";
    case 0xEE: return L"LOGGED_COMMON_PEAKS";
    case 0xEF: return L"LOG_FAULT_DETAIL_ENABLES";
    case 0xF0: return L"EXECUTE_FLASH";
    case 0xF1: return L"SECURITY";
    case 0xF2: return L"SECURITY_BIT_MASK";
    case 0xF3: return L"MFR_STATUS";
    case 0xF4: return L"GPI_FAULT_RESPONSES";
    case 0xF5: return L"MARGIN_CONFIG";
    case 0xF6: return L"SEQ_CONFIG";
    case 0xF7: return L"GPO_CONFIG_INDEX";
    case 0xF8: return L"GPO_CONFIG";
    case 0xF9: return L"GPI_CONFIG";
    case 0xFA: return L"GPIO_SELECT";
    case 0xFB: return L"GPIO_CONFIG";
    case 0xFC: return L"MISC_CONFIG";
    case 0xFD: return L"DEVICE_ID";
    default:
        return FormatBaseProfilePresetName(preset);
    }
}

std::wstring FormatPresetComboLabel(const mfc_tool::core::PmbusCommandPreset& preset,
                                    CPmbusTab::Profile profile) {
    std::wstringstream ss;
    std::wstring name = preset.name;

    if (profile == CPmbusTab::Profile::BasePmbus) {
        name = FormatBaseProfilePresetName(preset);
    } else if (profile == CPmbusTab::Profile::TiUcd90xxx) {
        name = TiUcd90xxxProfilePresetName(preset);
    }

    ss << L"0x"
       << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
       << static_cast<unsigned int>(preset.code)
       << L" "
       << name;
    return ss.str();
}

bool IsCrpsSpecificCode(std::uint8_t code) {
    switch (code) {
    case 0xB0:
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB8:
    case 0xC0:
    case 0xC1:
    case 0xC2:
    case 0xD0:
    case 0xD1:
    case 0xD2:
    case 0xD3:
    case 0xD4:
    case 0xD5:
    case 0xD6:
    case 0xD7:
    case 0xD8:
    case 0xD9:
    case 0xDA:
    case 0xDB:
    case 0xDC:
    case 0xDD:
    case 0xDE:
    case 0xDF:
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE9:
    case 0xEB:
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xF0:
    case 0xF1:
    case 0xF2:
    case 0xF3:
    case 0xFE:
        return true;
    default:
        return false;
    }
}

bool IsCrpsOverlayOnlyCode(std::uint8_t code) {
    switch (code) {
    case 0xD1:
    case 0xD2:
    case 0xD3:
    case 0xDA:
    case 0xDF:
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE9:
    case 0xEB:
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xF2:
    case 0xF3:
        return true;
    default:
        return false;
    }
}

bool IsUserOrMfrPolicyCode(std::uint8_t code) {
    return ((code >= 0xB0u) && (code <= 0xBFu)) || ((code >= 0xC0u) && (code <= 0xFDu));
}

bool IsBaseGenericNamespaceCode(std::uint8_t code) {
    return ((code >= 0xB0u) && (code <= 0xBFu)) || ((code >= 0xC0u) && (code <= 0xFDu));
}

bool IsTiUcd90xxxSpecificCode(std::uint8_t code) {
    return ((code >= 0xD0u) && (code <= 0xFDu)) ||
           code == 0xB5u || code == 0xB6u || code == 0xB7u || code == 0xB9u;
}

bool IsExpectedBadPecBridgeStatus(std::uint8_t status) {
    return status == 0x04u || status == 0x05u;
}

mfc_tool::core::PmbusTransactionType TiUcd90xxxDefaultTransaction(
    std::uint8_t code,
    mfc_tool::core::PmbusTransactionType fallback
) {
    switch (code) {
    case 0xB5:
    case 0xB6:
    case 0xB7:
    case 0xB9:
    case 0xD2:
    case 0xD3:
    case 0xD5:
    case 0xD7:
    case 0xDD:
    case 0xDF:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE8:
    case 0xE9:
    case 0xEA:
    case 0xEC:
    case 0xED:
    case 0xEF:
    case 0xF1:
    case 0xF2:
    case 0xF3:
    case 0xF6:
    case 0xF8:
    case 0xF9:
    case 0xFC:
    case 0xFD:
        return mfc_tool::core::PmbusTransactionType::BlockRead;
    case 0xD0:
    case 0xD1:
    case 0xD8:
    case 0xDC:
    case 0xE4:
    case 0xE5:
    case 0xEB:
        return mfc_tool::core::PmbusTransactionType::ReadWord;
    case 0xD6:
    case 0xDA:
    case 0xE0:
    case 0xE7:
    case 0xEE:
    case 0xF5:
    case 0xF7:
    case 0xFA:
    case 0xFB:
        return mfc_tool::core::PmbusTransactionType::ReadByte;
    case 0xD4:
    case 0xD9:
    case 0xDB:
    case 0xF0:
        return mfc_tool::core::PmbusTransactionType::SendByte;
    case 0xDE:
        return mfc_tool::core::PmbusTransactionType::WriteWord;
    default:
        return fallback;
    }
}

} // namespace

BEGIN_MESSAGE_MAP(CPmbusTab, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_PMBUS_MASTER_ENABLE, &CPmbusTab::OnMasterEnable)
    ON_BN_CLICKED(IDC_PMBUS_MASTER_DISABLE, &CPmbusTab::OnMasterDisable)
    ON_BN_CLICKED(IDC_PMBUS_EXECUTE, &CPmbusTab::OnExecute)
    ON_BN_CLICKED(IDC_PMBUS_SCAN, &CPmbusTab::OnScan)
    ON_BN_CLICKED(IDC_PMBUS_ARA, &CPmbusTab::OnAra)
    ON_BN_CLICKED(IDC_PMBUS_SMBALERT_READ, &CPmbusTab::OnSmbalertRead)
    ON_BN_CLICKED(IDC_PMBUS_SMBALERT_WRITE, &CPmbusTab::OnSmbalertWrite)
    ON_BN_CLICKED(IDC_PMBUS_PEC_CHECK, &CPmbusTab::OnUiSettingChanged)
    ON_BN_CLICKED(IDC_PMBUS_BAD_PEC_CHECK, &CPmbusTab::OnUiSettingChanged)
    ON_BN_CLICKED(IDC_PMBUS_EXT_CHECK, &CPmbusTab::OnUiSettingChanged)
    ON_CBN_SELCHANGE(IDC_PMBUS_PROFILE_COMBO, &CPmbusTab::OnProfileChanged)
    ON_CBN_SELCHANGE(IDC_PMBUS_MASTER_PORT_COMBO, &CPmbusTab::OnMasterPortChanged)
    ON_CBN_SELCHANGE(IDC_PMBUS_SYSTEM_POLICY_COMBO, &CPmbusTab::OnSystemPolicyChanged)
    ON_CBN_SELCHANGE(IDC_PMBUS_CMD_PRESET_COMBO, &CPmbusTab::OnCommandPresetChanged)
    ON_BN_CLICKED(IDC_PMBUS_ILLEGAL_BTN, &CPmbusTab::OnIllegalQuickTest)
    ON_BN_CLICKED(IDC_PMBUS_CHECKLIST_BASIC_BTN, &CPmbusTab::OnChecklistBasic)
    ON_BN_CLICKED(IDC_PMBUS_CHECKLIST_PEC_BTN, &CPmbusTab::OnChecklistPec)
    ON_BN_CLICKED(IDC_PMBUS_CHECKLIST_ERROR_BTN, &CPmbusTab::OnChecklistError)
    ON_BN_CLICKED(IDC_PMBUS_CHECKLIST_TELEMETRY_BTN, &CPmbusTab::OnChecklistTelemetry)
    ON_BN_CLICKED(IDC_PMBUS_CHECKLIST_MFR_BTN, &CPmbusTab::OnChecklistMfr)
    ON_BN_CLICKED(IDC_PMBUS_CHECKLIST_FULL_BTN, &CPmbusTab::OnChecklistFull)
    ON_BN_CLICKED(IDC_PMBUS_STOP, &CPmbusTab::OnStop)
    ON_BN_CLICKED(IDC_PMBUS_SCRIPT_LOAD, &CPmbusTab::OnScriptLoad)
    ON_BN_CLICKED(IDC_PMBUS_SCRIPT_RUN, &CPmbusTab::OnScriptRun)
    ON_BN_CLICKED(IDC_PMBUS_SCRIPT_SELECT_ALL, &CPmbusTab::OnScriptSelectAll)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_PMBUS_SCRIPT_LIST, &CPmbusTab::OnScriptListChanged)
END_MESSAGE_MAP()

BOOL CPmbusTab::Create(CWnd* parent, const RECT& rect, UINT id) {
    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                      reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
    return CWnd::CreateEx(0, cls, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, rect, parent, id);
}

void CPmbusTab::SetProfile(Profile profile) {
    profile_ = profile;
    if (::IsWindow(profile_combo_.GetSafeHwnd())) {
        RefreshProfileUi();
    }
}

bool CPmbusTab::IsCrpsProfile() const {
    return profile_ == Profile::Crps;
}

bool CPmbusTab::IsTiProfile() const {
    return profile_ == Profile::TiUcd90xxx;
}

bool CPmbusTab::IsLabValidationPolicy() const {
    if (!::IsWindow(system_policy_combo_.GetSafeHwnd())) {
        return false;
    }
    return system_policy_combo_.GetCurSel() == 1;
}

const wchar_t* CPmbusTab::ProfileDisplayName() const {
    if (IsCrpsProfile()) {
        return L"M-CRPS";
    }
    if (IsTiProfile()) {
        return L"TI UCD90xxx";
    }
    return L"PMBus Base";
}

const wchar_t* CPmbusTab::ProfileErrorTitle() const {
    if (IsCrpsProfile()) {
        return L"M-CRPS Error";
    }
    if (IsTiProfile()) {
        return L"TI UCD90xxx Error";
    }
    return L"PMBus Base Error";
}

const wchar_t* CPmbusTab::ProfileIniName() const {
    if (IsCrpsProfile()) {
        return kPmbusProfileCrpsName;
    }
    if (IsTiProfile()) {
        return kPmbusProfileTiName;
    }
    return kPmbusProfileBaseName;
}

std::wstring CPmbusTab::MasterOwnerId() const {
    if (IsCrpsProfile()) {
        return L"CRPS-M";
    }
    if (IsTiProfile()) {
        return L"TI-UCD-M";
    }
    return L"PMBUS-M";
}

bool CPmbusTab::OtherSharedProfileActive() const {
    if (pin_usage_ == nullptr) {
        return false;
    }
    return pin_usage_->AnyActiveExcept(
        {L"PMBUS-M", L"CRPS-M", L"TI-UCD-M", L"SMBUS-M", L"I2C0-M", L"I2C0-S", L"I2C1-M", L"I2C1-S", L"FW-UPLOAD-M"},
        {MasterOwnerId()});
}

bool CPmbusTab::IsPresetVisibleForProfile(const mfc_tool::core::PmbusCommandPreset& preset) const {
    if (IsCrpsProfile()) {
        return !IsUserOrMfrPolicyCode(preset.code) || IsCrpsSpecificCode(preset.code);
    }
    if (IsTiProfile()) {
        return !IsUserOrMfrPolicyCode(preset.code) || IsTiUcd90xxxSpecificCode(preset.code);
    }
    return true;
}

const mfc_tool::core::PmbusState& CPmbusTab::SelectState(const mfc_tool::core::AppState& state) const {
    if (IsCrpsProfile()) {
        return state.crps;
    }
    if (IsTiProfile()) {
        return state.ti_ucd90xxx;
    }
    return state.pmbus;
}

mfc_tool::core::PmbusState& CPmbusTab::SelectState(mfc_tool::core::AppState* state) const {
    if (IsCrpsProfile()) {
        return state->crps;
    }
    if (IsTiProfile()) {
        return state->ti_ucd90xxx;
    }
    return state->pmbus;
}

void CPmbusTab::RefreshProfileUi() {
    if (::IsWindow(profile_combo_.GetSafeHwnd())) {
        profile_combo_.SetCurSel(ProfileComboIndex(profile_));
    }
    if (::IsWindow(master_group_.GetSafeHwnd())) {
        const std::wstring master_title = std::wstring(ProfileDisplayName()) + L" Master";
        master_group_.SetWindowTextW(master_title.c_str());
    }
    if (::IsWindow(command_preset_combo_.GetSafeHwnd())) {
        PopulatePresetCombo();
        ApplyPresetToCommandUi();
    }
    SetRawRxText(L"");
    SetDecodedText(L"");
    SetScanSummaryText(L"");
    SetIllegalTestResultText(L"Not run");
    RefreshPinUsage();
    UpdateEnableState();
    Invalidate(FALSE);
}

void CPmbusTab::Bind(mfc_tool::core::BridgeService* service,
                     std::function<void(const std::wstring&)> logger,
                     mfc_tool::core::PinUsageRegistry* pin_usage,
                     std::function<void()> persist_settings) {
    service_ = service;
    log_ = std::move(logger);
    persist_settings_ = std::move(persist_settings);
    pin_usage_ = pin_usage;
    RefreshPinUsage();
}

int CPmbusTab::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    ui_font_.CreatePointFont(85, L"Segoe UI");

    auto mk_static = [this](CStatic& s, const wchar_t* text, UINT id) {
        s.Create(text, WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, id);
        s.SetFont(&ui_font_);
    };
    auto mk_edit = [this](CEdit& e, const wchar_t* text, UINT id, DWORD style = ES_AUTOHSCROLL) {
        e.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | style, CRect(0, 0, 0, 0), this, id);
        e.SetFont(&ui_font_);
        e.SetWindowTextW(text);
    };
    auto mk_btn = [this](CButton& b, const wchar_t* text, UINT id, DWORD style = BS_PUSHBUTTON) {
        b.Create(text, WS_CHILD | WS_VISIBLE | style, CRect(0, 0, 0, 0), this, id);
        b.SetFont(&ui_font_);
    };

    const std::wstring master_title = std::wstring(ProfileDisplayName()) + L" Master";

    mk_btn(master_group_, master_title.c_str(), IDC_PMBUS_MASTER_GROUP, BS_GROUPBOX);
    mk_static(profile_label_, L"Profile", IDC_PMBUS_PROFILE_LABEL);
    profile_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_PMBUS_PROFILE_COMBO);
    profile_combo_.SetFont(&ui_font_);
    profile_combo_.AddString(kPmbusProfileBaseName);
    profile_combo_.AddString(kPmbusProfileCrpsName);
    profile_combo_.AddString(kPmbusProfileTiName);
    profile_combo_.SetCurSel(ProfileComboIndex(profile_));
    mk_static(system_policy_label_, L"Policy", IDC_PMBUS_SYSTEM_POLICY_LABEL);
    system_policy_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_PMBUS_SYSTEM_POLICY_COMBO);
    system_policy_combo_.SetFont(&ui_font_);
    system_policy_combo_.AddString(kPmbusPolicyProductionName);
    system_policy_combo_.AddString(kPmbusPolicyLabName);
    system_policy_combo_.SetCurSel(0);
    mk_static(master_port_label_, L"I2C", IDC_PMBUS_MASTER_PORT_LABEL);
    master_port_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_PMBUS_MASTER_PORT_COMBO);
    master_port_combo_.SetFont(&ui_font_);
    mk_static(master_pins_label_, L"Pins", IDC_PMBUS_MASTER_PINS_LABEL);
    master_pins_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_PMBUS_MASTER_PINS_COMBO);
    master_pins_combo_.SetFont(&ui_font_);
    mk_static(speed_label_, L"Speed", IDC_PMBUS_SPEED_LABEL);
    speed_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_PMBUS_SPEED_COMBO);
    speed_combo_.SetFont(&ui_font_);
    speed_combo_.AddString(L"100000");
    speed_combo_.AddString(L"400000");
    speed_combo_.SetCurSel(0);
    mk_static(master_addr_label_, L"Addr", IDC_PMBUS_MASTER_ADDR_LABEL);
    mk_edit(master_addr_edit_, L"0x58", IDC_PMBUS_MASTER_ADDR_EDIT);
    mk_static(checklist_delay_label_, L"Delay ms", IDC_PMBUS_CHECKLIST_DELAY_LABEL);
    mk_edit(checklist_delay_edit_, L"10", IDC_PMBUS_CHECKLIST_DELAY_EDIT);
    mk_static(checklist_repeat_label_, L"Repeat", IDC_PMBUS_CHECKLIST_REPEAT_LABEL);
    mk_edit(checklist_repeat_edit_, L"1", IDC_PMBUS_CHECKLIST_REPEAT_EDIT);
    mk_btn(pec_check_, L"PEC", IDC_PMBUS_PEC_CHECK, BS_AUTOCHECKBOX);
    pec_check_.SetCheck(BST_UNCHECKED);
    mk_btn(bad_pec_check_, L"Force Bad PEC", IDC_PMBUS_BAD_PEC_CHECK, BS_AUTOCHECKBOX);
    bad_pec_check_.SetCheck(BST_UNCHECKED);
    mk_btn(master_enable_btn_, L"Enable Master", IDC_PMBUS_MASTER_ENABLE);
    mk_btn(master_disable_btn_, L"Disable Master", IDC_PMBUS_MASTER_DISABLE);
    mk_btn(scan_btn_, L"Scan", IDC_PMBUS_SCAN);
    mk_btn(ara_btn_, L"ARA", IDC_PMBUS_ARA);

    mk_static(command_preset_label_, L"Preset", IDC_PMBUS_CMD_PRESET_LABEL);
    command_preset_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_PMBUS_CMD_PRESET_COMBO);
    command_preset_combo_.SetFont(&ui_font_);
    PopulatePresetCombo();

    mk_static(command_code_label_, L"Command", IDC_PMBUS_CMD_CODE_LABEL);
    mk_edit(command_code_edit_, L"0x00", IDC_PMBUS_CMD_CODE_EDIT);
    mk_btn(ext_check_, L"Extended", IDC_PMBUS_EXT_CHECK, BS_AUTOCHECKBOX);
    ext_check_.SetCheck(BST_UNCHECKED);
    ext_type_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_PMBUS_EXT_TYPE_COMBO);
    ext_type_combo_.SetFont(&ui_font_);
    ext_type_combo_.AddString(L"MFR_EXT (0xFE)");
    ext_type_combo_.AddString(L"PMBUS_EXT (0xFF)");
    ext_type_combo_.SetCurSel(0);

    mk_static(transaction_label_, L"Protocol", IDC_PMBUS_TXN_LABEL);
    transaction_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(0, 0, 0, 0), this, IDC_PMBUS_TXN_COMBO);
    transaction_combo_.SetFont(&ui_font_);
    for (int i = 0; i <= static_cast<int>(mfc_tool::core::PmbusTransactionType::BlockWriteReadProcessCall); ++i) {
        auto type = static_cast<mfc_tool::core::PmbusTransactionType>(i);
        const int idx = transaction_combo_.AddString(mfc_tool::core::PmbusTransactionTypeText(type).c_str());
        transaction_combo_.SetItemData(idx, static_cast<DWORD_PTR>(i));
    }
    transaction_combo_.SetCurSel(0);

    mk_static(tx_hex_label_, L"TX HEX", IDC_PMBUS_TX_HEX_LABEL);
    mk_edit(tx_hex_edit_, L"00 80", IDC_PMBUS_TX_HEX_EDIT);
    mk_static(read_len_label_, L"Read Len", IDC_PMBUS_READ_LEN_LABEL);
    mk_edit(read_len_edit_, L"16", IDC_PMBUS_READ_LEN_EDIT);
    mk_btn(execute_btn_, L"Execute", IDC_PMBUS_EXECUTE);

    mk_static(raw_rx_label_, L"Raw RX", IDC_PMBUS_RAW_RX_LABEL);
    mk_edit(raw_rx_edit_, L"", IDC_PMBUS_RAW_RX_EDIT, ES_AUTOHSCROLL | ES_READONLY);
    mk_static(decoded_label_, L"Decoded", IDC_PMBUS_DECODED_LABEL);
    mk_edit(decoded_edit_, L"", IDC_PMBUS_DECODED_EDIT, ES_AUTOHSCROLL | ES_READONLY);
    mk_btn(scan_summary_group_, L"Scan + Device Summary", IDC_PMBUS_SCAN_SUMMARY_GROUP, BS_GROUPBOX);
    mk_static(scan_summary_label_, L"Summary", IDC_PMBUS_SCAN_SUMMARY_LABEL);
    mk_edit(scan_summary_edit_, L"", IDC_PMBUS_SCAN_SUMMARY_EDIT, ES_AUTOHSCROLL | ES_READONLY);
    mk_static(illegal_test_label_, L"Illegal Cmd", IDC_PMBUS_ILLEGAL_LABEL);
    mk_btn(illegal_test_btn_, L"Illegal Cmd", IDC_PMBUS_ILLEGAL_BTN);
    mk_static(illegal_result_label_, L"Quick Test Result", IDC_PMBUS_ILLEGAL_RESULT_LABEL);
    mk_edit(illegal_result_edit_, L"Not run", IDC_PMBUS_ILLEGAL_RESULT_EDIT, ES_AUTOHSCROLL | ES_READONLY);
    checklist_progress_.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH, CRect(0, 0, 0, 0), this, IDC_PMBUS_CHECKLIST_PROGRESS);
    mfc_tool::ui::SafeResetProgress(checklist_progress_, 100);
    checklist_progress_.ShowWindow(SW_HIDE);
    mk_btn(checklist_basic_btn_, L"Basic", IDC_PMBUS_CHECKLIST_BASIC_BTN);
    mk_btn(checklist_pec_btn_, L"PEC", IDC_PMBUS_CHECKLIST_PEC_BTN);
    mk_btn(checklist_error_btn_, L"Error", IDC_PMBUS_CHECKLIST_ERROR_BTN);
    mk_btn(checklist_telemetry_btn_, L"Telemetry", IDC_PMBUS_CHECKLIST_TELEMETRY_BTN);
    mk_btn(checklist_mfr_btn_, L"MFR", IDC_PMBUS_CHECKLIST_MFR_BTN);
    mk_btn(checklist_full_btn_, L"Full", IDC_PMBUS_CHECKLIST_FULL_BTN);
    mk_btn(stop_btn_, L"Stop", IDC_PMBUS_STOP);
    stop_btn_.EnableWindow(FALSE);
    mk_static(smbalert_label_, L"SMBALERT_MASK", IDC_PMBUS_SMBALERT_LABEL);
    mk_edit(smbalert_edit_, L"0x0000", IDC_PMBUS_SMBALERT_EDIT);
    mk_btn(smbalert_read_btn_, L"Read Mask", IDC_PMBUS_SMBALERT_READ);
    mk_btn(smbalert_write_btn_, L"Write Mask", IDC_PMBUS_SMBALERT_WRITE);
    mk_static(script_label_, L"Script", IDC_PMBUS_SCRIPT_LABEL);
    mk_edit(script_path_edit_, L"", IDC_PMBUS_SCRIPT_PATH, ES_AUTOHSCROLL | ES_READONLY);
    mk_btn(script_load_btn_, L"Load", IDC_PMBUS_SCRIPT_LOAD);
    mk_btn(script_run_btn_, L"Run", IDC_PMBUS_SCRIPT_RUN);
    mk_btn(script_select_all_check_, L"Select All", IDC_PMBUS_SCRIPT_SELECT_ALL, BS_AUTOCHECKBOX);
    script_select_all_check_.SetCheck(BST_CHECKED);
    script_list_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                        CRect(0, 0, 0, 0), this, IDC_PMBUS_SCRIPT_LIST);
    script_list_.SetFont(&ui_font_);
    script_list_.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
    script_list_.InsertColumn(0, L"#", LVCFMT_RIGHT, 42);
    script_list_.InsertColumn(1, L"Type", LVCFMT_LEFT, 126);
    script_list_.InsertColumn(2, L"Addr", LVCFMT_LEFT, 62);
    script_list_.InsertColumn(3, L"Reg", LVCFMT_LEFT, 62);
    script_list_.InsertColumn(4, L"Data", LVCFMT_LEFT, 150);
    script_list_.InsertColumn(5, L"Read", LVCFMT_RIGHT, 50);
    script_list_.InsertColumn(6, L"Summary", LVCFMT_LEFT, 360);
    mk_static(script_response_label_, L"Response", IDC_PMBUS_SCRIPT_RESPONSE_LABEL);
    mk_edit(script_response_edit_, L"", IDC_PMBUS_SCRIPT_RESPONSE, ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL);
    PopulatePortCombos();
    ApplyPresetToCommandUi();
    UpdateEnableState();
    return 0;
}

void CPmbusTab::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    CRect page(0, 0, cx, cy);
    LayoutControls(page);
}

void CPmbusTab::LayoutControls(const CRect& r) {
    const int margin = 6;
    const int gap = 6;
    const int row = 24;
    const int content_w = (std::max)(320, r.Width() - margin * 2);
    const int content_h = (std::max)(120, r.Height());
    const int ix = r.left + margin;
    const int iy = r.top + margin;
    const int master_h = (std::max)(210, content_h - margin * 2);
    const int protocol_w = (std::max)(180, content_w / 4);
    const int inner_left = ix + 8;
    const int inner_right = ix + content_w - 10;
    const int master_row_pitch = 28;
    const int master_row0 = iy + 16;
    const int master_row1 = master_row0 + master_row_pitch;
    const int master_row2 = master_row1 + master_row_pitch;
    const int master_row3 = master_row2 + master_row_pitch;
    const int master_row4 = master_row3 + master_row_pitch;
    const int master_row5 = master_row4 + master_row_pitch;
    const int master_row6 = master_row5 + master_row_pitch;
    const int master_row7 = master_row6 + master_row_pitch;
    const int master_row8 = master_row7 + master_row_pitch;
    mfc_tool::ui::SafeMoveWindow(master_group_, r);
    {
        const int pec_w = mfc_tool::ui::MeasureButtonMinWidth(pec_check_, 14);
        const int bad_pec_w = (std::max)(118, mfc_tool::ui::MeasureButtonMinWidth(bad_pec_check_, 16));
        const int enable_w = (std::max)(104, mfc_tool::ui::MeasureButtonMinWidth(master_enable_btn_));
        const int disable_w = (std::max)(108, mfc_tool::ui::MeasureButtonMinWidth(master_disable_btn_));
        const int scan_w = (std::max)(64, mfc_tool::ui::MeasureButtonMinWidth(scan_btn_));
        const int ara_w = (std::max)(60, mfc_tool::ui::MeasureButtonMinWidth(ara_btn_));
        const int actions_w = enable_w + gap + disable_w + gap + scan_w + gap + ara_w;
        const int actions_x = inner_right - actions_w;
        const int pec_x = actions_x - 16 - bad_pec_w - gap - pec_w;
        int x = inner_left;

        x = mfc_tool::ui::PlaceLabelAndControl(profile_label_, profile_combo_, x, master_row0 + 4, master_row0, 112, 300, gap) + 10;
        x = mfc_tool::ui::PlaceLabelAndControl(system_policy_label_, system_policy_combo_, x, master_row0 + 4, master_row0, 124, 300, gap) + 10;
        x = mfc_tool::ui::PlaceLabelAndControl(speed_label_, speed_combo_, x, master_row0 + 4, master_row0, 82, 300, gap) + 10;
        (void)mfc_tool::ui::PlaceLabelAndControl(master_addr_label_, master_addr_edit_, x, master_row0 + 4, master_row0, 68, row, gap);
        mfc_tool::ui::SafeMoveWindow(pec_check_, pec_x, master_row0 + 2, pec_w, 20);
        mfc_tool::ui::SafeMoveWindow(bad_pec_check_, pec_x + pec_w + gap, master_row0 + 2, bad_pec_w, 20);

        x = actions_x;
        mfc_tool::ui::SafeMoveWindow(master_enable_btn_, x, master_row0, enable_w, row);
        x += enable_w + gap;
        mfc_tool::ui::SafeMoveWindow(master_disable_btn_, x, master_row0, disable_w, row);
        x += disable_w + gap;
        mfc_tool::ui::SafeMoveWindow(scan_btn_, x, master_row0, scan_w, row);
        x += scan_w + gap;
        mfc_tool::ui::SafeMoveWindow(ara_btn_, x, master_row0, ara_w, row);
    }

    {
        const int second_right = inner_right;
        const int txn_w = (std::max)(160, protocol_w);
        const int txn_x = second_right - txn_w;
        int x = inner_left;
        x = mfc_tool::ui::PlaceLabelAndControl(master_port_label_, master_port_combo_, x, master_row1 + 4, master_row1, 66, 300, gap, 12) + 8;
        x = mfc_tool::ui::PlaceLabelAndControl(master_pins_label_, master_pins_combo_, x, master_row1 + 4, master_row1, 230, 300, gap, 12) + 12;
        x = mfc_tool::ui::PlaceLabelAndControl(command_preset_label_, command_preset_combo_, x, master_row1 + 4, master_row1, 150, 300, gap, 12) + 12;
        x = mfc_tool::ui::PlaceLabelAndControl(command_code_label_, command_code_edit_, x, master_row1 + 4, master_row1, 84, row, gap, 14) + 16;
        {
            const int ext_w = (std::max)(92, mfc_tool::ui::MeasureButtonMinWidth(ext_check_, 20));
            const int ext_combo_w = 114;
            mfc_tool::ui::SafeMoveWindow(ext_check_, x, master_row1 + 2, ext_w, 20);
            x += ext_w + gap;
            mfc_tool::ui::SafeMoveWindow(ext_type_combo_, x, master_row1, ext_combo_w, 300);
        }
        const int txn_label_w = mfc_tool::ui::PlaceLabel(transaction_label_, txn_x - mfc_tool::ui::MeasureControlTextWidth(transaction_label_, 12) - gap, master_row1 + 4, 12);
        mfc_tool::ui::SafeMoveWindow(transaction_combo_, txn_x, master_row1, txn_w, 300);
        (void)txn_label_w;
    }

    {
        const int execute_w = (std::max)(60, mfc_tool::ui::MeasureButtonMinWidth(execute_btn_));
        const int read_edit_w = 50;
        const int read_label_w = mfc_tool::ui::MeasureControlTextWidth(read_len_label_, 8);
        const int execute_x = inner_right - execute_w;
        const int read_edit_x = execute_x - gap - read_edit_w;
        const int read_label_x = read_edit_x - gap - read_label_w;
        const int tx_label_w = mfc_tool::ui::MeasureControlTextWidth(tx_hex_label_, 8);
        const int tx_edit_w = (std::max)(180, read_label_x - gap - (inner_left + tx_label_w + gap));
        mfc_tool::ui::PlaceLabelAndControl(tx_hex_label_, tx_hex_edit_, inner_left, master_row2 + 4, master_row2, tx_edit_w, row, gap);
        mfc_tool::ui::SafeMoveWindow(read_len_label_, read_label_x, master_row2 + 4, read_label_w, 18);
        mfc_tool::ui::SafeMoveWindow(read_len_edit_, read_edit_x, master_row2, read_edit_w, row);
        mfc_tool::ui::SafeMoveWindow(execute_btn_, execute_x, master_row2, execute_w, row);
    }

    {
        const int raw_label_w = mfc_tool::ui::MeasureControlTextWidth(raw_rx_label_, 8);
        const int raw_edit_w = (std::max)(180, inner_right - (inner_left + raw_label_w + gap));
        const int decoded_label_w = mfc_tool::ui::MeasureControlTextWidth(decoded_label_, 8);
        const int decoded_edit_w = (std::max)(180, inner_right - (inner_left + decoded_label_w + gap));
        mfc_tool::ui::PlaceLabelAndControl(raw_rx_label_, raw_rx_edit_, inner_left, master_row3 + 4, master_row3, raw_edit_w, row, gap);
        mfc_tool::ui::PlaceLabelAndControl(decoded_label_, decoded_edit_, inner_left, master_row4 + 4, master_row4, decoded_edit_w, row, gap);
    }
    mfc_tool::ui::SafeMoveWindow(scan_summary_group_, 0, 0, 0, 0);
    {
        const int helper_y = master_row5;
        const int summary_y = master_row6;
        const int quick_result_y = master_row7;
        const int mask_label_w = mfc_tool::ui::MeasureControlTextWidth(smbalert_label_, 8);
        const int mask_edit_w = 86;
        const int mask_btn_w = (std::max)(78, mfc_tool::ui::MeasureButtonMinWidth(smbalert_read_btn_));
        const int illegal_btn_w = (std::max)(90, mfc_tool::ui::MeasureButtonMinWidth(illegal_test_btn_));
        const int suite_btn_w = (std::max)(72, mfc_tool::ui::MeasureButtonMinWidth(checklist_basic_btn_, 12));
        const int mfr_btn_w = (std::max)(72, mfc_tool::ui::MeasureButtonMinWidth(checklist_mfr_btn_, 12));
        const int telemetry_btn_w = (std::max)(94, mfc_tool::ui::MeasureButtonMinWidth(checklist_telemetry_btn_, 12));
        const int stop_btn_w = (std::max)(66, mfc_tool::ui::MeasureButtonMinWidth(stop_btn_, 12));
        const int helper_gap = gap + 4;
        const int right_buttons_w = suite_btn_w + gap + suite_btn_w + gap + suite_btn_w + gap +
                                    telemetry_btn_w + gap + mfr_btn_w + gap + suite_btn_w + gap + stop_btn_w;
        int x = inner_left;

        mfc_tool::ui::SafeMoveWindow(smbalert_label_, x, helper_y + 4, mask_label_w, 18);
        x += mask_label_w + gap;
        mfc_tool::ui::SafeMoveWindow(smbalert_edit_, x, helper_y, mask_edit_w, row);
        x += mask_edit_w + gap;
        mfc_tool::ui::SafeMoveWindow(smbalert_read_btn_, x, helper_y, mask_btn_w, row);
        x += mask_btn_w + gap;
        mfc_tool::ui::SafeMoveWindow(smbalert_write_btn_, x, helper_y, mask_btn_w, row);
        x += mask_btn_w + helper_gap;
        mfc_tool::ui::SafeMoveWindow(illegal_test_label_, 0, 0, 0, 0);
        mfc_tool::ui::SafeMoveWindow(illegal_test_btn_, x, helper_y, illegal_btn_w, row);
        x = inner_right - right_buttons_w;
        mfc_tool::ui::SafeMoveWindow(checklist_basic_btn_, x, helper_y, suite_btn_w, row);
        x += suite_btn_w + gap;
        mfc_tool::ui::SafeMoveWindow(checklist_pec_btn_, x, helper_y, suite_btn_w, row);
        x += suite_btn_w + gap;
        mfc_tool::ui::SafeMoveWindow(checklist_error_btn_, x, helper_y, suite_btn_w, row);
        x += suite_btn_w + gap;
        mfc_tool::ui::SafeMoveWindow(checklist_telemetry_btn_, x, helper_y, telemetry_btn_w, row);
        x += telemetry_btn_w + gap;
        mfc_tool::ui::SafeMoveWindow(checklist_mfr_btn_, x, helper_y, mfr_btn_w, row);
        x += mfr_btn_w + gap;
        mfc_tool::ui::SafeMoveWindow(checklist_full_btn_, x, helper_y, suite_btn_w, row);
        x += suite_btn_w + gap;
        mfc_tool::ui::SafeMoveWindow(stop_btn_, x, helper_y, stop_btn_w, row);
        mfc_tool::ui::PlaceLabelAndControl(scan_summary_label_, scan_summary_edit_, inner_left, summary_y + 4, summary_y,
                                           inner_right - (inner_left + mfc_tool::ui::MeasureControlTextWidth(scan_summary_label_, 8) + gap),
                                           row, gap);
        {
            const int result_label_w = mfc_tool::ui::MeasureControlTextWidth(illegal_result_label_, 8);
            const int progress_w = (std::max)(150, (std::min)(220, content_w / 5));
            const int progress_x = inner_right - progress_w;
            const int delay_edit_w = 44;
            const int delay_label_w = mfc_tool::ui::MeasureControlTextWidth(checklist_delay_label_, 8);
            const int repeat_edit_w = 36;
            const int repeat_label_w = mfc_tool::ui::MeasureControlTextWidth(checklist_repeat_label_, 8);
            const int repeat_edit_x = progress_x - gap - repeat_edit_w;
            const int repeat_label_x = repeat_edit_x - gap - repeat_label_w;
            const int delay_edit_x = repeat_label_x - gap - delay_edit_w;
            const int delay_label_x = delay_edit_x - gap - delay_label_w;
            const int result_x = inner_left + result_label_w + gap;
            const int result_w = (std::max)(120, delay_label_x - gap - result_x);
            mfc_tool::ui::SafeMoveWindow(illegal_result_label_, inner_left, quick_result_y + 4, result_label_w, 18);
            mfc_tool::ui::SafeMoveWindow(illegal_result_edit_, result_x, quick_result_y, result_w, row);
            mfc_tool::ui::SafeMoveWindow(checklist_delay_label_, delay_label_x, quick_result_y + 4, delay_label_w, 18);
            mfc_tool::ui::SafeMoveWindow(checklist_delay_edit_, delay_edit_x, quick_result_y, delay_edit_w, row);
            mfc_tool::ui::SafeMoveWindow(checklist_repeat_label_, repeat_label_x, quick_result_y + 4, repeat_label_w, 18);
            mfc_tool::ui::SafeMoveWindow(checklist_repeat_edit_, repeat_edit_x, quick_result_y, repeat_edit_w, row);
            mfc_tool::ui::SafeMoveWindow(checklist_progress_, progress_x, quick_result_y + 4, progress_w, 16);
        }
    }
    {
        const int script_y = master_row8;
        const int load_w = (std::max)(64, mfc_tool::ui::MeasureButtonMinWidth(script_load_btn_));
        const int run_w = (std::max)(58, mfc_tool::ui::MeasureButtonMinWidth(script_run_btn_));
        const int buttons_w = load_w + gap + run_w;
        const int buttons_x = inner_right - buttons_w;
        const int script_label_w = mfc_tool::ui::MeasureControlTextWidth(script_label_, 8);
        const int path_x = inner_left + script_label_w + gap;
        const int path_w = (std::max)(160, buttons_x - gap - path_x);
        int x = buttons_x;

        mfc_tool::ui::SafeMoveWindow(script_label_, inner_left, script_y + 4, script_label_w, 18);
        mfc_tool::ui::SafeMoveWindow(script_path_edit_, path_x, script_y, path_w, row);
        mfc_tool::ui::SafeMoveWindow(script_load_btn_, x, script_y, load_w, row);
        x += load_w + gap;
        mfc_tool::ui::SafeMoveWindow(script_run_btn_, x, script_y, run_w, row);
    }
    {
        const int pane_y = master_row8 + master_row_pitch;
        const int pane_bottom = r.bottom - margin - 8;
        const int pane_h = (std::max)(70, pane_bottom - pane_y);
        const int left_w = (std::max)(260, ((inner_right - inner_left) - gap) / 2);
        const int right_x = inner_left + left_w + gap;
        const int right_w = (std::max)(220, inner_right - right_x);
        const int header_h = row;

        mfc_tool::ui::SafeMoveWindow(script_select_all_check_, inner_left, pane_y + 2, 98, header_h);
        mfc_tool::ui::SafeMoveWindow(script_response_label_, right_x, pane_y + 4, right_w, 18);
        mfc_tool::ui::SafeMoveWindow(script_list_, inner_left, pane_y + header_h, left_w, (std::max)(42, pane_h - header_h));
        mfc_tool::ui::SafeMoveWindow(script_response_edit_, right_x, pane_y + header_h, right_w, (std::max)(42, pane_h - header_h));
    }

}

void CPmbusTab::SetConnected(bool connected) {
    connected_ = connected;
    if (!connected_) {
        master_enabled_ = false;
    }
    UpdateEnableState();
}

void CPmbusTab::OnDisconnected() {
    master_enabled_ = false;
    checklist_running_ = false;
    script_running_ = false;
    cancel_requested_ = false;
    SetRawRxText(L"");
    SetDecodedText(L"");
    SetScanSummaryText(L"");
    SetIllegalTestResultText(L"Not run");
    UpdateEnableState();
}

void CPmbusTab::LoadState(const mfc_tool::core::AppState& state) {
    SetProfile(ProfileFromIniName(state.pmbus_profile));
    const auto& pmbus_state = SelectState(state);

    master_port_combo_.SetCurSel(mfc_tool::core::ParseInt(pmbus_state.master_i2c_port) == 1 ? 1 : 0);
    PopulateMasterPinCombo(pmbus_state.master_i2c_pins);
    master_addr_edit_.SetWindowTextW(pmbus_state.master_addr.c_str());
    checklist_delay_edit_.SetWindowTextW(pmbus_state.checklist_delay_ms.c_str());
    checklist_repeat_edit_.SetWindowTextW(pmbus_state.checklist_repeat_count.c_str());
    tx_hex_edit_.SetWindowTextW(pmbus_state.tx_hex.c_str());
    read_len_edit_.SetWindowTextW(pmbus_state.read_len.c_str());
    command_code_edit_.SetWindowTextW(pmbus_state.command_code.c_str());
    pec_check_.SetCheck(pmbus_state.pec_enable == L"1" ? BST_CHECKED : BST_UNCHECKED);
    bad_pec_check_.SetCheck(pmbus_state.bad_pec_enable == L"1" ? BST_CHECKED : BST_UNCHECKED);
    ext_check_.SetCheck(pmbus_state.extended_enable == L"1" ? BST_CHECKED : BST_UNCHECKED);
    system_policy_combo_.SetCurSel(SystemPolicyComboIndex(pmbus_state.system_policy));
    smbalert_edit_.SetWindowTextW(pmbus_state.smbalert_mask_hex.c_str());
    script_doc_.path = pmbus_state.smbus_script_path;
    script_doc_.rows.clear();
    SetScriptPathText();
    PopulateScriptList();
    SetScriptResponseText(L"");

    int speed_idx = 0;
    if (pmbus_state.speed_hz == L"400000") {
        speed_idx = 1;
    }
    speed_combo_.SetCurSel(speed_idx);

    int preset_idx = 0;
    int command_code_from_ini = -1;
    try {
        command_code_from_ini = mfc_tool::core::ParseInt(pmbus_state.command_code);
    } catch (const std::exception&) {
        command_code_from_ini = -1;
    }
    for (int i = 0; i < command_preset_combo_.GetCount(); ++i) {
        const auto code = static_cast<std::uint8_t>(command_preset_combo_.GetItemData(i) & 0xFFu);
        const auto* preset = mfc_tool::core::FindPmbusCommandPresetByCode(code);
        CString preset_text;
        command_preset_combo_.GetLBText(i, preset_text);
        if ((preset != nullptr && pmbus_state.command_preset == preset->name) ||
            (pmbus_state.command_preset == preset_text.GetString()) ||
            (command_code_from_ini >= 0 && code == static_cast<std::uint8_t>(command_code_from_ini & 0xFF))) {
            preset_idx = i;
            break;
        }
    }
    command_preset_combo_.SetCurSel(preset_idx);

    ext_type_combo_.SetCurSel(pmbus_state.extended_type == L"PMBUS_EXT" ? 1 : 0);

    int txn_idx = 0;
    for (int i = 0; i < transaction_combo_.GetCount(); ++i) {
        CString text;
        transaction_combo_.GetLBText(i, text);
        if (pmbus_state.transaction == text.GetString()) {
            txn_idx = i;
            break;
        }
    }
    transaction_combo_.SetCurSel(txn_idx);

    master_enabled_ = false;
    cached_vout_mode_ = 0x17;
    cached_vout_mode_exponent_ = -9;
    SetRawRxText(L"");
    SetDecodedText(L"");
    SetScanSummaryText(L"");
    SetIllegalTestResultText(L"Not run");
    UpdateEnableState();
}

void CPmbusTab::SaveState(mfc_tool::core::AppState* state) const {
    if (state == nullptr) {
        return;
    }
    state->pmbus_profile = ProfileIniName();
    auto& pmbus_state = SelectState(state);

    pmbus_state.speed_hz = speed_combo_.GetCurSel() == 1 ? L"400000" : L"100000";
    pmbus_state.master_i2c_port = std::to_wstring(CurrentMasterPort());
    pmbus_state.master_i2c_pins = CurrentMasterPinPair().ini_name;
    pmbus_state.master_addr = GetEditText(master_addr_edit_);
    pmbus_state.checklist_delay_ms = std::to_wstring(ChecklistCommandDelayMs());
    pmbus_state.checklist_repeat_count = std::to_wstring(ChecklistRepeatCount());
    pmbus_state.pec_enable = (pec_check_.GetCheck() == BST_CHECKED) ? L"1" : L"0";
    pmbus_state.bad_pec_enable = (bad_pec_check_.GetCheck() == BST_CHECKED) ? L"1" : L"0";
    pmbus_state.extended_enable = (ext_check_.GetCheck() == BST_CHECKED) ? L"1" : L"0";
    pmbus_state.extended_type = ext_type_combo_.GetCurSel() == 1 ? L"PMBUS_EXT" : L"MFR_EXT";
    pmbus_state.command_code = GetEditText(command_code_edit_);
    pmbus_state.tx_hex = GetEditText(tx_hex_edit_);
    pmbus_state.read_len = GetEditText(read_len_edit_);
    pmbus_state.smbalert_mask_hex = GetEditText(smbalert_edit_);
    pmbus_state.system_policy = IsLabValidationPolicy() ? kPmbusPolicyLabName : kPmbusPolicyProductionName;
    pmbus_state.smbus_script_path = script_doc_.path;

    {
        const int preset_sel = command_preset_combo_.GetCurSel();
        if (preset_sel != CB_ERR) {
            CString preset_text;
            command_preset_combo_.GetWindowTextW(preset_text);
            pmbus_state.command_preset = preset_text.GetString();
        }
    }

    CString txn_text;
    transaction_combo_.GetWindowTextW(txn_text);
    pmbus_state.transaction = txn_text.GetString();
}

void CPmbusTab::UpdateEnableState() {
    if (!::IsWindow(speed_combo_.GetSafeHwnd())) {
        return;
    }

    RefreshPinUsage();
    const BOOL connected = connected_ ? TRUE : FALSE;
    const BOOL shared_blocked = OtherSharedProfileActive() ? TRUE : FALSE;
    const BOOL busy = (checklist_running_ || script_running_) ? TRUE : FALSE;
    const BOOL lab_policy = IsLabValidationPolicy() ? TRUE : FALSE;
    const BOOL can_switch_profile = (!checklist_running_ && !master_enabled_) ? TRUE : FALSE;
    const BOOL can_switch_policy = (!checklist_running_) ? TRUE : FALSE;
    const BOOL can_use_master = (connected_ && master_enabled_ && !busy) ? TRUE : FALSE;
    mfc_tool::ui::SafeEnableWindow(profile_combo_, can_switch_profile);
    mfc_tool::ui::SafeEnableWindow(system_policy_combo_, can_switch_policy);
    mfc_tool::ui::SafeEnableWindow(master_port_combo_, (connected_ && !master_enabled_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(master_pins_combo_, (connected_ && !master_enabled_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(speed_combo_, (connected_ && !master_enabled_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(master_addr_edit_, (connected_ && !master_enabled_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(pec_check_, (connected_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(bad_pec_check_, (connected_ && !busy && pec_check_.GetCheck() == BST_CHECKED) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(master_enable_btn_, (connected_ && !master_enabled_ && !shared_blocked && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(master_disable_btn_, (connected_ && master_enabled_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(scan_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(ara_btn_, (can_use_master && lab_policy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(command_preset_combo_, (connected_ && !busy && ext_check_.GetCheck() != BST_CHECKED) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(command_code_edit_, (connected_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(ext_check_, (connected_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(ext_type_combo_, (connected_ && !busy && ext_check_.GetCheck() == BST_CHECKED) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(transaction_combo_, (connected_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(tx_hex_edit_, (connected_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(read_len_edit_, (connected_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(checklist_delay_edit_, (connected_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(checklist_repeat_edit_, (connected_ && !busy) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(execute_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(illegal_test_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(checklist_basic_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(checklist_pec_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(checklist_error_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(checklist_telemetry_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(checklist_mfr_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(checklist_full_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(stop_btn_, busy ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(smbalert_edit_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(smbalert_read_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(smbalert_write_btn_, can_use_master);
    mfc_tool::ui::SafeEnableWindow(script_load_btn_, busy ? FALSE : TRUE);
    mfc_tool::ui::SafeEnableWindow(script_run_btn_, (can_use_master && !script_doc_.rows.empty()) ? TRUE : FALSE);
    mfc_tool::ui::SafeEnableWindow(script_select_all_check_, busy ? FALSE : TRUE);
    mfc_tool::ui::SafeEnableWindow(script_list_, busy ? FALSE : TRUE);
}

void CPmbusTab::RefreshPinUsage() {
    if (pin_usage_ == nullptr) {
        return;
    }
    const std::wstring master_owner = MasterOwnerId();
    const auto& master_pins = CurrentMasterPinPair();

    pin_usage_->SetLabel(master_owner, std::wstring(ProfileDisplayName()) + L" master");
    pin_usage_->SetClaim(master_owner, {master_pins.sda_pin, master_pins.scl_pin});
    pin_usage_->SetActive(master_owner, master_enabled_);
}

void CPmbusTab::SetDecodedText(const std::wstring& text) {
    decoded_edit_.SetWindowTextW(text.c_str());
}

void CPmbusTab::SetRawRxText(const std::wstring& text) {
    raw_rx_edit_.SetWindowTextW(text.c_str());
}

void CPmbusTab::SetScanSummaryText(const std::wstring& text) {
    std::wstring one_line = text;
    size_t pos = 0;
    while ((pos = one_line.find(L"\r\n", pos)) != std::wstring::npos) {
        one_line.replace(pos, 2, L" | ");
        pos += 3;
    }
    while ((pos = one_line.find(L'\n', pos)) != std::wstring::npos) {
        one_line.replace(pos, 1, L" | ");
        pos += 3;
    }
    scan_summary_edit_.SetWindowTextW(one_line.c_str());
}

void CPmbusTab::SetIllegalTestResultText(const std::wstring& text) {
    illegal_result_edit_.SetWindowTextW(text.c_str());
}

void CPmbusTab::UpdatePmbusSummaryLine(const std::wstring& text) {
    SetScanSummaryText(text);
}

void CPmbusTab::SetScriptPathText() {
    if (::IsWindow(script_path_edit_.GetSafeHwnd())) {
        script_path_edit_.SetWindowTextW(script_doc_.path.c_str());
    }
}

void CPmbusTab::SetScriptResponseText(const std::wstring& text) {
    if (::IsWindow(script_response_edit_.GetSafeHwnd())) {
        script_response_edit_.SetWindowTextW(text.c_str());
    }
}

void CPmbusTab::AppendScriptResponse(const std::wstring& text) {
    if (!::IsWindow(script_response_edit_.GetSafeHwnd())) {
        return;
    }
    int len = script_response_edit_.GetWindowTextLengthW();
    script_response_edit_.SetSel(len, len);
    script_response_edit_.ReplaceSel((text + L"\r\n").c_str());
}

void CPmbusTab::UpdateScriptSummary() {
    size_t command_count = 0u;
    size_t pause_count = 0u;
    size_t read_count = 0u;
    size_t write_count = 0u;

    for (const auto& row : script_doc_.rows) {
        if (row.kind == mfc_tool::core::SmbusScriptRowKind::Command) {
            ++command_count;
            if (mfc_tool::core::SmbusScriptRowIsRead(row)) {
                ++read_count;
            }
            if (mfc_tool::core::SmbusScriptRowIsWrite(row)) {
                ++write_count;
            }
        } else if (row.kind == mfc_tool::core::SmbusScriptRowKind::Pause) {
            ++pause_count;
        }
    }

    SetScanSummaryText(L"Script rows=" + std::to_wstring(script_doc_.rows.size()) +
                       L" commands=" + std::to_wstring(command_count) +
                       L" pauses=" + std::to_wstring(pause_count) +
                       L" reads=" + std::to_wstring(read_count) +
                       L" writes=" + std::to_wstring(write_count));
}

void CPmbusTab::LoadScriptFromPath(const std::wstring& path) {
    mfc_tool::core::SmbusScriptDocument loaded;
    std::wstring error;

    if (!mfc_tool::core::LoadSmbusScriptCsv(path, &loaded, &error)) {
        throw std::runtime_error(WideToAnsiLossy(error));
    }
    script_doc_ = std::move(loaded);
    SetScriptPathText();
    PopulateScriptList();
    SetScriptResponseText(L"");
    UpdateScriptSummary();
    if (persist_settings_) {
        persist_settings_();
    }
}

void CPmbusTab::PopulateScriptList() {
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

bool CPmbusTab::ScriptMetadataPecEnabled() const {
    for (const auto& row : script_doc_.rows) {
        if (row.kind != mfc_tool::core::SmbusScriptRowKind::Comment) {
            continue;
        }
        for (const auto& field : row.fields) {
            std::wstring lower;
            for (wchar_t ch : field) {
                lower.push_back(static_cast<wchar_t>(towlower(ch)));
            }
            if (lower.find(L"pec=true") != std::wstring::npos) {
                return true;
            }
            if (lower.find(L"pec=false") != std::wstring::npos) {
                return false;
            }
        }
    }
    return false;
}

CPmbusTab::ExecResult CPmbusTab::ExecuteScriptCommandRow(const mfc_tool::core::SmbusScriptRow& row, bool pec) {
    const std::uint8_t addr = static_cast<std::uint8_t>(row.address & 0x7F);
    const std::uint8_t command = static_cast<std::uint8_t>(row.command & 0xFF);
    const int read_len = row.read_length > 0 ? row.read_length : 32;

    ThrowIfCancelRequested();
    if (row.kind != mfc_tool::core::SmbusScriptRowKind::Command) {
        return {};
    }

    switch (row.command_type) {
    case mfc_tool::core::SmbusScriptCommandType::QuickWrite:
    case mfc_tool::core::SmbusScriptCommandType::QuickRead: {
        ExecResult result;
        result.raw = service_->I2cMasterSmbusQuick(CurrentMasterPort(), addr,
                                                   row.command_type == mfc_tool::core::SmbusScriptCommandType::QuickRead);
        return result;
    }
    case mfc_tool::core::SmbusScriptCommandType::SendByte:
        if (row.command < 0) {
            throw std::invalid_argument("SendByte script row requires a register/command byte.");
        }
        return ExecSendByte(addr, {command}, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::ReceiveByte:
        return ExecReceiveByte(addr, pec);
    case mfc_tool::core::SmbusScriptCommandType::WriteByte:
        if (row.command < 0 || row.data.size() != 1u) {
            throw std::invalid_argument("WriteByte script row requires register and 1 data byte.");
        }
        return ExecWriteByCommand(addr, {command}, row.data, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::WriteWord:
        if (row.command < 0 || row.data.size() != 2u) {
            throw std::invalid_argument("WriteWord script row requires register and 2 data bytes.");
        }
        return ExecWriteByCommand(addr, {command}, row.data, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::ReadByte:
        if (row.command < 0) {
            throw std::invalid_argument("ReadByte script row requires a register/command byte.");
        }
        return ExecReadByCommand(addr, {command}, 1, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::ReadWord:
        if (row.command < 0) {
            throw std::invalid_argument("ReadWord script row requires a register/command byte.");
        }
        return ExecReadByCommand(addr, {command}, 2, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::Read32:
        if (row.command < 0) {
            throw std::invalid_argument("Read32 script row requires a register/command byte.");
        }
        return ExecReadByCommand(addr, {command}, 4, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::BlockWrite:
        if (row.command < 0 || row.data.size() > 255u) {
            throw std::invalid_argument("BlockWrite script row requires register and up to 255 data bytes.");
        }
        {
            std::vector<std::uint8_t> payload = {static_cast<std::uint8_t>(row.data.size() & 0xFFu)};
            payload.insert(payload.end(), row.data.begin(), row.data.end());
            return ExecWriteByCommand(addr, {command}, payload, pec, false);
        }
    case mfc_tool::core::SmbusScriptCommandType::BlockRead:
        if (row.command < 0) {
            throw std::invalid_argument("BlockRead script row requires a register/command byte.");
        }
        return ExecBlockReadCommand(addr, {command}, read_len, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::ProcessCall:
        if (row.command < 0 || row.data.size() != 2u) {
            throw std::invalid_argument("ProcessCall script row requires register and 2 data bytes.");
        }
        return ExecProcessCall(addr, {command}, row.data, pec, false);
    case mfc_tool::core::SmbusScriptCommandType::BlockWriteReadProcessCall:
        if (row.command < 0 || row.data.size() > 255u) {
            throw std::invalid_argument("BlockWriteReadProcessCall script row requires register and up to 255 data bytes.");
        }
        return ExecBlockWriteReadProcessCall(addr, {command}, row.data, read_len, pec, false);
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
            return ExecWriteByCommand(addr, {command}, row.data, true, true);
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (!IsExpectedBadPecBridgeStatus(e.status())) {
                throw;
            }
            ExecResult result;
            result.raw = {command};
            result.raw.insert(result.raw.end(), row.data.begin(), row.data.end());
            {
                std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1)};
                frame.insert(frame.end(), result.raw.begin(), result.raw.end());
                result.raw.push_back(mfc_tool::core::PmbusComputePec(frame) ^ 0xFFu);
            }
            result.data = {e.status()};
            return result;
        }
    default:
        throw std::invalid_argument("Unsupported script command type.");
    }
}

void CPmbusTab::FlushUiUpdates() {
    mfc_tool::ui::UpdateWindowsAndPumpPaint({
        &raw_rx_edit_,
        &decoded_edit_,
        &script_path_edit_,
        &script_list_,
        &script_response_edit_,
        &scan_summary_edit_,
        &illegal_result_edit_,
        &stop_btn_,
        &checklist_progress_
    });
    mfc_tool::ui::PumpControlMessages(stop_btn_);
}

void CPmbusTab::RequestCancel() {
    if (!checklist_running_ && !script_running_) {
        return;
    }
    cancel_requested_ = true;
    mfc_tool::ui::SafeEnableWindow(stop_btn_, FALSE);
    if (checklist_running_) {
        SetIllegalTestResultText(L"Stopping after current PMBus transaction...");
        SetDecodedText(L"Stopping after current PMBus transaction...");
    } else {
        AppendScriptResponse(L"Stopping after current script transaction...");
    }
    FlushUiUpdates();
}

void CPmbusTab::ResetCancel() {
    cancel_requested_ = false;
}

void CPmbusTab::ThrowIfCancelRequested() {
    FlushUiUpdates();
    if (cancel_requested_) {
        throw UserCancelled();
    }
}

bool CPmbusTab::SleepWithCancel(int delay_ms) {
    int remaining = (std::max)(0, delay_ms);
    while (remaining > 0) {
        const int chunk = (std::min)(remaining, 25);
        ::Sleep(static_cast<DWORD>(chunk));
        remaining -= chunk;
        ThrowIfCancelRequested();
    }
    return !cancel_requested_;
}

int CPmbusTab::ParseEditInt(const CEdit& edit) const {
    CString s;
    const_cast<CEdit&>(edit).GetWindowTextW(s);
    return mfc_tool::core::ParseInt(s.GetString());
}

std::wstring CPmbusTab::GetEditText(const CEdit& edit) const {
    CString s;
    const_cast<CEdit&>(edit).GetWindowTextW(s);
    return s.GetString();
}

DWORD CPmbusTab::ChecklistCommandDelayMs() const {
    try {
        const int delay_ms = ParseEditInt(checklist_delay_edit_);
        if (delay_ms < 0) {
            return kPmbusDefaultChecklistCommandDelayMs;
        }
        return static_cast<DWORD>((std::min)(delay_ms, static_cast<int>(kPmbusMaxChecklistCommandDelayMs)));
    } catch (const std::exception&) {
        return kPmbusDefaultChecklistCommandDelayMs;
    }
}

int CPmbusTab::ChecklistRepeatCount() const {
    try {
        const int repeat_count = ParseEditInt(checklist_repeat_edit_);
        if (repeat_count <= 0) {
            return kPmbusDefaultChecklistRepeatCount;
        }
        return (std::min)(repeat_count, kPmbusMaxChecklistRepeatCount);
    } catch (const std::exception&) {
        return kPmbusDefaultChecklistRepeatCount;
    }
}

void CPmbusTab::SleepAfterChecklistCommand() {
    if (checklist_running_) {
        SleepWithCancel(static_cast<int>(ChecklistCommandDelayMs()));
    }
}

std::uint8_t CPmbusTab::ParseCommandCode() const {
    return static_cast<std::uint8_t>(ParseEditInt(command_code_edit_) & 0xFF);
}

bool CPmbusTab::ExtendedModeEnabled() const {
    return ext_check_.GetCheck() == BST_CHECKED;
}

bool CPmbusTab::ForceBadPecEnabled() const {
    return pec_check_.GetCheck() == BST_CHECKED && bad_pec_check_.GetCheck() == BST_CHECKED;
}

int CPmbusTab::EstimateChecklistProgressTotal(int mode) const {
    switch (mode) {
    case kChecklistModeBasic:
        return 18;
    case kChecklistModePec:
        return 4;
    case kChecklistModeError:
        return 2;
    case kChecklistModeTelemetry:
        return 17;
    case kChecklistModeMfr:
        return IsCrpsProfile() ? 8 : 7;
    case kChecklistModeFull:
        return IsCrpsProfile() ? 64 : 62;
    default:
        return 1;
    }
}

std::wstring CPmbusTab::CurrentExtendedTypeText() const {
    return ext_type_combo_.GetCurSel() == 1 ? L"PMBUS_EXT (0xFF)" : L"MFR_EXT (0xFE)";
}

std::vector<std::uint8_t> CPmbusTab::CurrentCommandBytes() const {
    std::vector<std::uint8_t> command_bytes;
    if (ExtendedModeEnabled()) {
        command_bytes.push_back(ext_type_combo_.GetCurSel() == 1 ? 0xFFu : 0xFEu);
    }
    command_bytes.push_back(ParseCommandCode());
    return command_bytes;
}

std::vector<std::uint8_t> CPmbusTab::ParseTxHex(const CEdit& edit) const {
    return mfc_tool::core::ParseHexBytes(GetEditText(edit));
}

mfc_tool::core::PmbusTransactionType CPmbusTab::CurrentTransactionType() const {
    const int sel = transaction_combo_.GetCurSel();
    if (sel == CB_ERR) {
        return mfc_tool::core::PmbusTransactionType::SendByte;
    }
    return static_cast<mfc_tool::core::PmbusTransactionType>(transaction_combo_.GetItemData(sel));
}

void CPmbusTab::PopulatePresetCombo() {
    static const wchar_t* kCommonPresetNames[] = {
        L"PAGE",
        L"OPERATION",
        L"CLEAR_FAULTS",
        L"VOUT_MODE",
        L"VOUT_COMMAND",
        L"READ_VOUT",
        L"READ_IOUT",
        L"STATUS_BYTE",
        L"STATUS_WORD",
        L"STATUS_VOUT",
        L"STATUS_IOUT",
        L"STATUS_INPUT",
        L"STATUS_TEMPERATURE",
        L"STATUS_CML",
        L"STATUS_OTHER",
        L"STATUS_MFR_SPECIFIC",
        L"MFR_ID",
        L"MFR_MODEL",
        L"MFR_REVISION",
        L"MFR_LOCATION",
        L"MFR_DATE",
        L"MFR_SERIAL",
        L"PMBUS_REVISION",
        L"CAPABILITY",
        L"QUERY",
    };

    auto add_preset = [this](const mfc_tool::core::PmbusCommandPreset* preset) {
        if (preset == nullptr) {
            return;
        }
        if (!IsPresetVisibleForProfile(*preset)) {
            return;
        }
        for (int i = 0; i < command_preset_combo_.GetCount(); ++i) {
            if (static_cast<std::uint8_t>(command_preset_combo_.GetItemData(i) & 0xFFu) == preset->code) {
                return;
            }
        }
        {
            const std::wstring label = FormatPresetComboLabel(*preset, profile_);
            const int idx = command_preset_combo_.AddString(label.c_str());
            command_preset_combo_.SetItemData(idx, preset->code);
        }
    };

    command_preset_combo_.ResetContent();
    for (const wchar_t* name : kCommonPresetNames) {
        add_preset(mfc_tool::core::FindPmbusCommandPresetByName(name));
    }
    for (const auto& preset : mfc_tool::core::PmbusCommandPresets()) {
        add_preset(&preset);
    }
    if (command_preset_combo_.GetCount() > 0) {
        command_preset_combo_.SetCurSel(0);
    }
}

int CPmbusTab::CurrentSpeedHz() const {
    CString speed_text;
    speed_combo_.GetWindowTextW(speed_text);
    return mfc_tool::core::ParseInt(speed_text.GetString());
}

int CPmbusTab::CurrentMasterPort() const {
    const int sel = master_port_combo_.GetCurSel();
    if (sel != CB_ERR) {
        return static_cast<int>(master_port_combo_.GetItemData(sel));
    }
    return 0;
}

const mfc_tool::core::board_i2c::PinPair& CPmbusTab::CurrentMasterPinPair() const {
    const auto& pairs = mfc_tool::core::board_i2c::AllPinPairs();
    const int sel = master_pins_combo_.GetCurSel();
    if (sel != CB_ERR) {
        const auto idx = static_cast<size_t>(master_pins_combo_.GetItemData(sel));
        if (idx < pairs.size()) {
            return pairs[idx];
        }
    }
    const auto* fallback = mfc_tool::core::board_i2c::DefaultPinPair(CurrentMasterPort());
    return fallback != nullptr ? *fallback : pairs.front();
}

void CPmbusTab::PopulatePortCombos() {
    auto fill = [this](CComboBox& combo, int selected_port) {
        combo.ResetContent();
        int idx = combo.AddString(L"I2C0");
        combo.SetItemData(idx, 0);
        idx = combo.AddString(L"I2C1");
        combo.SetItemData(idx, 1);
        combo.SetCurSel(selected_port == 1 ? 1 : 0);
    };
    fill(master_port_combo_, 0);
    PopulateMasterPinCombo();
}

void CPmbusTab::PopulateMasterPinCombo(const std::wstring& preferred_ini_name) {
    const int port = CurrentMasterPort();
    int selected = 0;
    int row = 0;

    master_pins_combo_.ResetContent();
    const auto& pairs = mfc_tool::core::board_i2c::AllPinPairs();
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (pairs[i].i2c_port != port) {
            continue;
        }
        const int idx = master_pins_combo_.AddString(pairs[i].label);
        master_pins_combo_.SetItemData(idx, static_cast<DWORD_PTR>(i));
        if (preferred_ini_name == pairs[i].ini_name) {
            selected = row;
        }
        ++row;
    }
    if (master_pins_combo_.GetCount() > 0) {
        master_pins_combo_.SetCurSel(selected);
    }
}

void CPmbusTab::ApplyPresetToCommandUi() {
    const int sel = command_preset_combo_.GetCurSel();
    if (sel == CB_ERR) {
        return;
    }
    const auto code = static_cast<std::uint8_t>(command_preset_combo_.GetItemData(sel) & 0xFFu);
    const auto* preset = mfc_tool::core::FindPmbusCommandPresetByCode(code);
    wchar_t buf[8] = {};
    swprintf_s(buf, L"0x%02X", static_cast<unsigned int>(code));
    command_code_edit_.SetWindowTextW(buf);
    if (preset != nullptr) {
        mfc_tool::core::PmbusTransactionType txn = preset->preferred_txn;
        if (IsTiProfile()) {
            txn = TiUcd90xxxDefaultTransaction(code, txn);
        } else if (!IsCrpsProfile() && IsBaseGenericNamespaceCode(code)) {
            txn = mfc_tool::core::PmbusTransactionType::BlockRead;
        }
        transaction_combo_.SetCurSel(static_cast<int>(txn));
        if (txn == mfc_tool::core::PmbusTransactionType::ReadByte) {
            read_len_edit_.SetWindowTextW(L"1");
        } else if (txn == mfc_tool::core::PmbusTransactionType::Read32) {
            read_len_edit_.SetWindowTextW(L"4");
        } else if (txn == mfc_tool::core::PmbusTransactionType::ReadWord ||
                   txn == mfc_tool::core::PmbusTransactionType::ProcessCall) {
            read_len_edit_.SetWindowTextW(L"2");
        } else if (txn == mfc_tool::core::PmbusTransactionType::BlockRead) {
            read_len_edit_.SetWindowTextW(L"16");
        }
    }
}

void CPmbusTab::ReinitMasterBusForRetry() {
    ThrowIfCancelRequested();
    if (service_ == nullptr || !master_enabled_) {
        return;
    }
    const auto& pins = CurrentMasterPinPair();
    service_->I2cDeinit(CurrentMasterPort());
    service_->I2cMasterInit(CurrentMasterPort(), pins.sda_pin, pins.scl_pin, CurrentSpeedHz());
    SleepWithCancel(2);
}

bool CPmbusTab::ValidateCurrentTransaction(std::uint8_t command, mfc_tool::core::PmbusTransactionType txn) const {
    std::wstring allowed_text;
    if ((IsTiProfile() && IsTiUcd90xxxSpecificCode(command)) ||
        (!IsCrpsProfile() && IsBaseGenericNamespaceCode(command))) {
        return true;
    }
    if (!mfc_tool::core::PmbusTransactionAllowed(command, txn, ExtendedModeEnabled(), &allowed_text)) {
        std::wstring mode = ExtendedModeEnabled() ? (L"Extended " + CurrentExtendedTypeText()) : L"Standard";
        std::wstring msg = mode + L" command does not allow \"" +
                           mfc_tool::core::PmbusTransactionTypeText(txn) +
                           L"\".\r\nAllowed: " + allowed_text;
        ::MessageBoxW(m_hWnd, msg.c_str(), L"PMBus Transaction Error", MB_ICONERROR | MB_OK);
        return false;
    }
    return true;
}

CPmbusTab::ExecResult CPmbusTab::ExecReceiveByte(std::uint8_t addr, bool pec) {
    int attempt = 0;

    ThrowIfCancelRequested();
    for (attempt = 0; attempt < kPmbusRetryAttempts; ++attempt) {
        try {
            ThrowIfCancelRequested();
            ReinitMasterBusForRetry();
            ExecResult result;
            auto rx = service_->I2cMasterRead(CurrentMasterPort(), addr, pec ? 2 : 1);
            ThrowIfCancelRequested();
            const int count = rx.empty() ? 0 : static_cast<int>(rx[0]);
            if (count <= 0) {
                SleepAfterChecklistCommand();
                return result;
            }
            const size_t size = (std::min)(static_cast<size_t>(count), rx.size() > 1 ? rx.size() - 1 : static_cast<size_t>(0));
            result.raw.assign(rx.begin() + 1, rx.begin() + 1 + size);
            result.data = result.raw;
            if (pec && result.raw.size() >= 2u) {
                result.pec_checked = true;
                result.pec_rx = result.raw.back();
                result.data.pop_back();
                std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>((addr << 1) | 1u)};
                frame.insert(frame.end(), result.data.begin(), result.data.end());
                result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
                result.pec_ok = (result.pec_calc == result.pec_rx);
            }

            if (attempt < (kPmbusRetryAttempts - 1) && result.pec_checked && !result.pec_ok) {
                if (log_) {
                    log_(L"PMBus receive-byte transient PEC mismatch, retrying");
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }

            SleepAfterChecklistCommand();
            return result;
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (attempt < (kPmbusRetryAttempts - 1) && e.status() == 0x04u) {
                if (log_) {
                    log_(L"PMBus receive-byte transient IO_ERROR, retrying");
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }
            throw;
        }
    }

    throw std::runtime_error("PMBus receive byte retry exhausted");
}

CPmbusTab::ExecResult CPmbusTab::ExecSendByte(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes, bool pec, bool force_bad_pec) {
    ExecResult result;
    std::vector<std::uint8_t> tx = command_bytes;
    ThrowIfCancelRequested();
    if (pec) {
        std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1)};
        frame.insert(frame.end(), command_bytes.begin(), command_bytes.end());
        tx.push_back(mfc_tool::core::PmbusComputePec(frame) ^ (force_bad_pec ? 0xFFu : 0x00u));
    }
    service_->I2cMasterWrite(CurrentMasterPort(), addr, tx);
    ThrowIfCancelRequested();
    result.raw = tx;
    SleepAfterChecklistCommand();
    return result;
}

CPmbusTab::ExecResult CPmbusTab::ExecReadByCommand(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes, int read_len, bool pec, bool force_bad_pec) {
    int attempt = 0;

    ThrowIfCancelRequested();
    for (attempt = 0; attempt < kPmbusRetryAttempts; ++attempt) {
        try {
            ThrowIfCancelRequested();
            ReinitMasterBusForRetry();
            ExecResult result;
            std::vector<std::uint8_t> tx = command_bytes;
            // PMBus repeated-start read commands carry PEC only in the read response.
            result.raw = service_->I2cMasterWriteThenReadRaw(CurrentMasterPort(), addr, true, tx, read_len + (pec ? 1 : 0));
            ThrowIfCancelRequested();
            if (result.raw.empty()) {
                SleepAfterChecklistCommand();
                return result;
            }
            result.data = result.raw;
            if (pec && !result.raw.empty()) {
                result.pec_checked = true;
                result.pec_rx = result.raw.back();
                result.data.pop_back();
                std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1)};
                frame.insert(frame.end(), command_bytes.begin(), command_bytes.end());
                frame.push_back(static_cast<std::uint8_t>((addr << 1) | 1u));
                frame.insert(frame.end(), result.data.begin(), result.data.end());
                result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
                result.pec_ok = (result.pec_calc == result.pec_rx);
            }

            if (!force_bad_pec && attempt < (kPmbusRetryAttempts - 1) && result.pec_checked && !result.pec_ok) {
                if (log_) {
                    std::wstringstream ss;
                    ss << L"PMBus read transient PEC mismatch, retrying"
                       << L" raw=" << mfc_tool::core::HexDump(result.raw)
                       << L" rx=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
                       << static_cast<unsigned int>(result.pec_rx)
                       << L" calc=0x" << std::setw(2) << static_cast<unsigned int>(result.pec_calc);
                    log_(ss.str());
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }
            if (!force_bad_pec && result.pec_checked && !result.pec_ok) {
                std::ostringstream ss;
                ss << "PMBus read PEC mismatch: raw=";
                for (size_t i = 0; i < result.raw.size(); ++i) {
                    if (i != 0u) {
                        ss << ' ';
                    }
                    ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned int>(result.raw[i]);
                }
                ss << " rx=0x" << std::setw(2) << static_cast<unsigned int>(result.pec_rx)
                   << " calc=0x" << std::setw(2) << static_cast<unsigned int>(result.pec_calc);
                throw std::runtime_error(ss.str());
            }

            SleepAfterChecklistCommand();
            return result;
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (!force_bad_pec && attempt < (kPmbusRetryAttempts - 1) && e.status() == 0x04u) {
                if (log_) {
                    log_(L"PMBus read transient IO_ERROR, retrying");
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }
            throw;
        }
    }

    throw std::runtime_error("PMBus read retry exhausted");
}

CPmbusTab::ExecResult CPmbusTab::ExecBlockReadCommand(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes, int max_read_len, bool pec, bool force_bad_pec) {
    int attempt = 0;

    ThrowIfCancelRequested();
    for (attempt = 0; attempt < kPmbusRetryAttempts; ++attempt) {
        try {
            ThrowIfCancelRequested();
            ReinitMasterBusForRetry();
            ExecResult result;
            std::vector<std::uint8_t> tx = command_bytes;
            const int capped_max = (std::max)(1, (std::min)(max_read_len, 32));

            result.raw = service_->I2cMasterPmbusBlockReadRaw(CurrentMasterPort(), addr, tx, pec, capped_max);
            ThrowIfCancelRequested();
            if (result.raw.empty()) {
                SleepAfterChecklistCommand();
                return result;
            }

            if (result.raw.size() < 1u) {
                throw std::runtime_error("PMBus block read returned no block-count byte");
            }

            result.data = result.raw;
            if (pec) {
                if (result.data.size() < 2u) {
                    throw std::runtime_error("PMBus block read missing PEC byte");
                }
                result.pec_checked = true;
                result.pec_rx = result.data.back();
                result.data.pop_back();
            }

            {
                const std::uint8_t block_count = result.data[0];
                if (block_count > static_cast<std::uint8_t>(capped_max)) {
                    throw std::runtime_error("PMBus block read count exceeds requested max length");
                }
                if (result.data.size() != 1u + static_cast<size_t>(block_count)) {
                    throw std::runtime_error("PMBus block read length does not match returned block-count byte");
                }
                if (pec) {
                    std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1)};
                    frame.insert(frame.end(), command_bytes.begin(), command_bytes.end());
                    frame.push_back(static_cast<std::uint8_t>((addr << 1) | 1u));
                    frame.insert(frame.end(), result.data.begin(), result.data.end());
                    result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
                    result.pec_ok = (result.pec_calc == result.pec_rx);
                }
                result.data.erase(result.data.begin());
            }

            if (!force_bad_pec && attempt < (kPmbusRetryAttempts - 1) && result.pec_checked && !result.pec_ok) {
                if (log_) {
                    std::wstringstream ss;
                    ss << L"PMBus block read transient PEC mismatch, retrying"
                       << L" raw=" << mfc_tool::core::HexDump(result.raw)
                       << L" rx=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
                       << static_cast<unsigned int>(result.pec_rx)
                       << L" calc=0x" << std::setw(2) << static_cast<unsigned int>(result.pec_calc);
                    log_(ss.str());
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }

            SleepAfterChecklistCommand();
            return result;
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (!force_bad_pec && attempt < (kPmbusRetryAttempts - 1) && e.status() == 0x04u) {
                if (log_) {
                    log_(L"PMBus block read transient IO_ERROR, retrying");
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }
            throw;
        }
    }

    throw std::runtime_error("PMBus block read retry exhausted");
}

CPmbusTab::ExecResult CPmbusTab::ExecWriteByCommand(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes,
                                                    const std::vector<std::uint8_t>& data, bool pec, bool force_bad_pec) {
    ExecResult result;
    std::vector<std::uint8_t> tx = command_bytes;
    ThrowIfCancelRequested();
    tx.insert(tx.end(), data.begin(), data.end());
    if (pec) {
        std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1)};
        frame.insert(frame.end(), tx.begin(), tx.end());
        tx.push_back(mfc_tool::core::PmbusComputePec(frame) ^ (force_bad_pec ? 0xFFu : 0x00u));
    }
    service_->I2cMasterWrite(CurrentMasterPort(), addr, tx);
    ThrowIfCancelRequested();
    result.raw = tx;
    SleepAfterChecklistCommand();
    return result;
}

CPmbusTab::ExecResult CPmbusTab::ExecProcessCall(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes,
                                                 const std::vector<std::uint8_t>& data,
                                                 bool pec, bool force_bad_pec) {
    int attempt = 0;

    ThrowIfCancelRequested();
    for (attempt = 0; attempt < kPmbusRetryAttempts; ++attempt) {
        try {
            ThrowIfCancelRequested();
            ReinitMasterBusForRetry();
            ExecResult result;
            std::vector<std::uint8_t> tx = command_bytes;
            tx.push_back(data[0]);
            tx.push_back(data[1]);
            // Process Call is also a repeated-start read; the PEC is returned after the read word.
            result.raw = service_->I2cMasterWriteThenReadRaw(CurrentMasterPort(), addr, true, tx, 2 + (pec ? 1 : 0));
            ThrowIfCancelRequested();
            result.data = result.raw;
            if (pec && !result.raw.empty()) {
                result.pec_checked = true;
                result.pec_rx = result.raw.back();
                result.data.pop_back();
                std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1)};
                frame.insert(frame.end(), command_bytes.begin(), command_bytes.end());
                frame.push_back(data[0]);
                frame.push_back(data[1]);
                frame.push_back(static_cast<std::uint8_t>((addr << 1) | 1u));
                frame.insert(frame.end(), result.data.begin(), result.data.end());
                result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
                result.pec_ok = (result.pec_calc == result.pec_rx);
            }

            if (!force_bad_pec && attempt < (kPmbusRetryAttempts - 1) && result.pec_checked && !result.pec_ok) {
                if (log_) {
                    log_(L"PMBus process-call transient PEC mismatch, retrying");
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }

            SleepAfterChecklistCommand();
            return result;
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (!force_bad_pec && attempt < (kPmbusRetryAttempts - 1) && e.status() == 0x04u) {
                if (log_) {
                    log_(L"PMBus process-call transient IO_ERROR, retrying");
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }
            throw;
        }
    }

    throw std::runtime_error("PMBus process call retry exhausted");
}

CPmbusTab::ExecResult CPmbusTab::ExecBlockWriteReadProcessCall(std::uint8_t addr, const std::vector<std::uint8_t>& command_bytes,
                                                               const std::vector<std::uint8_t>& data,
                                                               int max_read_len, bool pec, bool force_bad_pec) {
    int attempt = 0;

    ThrowIfCancelRequested();
    for (attempt = 0; attempt < kPmbusRetryAttempts; ++attempt) {
        try {
            ThrowIfCancelRequested();
            ExecResult result;
            std::vector<std::uint8_t> tx = command_bytes;
            ReinitMasterBusForRetry();
            tx.push_back(static_cast<std::uint8_t>(data.size() & 0xFFu));
            tx.insert(tx.end(), data.begin(), data.end());
            // Block Write-Read Process Call returns one PEC at the end of the read phase.
            result.raw = service_->I2cMasterWriteThenReadRaw(CurrentMasterPort(), addr, true, tx, 1 + max_read_len + (pec ? 1 : 0));
            ThrowIfCancelRequested();
            if (result.raw.empty()) {
                SleepAfterChecklistCommand();
                return result;
            }
            result.data = result.raw;
            if (result.data.empty()) {
                SleepAfterChecklistCommand();
                return result;
            }
            {
                const std::uint8_t block_count = result.data[0];
                size_t payload_end = 1u + static_cast<size_t>(block_count);
                if (pec) {
                    result.pec_checked = true;
                    if (result.raw.size() < payload_end + 1u) {
                        throw std::runtime_error("block process call payload too short for PEC");
                    }
                    result.pec_rx = result.raw[payload_end];
                    std::vector<std::uint8_t> frame = {static_cast<std::uint8_t>(addr << 1)};
                    frame.insert(frame.end(), command_bytes.begin(), command_bytes.end());
                    frame.push_back(static_cast<std::uint8_t>(data.size() & 0xFFu));
                    frame.insert(frame.end(), data.begin(), data.end());
                    frame.push_back(static_cast<std::uint8_t>((addr << 1) | 1u));
                    frame.insert(frame.end(), result.raw.begin(), result.raw.begin() + payload_end);
                    result.pec_calc = mfc_tool::core::PmbusComputePec(frame);
                    result.pec_ok = (result.pec_calc == result.pec_rx);
                }
                if (result.data.size() > payload_end) {
                    result.data.resize(payload_end);
                }
                if (!result.data.empty()) {
                    result.data.erase(result.data.begin());
                }
            }

            if (!force_bad_pec && attempt < (kPmbusRetryAttempts - 1) && result.pec_checked && !result.pec_ok) {
                if (log_) {
                    log_(L"PMBus block-process-call transient PEC mismatch, retrying");
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }

            SleepAfterChecklistCommand();
            return result;
        } catch (const mfc_tool::hid::BridgeStatusException& e) {
            if (!force_bad_pec && attempt < (kPmbusRetryAttempts - 1) && e.status() == 0x04u) {
                if (log_) {
                    log_(L"PMBus block-process-call transient IO_ERROR, retrying");
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                continue;
            }
            throw;
        }
    }

    throw std::runtime_error("PMBus block process call retry exhausted");
}

std::wstring CPmbusTab::BuildDecodedText(std::uint8_t command, bool extended_mode, const ExecResult& result) {
    const auto* preset = (extended_mode ||
                          (IsTiProfile() && IsTiUcd90xxxSpecificCode(command)) ||
                          (!IsCrpsProfile() && IsBaseGenericNamespaceCode(command)))
                             ? nullptr
                             : mfc_tool::core::FindPmbusCommandPresetByCode(command);
    auto decoded = mfc_tool::core::DecodePmbusPayload(preset,
                                                      result.data,
                                                      cached_vout_mode_,
                                                      result.pec_checked,
                                                      result.pec_ok);
    if (preset != nullptr && preset->code == 0x20u && !result.data.empty()) {
        std::int8_t exp = -9;
        cached_vout_mode_ = result.data[0];
        if (mfc_tool::core::TryParseVoutModeExponent(result.data[0], &exp)) {
            cached_vout_mode_exponent_ = exp;
        }
    }
    if (result.pec_checked) {
        std::wstringstream ss;
        ss << decoded
           << L" | PEC(rx=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
           << static_cast<unsigned int>(result.pec_rx)
           << L", calc=0x" << std::setw(2) << static_cast<unsigned int>(result.pec_calc) << L")";
        return ss.str();
    }
    return decoded;
}

void CPmbusTab::ExecuteAraHelper(bool from_recovery) {
    if (!IsLabValidationPolicy()) {
        throw std::runtime_error("ARA helper is disabled by Production policy. Switch System Policy to Lab validation to run ARA.");
    }
    auto result = ExecReceiveByte(static_cast<std::uint8_t>(kPmbusAraAddr), pec_check_.GetCheck() == BST_CHECKED);
    SetRawRxText(mfc_tool::core::HexDump(result.raw));
    if (!result.data.empty()) {
        const std::uint8_t responder = static_cast<std::uint8_t>(result.data[0] >> 1);
        std::wstringstream ss;
        ss << L"ARA responder address=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
           << static_cast<unsigned int>(responder);
        SetDecodedText(ss.str());
        if (log_) {
            log_(from_recovery ? (L"PMBus ARA recovery: " + ss.str())
                               : (L"PMBus ARA: " + ss.str()));
        }
    } else {
        SetDecodedText(L"ARA returned no data");
        if (log_) {
            log_(from_recovery ? L"PMBus ARA recovery returned no data"
                               : L"PMBus ARA returned no data");
        }
    }
}

std::wstring CPmbusTab::AnsiToWide(const char* text) {
    if (text == nullptr) {
        return L"";
    }
    int n = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (n <= 0) {
        return L"";
    }
    std::wstring out(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, out.data(), n);
    return out;
}

void CPmbusTab::OnMasterEnable() {
    if (!connected_ || service_ == nullptr) {
        return;
    }
    try {
        const std::string profile_name = IsCrpsProfile() ? "M-CRPS" : (IsTiProfile() ? "TI UCD90xxx" : "PMBus");
        const std::wstring master_owner = MasterOwnerId();
        if (OtherSharedProfileActive()) {
            throw std::runtime_error("Another PMBus/SMBus function is already active on shared pins. Disable it first.");
        }
        const int master_port = CurrentMasterPort();
        const auto& master_pins = CurrentMasterPinPair();
        const std::wstring master_port_name = mfc_tool::core::board_i2c::PortLabel(master_port);
        if (pin_usage_ != nullptr && pin_usage_->IsActive(master_port_name)) {
            throw std::runtime_error("Selected I2C port is already active. Disable it first before enabling " + profile_name + " master.");
        }
        if (pin_usage_ != nullptr &&
            pin_usage_->AnyPinOccupied({master_pins.sda_pin, master_pins.scl_pin}, {master_owner})) {
            throw std::runtime_error(profile_name + " master requires the selected I2C pins. Disable the conflicting active function first.");
        }
        CString speed_text;
        speed_combo_.GetWindowTextW(speed_text);
        const int speed = mfc_tool::core::ParseInt(speed_text.GetString());
        service_->I2cMasterInit(master_port, master_pins.sda_pin, master_pins.scl_pin, speed);
        master_enabled_ = true;
        if (log_) {
            log_(std::wstring(ProfileDisplayName()) + L" master enabled on " + master_pins.label + L" at " + std::to_wstring(speed) + L"Hz");
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), ProfileErrorTitle(), MB_ICONERROR | MB_OK);
    }
    UpdateEnableState();
}

void CPmbusTab::OnMasterDisable() {
    if (!connected_ || service_ == nullptr) {
        return;
    }
    try {
        service_->I2cDeinit(CurrentMasterPort());
        master_enabled_ = false;
        if (log_) {
            log_(std::wstring(ProfileDisplayName()) + L" master disabled");
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), ProfileErrorTitle(), MB_ICONERROR | MB_OK);
    }
    UpdateEnableState();
}

void CPmbusTab::OnExecute() {
    if (!connected_ || service_ == nullptr || !master_enabled_) {
        return;
    }
    try {
        const std::uint8_t addr = static_cast<std::uint8_t>(ParseEditInt(master_addr_edit_) & 0x7F);
        const std::uint8_t command = ParseCommandCode();
        const bool pec = (pec_check_.GetCheck() == BST_CHECKED);
        const bool force_bad_pec = ForceBadPecEnabled();
        const auto txn = CurrentTransactionType();
        const auto data = ParseTxHex(tx_hex_edit_);
        const int read_len = (std::max)(0, ParseEditInt(read_len_edit_));
        const bool extended_mode = ExtendedModeEnabled();
        const auto command_bytes = CurrentCommandBytes();

        if (!ValidateCurrentTransaction(command, txn)) {
            return;
        }

        ExecResult result;
        switch (txn) {
        case mfc_tool::core::PmbusTransactionType::SendByte:
            result = ExecSendByte(addr, command_bytes, pec, force_bad_pec);
            break;
        case mfc_tool::core::PmbusTransactionType::ReceiveByte:
            if (extended_mode) {
                throw std::invalid_argument("Extended command mode does not support Receive Byte.");
            }
            result = ExecReceiveByte(addr, pec);
            break;
        case mfc_tool::core::PmbusTransactionType::WriteByte:
            if (data.size() != 1u) {
                throw std::invalid_argument("Write Byte requires exactly 1 data byte.");
            }
            result = ExecWriteByCommand(addr, command_bytes, data, pec, force_bad_pec);
            break;
        case mfc_tool::core::PmbusTransactionType::WriteWord:
            if (data.size() != 2u) {
                throw std::invalid_argument("Write Word requires exactly 2 data bytes.");
            }
            result = ExecWriteByCommand(addr, command_bytes, data, pec, force_bad_pec);
            break;
        case mfc_tool::core::PmbusTransactionType::ReadByte:
            result = ExecReadByCommand(addr, command_bytes, 1, pec, force_bad_pec);
            break;
        case mfc_tool::core::PmbusTransactionType::ReadWord:
            result = ExecReadByCommand(addr, command_bytes, 2, pec, force_bad_pec);
            break;
        case mfc_tool::core::PmbusTransactionType::Read32:
            result = ExecReadByCommand(addr, command_bytes, 4, pec, force_bad_pec);
            break;
        case mfc_tool::core::PmbusTransactionType::BlockWrite:
            if (data.size() > 255u) {
                throw std::invalid_argument("Block Write supports up to 255 data bytes.");
            }
            {
                std::vector<std::uint8_t> payload = {static_cast<std::uint8_t>(data.size() & 0xFFu)};
                payload.insert(payload.end(), data.begin(), data.end());
                result = ExecWriteByCommand(addr, command_bytes, payload, pec, force_bad_pec);
            }
            break;
        case mfc_tool::core::PmbusTransactionType::BlockRead:
            result = ExecBlockReadCommand(addr, command_bytes, read_len, pec, force_bad_pec);
            break;
        case mfc_tool::core::PmbusTransactionType::ProcessCall:
            if (data.size() != 2u) {
                throw std::invalid_argument("Process Call requires exactly 2 write data bytes.");
            }
            result = ExecProcessCall(addr, command_bytes, data, pec, force_bad_pec);
            break;
        case mfc_tool::core::PmbusTransactionType::BlockWriteReadProcessCall:
            if (data.size() > 255u) {
                throw std::invalid_argument("Block Write/Read Process Call supports up to 255 write data bytes.");
            }
            result = ExecBlockWriteReadProcessCall(addr, command_bytes, data, read_len, pec, force_bad_pec);
            break;
        default:
            throw std::invalid_argument("Unsupported PMBus transaction.");
        }

        SetRawRxText(mfc_tool::core::HexDump(result.raw));
        const std::wstring decoded = BuildDecodedText(command, extended_mode, result);
        SetDecodedText(decoded);
        UpdatePmbusSummaryLine((extended_mode ? (CurrentExtendedTypeText() + L" cmd=0x") : L"cmd=0x") +
                               GetEditText(command_code_edit_) + L" | " + mfc_tool::core::PmbusTransactionTypeText(txn));

        if (log_) {
            log_(L"PMBus " + mfc_tool::core::PmbusTransactionTypeText(txn) +
                 L" addr=" + GetEditText(master_addr_edit_) +
                 (extended_mode ? (L" ext=" + CurrentExtendedTypeText()) : L"") +
                 L" cmd=" + GetEditText(command_code_edit_) +
                 (force_bad_pec ? L" bad-pec=ON" : L"") +
                 L" raw=" + mfc_tool::core::HexDump(result.raw));
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"PMBus Error", MB_ICONERROR | MB_OK);
    }
}

void CPmbusTab::OnScriptLoad() {
    CFileDialog dlg(TRUE, L"csv", nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    L"CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) {
        return;
    }
    try {
        LoadScriptFromPath(dlg.GetPathName().GetString());
        if (log_) {
            log_(L"PMBus SMBus script loaded: " + script_doc_.path);
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"PMBus Script", MB_ICONERROR | MB_OK);
    }
    UpdateEnableState();
}

void CPmbusTab::OnScriptSelectAll() {
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

void CPmbusTab::OnScriptListChanged(NMHDR* pNMHDR, LRESULT* pResult) {
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

void CPmbusTab::OnScriptRun() {
    if (!connected_ || service_ == nullptr || !master_enabled_) {
        return;
    }
    if (script_doc_.rows.empty() && !script_doc_.path.empty()) {
        try {
            LoadScriptFromPath(script_doc_.path);
        } catch (const std::exception& e) {
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"PMBus Script", MB_ICONERROR | MB_OK);
            return;
        }
    }
    if (script_doc_.rows.empty()) {
        ::MessageBoxW(m_hWnd, L"Load an SMBus script CSV before running.", L"PMBus Script", MB_ICONWARNING | MB_OK);
        return;
    }

    ResetCancel();
    script_running_ = true;
    UpdateEnableState();
    try {
        const bool metadata_pec = ScriptMetadataPecEnabled();
        size_t command_count = 0u;
        size_t pause_count = 0u;
        size_t read_count = 0u;
        size_t write_count = 0u;
        size_t selected_count = 0u;
        size_t progress_done = 0u;
        size_t index = 0u;
        ExecResult last_result;

        for (const auto& row : script_doc_.rows) {
            if (row.selected &&
                (row.kind == mfc_tool::core::SmbusScriptRowKind::Pause ||
                 row.kind == mfc_tool::core::SmbusScriptRowKind::Command)) {
                ++selected_count;
            }
        }
        if (selected_count == 0u) {
            ::MessageBoxW(m_hWnd, L"Select at least one executable script row before running.", L"PMBus Script", MB_ICONWARNING | MB_OK);
            script_running_ = false;
            UpdateEnableState();
            return;
        }
        SetScriptResponseText(L"");
        mfc_tool::ui::SafeResetProgress(checklist_progress_, static_cast<int>(selected_count));
        SetScanSummaryText(L"Script running done=0/" + std::to_wstring(selected_count) +
                           L" remaining=" + std::to_wstring(selected_count));
        FlushUiUpdates();
        if (log_) {
            log_(L"PMBus SMBus script run started: " + script_doc_.path);
        }
        for (index = 0u; index < script_doc_.rows.size(); ++index) {
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
                last_result = ExecuteScriptCommandRow(row, row.pec || metadata_pec);
                ++command_count;
                if (mfc_tool::core::SmbusScriptRowIsRead(row)) {
                    ++read_count;
                    AppendScriptResponse(L"#" + std::to_wstring(index + 1u) + L" " +
                                         mfc_tool::core::SmbusScriptRowSummary(row));
                    AppendScriptResponse(L"  raw=" + mfc_tool::core::HexDump(last_result.raw));
                    AppendScriptResponse(L"  data=" + mfc_tool::core::HexDump(last_result.data));
                }
                if (mfc_tool::core::SmbusScriptRowIsWrite(row)) {
                    ++write_count;
                }
                if (!last_result.raw.empty()) {
                    SetRawRxText(mfc_tool::core::HexDump(last_result.raw));
                }
                if (row.delay_ms > 0) {
                    SleepWithCancel((std::min)(row.delay_ms, 10000));
                }
                ++progress_done;
                row_executed = true;
            }

            if (row_executed) {
                mfc_tool::ui::SafeSetProgressPos(checklist_progress_, static_cast<int>(progress_done));
                if ((progress_done % 10u) == 0u || progress_done == selected_count || index + 1u == script_doc_.rows.size()) {
                    SetDecodedText(L"Script row " + std::to_wstring(index + 1u) + L" | " +
                                   mfc_tool::core::SmbusScriptRowSummary(row));
                    SetScanSummaryText(L"Script running done=" + std::to_wstring(progress_done) +
                                       L"/" + std::to_wstring(selected_count) +
                                       L" remaining=" + std::to_wstring(selected_count - progress_done) +
                                       L" commands=" + std::to_wstring(command_count) +
                                       L" pauses=" + std::to_wstring(pause_count) +
                                       L" reads=" + std::to_wstring(read_count) +
                                       L" writes=" + std::to_wstring(write_count));
                }
                FlushUiUpdates();
            }
        }

        ThrowIfCancelRequested();
        mfc_tool::ui::SafeSetProgressPos(checklist_progress_, static_cast<int>(selected_count));
        SetScanSummaryText(L"Script complete commands=" + std::to_wstring(command_count) +
                           L" pauses=" + std::to_wstring(pause_count) +
                           L" reads=" + std::to_wstring(read_count) +
                           L" writes=" + std::to_wstring(write_count));
        SetDecodedText(L"Script complete");
        if (log_) {
            log_(L"PMBus SMBus script run complete: commands=" + std::to_wstring(command_count) +
                 L", pauses=" + std::to_wstring(pause_count) +
                 L", reads=" + std::to_wstring(read_count) +
                 L", writes=" + std::to_wstring(write_count));
        }
    } catch (const UserCancelled&) {
        SetScanSummaryText(L"Script stopped by user.");
        SetDecodedText(L"Script stopped by user.");
        AppendScriptResponse(L"Stopped by user.");
        if (log_) {
            log_(L"PMBus SMBus script run stopped by user.");
        }
    } catch (const std::exception& e) {
        const std::wstring msg = AnsiToWide(e.what());
        if (log_) {
            log_(L"PMBus SMBus script run failed: " + msg);
        }
        ::MessageBoxW(m_hWnd, msg.c_str(), L"PMBus Script", MB_ICONERROR | MB_OK);
    }
    script_running_ = false;
    cancel_requested_ = false;
    UpdateEnableState();
}

void CPmbusTab::OnIllegalQuickTest() {
    if (!connected_ || service_ == nullptr || !master_enabled_) {
        return;
    }
    try {
        const std::uint8_t addr = static_cast<std::uint8_t>(ParseEditInt(master_addr_edit_) & 0x7F);
        const std::uint8_t command = 0x0Fu;
        const bool pec = (pec_check_.GetCheck() == BST_CHECKED);
        const std::vector<std::uint8_t> command_bytes = {command};
        auto format_byte_hex = [](std::uint8_t value) -> std::wstring {
            std::wstringstream ss;
            ss << L"0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
               << static_cast<unsigned int>(value);
            return ss.str();
        };
        auto format_word_hex = [](std::uint16_t value) -> std::wstring {
            std::wstringstream ss;
            ss << L"0x" << std::uppercase << std::hex << std::setw(4) << std::setfill(L'0')
               << static_cast<unsigned int>(value);
            return ss.str();
        };
        try {
            ExecSendByte(addr, {0x03u}, pec, false);
            ReinitMasterBusForRetry();
            ::Sleep(2);
        } catch (const std::exception&) {
        }

        ExecResult result = ExecSendByte(addr, command_bytes, pec, false);
        const ExecResult status_cml = ExecReadByCommand(addr, {0x7Eu}, 1, pec, false);
        const ExecResult status_word = ExecReadByCommand(addr, {0x79u}, 2, pec, false);
        const std::uint8_t cml = status_cml.data.empty() ? 0x00u : status_cml.data[0];
        std::uint16_t word = 0x0000u;
        if (status_word.data.size() >= 2u) {
            word = static_cast<std::uint16_t>(static_cast<std::uint16_t>(status_word.data[0]) |
                                              (static_cast<std::uint16_t>(status_word.data[1]) << 8));
        }
        const bool unsupported_bit = ((cml & 0x80u) != 0u);
        const std::wstring raw_text = mfc_tool::core::HexDump(result.raw);
        std::wstring decoded;
        std::wstring quick_result;

        if (unsupported_bit) {
            decoded = L"Illegal command quick test: unsupported-command bit set in STATUS_CML";
            quick_result = L"PASS: STATUS_CML.INVALID_OR_UNSUPPORTED_COMMAND_RECEIVED set";
        } else {
            decoded = L"Illegal command quick test: ACKed but STATUS_CML unsupported-command bit not set";
            quick_result = L"WARN: ACKED, but STATUS_CML invalid-command bit not set";
        }

        decoded += L" | STATUS_CML=" + format_byte_hex(cml);
        SetRawRxText(raw_text);
        SetDecodedText(decoded);
        UpdatePmbusSummaryLine(L"Illegal command test | STATUS_CML=" + format_byte_hex(cml) +
                               L" | STATUS_WORD=" + format_word_hex(word));
        SetIllegalTestResultText(quick_result + L" | STATUS_CML=" + format_byte_hex(cml) +
                                 L" | STATUS_WORD=" + format_word_hex(word));
        if (log_) {
            std::wstringstream ss;
            ss << L"PMBus illegal-command quick test: addr=0x"
               << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0') << static_cast<unsigned int>(addr)
               << L" cmd=0x" << std::setw(2) << static_cast<unsigned int>(command)
               << L" -> " << quick_result
               << L" | STATUS_CML=" << format_byte_hex(cml)
               << L" | STATUS_WORD=" << format_word_hex(word);
            log_(ss.str());
        }

        try {
            ExecSendByte(addr, {0x03u}, pec, false);
            ReinitMasterBusForRetry();
            ::Sleep(2);
        } catch (const std::exception& e) {
            if (log_) {
                log_(L"PMBus illegal-command quick test cleanup failed: " + AnsiToWide(e.what()));
            }
        }
    } catch (const mfc_tool::hid::BridgeStatusException& e) {
        const std::wstring status_text = mfc_tool::core::StatusText(e.status());
        SetIllegalTestResultText(L"REJECTED: " + status_text);
        SetDecodedText(L"Illegal command quick test rejected: " + status_text);
        if (log_) {
            log_(L"PMBus illegal-command quick test rejected: " + status_text);
        }
    } catch (const mfc_tool::hid::BridgeException& e) {
        const std::wstring msg = AnsiToWide(e.what());
        SetIllegalTestResultText(L"ERROR: " + msg);
        SetDecodedText(L"Illegal command quick test bridge error: " + msg);
        if (log_) {
            log_(L"PMBus illegal-command quick test bridge error: " + msg);
        }
    } catch (const std::exception& e) {
        const std::wstring msg = AnsiToWide(e.what());
        SetIllegalTestResultText(L"ERROR: " + msg);
        SetDecodedText(L"Illegal command quick test error: " + msg);
        ::MessageBoxW(m_hWnd, msg.c_str(), L"PMBus Error", MB_ICONERROR | MB_OK);
    }
}

void CPmbusTab::RunChecklistSuite(int mode) {
    if (!connected_ || service_ == nullptr || !master_enabled_) {
        return;
    }
    if (checklist_running_) {
        return;
    }

    struct ChecklistItem {
        std::wstring name;
        std::wstring detail;
        bool pass = false;
        bool manual = false;
    };

    ResetCancel();
    mfc_tool::ui::ScopedBusyState busy(checklist_running_, [this]() {
        UpdateEnableState();
        FlushUiUpdates();
    });

    try {
        const std::uint8_t addr = static_cast<std::uint8_t>(ParseEditInt(master_addr_edit_) & 0x7F);
        const bool current_pec = (pec_check_.GetCheck() == BST_CHECKED);
        const bool is_crps_profile = IsCrpsProfile();
        const bool is_ti_profile = IsTiProfile();
        std::vector<ChecklistItem> items;
        std::wstring suite_name;
        int current_loop = 1;
        const int repeat_count = ChecklistRepeatCount();
        const int base_progress_total = EstimateChecklistProgressTotal(mode);
        int progress_total = base_progress_total * repeat_count;
        int progress_done = 0;

        switch (mode) {
        case kChecklistModeBasic: suite_name = L"Basic"; break;
        case kChecklistModePec: suite_name = L"PEC"; break;
        case kChecklistModeError: suite_name = L"Error"; break;
        case kChecklistModeTelemetry: suite_name = L"Telemetry"; break;
        case kChecklistModeMfr: suite_name = L"MFR"; break;
        default: suite_name = L"Full"; break;
        }

        mfc_tool::ui::SafeResetProgress(checklist_progress_, progress_total);
        if (log_) {
            log_(L"PMBus checklist [" + suite_name + L"] started, repeat=" + std::to_wstring(repeat_count));
        }
        SetRawRxText(L"");
        SetDecodedText(L"Running PMBus checklist [" + suite_name + L"], loop 1/" + std::to_wstring(repeat_count) + L"...");
        SetScanSummaryText(suite_name + L" checklist running: 0/" + std::to_wstring(progress_total));
        SetIllegalTestResultText(L"Starting...");
        FlushUiUpdates();
        ThrowIfCancelRequested();

        auto format_byte_hex = [](std::uint8_t value) -> std::wstring {
            std::wstringstream ss;
            ss << L"0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
               << static_cast<unsigned int>(value);
            return ss.str();
        };
        auto format_word_hex = [](std::uint16_t value) -> std::wstring {
            std::wstringstream ss;
            ss << L"0x" << std::uppercase << std::hex << std::setw(4) << std::setfill(L'0')
               << static_cast<unsigned int>(value);
            return ss.str();
        };
        auto to_narrow = [](const std::wstring& text) -> std::string {
            if (text.empty()) {
                return std::string();
            }
            int size = WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::vector<char> buffer(static_cast<size_t>((size > 0) ? size : 1), '\0');
            if (size > 0) {
                WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, buffer.data(), size, nullptr, nullptr);
            }
            return std::string(buffer.data());
        };
        auto expect = [&](bool cond, const std::wstring& text) {
            if (!cond) {
                throw std::runtime_error(to_narrow(text));
            }
        };
        auto read_byte = [&](std::uint8_t command, bool pec) -> std::uint8_t {
            const ExecResult result = ExecReadByCommand(addr, {command}, 1, pec, false);
            if (result.data.size() != 1u) {
                throw std::runtime_error("Read Byte returned unexpected length.");
            }
            return result.data[0];
        };
        auto read_word = [&](std::uint8_t command, bool pec) -> std::uint16_t {
            const ExecResult result = ExecReadByCommand(addr, {command}, 2, pec, false);
            if (result.data.size() != 2u) {
                throw std::runtime_error("Read Word returned unexpected length.");
            }
            return static_cast<std::uint16_t>(static_cast<std::uint16_t>(result.data[0]) |
                                              (static_cast<std::uint16_t>(result.data[1]) << 8));
        };
        auto read_block = [&](std::uint8_t command, int max_len, bool pec) -> ExecResult {
            return ExecBlockReadCommand(addr, {command}, max_len, pec, false);
        };
        auto write_byte = [&](std::uint8_t command, std::uint8_t value, bool pec, bool force_bad_pec) {
            ExecWriteByCommand(addr, {command}, {value}, pec, force_bad_pec);
        };
        auto send_byte = [&](std::uint8_t command, bool pec, bool force_bad_pec) {
            ExecSendByte(addr, {command}, pec, force_bad_pec);
        };
        auto make_block_payload = [](const std::vector<std::uint8_t>& payload) -> std::vector<std::uint8_t> {
            std::vector<std::uint8_t> data;
            data.push_back(static_cast<std::uint8_t>(payload.size() & 0xFFu));
            data.insert(data.end(), payload.begin(), payload.end());
            return data;
        };
        auto clear_faults = [&](bool pec) {
            send_byte(0x03u, pec, false);
        };
        auto recover_ara_best_effort = [&](const std::wstring& context) -> bool {
            bool recovered = false;

            if (IsLabValidationPolicy()) {
                try {
                    ExecuteAraHelper(true);
                    recovered = true;
                } catch (const std::exception& e) {
                    if (log_) {
                        log_(L"PMBus checklist " + context + L" ARA recovery failed: " + AnsiToWide(e.what()));
                    }
                }
            } else if (log_) {
                log_(L"PMBus checklist " + context + L" ARA recovery skipped by Production policy");
            }

            try {
                ReinitMasterBusForRetry();
            } catch (const std::exception&) {
            }
            SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
            return recovered;
        };
        auto clear_faults_best_effort = [&](bool pec, const std::wstring& context) -> bool {
            int attempt = 0;
            std::wstring last_error;

            for (attempt = 0; attempt < 3; ++attempt) {
                try {
                    ReinitMasterBusForRetry();
                    clear_faults(pec);
                    SleepWithCancel(2);
                    return true;
                } catch (const mfc_tool::hid::BridgeStatusException& e) {
                    last_error = L"bridge status error: " + mfc_tool::core::StatusText(e.status());
                    if (e.status() == 0x04u && attempt < 2) {
                        if (log_) {
                            log_(L"PMBus checklist " + context + L" clear-faults got IO_ERROR; trying ARA recovery");
                        }
                        recover_ara_best_effort(context + L" clear-faults");
                        continue;
                    }
                    break;
                } catch (const std::exception& e) {
                    last_error = AnsiToWide(e.what());
                    try {
                        ReinitMasterBusForRetry();
                    } catch (const std::exception&) {
                    }
                    SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                }
            }

            if (log_) {
                log_(L"PMBus checklist " + context + L" clear-faults failed: " + last_error);
            }
            return false;
        };
        auto read_status_cml_with_recovery = [&](const std::wstring& context) -> std::uint8_t {
            int attempt = 0;

            for (attempt = 0; attempt < 3; ++attempt) {
                try {
                    ReinitMasterBusForRetry();
                    return read_byte(0x7Eu, current_pec);
                } catch (const mfc_tool::hid::BridgeStatusException& e) {
                    if (e.status() == 0x04u && attempt < 2) {
                        if (log_) {
                            log_(L"PMBus checklist " + context + L" STATUS_CML got IO_ERROR; trying ARA recovery");
                        }
                        recover_ara_best_effort(context + L" STATUS_CML");
                        continue;
                    }
                    throw;
                }
            }

            throw std::runtime_error("STATUS_CML recovery retry exhausted.");
        };
        auto recover_after_error_suite = [&]() {
            clear_faults_best_effort(current_pec, L"post-error");
            recover_ara_best_effort(L"post-error");
            clear_faults_best_effort(current_pec, L"post-error final");

            ReinitMasterBusForRetry();
            SleepWithCancel(2);
        };
        auto build_write_frame = [&](std::uint8_t segment_addr,
                                    const std::vector<std::uint8_t>& command_bytes,
                                    const std::vector<std::uint8_t>& data,
                                    bool pec,
                                    bool force_bad_pec) -> std::vector<std::uint8_t> {
            std::vector<std::uint8_t> tx = command_bytes;
            tx.insert(tx.end(), data.begin(), data.end());
            if (pec) {
                std::vector<std::uint8_t> pec_frame = {static_cast<std::uint8_t>(segment_addr << 1)};
                pec_frame.insert(pec_frame.end(), tx.begin(), tx.end());
                tx.push_back(mfc_tool::core::PmbusComputePec(pec_frame) ^ (force_bad_pec ? 0xFFu : 0x00u));
            }
            return tx;
        };
        auto exec_group_write = [&](const std::vector<std::pair<std::uint8_t, std::vector<std::uint8_t>>>& segments) {
            int attempt = 0;
            for (attempt = 0; attempt < kPmbusRetryAttempts; ++attempt) {
                try {
                    std::vector<std::uint8_t> segment_blob;
                    size_t i = 0u;

                    ReinitMasterBusForRetry();
                    for (i = 0u; i < segments.size(); ++i) {
                        const std::uint8_t segment_addr = segments[i].first;
                        const std::vector<std::uint8_t>& payload = segments[i].second;
                        if (payload.empty() || payload.size() > 0xFFFFu) {
                            throw std::runtime_error("Invalid PMBus group-write segment length.");
                        }
                        segment_blob.push_back(segment_addr);
                        segment_blob.push_back(static_cast<std::uint8_t>(payload.size() & 0xFFu));
                        segment_blob.push_back(static_cast<std::uint8_t>((payload.size() >> 8) & 0xFFu));
                        segment_blob.insert(segment_blob.end(), payload.begin(), payload.end());
                    }

                    {
                        const std::vector<std::uint8_t> response =
                            service_->I2cMasterGroupWrite(CurrentMasterPort(), segment_blob, static_cast<int>(segments.size()));
                        if (response.empty() || response[0] != static_cast<std::uint8_t>(segments.size())) {
                            throw std::runtime_error("PMBus group-write response segment count mismatch.");
                        }
                    }
                    SleepAfterChecklistCommand();
                    return;
                } catch (const mfc_tool::hid::BridgeStatusException& e) {
                    if (attempt < (kPmbusRetryAttempts - 1) && e.status() == 0x04u) {
                        if (log_) {
                            log_(L"PMBus group-write transient IO_ERROR, retrying");
                        }
                        ReinitMasterBusForRetry();
                        SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                        continue;
                    }
                    throw;
                }
            }

            throw std::runtime_error("PMBus group-write retry exhausted");
        };
        auto update_progress = [&](const std::wstring& status, const std::wstring& name, int display_done) {
            const std::wstring loop_text =
                (repeat_count > 1) ? (L" loop " + std::to_wstring(current_loop) + L"/" + std::to_wstring(repeat_count)) : L"";
            if (display_done > progress_total) {
                progress_total = display_done;
                mfc_tool::ui::SafeSetProgressRange(checklist_progress_, progress_total);
            }
            mfc_tool::ui::SafeSetProgressPos(checklist_progress_, display_done);
            SetScanSummaryText(suite_name + L" checklist" + loop_text + L" running: " +
                               std::to_wstring((std::max)(0, display_done)) + L"/" +
                               std::to_wstring(progress_total));
            SetRawRxText(suite_name + L" checklist " + status + L": " + name);
            SetIllegalTestResultText(status + L": " + name);
            SetDecodedText(suite_name + L" checklist" + loop_text + L" " + status + L": " + name);
            FlushUiUpdates();
            ThrowIfCancelRequested();
        };
        auto loop_item_name = [&](const std::wstring& name) -> std::wstring {
            if (repeat_count <= 1) {
                return name;
            }
            return name + L" (loop " + std::to_wstring(current_loop) + L"/" + std::to_wstring(repeat_count) + L")";
        };
        auto update_case_stage = [&](const std::wstring& name, const std::wstring& stage) {
            update_progress(L"Running " + std::to_wstring(progress_done + 1) + L"/" +
                            std::to_wstring(progress_total),
                            loop_item_name(name) + L" - " + stage,
                            progress_done);
        };
        auto run_case = [&](const std::wstring& name, const std::function<std::wstring()>& fn) {
            ChecklistItem item;
            const int display_index = progress_done + 1;
            item.name = loop_item_name(name);
            ThrowIfCancelRequested();
            update_progress(L"Running " + std::to_wstring(display_index) + L"/" +
                            std::to_wstring(progress_total), item.name, progress_done);
            try {
                item.detail = fn();
                item.pass = true;
            } catch (const UserCancelled&) {
                throw;
            } catch (const std::exception& e) {
                item.detail = AnsiToWide(e.what());
                item.pass = false;
            }
            ++progress_done;
            update_progress((item.pass ? L"PASS" : L"FAIL") + std::wstring(L" ") +
                            std::to_wstring(progress_done) + L"/" + std::to_wstring(progress_total),
                            item.name, progress_done);
            if (log_) {
                log_(std::wstring(item.pass ? L"[PASS] " : L"[FAIL] ") +
                     item.name + L" -> " + item.detail);
            }
            FlushUiUpdates();
            ThrowIfCancelRequested();
            items.push_back(item);
        };
        auto run_manual = [&](const std::wstring& name, const std::wstring& detail) {
            ChecklistItem item;
            ThrowIfCancelRequested();
            item.name = loop_item_name(name);
            item.detail = detail;
            item.manual = true;
            item.pass = false;
            ++progress_done;
            update_progress(L"MANUAL " + std::to_wstring(progress_done) + L"/" +
                            std::to_wstring(progress_total), item.name, progress_done);
            if (log_) {
                log_(L"[MANUAL] " + item.name + L" -> " + item.detail);
            }
            FlushUiUpdates();
            ThrowIfCancelRequested();
            items.push_back(item);
        };
        auto run_bus_status_preflight = [&]() {
            run_case(L"Bus idle / recovery preflight", [&]() -> std::wstring {
                const std::vector<std::uint8_t> status = service_->I2cMasterBusStatus(CurrentMasterPort(), true);
                bool idle_before = false;
                bool recovered = false;
                bool idle_after = false;
                std::wstringstream ss;

                expect(status.size() >= 4u, L"Bridge returned invalid bus status payload.");
                idle_before = (status[1] != 0u);
                recovered = (status[2] != 0u);
                idle_after = (status[3] != 0u);

                ss << L"idle_before=" << (idle_before ? L"1" : L"0")
                   << L", recovered=" << (recovered ? L"1" : L"0")
                   << L", idle_after=" << (idle_after ? L"1" : L"0");
                if (log_) {
                    log_(L"PMBus bus status: " + ss.str());
                }
                expect(idle_after, L"SCL/SDA are not idle after bus recovery attempt.");
                return ss.str();
            });
        };
        auto run_contract_audit = [&]() {
            run_case(L"Contract-driven GUI presets", [&]() -> std::wstring {
                const auto& presets = mfc_tool::core::PmbusCommandPresets();
                const std::uint8_t required_base_codes[] = {0x00u, 0x20u, 0x21u, 0x78u, 0x79u, 0x98u, 0x99u, 0x9Au};
                const std::uint8_t required_crps_codes[] = {
                    0xB0u, 0xB1u, 0xB2u, 0xB3u, 0xB4u, 0xB8u,
                    0xC0u, 0xC1u, 0xC2u,
                    0xD0u, 0xD1u, 0xD2u, 0xD3u, 0xD4u, 0xD5u, 0xD6u, 0xD7u,
                    0xD8u, 0xD9u, 0xDAu, 0xDBu, 0xDCu, 0xDDu, 0xDEu, 0xDFu,
                    0xE0u, 0xE1u, 0xE2u, 0xE3u, 0xE4u, 0xE9u, 0xEBu, 0xECu,
                    0xEDu, 0xEEu, 0xF0u, 0xF1u, 0xF2u, 0xF3u
                };
                const std::uint8_t required_ti_codes[] = {
                    0xB5u, 0xB6u, 0xB7u, 0xB9u,
                    0xD0u, 0xD1u, 0xD2u, 0xD3u, 0xD4u, 0xD5u, 0xD6u, 0xD7u,
                    0xD8u, 0xD9u, 0xDAu, 0xDBu, 0xDCu, 0xDDu, 0xDEu, 0xDFu,
                    0xE0u, 0xE1u, 0xE2u, 0xE3u, 0xE4u, 0xE5u, 0xE7u, 0xE8u,
                    0xE9u, 0xEAu, 0xEBu, 0xECu, 0xEDu, 0xEEu, 0xEFu, 0xF0u,
                    0xF1u, 0xF2u, 0xF3u, 0xF4u, 0xF5u, 0xF6u, 0xF7u, 0xF8u,
                    0xF9u, 0xFAu, 0xFBu, 0xFCu, 0xFDu
                };
                const struct CrpsTxnCheck {
                    std::uint8_t code;
                    mfc_tool::core::PmbusTransactionType txn;
                    bool allowed;
                    const wchar_t* label;
                } crps_txn_checks[] = {
                    {0xB0u, mfc_tool::core::PmbusTransactionType::BlockWrite, true, L"USER_DATA Block Write"},
                    {0xB0u, mfc_tool::core::PmbusTransactionType::BlockRead, true, L"USER_DATA Block Read"},
                    {0xB0u, mfc_tool::core::PmbusTransactionType::ReadByte, false, L"USER_DATA Read Byte"},
                    {0xB3u, mfc_tool::core::PmbusTransactionType::BlockRead, true, L"MFR_EFFICIENCY_DATA Block Read"},
                    {0xC0u, mfc_tool::core::PmbusTransactionType::ReadWord, true, L"MFR_MAX_TEMP_1 Read Word"},
                    {0xD0u, mfc_tool::core::PmbusTransactionType::WriteByte, true, L"MFR_COLD_REDUNDANCY_CONFIG Write Byte"},
                    {0xD0u, mfc_tool::core::PmbusTransactionType::ReadByte, true, L"MFR_COLD_REDUNDANCY_CONFIG Read Byte"},
                    {0xD1u, mfc_tool::core::PmbusTransactionType::BlockRead, true, L"MFR_READ_CONFIG_FILE_SIZE Block Read"},
                    {0xD2u, mfc_tool::core::PmbusTransactionType::ReadWord, true, L"MFR_READ_CONFIG_BLOCK_SIZE Read Word"},
                    {0xD3u, mfc_tool::core::PmbusTransactionType::BlockWriteReadProcessCall, true, L"MFR_READ_CONFIG_FILE BWRPC"},
                    {0xD7u, mfc_tool::core::PmbusTransactionType::BlockWrite, true, L"MFR_FWUPLOAD Block Write"},
                    {0xD7u, mfc_tool::core::PmbusTransactionType::BlockRead, false, L"MFR_FWUPLOAD Block Read"},
                    {0xD8u, mfc_tool::core::PmbusTransactionType::WriteWord, true, L"MFR_FWUPLOAD_STATUS Write Word"},
                    {0xD9u, mfc_tool::core::PmbusTransactionType::BlockRead, true, L"MFR_FW_REVISION Block Read"},
                    {0xDAu, mfc_tool::core::PmbusTransactionType::BlockWriteReadProcessCall, true, L"MFR_SPDM BWRPC"},
                    {0xDCu, mfc_tool::core::PmbusTransactionType::BlockRead, true, L"MFR_BLACKBOX Block Read"},
                    {0xDDu, mfc_tool::core::PmbusTransactionType::BlockWriteReadProcessCall, true, L"MFR_REAL_TIME_BLACK_BOX BWRPC"},
                    {0xDEu, mfc_tool::core::PmbusTransactionType::BlockWriteReadProcessCall, true, L"MFR_SYSTEM_BLACK_BOX BWRPC"},
                    {0xDFu, mfc_tool::core::PmbusTransactionType::WriteByte, true, L"MFR_BLACK_BOX_CONFIG Write Byte"},
                    {0xE0u, mfc_tool::core::PmbusTransactionType::WriteByte, true, L"MFR_CLEAR_BLACK_BOX Write Byte"},
                    {0xE2u, mfc_tool::core::PmbusTransactionType::WriteWord, true, L"MFR_SYSTEM_LED_CNTL Write Word"},
                    {0xE3u, mfc_tool::core::PmbusTransactionType::ReadWord, true, L"MFR_FWUPLOAD_BLOCK_SIZE Read Word"},
                    {0xE9u, mfc_tool::core::PmbusTransactionType::BlockWrite, true, L"MFR_PEAK_CURRENT_RECORD Block Write"},
                    {0xE9u, mfc_tool::core::PmbusTransactionType::BlockRead, true, L"MFR_PEAK_CURRENT_RECORD Block Read"},
                    {0xEBu, mfc_tool::core::PmbusTransactionType::BlockRead, true, L"MFR_COMPONENT_ID Block Read"},
                    {0xECu, mfc_tool::core::PmbusTransactionType::ReadWord, true, L"MFR_TOT_POUT_MAX Read Word"},
                    {0xEDu, mfc_tool::core::PmbusTransactionType::WriteWord, true, L"MFR_VOUT_MARGINING Write Word"},
                    {0xEEu, mfc_tool::core::PmbusTransactionType::BlockWrite, true, L"MFR_OCWPL1_SETTING Block Write"},
                    {0xEEu, mfc_tool::core::PmbusTransactionType::BlockRead, true, L"MFR_OCWPL1_SETTING Block Read"},
                    {0xF1u, mfc_tool::core::PmbusTransactionType::BlockRead, true, L"MFR_MAX_IOUT_CAPABILITY Block Read"},
                    {0xF2u, mfc_tool::core::PmbusTransactionType::WriteWord, true, L"MFR_FPC_MAIN_MIN_OFF_TIME Write Word"},
                    {0xF3u, mfc_tool::core::PmbusTransactionType::ReadWord, true, L"MFR_FPC_12VSB_MIN_OFF_TIME Read Word"}
                };
                size_t i = 0u;

                expect(presets.size() == 222u,
                       L"Expected 222 GUI presets generated from PMBUS_COMMAND_CONTRACT.csv.");

                for (i = 0u; i < (sizeof(required_base_codes) / sizeof(required_base_codes[0])); ++i) {
                    const std::uint8_t code = required_base_codes[i];
                    expect(mfc_tool::core::FindPmbusCommandPresetByCode(code) != nullptr,
                           L"Missing generated preset for command " + format_byte_hex(code));
                }

                expect(mfc_tool::core::FindPmbusCommandPresetByCode(0xFEu) == nullptr,
                       L"0xFE must stay in Extended Command UI mode, not as a normal preset.");
                expect(mfc_tool::core::FindPmbusCommandPresetByCode(0xFFu) == nullptr,
                       L"0xFF must stay in Extended Command UI mode, not as a normal preset.");

                if (is_crps_profile) {
                    for (i = 0u; i < (sizeof(required_crps_codes) / sizeof(required_crps_codes[0])); ++i) {
                        const std::uint8_t code = required_crps_codes[i];
                        expect(mfc_tool::core::FindPmbusCommandPresetByCode(code) != nullptr,
                               L"Missing generated CRPS preset for command " + format_byte_hex(code));
                    }
                    for (i = 0u; i < (sizeof(crps_txn_checks) / sizeof(crps_txn_checks[0])); ++i) {
                        const CrpsTxnCheck& check = crps_txn_checks[i];
                        const bool allowed = mfc_tool::core::PmbusTransactionAllowed(check.code, check.txn, false, nullptr);
                        expect(allowed == check.allowed,
                               std::wstring(L"M-CRPS transaction mask mismatch: ") + check.label +
                               L" on " + format_byte_hex(check.code));
                    }
                    return L"222 generated presets; base + full M-CRPS Table 12-38 overlay masks OK; FE/FF remain Extended UI selectors";
                }
                if (is_ti_profile) {
                    for (i = 0u; i < (sizeof(required_ti_codes) / sizeof(required_ti_codes[0])); ++i) {
                        const std::uint8_t code = required_ti_codes[i];
                        const mfc_tool::core::PmbusCommandPreset* preset =
                            mfc_tool::core::FindPmbusCommandPresetByCode(code);
                        expect(preset != nullptr,
                               L"Missing generated TI UCD90xxx preset for command " + format_byte_hex(code));
                        expect(IsPresetVisibleForProfile(*preset),
                               L"TI UCD90xxx profile must expose command " + format_byte_hex(code));
                    }
                    expect(TiUcd90xxxProfilePresetName(*mfc_tool::core::FindPmbusCommandPresetByCode(0xE3u)) == L"PARM_VALUE",
                           L"TI UCD90xxx profile must label 0xE3 as PARM_VALUE.");
                    expect(TiUcd90xxxProfilePresetName(*mfc_tool::core::FindPmbusCommandPresetByCode(0xFDu)) == L"DEVICE_ID",
                           L"TI UCD90xxx profile must label 0xFD as DEVICE_ID.");
                    return L"222 generated presets; TI UCD90xxx overlay rows visible; FE/FF remain Extended UI selectors";
                }

                expect(IsPresetVisibleForProfile(*mfc_tool::core::FindPmbusCommandPresetByCode(0xB0u)),
                       L"PMBus base profile must expose USER_DATA presets with PMBus spec names.");
                expect(IsPresetVisibleForProfile(*mfc_tool::core::FindPmbusCommandPresetByCode(0xC0u)),
                       L"PMBus base profile must expose MFR_SPECIFIC_C0 with PMBus spec names.");
                expect(IsPresetVisibleForProfile(*mfc_tool::core::FindPmbusCommandPresetByCode(0xD4u)),
                       L"PMBus base profile must expose MFR_SPECIFIC presets with PMBus spec names.");
                return L"222 generated presets; base PMBus visible set includes USER_DATA/MFR_SPECIFIC namespace";
            });
        };
        auto run_basic = [&]() {
            run_case(L"Bus write ACK / CLEAR_FAULTS", [&]() -> std::wstring {
                clear_faults(current_pec);
                return L"SLA+W path ACKed";
            });
            run_case(L"Bus read ACK / PMBUS_REVISION", [&]() -> std::wstring {
                const std::uint8_t value = read_byte(0x98u, current_pec);
                expect(value == 0x33u, L"PMBUS_REVISION expected 0x33");
                return L"PMBUS_REVISION=" + format_byte_hex(value);
            });
            run_case(L"Repeated START read path", [&]() -> std::wstring {
                const std::uint8_t value = read_byte(0x98u, current_pec);
                expect(value == 0x33u, L"Repeated START readback mismatch.");
                return L"Read path OK, value=" + format_byte_hex(value);
            });
            run_case(L"PAGE write/readback", [&]() -> std::wstring {
                const std::uint8_t original = read_byte(0x00u, current_pec);
                write_byte(0x00u, original, current_pec, false);
                const std::uint8_t verify = read_byte(0x00u, current_pec);
                expect(verify == original, L"PAGE readback mismatch after write.");
                return L"PAGE=" + format_byte_hex(verify);
            });
            run_case(L"OPERATION write/readback", [&]() -> std::wstring {
                const std::uint8_t original = read_byte(0x01u, current_pec);
                write_byte(0x01u, 0x80u, current_pec, false);
                expect(read_byte(0x01u, current_pec) == 0x80u, L"OPERATION did not read back 0x80.");
                write_byte(0x01u, original, current_pec, false);
                expect(read_byte(0x01u, current_pec) == original, L"OPERATION restore mismatch.");
                return L"original=" + format_byte_hex(original) + L", restore OK";
            });
            run_case(L"ON_OFF_CONFIG write/readback", [&]() -> std::wstring {
                const std::uint8_t original = read_byte(0x02u, current_pec);
                const std::uint8_t test_value = static_cast<std::uint8_t>(original ^ 0x01u);
                write_byte(0x02u, test_value, current_pec, false);
                expect(read_byte(0x02u, current_pec) == test_value, L"ON_OFF_CONFIG test write mismatch.");
                write_byte(0x02u, original, current_pec, false);
                expect(read_byte(0x02u, current_pec) == original, L"ON_OFF_CONFIG restore mismatch.");
                return L"original=" + format_byte_hex(original) + L", test=" + format_byte_hex(test_value);
            });
            run_case(L"FAN_CONFIG_1_2 write/readback", [&]() -> std::wstring {
                const std::uint8_t original = read_byte(0x3Au, current_pec);
                const std::uint8_t test_value = static_cast<std::uint8_t>(original ^ 0x01u);
                write_byte(0x3Au, test_value, current_pec, false);
                expect(read_byte(0x3Au, current_pec) == test_value, L"FAN_CONFIG_1_2 test write mismatch.");
                write_byte(0x3Au, original, current_pec, false);
                expect(read_byte(0x3Au, current_pec) == original, L"FAN_CONFIG_1_2 restore mismatch.");
                return L"original=" + format_byte_hex(original) + L", test=" + format_byte_hex(test_value);
            });
            run_case(L"VOUT_MODE default", [&]() -> std::wstring {
                const std::uint8_t value = read_byte(0x20u, current_pec);
                expect(value == 0x17u, L"VOUT_MODE expected 0x17.");
                return L"VOUT_MODE=" + format_byte_hex(value);
            });
            run_case(L"STATUS_BYTE read", [&]() -> std::wstring {
                return L"STATUS_BYTE=" + format_byte_hex(read_byte(0x78u, current_pec));
            });
            run_case(L"STATUS_WORD read", [&]() -> std::wstring {
                return L"STATUS_WORD=" + format_word_hex(read_word(0x79u, current_pec));
            });
            run_case(L"STATUS_VOUT read", [&]() -> std::wstring {
                return L"STATUS_VOUT=" + format_byte_hex(read_byte(0x7Au, current_pec));
            });
            run_case(L"STATUS_IOUT read", [&]() -> std::wstring {
                return L"STATUS_IOUT=" + format_byte_hex(read_byte(0x7Bu, current_pec));
            });
            run_case(L"STATUS_INPUT read", [&]() -> std::wstring {
                return L"STATUS_INPUT=" + format_byte_hex(read_byte(0x7Cu, current_pec));
            });
            run_case(L"STATUS_TEMPERATURE read", [&]() -> std::wstring {
                return L"STATUS_TEMPERATURE=" + format_byte_hex(read_byte(0x7Du, current_pec));
            });
            run_case(L"STATUS_CML read", [&]() -> std::wstring {
                return L"STATUS_CML=" + format_byte_hex(read_byte(0x7Eu, current_pec));
            });
            run_case(L"STATUS_OTHER read", [&]() -> std::wstring {
                return L"STATUS_OTHER=" + format_byte_hex(read_byte(0x7Fu, current_pec));
            });
            run_case(L"STATUS_MFR_SPECIFIC read", [&]() -> std::wstring {
                return L"STATUS_MFR_SPECIFIC=" + format_byte_hex(read_byte(0x80u, current_pec));
            });
            run_case(L"STATUS_FANS_1_2 read", [&]() -> std::wstring {
                return L"STATUS_FANS_1_2=" + format_byte_hex(read_byte(0x81u, current_pec));
            });
        };
        auto run_pec = [&]() {
            run_case(L"PEC Read Byte / PMBUS_REVISION", [&]() -> std::wstring {
                const ExecResult result = ExecReadByCommand(addr, {0x98u}, 1, true, false);
                expect(result.pec_checked && result.pec_ok, L"Read Byte PEC check failed.");
                expect(result.data.size() == 1u && result.data[0] == 0x33u, L"PMBUS_REVISION expected 0x33.");
                return L"PEC OK, PMBUS_REVISION=" + format_byte_hex(result.data[0]);
            });
            run_case(L"PEC Read Word / STATUS_WORD", [&]() -> std::wstring {
                const ExecResult result = ExecReadByCommand(addr, {0x79u}, 2, true, false);
                expect(result.pec_checked && result.pec_ok, L"Read Word PEC check failed.");
                expect(result.data.size() == 2u, L"STATUS_WORD length mismatch.");
                return L"PEC OK, STATUS_WORD=" + format_word_hex(static_cast<std::uint16_t>(result.data[0] | (result.data[1] << 8)));
            });
            run_case(L"PEC Block Read / MFR_MODEL", [&]() -> std::wstring {
                const ExecResult result = ExecBlockReadCommand(addr, {0x9Au}, 32, true, false);
                expect(result.pec_checked && result.pec_ok, L"Block Read PEC check failed.");
                expect(!result.data.empty(), L"MFR_MODEL block payload empty.");
                return BuildDecodedText(0x9Au, false, result);
            });
            run_case(L"Force Bad PEC negative test", [&]() -> std::wstring {
                const std::uint8_t original = read_byte(0x02u, true);
                std::uint8_t cml = 0x00u;
                bool cml_read = false;
                int attempt = 0;
                clear_faults_best_effort(true, L"pre-bad-PEC");
                try {
                    write_byte(0x02u, original, true, true);
                } catch (const mfc_tool::hid::BridgeStatusException& e) {
                    if (log_) {
                        log_(L"PMBus bad-PEC write returned bridge status " + mfc_tool::core::StatusText(e.status()) +
                             L"; continuing to STATUS_CML validation");
                    }
                }
                ReinitMasterBusForRetry();
                SleepWithCancel(2);
                for (attempt = 0; attempt < 2; ++attempt) {
                    try {
                        cml = read_byte(0x7Eu, true);
                        cml_read = true;
                        break;
                    } catch (const mfc_tool::hid::BridgeStatusException& e) {
                        if (e.status() != 0x04u || attempt >= 1) {
                            clear_faults_best_effort(true, L"bad-PEC failed-read cleanup");
                            throw;
                        }
                        if (log_) {
                            log_(IsLabValidationPolicy()
                                     ? L"PMBus bad-PEC STATUS_CML read got IO_ERROR; trying ARA alias recovery"
                                     : L"PMBus bad-PEC STATUS_CML read got IO_ERROR; Production policy skips ARA recovery");
                        }
                        if (IsLabValidationPolicy()) {
                            try {
                                ExecuteAraHelper(false);
                            } catch (const std::exception& ara_error) {
                                if (log_) {
                                    log_(L"PMBus bad-PEC ARA recovery failed: " + AnsiToWide(ara_error.what()));
                                }
                            }
                        }
                        ReinitMasterBusForRetry();
                        SleepWithCancel(static_cast<int>(kPmbusRetryDelayMs));
                    }
                }
                clear_faults_best_effort(true, L"post-bad-PEC");
                expect(cml_read, L"STATUS_CML read failed after bad PEC.");
                expect((cml & 0x20u) != 0u, L"STATUS_CML bad PEC bit not set.");
                return L"STATUS_CML=" + format_byte_hex(cml);
            });
        };
        auto run_error = [&]() {
            run_case(L"Unsupported command probe", [&]() -> std::wstring {
                std::uint8_t cml = 0x00u;
                clear_faults_best_effort(current_pec, L"pre-unsupported-command");
                try {
                    send_byte(0x0Fu, current_pec, false);
                } catch (const mfc_tool::hid::BridgeStatusException& e) {
                    if (e.status() != 0x04u) {
                        clear_faults_best_effort(current_pec, L"unsupported-command cleanup");
                        throw;
                    }
                    if (log_) {
                        log_(L"PMBus unsupported-command probe returned IO_ERROR; continuing to STATUS_CML validation");
                    }
                    recover_ara_best_effort(L"unsupported-command probe");
                } catch (const std::exception&) {
                    clear_faults_best_effort(current_pec, L"unsupported-command cleanup");
                    throw;
                }
                cml = read_status_cml_with_recovery(L"unsupported-command probe");
                clear_faults_best_effort(current_pec, L"post-unsupported-command");
                expect((cml & 0x80u) != 0u, L"STATUS_CML unsupported-command bit not set.");
                return L"STATUS_CML=" + format_byte_hex(cml);
            });
            run_case(L"Illegal transaction probe", [&]() -> std::wstring {
                std::uint8_t cml = 0x00u;
                clear_faults_best_effort(current_pec, L"pre-illegal-transaction");
                try {
                    send_byte(0x79u, current_pec, false);
                } catch (const mfc_tool::hid::BridgeStatusException& e) {
                    if (e.status() != 0x04u) {
                        clear_faults_best_effort(current_pec, L"illegal-transaction cleanup");
                        throw;
                    }
                    if (log_) {
                        log_(L"PMBus illegal-transaction probe returned IO_ERROR; continuing to STATUS_CML validation");
                    }
                    recover_ara_best_effort(L"illegal-transaction probe");
                } catch (const std::exception&) {
                    clear_faults_best_effort(current_pec, L"illegal-transaction cleanup");
                    throw;
                }
                cml = read_status_cml_with_recovery(L"illegal-transaction probe");
                clear_faults_best_effort(current_pec, L"post-illegal-transaction");
                expect((cml & 0x40u) != 0u || (cml & 0x80u) != 0u, L"STATUS_CML invalid/unsupported data bit not set.");
                return L"STATUS_CML=" + format_byte_hex(cml);
            });
            recover_after_error_suite();
        };
        auto run_telemetry = [&]() {
            const std::uint8_t read_word_cmds[] = {0x88u, 0x89u, 0x8Bu, 0x8Cu, 0x8Du, 0x8Eu, 0x8Fu, 0x90u, 0x91u, 0x96u, 0x97u};
            const wchar_t* read_word_names[] = {L"READ_VIN", L"READ_IIN", L"READ_VOUT", L"READ_IOUT", L"READ_TEMPERATURE_1",
                                                L"READ_TEMPERATURE_2", L"READ_TEMPERATURE_3", L"READ_FAN_SPEED_1", L"READ_FAN_SPEED_2",
                                                L"READ_POUT", L"READ_PIN"};
            size_t i = 0u;
            for (i = 0u; i < sizeof(read_word_cmds) / sizeof(read_word_cmds[0]); ++i) {
                const std::uint8_t cmd = read_word_cmds[i];
                const wchar_t* name = read_word_names[i];
                run_case(name, [&, cmd]() -> std::wstring {
                    const ExecResult result = ExecReadByCommand(addr, {cmd}, 2, current_pec, false);
                    expect(result.data.size() == 2u, L"Telemetry read length mismatch.");
                    return BuildDecodedText(cmd, false, result);
                });
            }

            const std::uint8_t block_cmds[] = {0x86u, 0x87u, 0x99u, 0x9Au, 0x9Bu, 0x9Eu};
            const wchar_t* block_names[] = {L"READ_EIN", L"READ_EOUT", L"MFR_ID", L"MFR_MODEL", L"MFR_REVISION", L"MFR_SERIAL"};
            for (i = 0u; i < sizeof(block_cmds) / sizeof(block_cmds[0]); ++i) {
                const std::uint8_t cmd = block_cmds[i];
                const wchar_t* name = block_names[i];
                run_case(name, [&, cmd]() -> std::wstring {
                    const ExecResult result = read_block(cmd, 40, current_pec);
                    expect(!result.data.empty(), L"Block read returned empty payload.");
                    return BuildDecodedText(cmd, false, result);
                });
            }
        };
        auto run_table31_read_smoke = [&]() {
            const std::wstring case_name = L"Table31 read-only contract smoke";
            run_case(case_name, [&]() -> std::wstring {
                const std::uint8_t representative_policy_blocks[] = {
                    0xB0u, 0xBFu, 0xC4u, 0xD0u, 0xD4u, 0xD5u, 0xD6u, 0xD8u,
                    0xD9u, 0xDBu, 0xDCu, 0xF0u, 0xF1u, 0xFDu
                };
                int read_byte_count = 0;
                int read_word_count = 0;
                int read32_count = 0;
                int block_read_count = 0;
                size_t i = 0u;

                update_case_stage(case_name, L"checking generated read-capable presets");
                for (const auto& preset : mfc_tool::core::PmbusCommandPresets()) {
                    const bool broad_policy_namespace =
                        (preset.code >= 0xB0u && preset.code <= 0xBFu) ||
                        (preset.code >= 0xC4u && preset.code <= 0xFDu);
                    if (broad_policy_namespace) {
                        continue;
                    }

                    try {
                        if (preset.preferred_txn == mfc_tool::core::PmbusTransactionType::ReadByte) {
                            const ExecResult result = ExecReadByCommand(addr, {preset.code}, 1, current_pec, false);
                            expect(result.data.size() == 1u,
                                   std::wstring(preset.name) + L" ReadByte length mismatch.");
                            if (current_pec) {
                                expect(result.pec_checked && result.pec_ok,
                                       std::wstring(preset.name) + L" ReadByte PEC failed.");
                            }
                            (void)BuildDecodedText(preset.code, false, result);
                            ++read_byte_count;
                        } else if (preset.preferred_txn == mfc_tool::core::PmbusTransactionType::ReadWord) {
                            const ExecResult result = ExecReadByCommand(addr, {preset.code}, 2, current_pec, false);
                            expect(result.data.size() == 2u,
                                   std::wstring(preset.name) + L" ReadWord length mismatch.");
                            if (current_pec) {
                                expect(result.pec_checked && result.pec_ok,
                                       std::wstring(preset.name) + L" ReadWord PEC failed.");
                            }
                            (void)BuildDecodedText(preset.code, false, result);
                            ++read_word_count;
                        } else if (preset.preferred_txn == mfc_tool::core::PmbusTransactionType::Read32) {
                            const ExecResult result = ExecReadByCommand(addr, {preset.code}, 4, current_pec, false);
                            expect(result.data.size() == 4u,
                                   std::wstring(preset.name) + L" Read32 length mismatch.");
                            if (current_pec) {
                                expect(result.pec_checked && result.pec_ok,
                                       std::wstring(preset.name) + L" Read32 PEC failed.");
                            }
                            (void)BuildDecodedText(preset.code, false, result);
                            ++read32_count;
                        } else if (preset.preferred_txn == mfc_tool::core::PmbusTransactionType::BlockRead) {
                            const ExecResult result = read_block(preset.code, 32, current_pec);
                            if (current_pec) {
                                expect(result.pec_checked && result.pec_ok,
                                       std::wstring(preset.name) + L" BlockRead PEC failed.");
                            }
                            (void)BuildDecodedText(preset.code, false, result);
                            ++block_read_count;
                        }
                    } catch (const std::exception& e) {
                        throw std::runtime_error(to_narrow(format_byte_hex(preset.code) + L" " +
                                                           std::wstring(preset.name) + L" " +
                                                           mfc_tool::core::PmbusTransactionTypeText(preset.preferred_txn) +
                                                           L": " + AnsiToWide(e.what())));
                    }

                    {
                        const int checked_reads = read_byte_count + read_word_count + read32_count + block_read_count;
                        if (checked_reads > 0 && (checked_reads % 8) == 0) {
                            update_case_stage(case_name, std::wstring(preset.name) + L" checked");
                        }
                    }
                }

                if (!is_ti_profile) {
                    update_case_stage(case_name, L"checking representative policy namespace");
                    for (i = 0u; i < sizeof(representative_policy_blocks) / sizeof(representative_policy_blocks[0]); ++i) {
                        const std::uint8_t code = representative_policy_blocks[i];
                        const auto* preset = mfc_tool::core::FindPmbusCommandPresetByCode(code);
                        expect(preset != nullptr, L"Missing policy representative preset " + format_byte_hex(code));
                        try {
                            if (preset->preferred_txn == mfc_tool::core::PmbusTransactionType::ReadByte) {
                                const ExecResult result = ExecReadByCommand(addr, {code}, 1, current_pec, false);
                                expect(result.data.size() == 1u,
                                       std::wstring(preset->name) + L" representative ReadByte length mismatch.");
                                if (current_pec) {
                                    expect(result.pec_checked && result.pec_ok,
                                           std::wstring(preset->name) + L" representative ReadByte PEC failed.");
                                }
                                (void)BuildDecodedText(code, false, result);
                                ++read_byte_count;
                            } else if (preset->preferred_txn == mfc_tool::core::PmbusTransactionType::ReadWord) {
                                const ExecResult result = ExecReadByCommand(addr, {code}, 2, current_pec, false);
                                expect(result.data.size() == 2u,
                                       std::wstring(preset->name) + L" representative ReadWord length mismatch.");
                                if (current_pec) {
                                    expect(result.pec_checked && result.pec_ok,
                                           std::wstring(preset->name) + L" representative ReadWord PEC failed.");
                                }
                                (void)BuildDecodedText(code, false, result);
                                ++read_word_count;
                            } else if (preset->preferred_txn == mfc_tool::core::PmbusTransactionType::Read32) {
                                const ExecResult result = ExecReadByCommand(addr, {code}, 4, current_pec, false);
                                expect(result.data.size() == 4u,
                                       std::wstring(preset->name) + L" representative Read32 length mismatch.");
                                if (current_pec) {
                                    expect(result.pec_checked && result.pec_ok,
                                           std::wstring(preset->name) + L" representative Read32 PEC failed.");
                                }
                                (void)BuildDecodedText(code, false, result);
                                ++read32_count;
                            } else if (preset->preferred_txn == mfc_tool::core::PmbusTransactionType::BlockRead) {
                                const ExecResult result = read_block(code, 32, current_pec);
                                if (current_pec) {
                                    expect(result.pec_checked && result.pec_ok,
                                           std::wstring(preset->name) + L" representative BlockRead PEC failed.");
                                }
                                (void)BuildDecodedText(code, false, result);
                                ++block_read_count;
                            } else {
                                throw std::runtime_error("representative command is not read-capable");
                            }
                        } catch (const std::exception& e) {
                            throw std::runtime_error(to_narrow(format_byte_hex(code) + L" " +
                                                               std::wstring(preset->name) +
                                                               L" representative " +
                                                               mfc_tool::core::PmbusTransactionTypeText(preset->preferred_txn) +
                                                               L": " + AnsiToWide(e.what())));
                        }
                    }
                }

                update_case_stage(case_name, L"summarizing read-only smoke result");
                return L"ReadByte=" + std::to_wstring(read_byte_count) +
                       L", ReadWord=" + std::to_wstring(read_word_count) +
                       L", Read32=" + std::to_wstring(read32_count) +
                       L", BlockRead=" + std::to_wstring(block_read_count) +
                       L" commands OK" +
                       (is_ti_profile ? L"; TI namespace covered by profile sweep" : L"");
            });
        };
        auto run_profile_command_coverage = [&]() {
            const std::wstring case_name = L"Profile command coverage sweep";
            run_case(case_name, [&]() -> std::wstring {
                using Txn = mfc_tool::core::PmbusTransactionType;

                int visible_count = 0;
                int checked_count = 0;
                int skipped_count = 0;
                int read_byte_count = 0;
                int read_word_count = 0;
                int read32_count = 0;
                int block_read_count = 0;
                int bwrpc_count = 0;
                std::vector<std::wstring> skipped_examples;

                auto is_read_like = [](Txn txn) -> bool {
                    return txn == Txn::ReadByte ||
                           txn == Txn::ReadWord ||
                           txn == Txn::Read32 ||
                           txn == Txn::BlockRead ||
                           txn == Txn::BlockWriteReadProcessCall;
                };
                auto select_read_txn = [&](const mfc_tool::core::PmbusCommandPreset& preset,
                                           Txn* txn) -> bool {
                    const bool ti_specific = is_ti_profile && IsTiUcd90xxxSpecificCode(preset.code);
                    const Txn candidates[] = {
                        Txn::ReadByte,
                        Txn::ReadWord,
                        Txn::Read32,
                        Txn::BlockRead
                    };
                    size_t i = 0u;

                    if (!is_crps_profile && !is_ti_profile && IsCrpsOverlayOnlyCode(preset.code)) {
                        *txn = Txn::BlockRead;
                        return true;
                    }
                    if (ti_specific) {
                        const Txn selected = TiUcd90xxxDefaultTransaction(preset.code, preset.preferred_txn);
                        if (is_read_like(selected)) {
                            *txn = selected;
                            return true;
                        }
                        return false;
                    }
                    for (i = 0u; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
                        if (mfc_tool::core::PmbusTransactionAllowed(preset.code, candidates[i], false, nullptr)) {
                            *txn = candidates[i];
                            return true;
                        }
                    }
                    if (mfc_tool::core::PmbusTransactionAllowed(preset.code, Txn::BlockWriteReadProcessCall, false, nullptr)) {
                        *txn = Txn::BlockWriteReadProcessCall;
                        return true;
                    }
                    return false;
                };
                auto exec_known_safe_bwrpc = [&](std::uint8_t code, ExecResult* result) -> bool {
                    switch (code) {
                    case 0x06u:
                        *result = ExecBlockWriteReadProcessCall(addr, {code}, {0x00u, 0x98u}, 1, current_pec, false);
                        return true;
                    case 0x1Au:
                        *result = ExecBlockWriteReadProcessCall(addr, {code}, {0x98u}, 1, current_pec, false);
                        return true;
                    case 0x1Bu:
                        *result = ExecBlockWriteReadProcessCall(addr, {code}, {0x00u}, 2, current_pec, false);
                        return true;
                    case 0x30u:
                        *result = ExecBlockWriteReadProcessCall(addr, {code}, {0x8Bu}, 5, current_pec, false);
                        return true;
                    case 0xD3u:
                        *result = ExecBlockWriteReadProcessCall(addr, {code}, {0x00u}, 16, current_pec, false);
                        return true;
                    case 0xDAu:
                        *result = ExecBlockWriteReadProcessCall(addr, {code}, {0x00u, 0x01u}, 16, current_pec, false);
                        return true;
                    default:
                        return false;
                    }
                };
                auto note_skipped = [&](const mfc_tool::core::PmbusCommandPreset& preset) {
                    ++skipped_count;
                    if (skipped_examples.size() < 8u) {
                        skipped_examples.push_back(FormatPresetComboLabel(preset, profile_));
                    }
                };

                update_case_stage(case_name, std::wstring(ProfileDisplayName()) + L" scanning visible commands");
                for (const auto& preset : mfc_tool::core::PmbusCommandPresets()) {
                    Txn txn = preset.preferred_txn;
                    const std::wstring preset_label = FormatPresetComboLabel(preset, profile_);

                    if (!IsPresetVisibleForProfile(preset)) {
                        continue;
                    }
                    ++visible_count;
                    if ((visible_count % 8) == 1) {
                        update_case_stage(case_name, preset_label + L" visible=" + std::to_wstring(visible_count) +
                                           L", checked=" + std::to_wstring(checked_count) +
                                           L", skipped=" + std::to_wstring(skipped_count));
                    }
                    if (!select_read_txn(preset, &txn)) {
                        note_skipped(preset);
                        continue;
                    }

                    try {
                        if (txn == Txn::ReadByte) {
                            const ExecResult result = ExecReadByCommand(addr, {preset.code}, 1, current_pec, false);
                            expect(result.data.size() == 1u,
                                   preset_label + L" ReadByte length mismatch.");
                            if (current_pec) {
                                expect(result.pec_checked && result.pec_ok,
                                       preset_label + L" ReadByte PEC failed.");
                            }
                            (void)BuildDecodedText(preset.code, false, result);
                            ++read_byte_count;
                        } else if (txn == Txn::ReadWord) {
                            const ExecResult result = ExecReadByCommand(addr, {preset.code}, 2, current_pec, false);
                            expect(result.data.size() == 2u,
                                   preset_label + L" ReadWord length mismatch.");
                            if (current_pec) {
                                expect(result.pec_checked && result.pec_ok,
                                       preset_label + L" ReadWord PEC failed.");
                            }
                            (void)BuildDecodedText(preset.code, false, result);
                            ++read_word_count;
                        } else if (txn == Txn::Read32) {
                            const ExecResult result = ExecReadByCommand(addr, {preset.code}, 4, current_pec, false);
                            expect(result.data.size() == 4u,
                                   preset_label + L" Read32 length mismatch.");
                            if (current_pec) {
                                expect(result.pec_checked && result.pec_ok,
                                       preset_label + L" Read32 PEC failed.");
                            }
                            (void)BuildDecodedText(preset.code, false, result);
                            ++read32_count;
                        } else if (txn == Txn::BlockRead) {
                            const ExecResult result = read_block(preset.code, 32, current_pec);
                            if (current_pec) {
                                expect(result.pec_checked && result.pec_ok,
                                       preset_label + L" BlockRead PEC failed.");
                            }
                            (void)BuildDecodedText(preset.code, false, result);
                            ++block_read_count;
                        } else if (txn == Txn::BlockWriteReadProcessCall) {
                            ExecResult result;
                            if (!exec_known_safe_bwrpc(preset.code, &result)) {
                                note_skipped(preset);
                                continue;
                            }
                            if (current_pec) {
                                expect(result.pec_checked && result.pec_ok,
                                       preset_label + L" BWRPC PEC failed.");
                            }
                            (void)BuildDecodedText(preset.code, false, result);
                            ++bwrpc_count;
                        } else {
                            note_skipped(preset);
                            continue;
                        }
                    } catch (const std::exception& e) {
                        throw std::runtime_error(to_narrow(preset_label + L" " +
                                                           mfc_tool::core::PmbusTransactionTypeText(txn) +
                                                           L": " + AnsiToWide(e.what())));
                    }

                    ++checked_count;
                }

                update_case_stage(case_name, L"checked=" + std::to_wstring(checked_count) +
                                   L", skipped=" + std::to_wstring(skipped_count));
                {
                    std::wstring detail = std::wstring(ProfileDisplayName()) +
                                          L" visible=" + std::to_wstring(visible_count) +
                                          L", read-capable checked=" + std::to_wstring(checked_count) +
                                          L" (ReadByte=" + std::to_wstring(read_byte_count) +
                                          L", ReadWord=" + std::to_wstring(read_word_count) +
                                          L", Read32=" + std::to_wstring(read32_count) +
                                          L", BlockRead=" + std::to_wstring(block_read_count) +
                                          L", BWRPC=" + std::to_wstring(bwrpc_count) +
                                          L"), skipped write-only/side-effect=" +
                                          std::to_wstring(skipped_count);
                    if (!skipped_examples.empty()) {
                        detail += L" examples: ";
                        for (size_t i = 0u; i < skipped_examples.size(); ++i) {
                            if (i != 0u) {
                                detail += L"; ";
                            }
                            detail += skipped_examples[i];
                        }
                    }
                    return detail;
                }
            });
        };
        auto run_transport_extensions = [&]() {
            run_profile_command_coverage();
            run_case(L"Write Word / Process Call / VOUT_COMMAND", [&]() -> std::wstring {
                const std::uint16_t original = read_word(0x21u, current_pec);
                const std::uint16_t test_value = static_cast<std::uint16_t>(original ^ 0x0001u);
                const std::vector<std::uint8_t> test_data = {
                    static_cast<std::uint8_t>(test_value & 0xFFu),
                    static_cast<std::uint8_t>((test_value >> 8) & 0xFFu)
                };
                const std::vector<std::uint8_t> original_data = {
                    static_cast<std::uint8_t>(original & 0xFFu),
                    static_cast<std::uint8_t>((original >> 8) & 0xFFu)
                };
                const ExecResult process_result = ExecProcessCall(addr, {0x21u}, test_data, current_pec, false);
                std::uint16_t echoed = 0u;

                expect(process_result.data.size() == 2u, L"Process Call returned unexpected length.");
                echoed = static_cast<std::uint16_t>(process_result.data[0] |
                                                    (static_cast<std::uint16_t>(process_result.data[1]) << 8));
                expect(echoed == test_value, L"Process Call readback mismatch.");
                expect(read_word(0x21u, current_pec) == test_value, L"VOUT_COMMAND write/readback mismatch.");

                ExecWriteByCommand(addr, {0x21u}, original_data, current_pec, false);
                expect(read_word(0x21u, current_pec) == original, L"VOUT_COMMAND restore mismatch.");
                return L"original=" + format_word_hex(original) + L", echoed=" + format_word_hex(echoed);
            });
            run_case(L"VOUT_MODE Direct/IEEE/VID conversions", [&]() -> std::wstring {
                const std::uint8_t original_mode = read_byte(0x20u, current_pec);
                const std::uint16_t original_command = read_word(0x21u, current_pec);
                const std::uint16_t direct_12v = 0x2EE0u;
                const std::uint16_t ieee_half_12v = 0x4A00u;
                const std::uint16_t vid_raw = 0x0123u;
                bool restore_needed = true;

                auto write_word_value = [&](std::uint8_t command, std::uint16_t value) {
                    const std::vector<std::uint8_t> data = {
                        static_cast<std::uint8_t>(value & 0xFFu),
                        static_cast<std::uint8_t>((value >> 8) & 0xFFu)
                    };
                    ExecWriteByCommand(addr, {command}, data, current_pec, false);
                };
                auto restore_vout = [&]() {
                    write_byte(0x20u, original_mode, current_pec, false);
                    write_word_value(0x21u, original_command);
                    (void)read_word(0x21u, current_pec);
                };

                try {
                    write_byte(0x20u, 0x40u, current_pec, false);
                    expect(read_byte(0x20u, current_pec) == 0x40u, L"Direct VOUT_MODE readback mismatch.");
                    write_word_value(0x21u, direct_12v);
                    expect(read_word(0x21u, current_pec) == direct_12v, L"Direct VOUT_COMMAND readback mismatch.");
                    expect(read_word(0x8Bu, current_pec) == direct_12v, L"Direct READ_VOUT did not follow mV coefficient conversion.");

                    write_byte(0x20u, 0x60u, current_pec, false);
                    expect(read_byte(0x20u, current_pec) == 0x60u, L"IEEE-half VOUT_MODE readback mismatch.");
                    write_word_value(0x21u, ieee_half_12v);
                    expect(read_word(0x21u, current_pec) == ieee_half_12v, L"IEEE-half VOUT_COMMAND readback mismatch.");
                    expect(read_word(0x8Bu, current_pec) == ieee_half_12v, L"IEEE-half READ_VOUT did not follow half-float conversion.");

                    write_byte(0x20u, 0x3Eu, current_pec, false);
                    expect(read_byte(0x20u, current_pec) == 0x3Eu, L"VID VOUT_MODE selector readback mismatch.");
                    write_word_value(0x21u, vid_raw);
                    expect(read_word(0x21u, current_pec) == vid_raw, L"VID VOUT_COMMAND raw readback mismatch.");
                    expect(read_word(0x8Bu, current_pec) == vid_raw, L"VID READ_VOUT raw mirror mismatch.");

                    restore_vout();
                    restore_needed = false;
                } catch (...) {
                    if (restore_needed) {
                        try {
                            restore_vout();
                        } catch (const std::exception& e) {
                            if (log_) {
                                log_(L"PMBus VOUT_MODE conversion test restore failed: " + AnsiToWide(e.what()));
                            }
                        }
                    }
                    throw;
                }

                return L"Direct raw=0x2EE0, IEEE-half raw=0x4A00, VID raw=0x0123; original restored";
            });
            if (is_crps_profile) {
                run_case(L"Block Write / USER_DATA_00 restore", [&]() -> std::wstring {
                    const ExecResult original_result = read_block(0xB0u, 32, current_pec);
                    const std::vector<std::uint8_t> original = original_result.data;
                    const std::vector<std::uint8_t> test_data = {
                        0x55u, 0xAAu, 0x10u, 0x20u, 0x30u, 0x40u
                    };
                    std::vector<std::uint8_t> block_payload;

                    block_payload.push_back(static_cast<std::uint8_t>(test_data.size()));
                    block_payload.insert(block_payload.end(), test_data.begin(), test_data.end());
                    ExecWriteByCommand(addr, {0xB0u}, block_payload, current_pec, false);
                    {
                        const ExecResult verify_result = read_block(0xB0u, 32, current_pec);
                        expect(verify_result.data == test_data, L"USER_DATA_00 block write/readback mismatch.");
                    }

                    block_payload.clear();
                    block_payload.push_back(static_cast<std::uint8_t>(original.size()));
                    block_payload.insert(block_payload.end(), original.begin(), original.end());
                    ExecWriteByCommand(addr, {0xB0u}, block_payload, current_pec, false);
                    expect(read_block(0xB0u, 32, current_pec).data == original, L"USER_DATA_00 restore mismatch.");
                    return L"wrote/read/restored " + std::to_wstring(test_data.size()) + L" bytes";
                });
            }
            run_case(L"Block Write-Read Process Call / QUERY", [&]() -> std::wstring {
                const ExecResult result = ExecBlockWriteReadProcessCall(addr, {0x1Au}, {0x98u}, 1, current_pec, false);
                expect(result.data.size() == 1u, L"QUERY result length mismatch.");
                if (current_pec) {
                    expect(result.pec_checked && result.pec_ok, L"QUERY PEC failed.");
                }
                return L"QUERY(PMBUS_REVISION)=" + format_byte_hex(result.data[0]);
            });
            run_case(L"SMBALERT_MASK shadow read/write restore", [&]() -> std::wstring {
                const ExecResult original_result = ExecBlockWriteReadProcessCall(addr, {0x1Bu}, {0x00u}, 2, current_pec, false);
                std::uint16_t original = 0u;
                std::uint16_t test_value = 0u;
                std::uint16_t verify_value = 0u;
                std::uint16_t final_value = 0u;
                std::vector<std::uint8_t> original_data;
                std::vector<std::uint8_t> test_data;
                bool restore_needed = false;

                expect(original_result.data.size() >= 2u, L"SMBALERT_MASK read returned less than 2 bytes.");
                if (current_pec) {
                    expect(original_result.pec_checked && original_result.pec_ok, L"SMBALERT_MASK read PEC failed.");
                }

                original = static_cast<std::uint16_t>(original_result.data[0] |
                                                       (static_cast<std::uint16_t>(original_result.data[1]) << 8));
                test_value = static_cast<std::uint16_t>(original ^ 0x0001u);
                test_data = {
                    static_cast<std::uint8_t>(test_value & 0xFFu),
                    static_cast<std::uint8_t>((test_value >> 8) & 0xFFu)
                };
                original_data = {
                    static_cast<std::uint8_t>(original & 0xFFu),
                    static_cast<std::uint8_t>((original >> 8) & 0xFFu)
                };

                try {
                    ExecWriteByCommand(addr, {0x1Bu}, test_data, current_pec, false);
                    restore_needed = true;
                    {
                        const ExecResult verify_result = ExecBlockWriteReadProcessCall(addr, {0x1Bu}, {0x00u}, 2, current_pec, false);
                        expect(verify_result.data.size() >= 2u, L"SMBALERT_MASK verify read returned less than 2 bytes.");
                        if (current_pec) {
                            expect(verify_result.pec_checked && verify_result.pec_ok, L"SMBALERT_MASK verify PEC failed.");
                        }
                        verify_value = static_cast<std::uint16_t>(verify_result.data[0] |
                                                                  (static_cast<std::uint16_t>(verify_result.data[1]) << 8));
                    }
                    expect(verify_value == test_value, L"SMBALERT_MASK write/readback mismatch.");

                    ExecWriteByCommand(addr, {0x1Bu}, original_data, current_pec, false);
                    restore_needed = false;
                    {
                        const ExecResult final_result = ExecBlockWriteReadProcessCall(addr, {0x1Bu}, {0x00u}, 2, current_pec, false);
                        expect(final_result.data.size() >= 2u, L"SMBALERT_MASK final read returned less than 2 bytes.");
                        if (current_pec) {
                            expect(final_result.pec_checked && final_result.pec_ok, L"SMBALERT_MASK final PEC failed.");
                        }
                        final_value = static_cast<std::uint16_t>(final_result.data[0] |
                                                                 (static_cast<std::uint16_t>(final_result.data[1]) << 8));
                    }
                    expect(final_value == original, L"SMBALERT_MASK restore mismatch.");
                } catch (...) {
                    if (restore_needed) {
                        try {
                            ExecWriteByCommand(addr, {0x1Bu}, original_data, current_pec, false);
                        } catch (const std::exception& e) {
                            if (log_) {
                                log_(L"PMBus SMBALERT_MASK restore failed: " + AnsiToWide(e.what()));
                            }
                        }
                    }
                    throw;
                }

                return L"original=" + format_word_hex(original) + L", test=" + format_word_hex(test_value) +
                       L", restored=" + format_word_hex(final_value);
            });
            if (is_crps_profile) {
                const std::wstring case_name = L"M-CRPS profile command shadows";
                run_case(case_name, [&]() -> std::wstring {
                    update_case_stage(case_name, L"reading B3/C0-C2/D0 baseline");
                    const ExecResult efficiency_data = read_block(0xB3u, 16, current_pec);
                    const std::uint16_t max_temp_1 = read_word(0xC0u, current_pec);
                    const std::uint16_t max_temp_2 = read_word(0xC1u, current_pec);
                    const std::uint16_t max_temp_3 = read_word(0xC2u, current_pec);
                    const std::uint8_t cold_redundancy_original = read_byte(0xD0u, current_pec);
                    const std::uint8_t cold_redundancy_test = static_cast<std::uint8_t>(cold_redundancy_original ^ 0x01u);
                    const std::uint16_t hw_compatibility = read_word(0xD4u, current_pec);
                    const std::uint8_t fwupload_capability = read_byte(0xD5u, current_pec);
                    const std::uint8_t fwupload_mode = read_byte(0xD6u, current_pec);
                    update_case_stage(case_name, L"reading D1-D9 config/FW fields");
                    const ExecResult config_file_size = read_block(0xD1u, 8, current_pec);
                    const std::uint16_t config_block_size = read_word(0xD2u, current_pec);
                    const ExecResult config_file = ExecBlockWriteReadProcessCall(addr, {0xD3u}, {0x00u}, 16, current_pec, false);
                    const std::uint16_t fwupload_status_original = read_word(0xD8u, current_pec);
                    const std::uint16_t fwupload_status_test = static_cast<std::uint16_t>(fwupload_status_original ^ 0x0001u);
                    const ExecResult fw_revision = read_block(0xD9u, 8, current_pec);
                    update_case_stage(case_name, L"reading DA-DE blackbox/SPDM fields");
                    const ExecResult spdm_response = ExecBlockWriteReadProcessCall(addr, {0xDAu}, {0x00u, 0x01u}, 16, current_pec, false);
                    const std::uint8_t fru_original = read_byte(0xDBu, current_pec);
                    const std::uint8_t fru_test = static_cast<std::uint8_t>(fru_original ^ 0x01u);
                    const ExecResult blackbox = read_block(0xDCu, 32, current_pec);
                    const ExecResult realtime_blackbox = read_block(0xDDu, 8, current_pec);
                    const ExecResult system_blackbox = read_block(0xDEu, 32, current_pec);
                    update_case_stage(case_name, L"reading DF-F3 writable baselines");
                    const std::uint8_t blackbox_config_original = read_byte(0xDFu, current_pec);
                    const std::uint8_t blackbox_config_test = static_cast<std::uint8_t>(blackbox_config_original ^ 0x01u);
                    const std::uint8_t line_status_original = read_byte(0xE1u, current_pec);
                    const std::uint8_t line_status_test = static_cast<std::uint8_t>(line_status_original ^ 0x01u);
                    const std::uint16_t system_led_original = read_word(0xE2u, current_pec);
                    const std::uint16_t system_led_test = static_cast<std::uint16_t>(system_led_original ^ 0x0001u);
                    const std::uint16_t fwupload_block_size = read_word(0xE3u, current_pec);
                    const std::uint8_t status_sim_original = read_byte(0xE4u, current_pec);
                    const std::uint8_t status_sim_test = static_cast<std::uint8_t>(status_sim_original ^ 0x01u);
                    const ExecResult peak_current_original = read_block(0xE9u, 16, current_pec);
                    const ExecResult component_id = read_block(0xEBu, 16, current_pec);
                    const std::uint16_t total_pout_max = read_word(0xECu, current_pec);
                    const std::uint16_t vout_margining_original = read_word(0xEDu, current_pec);
                    const std::uint16_t vout_margining_test = static_cast<std::uint16_t>(vout_margining_original ^ 0x0001u);
                    const ExecResult ocwpl1_original = read_block(0xEEu, 16, current_pec);
                    const std::uint16_t pwok_original = read_word(0xF0u, current_pec);
                    const std::uint16_t pwok_test = static_cast<std::uint16_t>(pwok_original ^ 0x0001u);
                    const ExecResult max_iout_capability = read_block(0xF1u, 16, current_pec);
                    const std::uint16_t fpc_main_original = read_word(0xF2u, current_pec);
                    const std::uint16_t fpc_main_test = static_cast<std::uint16_t>(fpc_main_original ^ 0x0001u);
                    const std::uint16_t fpc_12vsb_original = read_word(0xF3u, current_pec);
                    const std::uint16_t fpc_12vsb_test = static_cast<std::uint16_t>(fpc_12vsb_original ^ 0x0001u);
                    auto word_data = [](std::uint16_t value) -> std::vector<std::uint8_t> {
                        return {
                            static_cast<std::uint8_t>(value & 0xFFu),
                            static_cast<std::uint8_t>((value >> 8) & 0xFFu)
                        };
                    };
                    auto block_payload = [](const std::vector<std::uint8_t>& payload) -> std::vector<std::uint8_t> {
                        std::vector<std::uint8_t> data;
                        data.push_back(static_cast<std::uint8_t>(payload.size() & 0xFFu));
                        data.insert(data.end(), payload.begin(), payload.end());
                        return data;
                    };
                    auto expect_block_equal = [&](const std::vector<std::uint8_t>& actual,
                                                  const std::vector<std::uint8_t>& expected,
                                                  const std::wstring& label) {
                        if (actual != expected) {
                            expect(false,
                                   label + L" mismatch: expected=" + mfc_tool::core::HexDump(expected) +
                                       L", actual=" + mfc_tool::core::HexDump(actual));
                        }
                    };
                    const std::vector<std::uint8_t> peak_current_test = {
                        0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u
                    };
                    const std::vector<std::uint8_t> ocwpl1_test = {
                        0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u, 0x70u, 0x80u
                    };
                    bool cold_redundancy_restore_needed = false;
                    bool fwupload_status_restore_needed = false;
                    bool fru_restore_needed = false;
                    bool blackbox_config_restore_needed = false;
                    bool line_status_restore_needed = false;
                    bool system_led_restore_needed = false;
                    bool status_sim_restore_needed = false;
                    bool peak_current_restore_needed = false;
                    bool vout_margining_restore_needed = false;
                    bool ocwpl1_restore_needed = false;
                    bool pwok_restore_needed = false;
                    bool fpc_main_restore_needed = false;
                    bool fpc_12vsb_restore_needed = false;

                    update_case_stage(case_name, L"validating CRPS response sizes");
                    expect(efficiency_data.data.size() == 8u, L"MFR_EFFICIENCY_DATA must return 8 payload bytes.");
                    expect(config_file_size.data.size() == 4u, L"MFR_READ_CONFIG_FILE_SIZE must return 4 payload bytes.");
                    expect(config_block_size > 0u, L"MFR_READ_CONFIG_BLOCK_SIZE must be non-zero.");
                    expect(!config_file.data.empty(), L"MFR_READ_CONFIG_FILE BWRPC returned empty payload.");
                    expect((fwupload_mode & 0xFEu) == 0u, L"MFR_FWUPLOAD_MODE must use bit0 only.");
                    expect(fw_revision.data.size() == 7u, L"MFR_FW_REVISION must return 7 payload bytes.");
                    expect(!spdm_response.data.empty(), L"MFR_SPDM BWRPC returned empty payload.");
                    expect(!blackbox.data.empty() && blackbox.data.size() <= 32u,
                           L"MFR_BLACKBOX bounded placeholder must return 1..32 payload bytes.");
                    expect(realtime_blackbox.data.size() == 4u, L"MFR_REAL_TIME_BLACK_BOX must return 4 payload bytes.");
                    expect(!system_blackbox.data.empty() && system_blackbox.data.size() <= 32u,
                           L"MFR_SYSTEM_BLACK_BOX bounded placeholder must return 1..32 payload bytes.");
                    expect(fwupload_block_size > 0u, L"MFR_FWUPLOAD_BLOCK_SIZE must be non-zero.");
                    expect(peak_current_original.data.size() == 8u,
                           L"MFR_PEAK_CURRENT_RECORD must return 8 payload bytes.");
                    expect(component_id.data.size() == 12u, L"MFR_COMPONENT_ID must return 12 payload bytes.");
                    expect(ocwpl1_original.data.size() == 8u, L"MFR_OCWPL1_SETTING must return 8 payload bytes.");
                    expect(max_iout_capability.data.size() == 14u,
                           L"MFR_MAX_IOUT_CAPABILITY must return 14 payload bytes.");

                    try {
                        update_case_stage(case_name, L"write/read/restore D0-D8");
                        write_byte(0xD0u, cold_redundancy_test, current_pec, false);
                        cold_redundancy_restore_needed = true;
                        expect(read_byte(0xD0u, current_pec) == cold_redundancy_test,
                               L"MFR_COLD_REDUNDANCY_CONFIG write/readback mismatch.");
                        write_byte(0xD0u, cold_redundancy_original, current_pec, false);
                        cold_redundancy_restore_needed = false;
                        expect(read_byte(0xD0u, current_pec) == cold_redundancy_original,
                               L"MFR_COLD_REDUNDANCY_CONFIG restore mismatch.");

                        ExecWriteByCommand(addr, {0xD8u}, word_data(fwupload_status_test), current_pec, false);
                        fwupload_status_restore_needed = true;
                        expect(read_word(0xD8u, current_pec) == fwupload_status_test,
                               L"MFR_FWUPLOAD_STATUS write/readback mismatch.");
                        ExecWriteByCommand(addr, {0xD8u}, word_data(fwupload_status_original), current_pec, false);
                        fwupload_status_restore_needed = false;
                        expect(read_word(0xD8u, current_pec) == fwupload_status_original,
                               L"MFR_FWUPLOAD_STATUS restore mismatch.");

                        update_case_stage(case_name, L"write/read/restore DB-E4");
                        write_byte(0xDBu, fru_test, current_pec, false);
                        fru_restore_needed = true;
                        expect(read_byte(0xDBu, current_pec) == fru_test, L"MFR_FRU_PROTECTION write/readback mismatch.");
                        write_byte(0xDBu, fru_original, current_pec, false);
                        fru_restore_needed = false;
                        expect(read_byte(0xDBu, current_pec) == fru_original, L"MFR_FRU_PROTECTION restore mismatch.");

                        write_byte(0xDFu, blackbox_config_test, current_pec, false);
                        blackbox_config_restore_needed = true;
                        expect(read_byte(0xDFu, current_pec) == blackbox_config_test, L"MFR_BLACK_BOX_CONFIG write/readback mismatch.");
                        write_byte(0xDFu, blackbox_config_original, current_pec, false);
                        blackbox_config_restore_needed = false;
                        expect(read_byte(0xDFu, current_pec) == blackbox_config_original, L"MFR_BLACK_BOX_CONFIG restore mismatch.");

                        write_byte(0xE1u, line_status_test, current_pec, false);
                        line_status_restore_needed = true;
                        expect(read_byte(0xE1u, current_pec) == line_status_test, L"MFR_LINE_STATUS write/readback mismatch.");
                        write_byte(0xE1u, line_status_original, current_pec, false);
                        line_status_restore_needed = false;
                        expect(read_byte(0xE1u, current_pec) == line_status_original, L"MFR_LINE_STATUS restore mismatch.");

                        ExecWriteByCommand(addr, {0xE2u}, word_data(system_led_test), current_pec, false);
                        system_led_restore_needed = true;
                        expect(read_word(0xE2u, current_pec) == system_led_test, L"MFR_SYSTEM_LED_CNTL write/readback mismatch.");
                        ExecWriteByCommand(addr, {0xE2u}, word_data(system_led_original), current_pec, false);
                        system_led_restore_needed = false;
                        expect(read_word(0xE2u, current_pec) == system_led_original, L"MFR_SYSTEM_LED_CNTL restore mismatch.");

                        write_byte(0xE4u, status_sim_test, current_pec, false);
                        status_sim_restore_needed = true;
                        expect(read_byte(0xE4u, current_pec) == status_sim_test, L"MFR_EN_STATUS_SIMULATION_CMD write/readback mismatch.");
                        write_byte(0xE4u, status_sim_original, current_pec, false);
                        status_sim_restore_needed = false;
                        expect(read_byte(0xE4u, current_pec) == status_sim_original, L"MFR_EN_STATUS_SIMULATION_CMD restore mismatch.");

                        update_case_stage(case_name, L"write/read/restore E9-EE");
                        ExecWriteByCommand(addr, {0xE9u}, block_payload(peak_current_test), current_pec, false);
                        peak_current_restore_needed = true;
                        expect_block_equal(read_block(0xE9u, 16, current_pec).data,
                                           peak_current_test,
                                           L"MFR_PEAK_CURRENT_RECORD block write/readback");
                        ExecWriteByCommand(addr, {0xE9u}, block_payload(peak_current_original.data), current_pec, false);
                        peak_current_restore_needed = false;
                        expect_block_equal(read_block(0xE9u, 16, current_pec).data,
                                           peak_current_original.data,
                                           L"MFR_PEAK_CURRENT_RECORD restore");

                        ExecWriteByCommand(addr, {0xEDu}, word_data(vout_margining_test), current_pec, false);
                        vout_margining_restore_needed = true;
                        expect(read_word(0xEDu, current_pec) == vout_margining_test, L"MFR_VOUT_MARGINING write/readback mismatch.");
                        ExecWriteByCommand(addr, {0xEDu}, word_data(vout_margining_original), current_pec, false);
                        vout_margining_restore_needed = false;
                        expect(read_word(0xEDu, current_pec) == vout_margining_original, L"MFR_VOUT_MARGINING restore mismatch.");

                        ExecWriteByCommand(addr, {0xEEu}, block_payload(ocwpl1_test), current_pec, false);
                        ocwpl1_restore_needed = true;
                        expect_block_equal(read_block(0xEEu, 16, current_pec).data,
                                           ocwpl1_test,
                                           L"MFR_OCWPL1_SETTING block write/readback");
                        ExecWriteByCommand(addr, {0xEEu}, block_payload(ocwpl1_original.data), current_pec, false);
                        ocwpl1_restore_needed = false;
                        expect_block_equal(read_block(0xEEu, 16, current_pec).data,
                                           ocwpl1_original.data,
                                           L"MFR_OCWPL1_SETTING restore");

                        update_case_stage(case_name, L"write/read/restore F0-F3");
                        ExecWriteByCommand(addr, {0xF0u}, word_data(pwok_test), current_pec, false);
                        pwok_restore_needed = true;
                        expect(read_word(0xF0u, current_pec) == pwok_test, L"MFR_PWOK_WARNING_TIME write/readback mismatch.");
                        ExecWriteByCommand(addr, {0xF0u}, word_data(pwok_original), current_pec, false);
                        pwok_restore_needed = false;
                        expect(read_word(0xF0u, current_pec) == pwok_original, L"MFR_PWOK_WARNING_TIME restore mismatch.");

                        ExecWriteByCommand(addr, {0xF2u}, word_data(fpc_main_test), current_pec, false);
                        fpc_main_restore_needed = true;
                        expect(read_word(0xF2u, current_pec) == fpc_main_test, L"MFR_FPC_MAIN_MIN_OFF_TIME write/readback mismatch.");
                        ExecWriteByCommand(addr, {0xF2u}, word_data(fpc_main_original), current_pec, false);
                        fpc_main_restore_needed = false;
                        expect(read_word(0xF2u, current_pec) == fpc_main_original, L"MFR_FPC_MAIN_MIN_OFF_TIME restore mismatch.");

                        ExecWriteByCommand(addr, {0xF3u}, word_data(fpc_12vsb_test), current_pec, false);
                        fpc_12vsb_restore_needed = true;
                        expect(read_word(0xF3u, current_pec) == fpc_12vsb_test, L"MFR_FPC_12VSB_MIN_OFF_TIME write/readback mismatch.");
                        ExecWriteByCommand(addr, {0xF3u}, word_data(fpc_12vsb_original), current_pec, false);
                        fpc_12vsb_restore_needed = false;
                        expect(read_word(0xF3u, current_pec) == fpc_12vsb_original, L"MFR_FPC_12VSB_MIN_OFF_TIME restore mismatch.");
                    } catch (...) {
                        if (cold_redundancy_restore_needed) {
                            try {
                                write_byte(0xD0u, cold_redundancy_original, current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_COLD_REDUNDANCY_CONFIG restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (fwupload_status_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xD8u}, word_data(fwupload_status_original), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_FWUPLOAD_STATUS restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (fru_restore_needed) {
                            try {
                                write_byte(0xDBu, fru_original, current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_FRU_PROTECTION restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (blackbox_config_restore_needed) {
                            try {
                                write_byte(0xDFu, blackbox_config_original, current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_BLACK_BOX_CONFIG restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (line_status_restore_needed) {
                            try {
                                write_byte(0xE1u, line_status_original, current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_LINE_STATUS restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (system_led_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xE2u}, word_data(system_led_original), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_SYSTEM_LED_CNTL restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (status_sim_restore_needed) {
                            try {
                                write_byte(0xE4u, status_sim_original, current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_EN_STATUS_SIMULATION_CMD restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (peak_current_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xE9u}, block_payload(peak_current_original.data), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_PEAK_CURRENT_RECORD restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (vout_margining_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xEDu}, word_data(vout_margining_original), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_VOUT_MARGINING restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (ocwpl1_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xEEu}, block_payload(ocwpl1_original.data), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_OCWPL1_SETTING restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (pwok_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xF0u}, word_data(pwok_original), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_PWOK_WARNING_TIME restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (fpc_main_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xF2u}, word_data(fpc_main_original), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_FPC_MAIN_MIN_OFF_TIME restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (fpc_12vsb_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xF3u}, word_data(fpc_12vsb_original), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"CRPS MFR_FPC_12VSB_MIN_OFF_TIME restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        throw;
                    }

                    return L"32 focused CRPS commands checked; B3=8B, C0/C1/C2=" + format_word_hex(max_temp_1) + L"/" +
                           format_word_hex(max_temp_2) + L"/" + format_word_hex(max_temp_3) +
                           L", D0 restored, D1=4B, D2=" + format_word_hex(config_block_size) +
                           L", D3=" + std::to_wstring(config_file.data.size()) + L"B" +
                           L", D4=" + format_word_hex(hw_compatibility) +
                           L", D5=" + format_byte_hex(fwupload_capability) +
                           L", D6=" + format_byte_hex(fwupload_mode) +
                           L", D8 restored, D9=7B, DA=" + std::to_wstring(spdm_response.data.size()) + L"B" +
                           L", DB/DF/E1/E2/E4/ED/F0/F2/F3 restored, DC=" +
                           std::to_wstring(blackbox.data.size()) + L"B, DD=4B, DE=" +
                           std::to_wstring(system_blackbox.data.size()) +
                           L"B, E3=" + format_word_hex(fwupload_block_size) +
                           L", E9 restored, EB=12B, EC=" + format_word_hex(total_pout_max) +
                           L", EE restored, F1=14B";
                });
            }
            if (!is_crps_profile && !is_ti_profile) {
                run_case(L"PMBus Base USER/MFR namespace shadows", [&]() -> std::wstring {
                    const ExecResult user_original = read_block(0xB0u, 32, current_pec);
                    const ExecResult mfr_original = read_block(0xC4u, 32, current_pec);
                    const std::vector<std::uint8_t> user_test = {0x55u, 0xA5u, 0x10u, 0x20u};
                    const std::vector<std::uint8_t> mfr_test = {0xC4u, 0x11u, 0x22u, 0x33u};
                    bool user_restore_needed = false;
                    bool mfr_restore_needed = false;

                    try {
                        ExecWriteByCommand(addr, {0xB0u}, make_block_payload(user_test), current_pec, false);
                        user_restore_needed = true;
                        expect(read_block(0xB0u, 32, current_pec).data == user_test,
                               L"USER_DATA_00 block write/readback mismatch.");

                        ExecWriteByCommand(addr, {0xC4u}, make_block_payload(mfr_test), current_pec, false);
                        mfr_restore_needed = true;
                        expect(read_block(0xC4u, 32, current_pec).data == mfr_test,
                               L"MFR_SPECIFIC_C4 block write/readback mismatch.");

                        ExecWriteByCommand(addr, {0xB0u}, make_block_payload(user_original.data), current_pec, false);
                        user_restore_needed = false;
                        expect(read_block(0xB0u, 32, current_pec).data == user_original.data,
                               L"USER_DATA_00 restore mismatch.");

                        ExecWriteByCommand(addr, {0xC4u}, make_block_payload(mfr_original.data), current_pec, false);
                        mfr_restore_needed = false;
                        expect(read_block(0xC4u, 32, current_pec).data == mfr_original.data,
                               L"MFR_SPECIFIC_C4 restore mismatch.");
                    } catch (...) {
                        if (user_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xB0u}, make_block_payload(user_original.data), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"PMBus Base USER_DATA_00 restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        if (mfr_restore_needed) {
                            try {
                                ExecWriteByCommand(addr, {0xC4u}, make_block_payload(mfr_original.data), current_pec, false);
                            } catch (const std::exception& e) {
                                if (log_) {
                                    log_(L"PMBus Base MFR_SPECIFIC_C4 restore failed: " + AnsiToWide(e.what()));
                                }
                            }
                        }
                        throw;
                    }

                    return L"USER_DATA_00 and MFR_SPECIFIC_C4 block shadows wrote/read/restored";
                });
            } else if (is_ti_profile) {
                run_case(L"TI UCD90xxx profile command smoke", [&]() -> std::wstring {
                    const std::uint16_t seq_timeout = read_word(0xD0u, current_pec);
                    const std::uint8_t num_pages = read_byte(0xD6u, current_pec);
                    const ExecResult parm_value = read_block(0xE3u, 32, current_pec);
                    const ExecResult seq_config = read_block(0xF6u, 32, current_pec);
                    const ExecResult device_id = read_block(0xFDu, 32, current_pec);

                    expect(!parm_value.data.empty() && parm_value.data.size() <= 32u,
                           L"TI PARM_VALUE block read must return 1..32 bytes.");
                    expect(!seq_config.data.empty() && seq_config.data.size() <= 32u,
                           L"TI SEQ_CONFIG block read must return 1..32 bytes.");
                    expect(!device_id.data.empty() && device_id.data.size() <= 32u,
                           L"TI DEVICE_ID block read must return 1..32 bytes.");

                    return L"SEQ_TIMEOUT=" + format_word_hex(seq_timeout) +
                           L", NUM_PAGES=" + format_byte_hex(num_pages) +
                           L", PARM_VALUE=" + std::to_wstring(parm_value.data.size()) + L"B" +
                           L", SEQ_CONFIG=" + std::to_wstring(seq_config.data.size()) + L"B" +
                           L", DEVICE_ID=" + std::to_wstring(device_id.data.size()) + L"B";
                });
            }
            run_case(L"Group Command / PAGE + PHASE restore", [&]() -> std::wstring {
                const std::uint8_t original_page = read_byte(0x00u, current_pec);
                const std::uint8_t original_phase = read_byte(0x04u, current_pec);
                const std::uint8_t test_page = 0x00u;
                const std::uint8_t test_phase = static_cast<std::uint8_t>(original_phase ^ 0x01u);
                const std::vector<std::pair<std::uint8_t, std::vector<std::uint8_t>>> segments = {
                    {addr, build_write_frame(addr, {0x00u}, {test_page}, current_pec, false)},
                    {addr, build_write_frame(addr, {0x04u}, {test_phase}, current_pec, false)}
                };

                exec_group_write(segments);
                expect(read_byte(0x00u, current_pec) == test_page, L"Group PAGE write/readback mismatch.");
                expect(read_byte(0x04u, current_pec) == test_phase, L"Group PHASE write/readback mismatch.");
                write_byte(0x04u, original_phase, current_pec, false);
                write_byte(0x00u, original_page, current_pec, false);
                expect(read_byte(0x04u, current_pec) == original_phase, L"PHASE restore mismatch after group command.");
                expect(read_byte(0x00u, current_pec) == original_page, L"PAGE restore mismatch after group command.");
                return L"PAGE " + format_byte_hex(original_page) + L" restored, PHASE " +
                       format_byte_hex(original_phase) + L" restored";
            });
        };
        auto run_stress_ordering = [&]() {
            run_case(L"Stress Read Byte / Read Word / Read Byte ordering", [&]() -> std::wstring {
                const std::uint8_t rev_a = read_byte(0x98u, current_pec);
                const std::uint16_t status_word = read_word(0x79u, current_pec);
                const std::uint8_t rev_b = read_byte(0x98u, current_pec);

                expect(rev_a == 0x33u && rev_b == 0x33u, L"PMBUS_REVISION changed across mixed read ordering.");
                return L"PMBUS_REVISION " + format_byte_hex(rev_a) + L" -> STATUS_WORD " +
                       format_word_hex(status_word) + L" -> PMBUS_REVISION " + format_byte_hex(rev_b);
            });
            run_case(L"Stress Block Read then Read Byte", [&]() -> std::wstring {
                const ExecResult block_result = read_block(0x9Au, 32, current_pec);
                const std::uint8_t revision = read_byte(0x98u, current_pec);

                expect(!block_result.data.empty(), L"MFR_MODEL block payload empty before short read.");
                expect(revision == 0x33u, L"PMBUS_REVISION mismatch after Block Read.");
                return L"MFR_MODEL block=" + std::to_wstring(block_result.data.size()) +
                       L"B -> PMBUS_REVISION=" + format_byte_hex(revision);
            });
            run_case(L"Stress Process Call then Read Byte", [&]() -> std::wstring {
                const std::uint16_t original = read_word(0x21u, current_pec);
                const std::vector<std::uint8_t> data = {
                    static_cast<std::uint8_t>(original & 0xFFu),
                    static_cast<std::uint8_t>((original >> 8) & 0xFFu)
                };
                const ExecResult process_result = ExecProcessCall(addr, {0x21u}, data, current_pec, false);
                const std::uint8_t revision = read_byte(0x98u, current_pec);
                std::uint16_t echoed = 0u;

                expect(process_result.data.size() == 2u, L"Process Call returned unexpected length before short read.");
                echoed = static_cast<std::uint16_t>(process_result.data[0] |
                                                    (static_cast<std::uint16_t>(process_result.data[1]) << 8));
                expect(echoed == original, L"Process Call echo mismatch before short read.");
                expect(revision == 0x33u, L"PMBUS_REVISION mismatch after Process Call.");
                return L"VOUT_COMMAND echoed=" + format_word_hex(echoed) +
                       L" -> PMBUS_REVISION=" + format_byte_hex(revision);
            });
            run_case(L"Stress Block Write-Read then Read Byte", [&]() -> std::wstring {
                const ExecResult query_result = ExecBlockWriteReadProcessCall(addr, {0x1Au}, {0x98u}, 1, current_pec, false);
                const std::uint8_t revision = read_byte(0x98u, current_pec);

                expect(query_result.data.size() == 1u, L"QUERY(PMBUS_REVISION) result length mismatch before short read.");
                expect(revision == 0x33u, L"PMBUS_REVISION mismatch after Block Write-Read Process Call.");
                return L"QUERY(PMBUS_REVISION)=" + format_byte_hex(query_result.data[0]) +
                       L" -> PMBUS_REVISION=" + format_byte_hex(revision);
            });
            run_case(L"Stress Bad PEC then positive Read Byte", [&]() -> std::wstring {
                const std::uint8_t original = read_byte(0x02u, true);
                std::uint8_t revision = 0u;

                clear_faults_best_effort(true, L"stress bad-PEC preflight");
                try {
                    write_byte(0x02u, original, true, true);
                } catch (const mfc_tool::hid::BridgeStatusException& e) {
                    if (log_) {
                        log_(L"PMBus stress bad-PEC write returned bridge status " +
                             mfc_tool::core::StatusText(e.status()) + L"; validating post-cleanup read");
                    }
                } catch (...) {
                    clear_faults_best_effort(true, L"stress bad-PEC exception cleanup");
                    throw;
                }

                recover_ara_best_effort(L"stress bad-PEC");
                clear_faults_best_effort(true, L"stress bad-PEC cleanup");
                ReinitMasterBusForRetry();
                SleepWithCancel(2);

                revision = read_byte(0x98u, current_pec);
                expect(revision == 0x33u, L"PMBUS_REVISION mismatch after bad PEC cleanup.");
                return L"bad PEC cleaned, PMBUS_REVISION=" + format_byte_hex(revision);
            });
        };
        auto run_manual_full_items = [&]() {
            run_manual(L"Receive Byte transaction", L"Manual: execute a target-specific Receive Byte case and verify returned data + PEC.");
            run_manual(L"SMBALERT_MASK side-effect policy", L"Manual/conditional: target-specific mask bit semantics and SMBALERT side effects must be reviewed with the device design.");
            run_manual(L"ARA end-to-end", L"Manual/lab-policy: if the target asserts SMBALERT externally, run ARA and verify the responder address.");
            run_manual(L"No debug printf in ISR path", L"Manual: verify no ISR-path debug print side effect under sustained traffic.");
            run_manual(L"Threshold / fault injection commands", L"Manual: VOUT/IOUT/VIN/TEMP warn/fault threshold scenarios still require external condition review.");
            if (is_crps_profile) {
                run_manual(L"FWUPLOAD / blackbox side-effect flow",
                           L"Manual: MFR_FWUPLOAD / MFR_FWUPLOAD_STATUS and MFR_CLEAR_BLACK_BOX side-effect sequences are not auto-run.");
            }
            run_manual(L"Forced clock-low timeout", L"Manual: force SCL/SDA stuck-low and verify timeout/recover/recover-fail path with a logic analyzer.");
        };

        update_progress(L"Running 0/" + std::to_wstring(progress_total), L"preparing", 0);

        for (current_loop = 1; current_loop <= repeat_count; ++current_loop) {
            if (log_) {
                log_(L"PMBus checklist [" + suite_name + L"] loop " +
                     std::to_wstring(current_loop) + L"/" + std::to_wstring(repeat_count) + L" started");
            }

            switch (mode) {
            case kChecklistModeBasic:
                run_basic();
                break;
            case kChecklistModePec:
                run_pec();
                break;
            case kChecklistModeError:
                run_error();
                break;
            case kChecklistModeTelemetry:
                run_telemetry();
                break;
            case kChecklistModeMfr:
                run_transport_extensions();
                break;
            case kChecklistModeFull:
                update_progress(L"Running " + std::to_wstring(progress_done) + L"/" +
                                std::to_wstring(progress_total), L"full preflight clear faults", progress_done);
                clear_faults_best_effort(current_pec, L"full preflight");
                ReinitMasterBusForRetry();
                SleepWithCancel(2);
                run_bus_status_preflight();
                run_contract_audit();
                run_basic();
                run_telemetry();
                run_table31_read_smoke();
                run_transport_extensions();
                run_stress_ordering();
                run_pec();
                run_error();
                run_manual_full_items();
                break;
            default:
                throw std::runtime_error("Unknown PMBus checklist mode.");
            }
        }

        int pass_count = 0;
        int fail_count = 0;
        int manual_count = 0;
        std::vector<std::wstring> fail_lines;
        std::vector<std::wstring> manual_lines;
        size_t i = 0u;

        for (i = 0u; i < items.size(); ++i) {
            const ChecklistItem& item = items[i];
            if (item.manual) {
                ++manual_count;
                manual_lines.push_back(item.name + L": " + item.detail);
            } else if (item.pass) {
                ++pass_count;
            } else {
                ++fail_count;
                fail_lines.push_back(item.name + L": " + item.detail);
            }
        }

        {
            std::wstringstream ss;
            ss << suite_name << L" checklist: loops " << repeat_count << L", PASS " << pass_count
               << L" test group(s), FAIL " << fail_count;
            if (manual_count > 0) {
                ss << L", MANUAL " << manual_count;
            }
            SetScanSummaryText(ss.str());
        }

        if (!fail_lines.empty()) {
            std::wstring text = fail_lines.front();
            size_t max_index = (std::min)(fail_lines.size(), static_cast<size_t>(3));
            for (i = 1u; i < max_index; ++i) {
                text += L" | " + fail_lines[i];
            }
            if (fail_lines.size() > max_index) {
                text += L" | ...";
            }
            SetIllegalTestResultText(text);
            SetDecodedText(L"Checklist failures: " + text);
        } else if (!manual_lines.empty()) {
            std::wstring text = L"Automated checks PASS";
            size_t max_index = (std::min)(manual_lines.size(), static_cast<size_t>(2));
            for (i = 0u; i < max_index; ++i) {
                text += (i == 0u) ? L" | " : L" | ";
                text += manual_lines[i];
            }
            if (manual_lines.size() > max_index) {
                text += L" | ...";
            }
            SetIllegalTestResultText(text);
            SetDecodedText(L"Checklist complete. Remaining manual items listed in result/log.");
        } else {
            SetIllegalTestResultText(L"All automated checklist items passed.");
            SetDecodedText(L"All automated checklist items passed. See log for per-item detail.");
        }

        mfc_tool::ui::SafeSetProgressRange(checklist_progress_, progress_done);
        mfc_tool::ui::SafeSetProgressPos(checklist_progress_, progress_done);
        if (log_) {
            std::wstringstream summary_log;
            summary_log << L"PMBus checklist [" << suite_name << L"] complete: loops " << repeat_count
                        << L", PASS " << pass_count
                        << L" test group(s), FAIL " << fail_count;
            if (manual_count > 0) {
                summary_log << L", MANUAL " << manual_count;
            }
            log_(summary_log.str());
        }
        cancel_requested_ = false;
        busy.Reset();
    } catch (const UserCancelled&) {
        SetScanSummaryText(L"Checklist stopped by user.");
        SetIllegalTestResultText(L"Stopped by user.");
        SetDecodedText(L"Checklist stopped by user.");
        if (log_) {
            log_(L"PMBus checklist stopped by user.");
        }
        cancel_requested_ = false;
        busy.Reset();
    } catch (const std::exception& e) {
        const std::wstring msg = AnsiToWide(e.what());
        SetIllegalTestResultText(L"ERROR: " + msg);
        SetDecodedText(L"Checklist suite error: " + msg);
        if (log_) {
            log_(L"PMBus checklist error: " + msg);
        }
        cancel_requested_ = false;
        busy.Reset();
        ::MessageBoxW(m_hWnd, msg.c_str(), L"PMBus Error", MB_ICONERROR | MB_OK);
    }
}

void CPmbusTab::OnStop() {
    RequestCancel();
}

void CPmbusTab::OnChecklistBasic() {
    RunChecklistSuite(kChecklistModeBasic);
}

void CPmbusTab::OnChecklistPec() {
    RunChecklistSuite(kChecklistModePec);
}

void CPmbusTab::OnChecklistError() {
    RunChecklistSuite(kChecklistModeError);
}

void CPmbusTab::OnChecklistTelemetry() {
    RunChecklistSuite(kChecklistModeTelemetry);
}

void CPmbusTab::OnChecklistMfr() {
    RunChecklistSuite(kChecklistModeMfr);
}

void CPmbusTab::OnChecklistFull() {
    RunChecklistSuite(kChecklistModeFull);
}

void CPmbusTab::OnScan() {
    if (!connected_ || service_ == nullptr || !master_enabled_) {
        return;
    }
    try {
        int found = 0;
        const int target_addr = ParseEditInt(master_addr_edit_) & 0x7F;
        std::wstringstream summary;
        const bool pec = (pec_check_.GetCheck() == BST_CHECKED);
        bool preflight_ok = true;
        std::wstring preflight_error;
        auto pace_scan_stage = []() {
            ::Sleep(kPmbusScanStageDelayMs);
        };
        auto log_block_read_failure = [&](int addr, const wchar_t* command_name, const std::exception& e) {
            if (log_) {
                std::wstringstream ss;
                ss << L"PMBus scan detail: addr=0x"
                   << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0') << addr
                   << std::dec << L", " << command_name
                   << L" block read failed: " << AnsiToWide(e.what());
                log_(ss.str());
            }
        };
        auto format_scan_byte = [](std::uint8_t value) -> std::string {
            std::ostringstream ss;
            ss << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(value);
            return ss.str();
        };
        auto require_valid_scan_revision = [&](const ExecResult& result) {
            if (pec && (!result.pec_checked || !result.pec_ok)) {
                std::ostringstream ss;
                ss << "PMBUS_REVISION PEC failed";
                if (!result.data.empty()) {
                    ss << ": data=" << format_scan_byte(result.data[0]);
                }
                ss << ", rx=" << format_scan_byte(result.pec_rx)
                   << ", calc=" << format_scan_byte(result.pec_calc);
                throw std::runtime_error(ss.str());
            }
            if (result.data.size() != 1u) {
                throw std::runtime_error("PMBUS_REVISION returned unexpected length");
            }
            if (result.data[0] != 0x33u) {
                std::ostringstream ss;
                ss << "PMBUS_REVISION unexpected value: data="
                   << format_scan_byte(result.data[0])
                   << ", expected=0x33";
                throw std::runtime_error(ss.str());
            }
        };

        summary << L"Addr  Rev            MFR_ID              MFR_MODEL";
        try {
            ReinitMasterBusForRetry();
            ::Sleep(kPmbusScanPreflightSettleMs);
            if (log_) {
                std::wstringstream ss;
                ss << L"PMBus scan preflight recover OK: addr=0x"
                   << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0') << target_addr;
                log_(ss.str());
            }
        } catch (const std::exception& e) {
            preflight_ok = false;
            preflight_error = AnsiToWide(e.what());
            if (log_) {
                std::wstringstream ss;
                ss << L"PMBus scan preflight recover failed: addr=0x"
                   << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0') << target_addr
                   << std::dec << L", " << preflight_error;
                log_(ss.str());
            }
        }

        if (!preflight_ok) {
            summary << L"\r\n(preflight recover failed: " << preflight_error << L")";
            SetScanSummaryText(summary.str());
            if (log_) {
                log_(L"PMBus scan complete: 0 device(s) responded to PMBUS_REVISION");
            }
            return;
        }

        for (int addr = target_addr; addr <= target_addr; ++addr) {
            try {
                std::wstring mfr_id = L"-";
                std::wstring mfr_model = L"-";
                auto result = ExecReadByCommand(static_cast<std::uint8_t>(addr), {0x98u}, 1, pec, false);
                if (!result.raw.empty() || !result.data.empty()) {
                    require_valid_scan_revision(result);
                    std::wstringstream rev_text;
                    ++found;
                    rev_text << L"1." << static_cast<unsigned int>((result.data[0] >> 4) & 0x0Fu)
                             << L"/1." << static_cast<unsigned int>(result.data[0] & 0x0Fu);
                    pace_scan_stage();
                    try {
                        auto id_result = ExecBlockReadCommand(static_cast<std::uint8_t>(addr), {0x99u}, 32, pec, false);
                        if (!id_result.data.empty()) {
                            mfr_id = BuildDecodedText(0x99u, false, id_result);
                        }
                    } catch (const std::exception& e) {
                        log_block_read_failure(addr, L"MFR_ID", e);
                    }
                    pace_scan_stage();
                    try {
                        auto model_result = ExecBlockReadCommand(static_cast<std::uint8_t>(addr), {0x9Au}, 32, pec, false);
                        if (!model_result.data.empty()) {
                            mfr_model = BuildDecodedText(0x9Au, false, model_result);
                        }
                    } catch (const std::exception& e) {
                        log_block_read_failure(addr, L"MFR_MODEL", e);
                    }
                    summary << L"\r\n0x"
                            << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0') << addr
                            << std::dec << L"  "
                            << rev_text.str()
                            << L"  "
                            << mfr_id
                            << L"  "
                            << mfr_model;
                    if (log_) {
                        std::wstringstream ss;
                        ss << L"PMBus scan hit: addr=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0') << addr
                           << std::dec << L", PMBUS_REVISION=" << rev_text.str()
                           << L", MFR_ID=" << mfr_id
                           << L", MFR_MODEL=" << mfr_model;
                        log_(ss.str());
                    }
                    pace_scan_stage();
                }
            } catch (const std::exception& e) {
                if (log_) {
                    std::wstringstream ss;
                    ss << L"PMBus scan miss: addr=0x"
                       << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0') << addr
                       << std::dec << L", " << AnsiToWide(e.what());
                    log_(ss.str());
                }
                pace_scan_stage();
            }
        }
        if (found == 0) {
            summary << L"\r\n(no PMBus device responded to PMBUS_REVISION)";
        }
        SetScanSummaryText(summary.str());
        if (log_) {
            log_(L"PMBus scan complete: " + std::to_wstring(found) + L" device(s) responded to PMBUS_REVISION");
        }
    } catch (const std::exception& e) {
        const std::wstring msg = AnsiToWide(e.what());
        SetDecodedText(L"PMBus scan error: " + msg);
        SetScanSummaryText(L"Scan failed: " + msg);
        if (log_) {
            log_(L"PMBus scan error: " + msg);
        }
    }
}

void CPmbusTab::OnAra() {
    if (!connected_ || service_ == nullptr || !master_enabled_) {
        return;
    }
    try {
        ExecuteAraHelper(false);
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"PMBus Error", MB_ICONERROR | MB_OK);
    }
}

void CPmbusTab::OnSmbalertRead() {
    if (!connected_ || service_ == nullptr || !master_enabled_) {
        return;
    }
    try {
        const std::uint8_t addr = static_cast<std::uint8_t>(ParseEditInt(master_addr_edit_) & 0x7F);
        const bool pec = (pec_check_.GetCheck() == BST_CHECKED);
        ExecResult result = ExecBlockWriteReadProcessCall(addr, {0x1Bu}, {0x00u}, 2, pec, false);
        if (result.data.size() < 2u) {
            throw std::runtime_error("SMBALERT_MASK read returned less than 2 bytes.");
        }
        const std::uint16_t mask = static_cast<std::uint16_t>(result.data[0] | (result.data[1] << 8));
        wchar_t buf[16] = {};
        swprintf_s(buf, L"0x%04X", static_cast<unsigned int>(mask));
        smbalert_edit_.SetWindowTextW(buf);
        SetRawRxText(mfc_tool::core::HexDump(result.raw));
        SetDecodedText(BuildDecodedText(0x1Bu, false, result));
        UpdatePmbusSummaryLine(L"SMBALERT_MASK read");
        if (log_) {
            log_(L"PMBus SMBALERT_MASK read -> " + std::wstring(buf));
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"PMBus Error", MB_ICONERROR | MB_OK);
    }
}

void CPmbusTab::OnSmbalertWrite() {
    if (!connected_ || service_ == nullptr || !master_enabled_) {
        return;
    }
    try {
        const std::uint8_t addr = static_cast<std::uint8_t>(ParseEditInt(master_addr_edit_) & 0x7F);
        const std::uint16_t mask = static_cast<std::uint16_t>(ParseEditInt(smbalert_edit_) & 0xFFFF);
        const bool pec = (pec_check_.GetCheck() == BST_CHECKED);
        const bool force_bad_pec = ForceBadPecEnabled();
        std::vector<std::uint8_t> data = {
            static_cast<std::uint8_t>(mask & 0xFFu),
            static_cast<std::uint8_t>((mask >> 8) & 0xFFu)
        };
        ExecResult result = ExecWriteByCommand(addr, {0x1Bu}, data, pec, force_bad_pec);
        SetRawRxText(mfc_tool::core::HexDump(result.raw));
        SetDecodedText(L"SMBALERT_MASK write OK");
        UpdatePmbusSummaryLine(L"SMBALERT_MASK write");
        if (log_) {
            wchar_t buf[16] = {};
            swprintf_s(buf, L"0x%04X", static_cast<unsigned int>(mask));
            log_(L"PMBus SMBALERT_MASK write <- " + std::wstring(buf) + (force_bad_pec ? L" (Bad PEC)" : L""));
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"PMBus Error", MB_ICONERROR | MB_OK);
    }
}

void CPmbusTab::OnCommandPresetChanged() {
    ApplyPresetToCommandUi();
}

void CPmbusTab::OnUiSettingChanged() {
    UpdateEnableState();
}

void CPmbusTab::OnMasterPortChanged() {
    PopulateMasterPinCombo();
    RefreshPinUsage();
    if (persist_settings_) {
        persist_settings_();
    }
}

void CPmbusTab::OnProfileChanged() {
    if (master_enabled_) {
        profile_combo_.SetCurSel(ProfileComboIndex(profile_));
        if (log_) {
            log_(L"PMBus profile change ignored while the master bus is active. Disable master first.");
        }
        return;
    }

    const int sel = profile_combo_.GetCurSel();
    SetProfile((sel == 1) ? Profile::Crps : ((sel == 2) ? Profile::TiUcd90xxx : Profile::BasePmbus));
    if (log_) {
        log_(L"PMBus profile selected: " + std::wstring(ProfileIniName()));
    }
    if (persist_settings_) {
        persist_settings_();
    }
}

void CPmbusTab::OnSystemPolicyChanged() {
    const bool lab_policy = IsLabValidationPolicy();

    if (log_) {
        log_(std::wstring(L"PMBus system policy selected: ") +
             (lab_policy ? kPmbusPolicyLabName : kPmbusPolicyProductionName));
    }
    UpdateEnableState();
    if (persist_settings_) {
        persist_settings_();
    }
}
