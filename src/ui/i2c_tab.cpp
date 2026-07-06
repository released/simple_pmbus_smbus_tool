#include "i2c_tab.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../core/text_utils.h"
#include "../hid/hid_bridge_client.h"
#include "layout_utils.h"

namespace {

enum : UINT {
    IDC_I2C_CONFIG_GROUP = 14100,
    IDC_I2C_PORT_LABEL,
    IDC_I2C_PORT_COMBO,
    IDC_I2C_PINS_LABEL,
    IDC_I2C_PINS_COMBO,
    IDC_I2C_ADDR_LABEL,
    IDC_I2C_ADDR_EDIT,
    IDC_I2C_BAUD_LABEL,
    IDC_I2C_BAUD_EDIT,
    IDC_I2C_RESTART_CHECK,
    IDC_I2C_MASTER_ENABLE,
    IDC_I2C_SLAVE_ENABLE,
    IDC_I2C_DISABLE,
    IDC_I2C_MASTER_GROUP,
    IDC_I2C_MASTER_TX_LABEL,
    IDC_I2C_MASTER_TX_EDIT,
    IDC_I2C_MASTER_READ_LABEL,
    IDC_I2C_MASTER_READ_EDIT,
    IDC_I2C_MASTER_WRITE,
    IDC_I2C_MASTER_READ,
    IDC_I2C_MASTER_WR,
    IDC_I2C_MASTER_RX_LABEL,
    IDC_I2C_MASTER_RX_EDIT,
    IDC_I2C_SLAVE_GROUP,
    IDC_I2C_SLAVE_TX_LABEL,
    IDC_I2C_SLAVE_TX_EDIT,
    IDC_I2C_SLAVE_RX_MAX_LABEL,
    IDC_I2C_SLAVE_RX_MAX_EDIT,
    IDC_I2C_SLAVE_SET_TX,
    IDC_I2C_SLAVE_GET_RX,
    IDC_I2C_SLAVE_RX_LABEL,
    IDC_I2C_SLAVE_RX_EDIT,
    IDC_I2C_TOOLS_GROUP,
    IDC_I2C_MONITOR_LABEL,
    IDC_I2C_MONITOR_EDIT,
    IDC_I2C_MONITOR_START,
    IDC_I2C_MONITOR_STOP,
    IDC_I2C_INTERVAL_LABEL,
    IDC_I2C_INTERVAL_EDIT,
    IDC_I2C_INTERVAL_START,
    IDC_I2C_INTERVAL_STOP,
    IDC_I2C_TARGET_LABEL,
    IDC_I2C_TARGET_COMBO,
    IDC_I2C_GEN_LEN_LABEL,
    IDC_I2C_GEN_LEN_EDIT,
    IDC_I2C_GEN_START_LABEL,
    IDC_I2C_GEN_START_EDIT,
    IDC_I2C_GEN_STEP_LABEL,
    IDC_I2C_GEN_STEP_EDIT,
    IDC_I2C_GEN_BTN,
    IDC_I2C_COUNTER_CHECK,
    IDC_I2C_COUNTER_IDX_LABEL,
    IDC_I2C_COUNTER_IDX_EDIT,
    IDC_I2C_COUNTER_STEP_LABEL,
    IDC_I2C_COUNTER_STEP_EDIT,
    IDC_I2C_COUNTER_RESET,
    IDC_I2C_STATUS_GROUP,
    IDC_I2C_STATUS_EDIT,
};

constexpr int kI2cMaxSlaveTxPerReport = static_cast<int>(mfc_tool::core::kBridgeMaxPayload - 2u);
constexpr int kI2cMaxShortRead = static_cast<int>(mfc_tool::core::kBridgeMaxPayload - 1u);
constexpr int kI2cMaxIntervalErrorStreak = 5;

std::wstring ResponseBytesText(const std::vector<std::uint8_t>& rx) {
    if (rx.empty()) {
        return L"(empty)";
    }
    return mfc_tool::core::HexDump(rx);
}

} // namespace

BEGIN_MESSAGE_MAP(CI2cTab, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_I2C_PORT_COMBO, &CI2cTab::OnPortChanged)
    ON_CBN_SELCHANGE(IDC_I2C_PINS_COMBO, &CI2cTab::OnPinsChanged)
    ON_BN_CLICKED(IDC_I2C_MASTER_ENABLE, &CI2cTab::OnMasterEnable)
    ON_BN_CLICKED(IDC_I2C_SLAVE_ENABLE, &CI2cTab::OnSlaveEnable)
    ON_BN_CLICKED(IDC_I2C_DISABLE, &CI2cTab::OnDisablePort)
    ON_BN_CLICKED(IDC_I2C_MASTER_WRITE, &CI2cTab::OnMasterWrite)
    ON_BN_CLICKED(IDC_I2C_MASTER_READ, &CI2cTab::OnMasterRead)
    ON_BN_CLICKED(IDC_I2C_MASTER_WR, &CI2cTab::OnWriteThenRead)
    ON_BN_CLICKED(IDC_I2C_SLAVE_SET_TX, &CI2cTab::OnSlaveSetTx)
    ON_BN_CLICKED(IDC_I2C_SLAVE_GET_RX, &CI2cTab::OnSlaveGetRx)
    ON_BN_CLICKED(IDC_I2C_MONITOR_START, &CI2cTab::OnMonitorStart)
    ON_BN_CLICKED(IDC_I2C_MONITOR_STOP, &CI2cTab::OnMonitorStop)
    ON_BN_CLICKED(IDC_I2C_INTERVAL_START, &CI2cTab::OnIntervalStart)
    ON_BN_CLICKED(IDC_I2C_INTERVAL_STOP, &CI2cTab::OnIntervalStop)
    ON_BN_CLICKED(IDC_I2C_GEN_BTN, &CI2cTab::OnGenerateData)
    ON_BN_CLICKED(IDC_I2C_COUNTER_CHECK, &CI2cTab::OnCounterChanged)
    ON_BN_CLICKED(IDC_I2C_COUNTER_RESET, &CI2cTab::OnCounterReset)
    ON_EN_CHANGE(IDC_I2C_MASTER_TX_EDIT, &CI2cTab::OnMasterTxChanged)
    ON_EN_CHANGE(IDC_I2C_SLAVE_TX_EDIT, &CI2cTab::OnSlaveTxChanged)
END_MESSAGE_MAP()

BOOL CI2cTab::Create(CWnd* parent, const RECT& rect, UINT id) {
    CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                                      reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
    return CWnd::CreateEx(WS_EX_CONTROLPARENT, cls, L"I2C", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          rect, parent, id);
}

void CI2cTab::Bind(mfc_tool::core::BridgeService* service,
                   std::function<void(const std::wstring&)> logger,
                   mfc_tool::core::PinUsageRegistry* pin_usage) {
    service_ = service;
    log_ = std::move(logger);
    pin_usage_ = pin_usage;
    RefreshPinUsage();
}

int CI2cTab::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    ui_font_.CreatePointFont(90, L"Segoe UI");

    auto fail = [this](BOOL ok, const wchar_t* name) -> bool {
        if (!ok && log_) {
            log_(std::wstring(L"Create I2C control failed: ") + name);
        }
        return ok != FALSE;
    };

    if (!fail(config_group_.Create(L"I2C Bridge", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_I2C_CONFIG_GROUP), L"config group")) return -1;
    if (!fail(port_label_.Create(L"Port", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_PORT_LABEL), L"port label")) return -1;
    if (!fail(port_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_I2C_PORT_COMBO), L"port combo")) return -1;
    if (!fail(pins_label_.Create(L"Pins", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_PINS_LABEL), L"pins label")) return -1;
    if (!fail(pins_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_I2C_PINS_COMBO), L"pins combo")) return -1;
    if (!fail(addr_label_.Create(L"Addr", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_ADDR_LABEL), L"addr label")) return -1;
    if (!fail(addr_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_ADDR_EDIT), L"addr edit")) return -1;
    if (!fail(baud_label_.Create(L"Speed", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_BAUD_LABEL), L"baud label")) return -1;
    if (!fail(baud_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_BAUD_EDIT), L"baud edit")) return -1;
    if (!fail(repeated_start_check_.Create(L"Repeated Start", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(), this, IDC_I2C_RESTART_CHECK), L"restart check")) return -1;
    if (!fail(master_enable_btn_.Create(L"Enable Master", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_MASTER_ENABLE), L"enable master")) return -1;
    if (!fail(slave_enable_btn_.Create(L"Enable Slave", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_SLAVE_ENABLE), L"enable slave")) return -1;
    if (!fail(disable_btn_.Create(L"Disable", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_DISABLE), L"disable")) return -1;

    if (!fail(master_group_.Create(L"Master", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_I2C_MASTER_GROUP), L"master group")) return -1;
    if (!fail(master_tx_label_.Create(L"TX HEX", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_MASTER_TX_LABEL), L"master tx label")) return -1;
    if (!fail(master_tx_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_MASTER_TX_EDIT), L"master tx edit")) return -1;
    if (!fail(master_read_label_.Create(L"Read Len", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_MASTER_READ_LABEL), L"master read label")) return -1;
    if (!fail(master_read_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_MASTER_READ_EDIT), L"master read edit")) return -1;
    if (!fail(master_write_btn_.Create(L"Write", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_MASTER_WRITE), L"master write")) return -1;
    if (!fail(master_read_btn_.Create(L"Read", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_MASTER_READ), L"master read")) return -1;
    if (!fail(write_read_btn_.Create(L"Write Then Read", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_MASTER_WR), L"write read")) return -1;
    if (!fail(master_rx_label_.Create(L"Master RX", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_MASTER_RX_LABEL), L"master rx label")) return -1;
    if (!fail(master_rx_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, CRect(), this, IDC_I2C_MASTER_RX_EDIT), L"master rx edit")) return -1;

    if (!fail(slave_group_.Create(L"Slave", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_I2C_SLAVE_GROUP), L"slave group")) return -1;
    if (!fail(slave_tx_label_.Create(L"TX HEX", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_SLAVE_TX_LABEL), L"slave tx label")) return -1;
    if (!fail(slave_tx_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_SLAVE_TX_EDIT), L"slave tx edit")) return -1;
    if (!fail(slave_rx_max_label_.Create(L"RX Max", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_SLAVE_RX_MAX_LABEL), L"slave rx max label")) return -1;
    if (!fail(slave_rx_max_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_SLAVE_RX_MAX_EDIT), L"slave rx max edit")) return -1;
    if (!fail(slave_set_tx_btn_.Create(L"Set TX", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_SLAVE_SET_TX), L"slave set tx")) return -1;
    if (!fail(slave_get_rx_btn_.Create(L"Get RX", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_SLAVE_GET_RX), L"slave get rx")) return -1;
    if (!fail(slave_rx_label_.Create(L"Slave RX", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_SLAVE_RX_LABEL), L"slave rx label")) return -1;
    if (!fail(slave_rx_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, CRect(), this, IDC_I2C_SLAVE_RX_EDIT), L"slave rx edit")) return -1;

    if (!fail(tools_group_.Create(L"Tools", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_I2C_TOOLS_GROUP), L"tools group")) return -1;
    if (!fail(monitor_label_.Create(L"Monitor ms", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_MONITOR_LABEL), L"monitor label")) return -1;
    if (!fail(monitor_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_MONITOR_EDIT), L"monitor edit")) return -1;
    if (!fail(monitor_start_btn_.Create(L"Start", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_MONITOR_START), L"monitor start")) return -1;
    if (!fail(monitor_stop_btn_.Create(L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_MONITOR_STOP), L"monitor stop")) return -1;
    if (!fail(interval_label_.Create(L"Send 100ms", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_INTERVAL_LABEL), L"interval label")) return -1;
    if (!fail(interval_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_INTERVAL_EDIT), L"interval edit")) return -1;
    if (!fail(interval_start_btn_.Create(L"Start", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_INTERVAL_START), L"interval start")) return -1;
    if (!fail(interval_stop_btn_.Create(L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_INTERVAL_STOP), L"interval stop")) return -1;
    if (!fail(target_label_.Create(L"Target", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_TARGET_LABEL), L"target label")) return -1;
    if (!fail(target_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_I2C_TARGET_COMBO), L"target combo")) return -1;
    if (!fail(gen_len_label_.Create(L"Len", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_GEN_LEN_LABEL), L"gen len label")) return -1;
    if (!fail(gen_len_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_GEN_LEN_EDIT), L"gen len edit")) return -1;
    if (!fail(gen_start_label_.Create(L"Start", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_GEN_START_LABEL), L"gen start label")) return -1;
    if (!fail(gen_start_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_GEN_START_EDIT), L"gen start edit")) return -1;
    if (!fail(gen_step_label_.Create(L"Step", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_GEN_STEP_LABEL), L"gen step label")) return -1;
    if (!fail(gen_step_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_GEN_STEP_EDIT), L"gen step edit")) return -1;
    if (!fail(gen_btn_.Create(L"Generate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_GEN_BTN), L"generate")) return -1;
    if (!fail(counter_check_.Create(L"Counter", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(), this, IDC_I2C_COUNTER_CHECK), L"counter check")) return -1;
    if (!fail(counter_idx_label_.Create(L"Idx", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_COUNTER_IDX_LABEL), L"counter idx label")) return -1;
    if (!fail(counter_idx_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_COUNTER_IDX_EDIT), L"counter idx edit")) return -1;
    if (!fail(counter_step_label_.Create(L"Step", WS_CHILD | WS_VISIBLE, CRect(), this, IDC_I2C_COUNTER_STEP_LABEL), L"counter step label")) return -1;
    if (!fail(counter_step_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_I2C_COUNTER_STEP_EDIT), L"counter step edit")) return -1;
    if (!fail(counter_reset_btn_.Create(L"Reset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_I2C_COUNTER_RESET), L"counter reset")) return -1;

    if (!fail(status_group_.Create(L"Response", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(), this, IDC_I2C_STATUS_GROUP), L"status group")) return -1;
    if (!fail(status_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, CRect(), this, IDC_I2C_STATUS_EDIT), L"status edit")) return -1;

    std::vector<CWnd*> controls = {
        &config_group_, &port_label_, &port_combo_, &pins_label_, &pins_combo_, &addr_label_, &addr_edit_,
        &baud_label_, &baud_edit_, &repeated_start_check_, &master_enable_btn_, &slave_enable_btn_, &disable_btn_,
        &master_group_, &master_tx_label_, &master_tx_edit_, &master_read_label_, &master_read_edit_,
        &master_write_btn_, &master_read_btn_, &write_read_btn_, &master_rx_label_, &master_rx_edit_,
        &slave_group_, &slave_tx_label_, &slave_tx_edit_, &slave_rx_max_label_, &slave_rx_max_edit_,
        &slave_set_tx_btn_, &slave_get_rx_btn_, &slave_rx_label_, &slave_rx_edit_, &tools_group_,
        &monitor_label_, &monitor_edit_, &monitor_start_btn_, &monitor_stop_btn_, &interval_label_, &interval_edit_,
        &interval_start_btn_, &interval_stop_btn_, &target_label_, &target_combo_, &gen_len_label_, &gen_len_edit_,
        &gen_start_label_, &gen_start_edit_, &gen_step_label_, &gen_step_edit_, &gen_btn_, &counter_check_,
        &counter_idx_label_, &counter_idx_edit_, &counter_step_label_, &counter_step_edit_, &counter_reset_btn_,
        &status_group_, &status_edit_
    };
    for (CWnd* w : controls) {
        w->SetFont(&ui_font_);
    }

    target_combo_.AddString(L"Master TX");
    target_combo_.AddString(L"Slave TX");
    target_combo_.SetCurSel(0);

    port_state_[0].pin_pair = L"I2C0_PB4_PB5";
    port_state_[1].pin_pair = L"I2C1_PB2_PB3";
    PopulatePortCombo();
    LoadVisibleFromState();
    SetConnected(false);
    return 0;
}

void CI2cTab::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    if (!::IsWindow(config_group_.GetSafeHwnd())) {
        return;
    }

    const int margin = 8;
    const int gap = 6;
    const int row_h = 26;
    const int label_y_pad = 4;
    const int group_top_pad = 22;
    const int config_h = 104;
    const int tools_h = 104;
    const int status_h = (std::max)(90, cy / 5);
    const int available_w = (std::max)(200, cx - margin * 2);
    const int half_w = (std::max)(260, (available_w - gap) / 2);
    const int master_slave_h = (std::max)(132, cy - margin * 5 - config_h - tools_h - status_h);
    int x;
    int y;
    int right_x;

    mfc_tool::ui::SafeMoveWindow(config_group_, margin, margin, available_w, config_h);
    x = margin + 12;
    y = margin + group_top_pad;
    {
        const int group_left = margin + 12;
        const int group_right = margin + available_w - 12;
        const int port_w = 78;
        const int addr_w = 70;
        const int baud_w = 92;
        const int restart_w = (std::max)(126, mfc_tool::ui::MeasureButtonMinWidth(repeated_start_check_, 8));
        const int fixed_w =
            mfc_tool::ui::MeasureControlTextWidth(port_label_, 8) + gap + port_w + gap +
            mfc_tool::ui::MeasureControlTextWidth(pins_label_, 8) + gap +
            mfc_tool::ui::MeasureControlTextWidth(addr_label_, 8) + gap + addr_w + gap +
            mfc_tool::ui::MeasureControlTextWidth(baud_label_, 8) + gap + baud_w + gap +
            restart_w;
        const int pins_w = (std::max)(180, group_right - group_left - fixed_w);
        const int master_w = (std::max)(112, mfc_tool::ui::MeasureButtonMinWidth(master_enable_btn_));
        const int slave_w = (std::max)(104, mfc_tool::ui::MeasureButtonMinWidth(slave_enable_btn_));
        const int disable_w = (std::max)(78, mfc_tool::ui::MeasureButtonMinWidth(disable_btn_));
        const int action_w = master_w + gap + slave_w + gap + disable_w;
        const int action_x = (std::max)(group_left, group_left + ((group_right - group_left) - action_w) / 2);
        int action_y;

        x = mfc_tool::ui::PlaceLabelAndControl(port_label_, port_combo_, x, y + label_y_pad, y, port_w, row_h + 120, gap, 8) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(pins_label_, pins_combo_, x, y + label_y_pad, y, pins_w, row_h + 160, gap, 8) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(addr_label_, addr_edit_, x, y + label_y_pad, y, addr_w, row_h, gap, 8) + gap;
        x = mfc_tool::ui::PlaceLabelAndControl(baud_label_, baud_edit_, x, y + label_y_pad, y, baud_w, row_h, gap, 8) + gap;
        mfc_tool::ui::SafeMoveWindow(repeated_start_check_, x, y + 3, restart_w, 20);

        action_y = y + row_h + 8;
        x = action_x;
        mfc_tool::ui::SafeMoveWindow(master_enable_btn_, x, action_y, master_w, row_h);
        x += master_w + gap;
        mfc_tool::ui::SafeMoveWindow(slave_enable_btn_, x, action_y, slave_w, row_h);
        x += slave_w + gap;
        mfc_tool::ui::SafeMoveWindow(disable_btn_, x, action_y, disable_w, row_h);
    }

    y = margin + config_h + gap;
    mfc_tool::ui::SafeMoveWindow(master_group_, margin, y, half_w, master_slave_h);
    x = margin + 12;
    int gy = y + group_top_pad;
    right_x = margin + half_w + gap;
    mfc_tool::ui::PlaceLabel(master_tx_label_, x, gy + label_y_pad);
    mfc_tool::ui::SafeMoveWindow(master_tx_edit_, x + 62, gy, (std::max)(140, half_w - 260), row_h);
    mfc_tool::ui::PlaceLabel(master_read_label_, x + half_w - 188, gy + label_y_pad);
    mfc_tool::ui::SafeMoveWindow(master_read_edit_, x + half_w - 118, gy, 58, row_h);
    gy += row_h + gap;
    mfc_tool::ui::SafeMoveWindow(master_write_btn_, x, gy, 82, row_h);
    mfc_tool::ui::SafeMoveWindow(master_read_btn_, x + 88, gy, 82, row_h);
    mfc_tool::ui::SafeMoveWindow(write_read_btn_, x + 176, gy, 132, row_h);
    gy += row_h + gap;
    mfc_tool::ui::PlaceLabel(master_rx_label_, x, gy + label_y_pad);
    gy += 20;
    mfc_tool::ui::SafeMoveWindow(master_rx_edit_, x, gy, half_w - 24, (std::max)(50, y + master_slave_h - gy - 10));

    mfc_tool::ui::SafeMoveWindow(slave_group_, right_x, y, available_w - half_w - gap, master_slave_h);
    x = right_x + 12;
    gy = y + group_top_pad;
    const int slave_w = available_w - half_w - gap;
    mfc_tool::ui::PlaceLabel(slave_tx_label_, x, gy + label_y_pad);
    mfc_tool::ui::SafeMoveWindow(slave_tx_edit_, x + 62, gy, (std::max)(140, slave_w - 260), row_h);
    mfc_tool::ui::PlaceLabel(slave_rx_max_label_, x + slave_w - 188, gy + label_y_pad);
    mfc_tool::ui::SafeMoveWindow(slave_rx_max_edit_, x + slave_w - 118, gy, 58, row_h);
    gy += row_h + gap;
    mfc_tool::ui::SafeMoveWindow(slave_set_tx_btn_, x, gy, 82, row_h);
    mfc_tool::ui::SafeMoveWindow(slave_get_rx_btn_, x + 88, gy, 82, row_h);
    gy += row_h + gap;
    mfc_tool::ui::PlaceLabel(slave_rx_label_, x, gy + label_y_pad);
    gy += 20;
    mfc_tool::ui::SafeMoveWindow(slave_rx_edit_, x, gy, slave_w - 24, (std::max)(50, y + master_slave_h - gy - 10));

    y += master_slave_h + gap;
    mfc_tool::ui::SafeMoveWindow(tools_group_, margin, y, available_w, tools_h);
    x = margin + 12;
    gy = y + group_top_pad;
    x = mfc_tool::ui::PlaceLabelAndControl(monitor_label_, monitor_edit_, x, gy + label_y_pad, gy, 70, row_h, gap, 8) + gap;
    mfc_tool::ui::SafeMoveWindow(monitor_start_btn_, x, gy, 70, row_h);
    x += 76;
    mfc_tool::ui::SafeMoveWindow(monitor_stop_btn_, x, gy, 70, row_h);
    x += 86;
    x = mfc_tool::ui::PlaceLabelAndControl(interval_label_, interval_edit_, x, gy + label_y_pad, gy, 58, row_h, gap, 8) + gap;
    mfc_tool::ui::SafeMoveWindow(interval_start_btn_, x, gy, 70, row_h);
    x += 76;
    mfc_tool::ui::SafeMoveWindow(interval_stop_btn_, x, gy, 70, row_h);

    x = margin + 12;
    gy += row_h + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(target_label_, target_combo_, x, gy + label_y_pad, gy, 110, row_h + 80, gap, 8) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(gen_len_label_, gen_len_edit_, x, gy + label_y_pad, gy, 58, row_h, gap, 8) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(gen_start_label_, gen_start_edit_, x, gy + label_y_pad, gy, 70, row_h, gap, 8) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(gen_step_label_, gen_step_edit_, x, gy + label_y_pad, gy, 58, row_h, gap, 8) + gap;
    mfc_tool::ui::SafeMoveWindow(gen_btn_, x, gy, 86, row_h);
    x += 100;
    mfc_tool::ui::SafeMoveWindow(counter_check_, x, gy + 3, 88, 20);
    x += 96;
    x = mfc_tool::ui::PlaceLabelAndControl(counter_idx_label_, counter_idx_edit_, x, gy + label_y_pad, gy, 58, row_h, gap, 8) + gap;
    x = mfc_tool::ui::PlaceLabelAndControl(counter_step_label_, counter_step_edit_, x, gy + label_y_pad, gy, 58, row_h, gap, 8) + gap;
    mfc_tool::ui::SafeMoveWindow(counter_reset_btn_, x, gy, 70, row_h);

    y += tools_h + gap;
    mfc_tool::ui::SafeMoveWindow(status_group_, margin, y, available_w, (std::max)(60, cy - y - margin));
    mfc_tool::ui::SafeMoveWindow(status_edit_, margin + 12, y + group_top_pad, available_w - 24, (std::max)(34, cy - y - margin - group_top_pad - 8));
}

void CI2cTab::SetConnected(bool connected) {
    connected_ = connected;
    if (!connected_) {
        OnDisconnected();
    }
    UpdateEnableState();
}

void CI2cTab::OnDisconnected() {
    StopAllTimers();
    for (int port = 0; port < 2; ++port) {
        runtime_[port] = PortRuntime{};
    }
    RefreshPinUsage();
    UpdateEnableState();
}

void CI2cTab::LoadState(const mfc_tool::core::AppState& state) {
    for (int port = 0; port < 2; ++port) {
        const auto& src = state.i2c[port];
        auto& dst = port_state_[port];
        dst.pin_pair = src.pin_pair;
        dst.baud = src.baud;
        dst.addr = src.addr;
        dst.repeated_start = src.repeated_start;
        dst.master_tx_hex = src.master_tx_hex;
        dst.master_read_len = src.master_read_len;
        dst.slave_tx_hex = src.slave_tx_hex;
        dst.slave_rx_max = src.slave_rx_max;
        dst.monitor_interval_ms = src.monitor_interval_ms;
        dst.interval_send_100ms = src.interval_send_100ms;
        dst.tx_target = src.tx_target;
        dst.gen_len = src.gen_len;
        dst.gen_start = src.gen_start;
        dst.gen_step = src.gen_step;
        dst.counter_enable = src.counter_enable;
        dst.counter_index = src.counter_index;
        dst.counter_step = src.counter_step;
    }
    LoadVisibleFromState();
}

void CI2cTab::SaveState(mfc_tool::core::AppState* state) const {
    if (state == nullptr) {
        return;
    }
    const_cast<CI2cTab*>(this)->SaveVisibleToState();
    for (int port = 0; port < 2; ++port) {
        auto& dst = state->i2c[port];
        const auto& src = port_state_[port];
        dst.pin_pair = src.pin_pair;
        dst.baud = src.baud;
        dst.addr = src.addr;
        dst.repeated_start = src.repeated_start;
        dst.master_tx_hex = src.master_tx_hex;
        dst.master_read_len = src.master_read_len;
        dst.slave_tx_hex = src.slave_tx_hex;
        dst.slave_rx_max = src.slave_rx_max;
        dst.monitor_interval_ms = src.monitor_interval_ms;
        dst.interval_send_100ms = src.interval_send_100ms;
        dst.tx_target = src.tx_target;
        dst.gen_len = src.gen_len;
        dst.gen_start = src.gen_start;
        dst.gen_step = src.gen_step;
        dst.counter_enable = src.counter_enable;
        dst.counter_index = src.counter_index;
        dst.counter_step = src.counter_step;
    }
}

void CI2cTab::PopulatePortCombo() {
    port_combo_.ResetContent();
    int idx = port_combo_.AddString(L"I2C0");
    port_combo_.SetItemData(idx, 0);
    idx = port_combo_.AddString(L"I2C1");
    port_combo_.SetItemData(idx, 1);
    port_combo_.SetCurSel(visible_port_ == 1 ? 1 : 0);
}

void CI2cTab::PopulatePinCombo(const std::wstring& preferred_ini_name) {
    const int port = visible_port_;
    const auto& pairs = mfc_tool::core::board_i2c::AllPinPairs();
    const std::wstring preferred = preferred_ini_name.empty() ? port_state_[port].pin_pair : preferred_ini_name;
    int selected = 0;
    int row = 0;

    pins_combo_.ResetContent();
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (pairs[i].i2c_port != port) {
            continue;
        }
        const int idx = pins_combo_.AddString(pairs[i].label);
        pins_combo_.SetItemData(idx, static_cast<DWORD_PTR>(i));
        if (preferred == pairs[i].ini_name) {
            selected = row;
        }
        ++row;
    }
    if (pins_combo_.GetCount() > 0) {
        pins_combo_.SetCurSel(selected);
    }
}

void CI2cTab::SaveVisibleToState() {
    if (loading_ || !::IsWindow(port_combo_.GetSafeHwnd())) {
        return;
    }

    auto& s = port_state_[visible_port_];
    s.pin_pair = CurrentPinPair().ini_name;
    s.baud = GetEditText(baud_edit_);
    s.addr = GetEditText(addr_edit_);
    s.repeated_start = repeated_start_check_.GetCheck() == BST_CHECKED ? L"1" : L"0";
    s.master_tx_hex = GetEditText(master_tx_edit_);
    s.master_read_len = GetEditText(master_read_edit_);
    s.slave_tx_hex = GetEditText(slave_tx_edit_);
    s.slave_rx_max = GetEditText(slave_rx_max_edit_);
    s.monitor_interval_ms = GetEditText(monitor_edit_);
    s.interval_send_100ms = GetEditText(interval_edit_);
    s.tx_target = target_combo_.GetCurSel() == 1 ? L"slave" : L"master";
    s.gen_len = GetEditText(gen_len_edit_);
    s.gen_start = GetEditText(gen_start_edit_);
    s.gen_step = GetEditText(gen_step_edit_);
    s.counter_enable = counter_check_.GetCheck() == BST_CHECKED ? L"1" : L"0";
    s.counter_index = GetEditText(counter_idx_edit_);
    s.counter_step = GetEditText(counter_step_edit_);
}

void CI2cTab::LoadVisibleFromState() {
    if (!::IsWindow(port_combo_.GetSafeHwnd())) {
        return;
    }

    loading_ = true;
    port_combo_.SetCurSel(visible_port_ == 1 ? 1 : 0);
    PopulatePinCombo();

    const auto& s = port_state_[visible_port_];
    baud_edit_.SetWindowTextW(s.baud.c_str());
    addr_edit_.SetWindowTextW(s.addr.c_str());
    repeated_start_check_.SetCheck(s.repeated_start == L"1" ? BST_CHECKED : BST_UNCHECKED);
    master_tx_edit_.SetWindowTextW(s.master_tx_hex.c_str());
    master_read_edit_.SetWindowTextW(s.master_read_len.c_str());
    slave_tx_edit_.SetWindowTextW(s.slave_tx_hex.c_str());
    slave_rx_max_edit_.SetWindowTextW(s.slave_rx_max.c_str());
    monitor_edit_.SetWindowTextW(s.monitor_interval_ms.c_str());
    interval_edit_.SetWindowTextW(s.interval_send_100ms.c_str());
    target_combo_.SetCurSel(s.tx_target == L"slave" ? 1 : 0);
    gen_len_edit_.SetWindowTextW(s.gen_len.c_str());
    gen_start_edit_.SetWindowTextW(s.gen_start.c_str());
    gen_step_edit_.SetWindowTextW(s.gen_step.c_str());
    counter_check_.SetCheck(s.counter_enable == L"1" ? BST_CHECKED : BST_UNCHECKED);
    counter_idx_edit_.SetWindowTextW(s.counter_index.c_str());
    counter_step_edit_.SetWindowTextW(s.counter_step.c_str());
    loading_ = false;

    UpdateTxHexLabels();
    RefreshPinUsage();
    UpdateEnableState();
}

void CI2cTab::UpdateEnableState() {
    if (!::IsWindow(master_enable_btn_.GetSafeHwnd())) {
        return;
    }

    SaveVisibleToState();
    const int port = visible_port_;
    const Role role = runtime_[port].role;
    const bool ready = connected_ && service_ != nullptr;
    const bool active = role != Role::None;
    const bool is_master = role == Role::Master;
    const bool is_slave = role == Role::Slave;
    const bool monitor = runtime_[port].monitoring;
    const bool interval = runtime_[port].interval_sending;
    const bool shared_active = pin_usage_ != nullptr &&
        pin_usage_->AnyActiveExcept(SharedOwnerIds(), {OwnerId(port, Role::Master), OwnerId(port, Role::Slave)});

    mfc_tool::ui::SafeEnableWindow(port_combo_, ready && !active && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(pins_combo_, ready && !active && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(addr_edit_, ready && !active && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(baud_edit_, ready && !active && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(repeated_start_check_, ready && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(master_enable_btn_, ready && !active && !shared_active);
    mfc_tool::ui::SafeEnableWindow(slave_enable_btn_, ready && !active && !shared_active);
    mfc_tool::ui::SafeEnableWindow(disable_btn_, ready && active && !monitor && !interval);

    mfc_tool::ui::SafeEnableWindow(master_tx_edit_, ready && !monitor);
    mfc_tool::ui::SafeEnableWindow(master_read_edit_, ready && !monitor);
    mfc_tool::ui::SafeEnableWindow(master_write_btn_, ready && is_master && !monitor);
    mfc_tool::ui::SafeEnableWindow(master_read_btn_, ready && is_master && !monitor);
    mfc_tool::ui::SafeEnableWindow(write_read_btn_, ready && is_master && !monitor);

    mfc_tool::ui::SafeEnableWindow(slave_tx_edit_, ready && !monitor);
    mfc_tool::ui::SafeEnableWindow(slave_rx_max_edit_, ready && !monitor);
    mfc_tool::ui::SafeEnableWindow(slave_set_tx_btn_, ready && is_slave && !monitor);
    mfc_tool::ui::SafeEnableWindow(slave_get_rx_btn_, ready && is_slave && !monitor);

    mfc_tool::ui::SafeEnableWindow(monitor_edit_, ready && active && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(monitor_start_btn_, ready && active && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(monitor_stop_btn_, ready && monitor);
    mfc_tool::ui::SafeEnableWindow(interval_edit_, ready && active && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(interval_start_btn_, ready && active && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(interval_stop_btn_, ready && interval);
    mfc_tool::ui::SafeEnableWindow(target_combo_, ready && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(gen_len_edit_, ready && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(gen_start_edit_, ready && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(gen_step_edit_, ready && !monitor && !interval);
    mfc_tool::ui::SafeEnableWindow(gen_btn_, ready && !monitor && !interval);
    mfc_tool::ui::SetCounterControlsEnabled(counter_check_, counter_idx_edit_, counter_step_edit_,
                                            counter_reset_btn_, ready && !monitor && !interval);
}

void CI2cTab::RefreshPinUsage() {
    if (pin_usage_ == nullptr) {
        return;
    }

    for (int port = 0; port < 2; ++port) {
        const auto& pins = PinPairForPort(port);
        const std::wstring port_name = mfc_tool::core::board_i2c::PortLabel(port);
        const std::wstring master_owner = OwnerId(port, Role::Master);
        const std::wstring slave_owner = OwnerId(port, Role::Slave);

        pin_usage_->SetLabel(master_owner, port_name + L" master");
        pin_usage_->SetLabel(slave_owner, port_name + L" slave");
        pin_usage_->SetClaim(master_owner, {pins.sda_pin, pins.scl_pin});
        pin_usage_->SetClaim(slave_owner, {pins.sda_pin, pins.scl_pin});
        pin_usage_->SetActive(master_owner, runtime_[port].role == Role::Master);
        pin_usage_->SetActive(slave_owner, runtime_[port].role == Role::Slave);
    }
}

void CI2cTab::SetPortRole(int port, Role role) {
    if (port < 0 || port > 1) {
        return;
    }
    runtime_[port].role = role;
    if (role == Role::None) {
        StopPortTimers(port);
    }
    RefreshPinUsage();
    UpdateEnableState();
}

void CI2cTab::StopPortTimers(int port) {
    if (port != visible_port_) {
        runtime_[port].monitoring = false;
        runtime_[port].interval_sending = false;
        return;
    }
    if (runtime_[port].monitoring) {
        KillTimer(kMonitorTimer);
    }
    if (runtime_[port].interval_sending) {
        KillTimer(kIntervalTimer);
    }
    runtime_[port].monitoring = false;
    runtime_[port].interval_sending = false;
    runtime_[port].interval_error_streak = 0;
}

void CI2cTab::StopAllTimers() {
    KillTimer(kMonitorTimer);
    KillTimer(kIntervalTimer);
    for (int port = 0; port < 2; ++port) {
        runtime_[port].monitoring = false;
        runtime_[port].interval_sending = false;
        runtime_[port].interval_error_streak = 0;
    }
}

void CI2cTab::DisableOtherPort(int port) {
    const int other_port = port == 0 ? 1 : 0;
    runtime_[other_port].role = Role::None;
    runtime_[other_port].monitoring = false;
    runtime_[other_port].interval_sending = false;
}

void CI2cTab::OnPortChanged() {
    if (loading_) {
        return;
    }
    SaveVisibleToState();
    visible_port_ = CurrentPort();
    LoadVisibleFromState();
}

void CI2cTab::OnPinsChanged() {
    if (loading_) {
        return;
    }
    SaveVisibleToState();
    RefreshPinUsage();
}

void CI2cTab::OnMasterEnable() {
    try {
        if (service_ == nullptr || !connected_) {
            return;
        }
        SaveVisibleToState();
        const int port = visible_port_;
        const auto& pins = CurrentPinPair();

        if (pin_usage_ != nullptr &&
            pin_usage_->AnyActiveExcept(SharedOwnerIds(), {OwnerId(port, Role::Master), OwnerId(port, Role::Slave)})) {
            throw std::runtime_error("Another I2C/PMBus/SMBus function is already active. Disable it first.");
        }
        if (pin_usage_ != nullptr &&
            pin_usage_->AnyPinOccupied({pins.sda_pin, pins.scl_pin}, {OwnerId(port, Role::Master), OwnerId(port, Role::Slave)})) {
            throw std::runtime_error("The selected I2C pins are already active in another function.");
        }

        service_->I2cMasterInit(port, pins.sda_pin, pins.scl_pin, CurrentBaud());
        DisableOtherPort(port);
        SetPortRole(port, Role::Master);
        SetStatusText(mfc_tool::core::board_i2c::PortLabel(port) + L" master enabled on " + pins.label);
        if (log_) {
            log_(mfc_tool::core::board_i2c::PortLabel(port) + L" master enabled on " + pins.label);
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Error", MB_ICONERROR | MB_OK);
    }
    RefreshPinUsage();
    UpdateEnableState();
}

void CI2cTab::OnSlaveEnable() {
    try {
        if (service_ == nullptr || !connected_) {
            return;
        }
        SaveVisibleToState();
        const int port = visible_port_;
        const auto& pins = CurrentPinPair();

        if (pin_usage_ != nullptr &&
            pin_usage_->AnyActiveExcept(SharedOwnerIds(), {OwnerId(port, Role::Master), OwnerId(port, Role::Slave)})) {
            throw std::runtime_error("Another I2C/PMBus/SMBus function is already active. Disable it first.");
        }
        if (pin_usage_ != nullptr &&
            pin_usage_->AnyPinOccupied({pins.sda_pin, pins.scl_pin}, {OwnerId(port, Role::Master), OwnerId(port, Role::Slave)})) {
            throw std::runtime_error("The selected I2C pins are already active in another function.");
        }

        service_->I2cSlaveInit(port, pins.sda_pin, pins.scl_pin, CurrentAddress(), CurrentBaud());
        DisableOtherPort(port);
        SetPortRole(port, Role::Slave);
        (void)SlaveSetTxInternal(false, false);
        SetStatusText(mfc_tool::core::board_i2c::PortLabel(port) + L" slave enabled on " + pins.label);
        if (log_) {
            log_(mfc_tool::core::board_i2c::PortLabel(port) + L" slave enabled on " + pins.label);
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Error", MB_ICONERROR | MB_OK);
    }
    RefreshPinUsage();
    UpdateEnableState();
}

void CI2cTab::OnDisablePort() {
    try {
        if (service_ != nullptr && connected_) {
            service_->I2cDeinit(visible_port_);
        }
        SetPortRole(visible_port_, Role::None);
        SetStatusText(mfc_tool::core::board_i2c::PortLabel(visible_port_) + L" disabled");
        if (log_) {
            log_(mfc_tool::core::board_i2c::PortLabel(visible_port_) + L" disabled");
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Error", MB_ICONERROR | MB_OK);
    }
    RefreshPinUsage();
    UpdateEnableState();
}

void CI2cTab::OnMasterWrite() {
    try {
        MasterWriteInternal(true);
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Master Write", MB_ICONERROR | MB_OK);
    }
}

void CI2cTab::OnMasterRead() {
    try {
        MasterReadInternal(true);
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Master Read", MB_ICONERROR | MB_OK);
    }
}

void CI2cTab::OnWriteThenRead() {
    try {
        WriteThenReadInternal(true);
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Write Then Read", MB_ICONERROR | MB_OK);
    }
}

void CI2cTab::OnSlaveSetTx() {
    try {
        (void)SlaveSetTxInternal(true, false);
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Slave TX", MB_ICONERROR | MB_OK);
    }
}

void CI2cTab::OnSlaveGetRx() {
    try {
        SlaveGetRxInternal(true);
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Slave RX", MB_ICONERROR | MB_OK);
    }
}

void CI2cTab::MasterWriteInternal(bool show_message) {
    if (service_ == nullptr || runtime_[visible_port_].role != Role::Master) {
        return;
    }
    std::vector<std::uint8_t> tx = BuildTxPayload(true, true, true);
    std::vector<std::uint8_t> rx = service_->I2cMasterWrite(visible_port_, CurrentAddress(), tx);
    const std::wstring text = L"Master write TX=" + mfc_tool::core::HexDump(tx) + L" RESP=" + ResponseBytesText(rx);
    SetStatusText(text);
    if (show_message && log_) {
        log_(text);
    }
}

void CI2cTab::MasterReadInternal(bool show_message) {
    if (service_ == nullptr || runtime_[visible_port_].role != Role::Master) {
        return;
    }
    const int read_len = std::clamp(ParseEditInt(master_read_edit_), 0, kI2cMaxShortRead);
    std::vector<std::uint8_t> rx = service_->I2cMasterRead(visible_port_, CurrentAddress(), read_len);
    const std::wstring text = L"Master read RX=" + ResponseBytesText(rx);
    master_rx_edit_.SetWindowTextW(text.c_str());
    SetStatusText(text);
    if (show_message && log_) {
        log_(text);
    }
}

void CI2cTab::WriteThenReadInternal(bool show_message) {
    if (service_ == nullptr || runtime_[visible_port_].role != Role::Master) {
        return;
    }
    std::vector<std::uint8_t> tx = BuildTxPayload(true, true, true);
    const int read_len = std::clamp(ParseEditInt(master_read_edit_), 0, kI2cMaxShortRead);
    const bool repeated_start = repeated_start_check_.GetCheck() == BST_CHECKED;
    std::vector<std::uint8_t> rx = service_->I2cMasterWriteThenRead(visible_port_, CurrentAddress(), repeated_start, tx, read_len);
    const std::wstring text = L"Master write-read TX=" + mfc_tool::core::HexDump(tx) +
        L" RX=" + ResponseBytesText(rx);
    master_rx_edit_.SetWindowTextW(text.c_str());
    SetStatusText(text);
    if (show_message && log_) {
        log_(text);
    }
}

bool CI2cTab::SlaveSetTxInternal(bool show_message, bool advance_counter) {
    if (service_ == nullptr || runtime_[visible_port_].role != Role::Slave) {
        return false;
    }
    std::vector<std::uint8_t> tx = BuildTxPayload(false, true, advance_counter);
    if (static_cast<int>(tx.size()) > kI2cMaxSlaveTxPerReport) {
        throw std::runtime_error("I2C slave TX is limited to 56 bytes per HID report.");
    }
    std::vector<std::uint8_t> rx = service_->I2cSlaveSetTx(visible_port_, tx);
    const std::wstring text = L"Slave TX updated TX=" + mfc_tool::core::HexDump(tx) +
        L" RESP=" + ResponseBytesText(rx);
    SetStatusText(text);
    if (show_message && log_) {
        log_(text);
    }
    return true;
}

void CI2cTab::SlaveGetRxInternal(bool show_message) {
    if (service_ == nullptr || runtime_[visible_port_].role != Role::Slave) {
        return;
    }
    const int max_len = std::clamp(ParseEditInt(slave_rx_max_edit_), 0, kI2cMaxShortRead);
    std::vector<std::uint8_t> rx = service_->I2cSlaveGetRx(visible_port_, max_len);
    const std::wstring text = L"Slave RX=" + ResponseBytesText(rx);
    slave_rx_edit_.SetWindowTextW(text.c_str());
    SetStatusText(text);
    if (show_message && log_) {
        log_(text);
    }
}

void CI2cTab::OnMonitorStart() {
    try {
        if (runtime_[visible_port_].role == Role::None) {
            return;
        }
        SaveVisibleToState();
        const int interval = std::clamp(ParseEditInt(monitor_edit_), 20, 60000);
        runtime_[visible_port_].monitoring = true;
        SetTimer(kMonitorTimer, static_cast<UINT>(interval), nullptr);
        SetStatusText(L"I2C monitor started");
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Monitor", MB_ICONERROR | MB_OK);
    }
    UpdateEnableState();
}

void CI2cTab::OnMonitorStop() {
    KillTimer(kMonitorTimer);
    runtime_[visible_port_].monitoring = false;
    SetStatusText(L"I2C monitor stopped");
    UpdateEnableState();
}

void CI2cTab::OnIntervalStart() {
    try {
        if (runtime_[visible_port_].role == Role::None) {
            return;
        }
        SaveVisibleToState();
        const int units = std::clamp(ParseEditInt(interval_edit_), 1, 600);
        runtime_[visible_port_].interval_sending = true;
        runtime_[visible_port_].interval_error_streak = 0;
        SetTimer(kIntervalTimer, static_cast<UINT>(units * 100), nullptr);
        SetStatusText(L"I2C interval send started");
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Interval", MB_ICONERROR | MB_OK);
    }
    UpdateEnableState();
}

void CI2cTab::OnIntervalStop() {
    KillTimer(kIntervalTimer);
    runtime_[visible_port_].interval_sending = false;
    runtime_[visible_port_].interval_error_streak = 0;
    SetStatusText(L"I2C interval send stopped");
    UpdateEnableState();
}

void CI2cTab::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kMonitorTimer) {
        MonitorPoll();
        return;
    }
    if (nIDEvent == kIntervalTimer) {
        IntervalTick();
        return;
    }
    CWnd::OnTimer(nIDEvent);
}

void CI2cTab::MonitorPoll() {
    try {
        if (runtime_[visible_port_].role == Role::Master) {
            MasterReadInternal(false);
        } else if (runtime_[visible_port_].role == Role::Slave) {
            SlaveGetRxInternal(false);
        }
    } catch (const std::exception& e) {
        OnMonitorStop();
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Monitor", MB_ICONERROR | MB_OK);
    }
}

void CI2cTab::IntervalTick() {
    try {
        if (runtime_[visible_port_].role == Role::Master) {
            MasterWriteInternal(false);
        } else if (runtime_[visible_port_].role == Role::Slave) {
            (void)SlaveSetTxInternal(false, true);
        }
        runtime_[visible_port_].interval_error_streak = 0;
    } catch (const std::exception& e) {
        ++runtime_[visible_port_].interval_error_streak;
        AppendStatusText(L"Interval error: " + AnsiToWide(e.what()));
        if (runtime_[visible_port_].interval_error_streak >= kI2cMaxIntervalErrorStreak) {
            OnIntervalStop();
            ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Interval", MB_ICONERROR | MB_OK);
        }
    }
}

void CI2cTab::OnGenerateData() {
    try {
        const int len = std::clamp(ParseEditInt(gen_len_edit_), 0, kI2cMaxSlaveTxPerReport);
        const int start = ParseEditInt(gen_start_edit_);
        int step = ParseEditInt(gen_step_edit_);
        std::vector<std::uint8_t> data;
        std::wstringstream ss;

        if (step == 0) {
            step = 1;
        }
        data.reserve(static_cast<size_t>(len));
        for (int i = 0; i < len; ++i) {
            data.push_back(static_cast<std::uint8_t>((start + i * step) & 0xFF));
        }

        ss << mfc_tool::core::HexDump(data);
        if (target_combo_.GetCurSel() == 1) {
            slave_tx_edit_.SetWindowTextW(ss.str().c_str());
        } else {
            master_tx_edit_.SetWindowTextW(ss.str().c_str());
        }
        UpdateTxHexLabels();
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"I2C Generate", MB_ICONERROR | MB_OK);
    }
}

void CI2cTab::OnCounterChanged() {
    runtime_[visible_port_].counter_seeded = false;
    UpdateEnableState();
}

void CI2cTab::OnCounterReset() {
    runtime_[visible_port_].counter_seeded = false;
    runtime_[visible_port_].counter_value = 0;
    SetStatusText(L"I2C counter reset");
}

void CI2cTab::OnMasterTxChanged() {
    if (!loading_) {
        UpdateTxHexLabels();
    }
}

void CI2cTab::OnSlaveTxChanged() {
    if (!loading_) {
        UpdateTxHexLabels();
    }
}

void CI2cTab::UpdateTxHexLabels() {
    try {
        const auto mtx = ParseTxHex(master_tx_edit_);
        master_tx_label_.SetWindowTextW((L"TX HEX (" + std::to_wstring(mtx.size()) + L")").c_str());
    } catch (...) {
        master_tx_label_.SetWindowTextW(L"TX HEX");
    }
    try {
        const auto stx = ParseTxHex(slave_tx_edit_);
        slave_tx_label_.SetWindowTextW((L"TX HEX (" + std::to_wstring(stx.size()) + L")").c_str());
    } catch (...) {
        slave_tx_label_.SetWindowTextW(L"TX HEX");
    }
}

void CI2cTab::SetStatusText(const std::wstring& text) {
    if (::IsWindow(status_edit_.GetSafeHwnd())) {
        status_edit_.SetWindowTextW(text.c_str());
    }
}

void CI2cTab::AppendStatusText(const std::wstring& text) {
    if (!::IsWindow(status_edit_.GetSafeHwnd())) {
        return;
    }
    int len = status_edit_.GetWindowTextLengthW();
    status_edit_.SetSel(len, len);
    status_edit_.ReplaceSel((text + L"\r\n").c_str());
}

std::vector<std::uint8_t> CI2cTab::BuildTxPayload(bool for_master, bool update_ui, bool advance_counter) {
    CEdit& edit = for_master ? master_tx_edit_ : slave_tx_edit_;
    std::vector<std::uint8_t> data = ParseTxHex(edit);

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
            if (!runtime_[visible_port_].counter_seeded) {
                runtime_[visible_port_].counter_value = data[static_cast<size_t>(idx)];
                runtime_[visible_port_].counter_seeded = true;
            }
            data[static_cast<size_t>(idx)] = static_cast<std::uint8_t>(runtime_[visible_port_].counter_value & 0xFFu);
            if (advance_counter) {
                runtime_[visible_port_].counter_value =
                    static_cast<unsigned int>((runtime_[visible_port_].counter_value + static_cast<unsigned int>(step)) & 0xFFu);
            }
        }
    }

    if (update_ui) {
        edit.SetWindowTextW(mfc_tool::core::HexDump(data).c_str());
        UpdateTxHexLabels();
    }
    return data;
}

std::vector<std::uint8_t> CI2cTab::ParseTxHex(const CEdit& edit) const {
    return mfc_tool::core::ParseHexBytes(GetEditText(edit));
}

int CI2cTab::CurrentPort() const {
    const int sel = port_combo_.GetCurSel();
    if (sel != CB_ERR) {
        return static_cast<int>(port_combo_.GetItemData(sel));
    }
    return visible_port_;
}

int CI2cTab::ParseEditInt(const CEdit& edit) const {
    return mfc_tool::core::ParseInt(GetEditText(edit));
}

int CI2cTab::CurrentAddress() const {
    return ParseEditInt(addr_edit_) & 0x7F;
}

int CI2cTab::CurrentBaud() const {
    return std::clamp(ParseEditInt(baud_edit_), 10000, 1000000);
}

std::wstring CI2cTab::GetEditText(const CEdit& edit) const {
    CString s;
    const_cast<CEdit&>(edit).GetWindowTextW(s);
    return s.GetString();
}

const mfc_tool::core::board_i2c::PinPair& CI2cTab::CurrentPinPair() const {
    const auto& pairs = mfc_tool::core::board_i2c::AllPinPairs();
    const int sel = pins_combo_.GetCurSel();
    if (sel != CB_ERR) {
        const auto idx = static_cast<size_t>(pins_combo_.GetItemData(sel));
        if (idx < pairs.size()) {
            return pairs[idx];
        }
    }
    const auto* fallback = mfc_tool::core::board_i2c::DefaultPinPair(visible_port_);
    return fallback != nullptr ? *fallback : pairs.front();
}

const mfc_tool::core::board_i2c::PinPair& CI2cTab::PinPairForPort(int port) const {
    const auto& pairs = mfc_tool::core::board_i2c::AllPinPairs();
    const auto* saved = mfc_tool::core::board_i2c::FindPinPairByIniName(port_state_[port].pin_pair);
    if (saved != nullptr && saved->i2c_port == port) {
        return *saved;
    }
    const auto* fallback = mfc_tool::core::board_i2c::DefaultPinPair(port);
    return fallback != nullptr ? *fallback : pairs.front();
}

std::wstring CI2cTab::OwnerId(int port, Role role) {
    const wchar_t* suffix = role == Role::Slave ? L"-S" : L"-M";
    return mfc_tool::core::board_i2c::PortLabel(port) + suffix;
}

std::vector<std::wstring> CI2cTab::SharedOwnerIds() {
    return {
        L"PMBUS-M",
        L"CRPS-M",
        L"TI-UCD-M",
        L"SMBUS-M",
        L"I2C0-M",
        L"I2C0-S",
        L"I2C1-M",
        L"I2C1-S",
        L"FW-UPLOAD-M",
    };
}

std::wstring CI2cTab::AnsiToWide(const char* text) {
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
