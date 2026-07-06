#include "script_tab.h"

#include <afxdlgs.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../core/pmbus_utils.h"
#include "../core/text_utils.h"
#include "layout_utils.h"

namespace {

enum : UINT {
    IDC_SCRIPT_PATH = 19100,
    IDC_SCRIPT_LOAD,
    IDC_SCRIPT_SAVE,
    IDC_SCRIPT_SAVE_AS,
    IDC_SCRIPT_LIST,
    IDC_SCRIPT_SELECT_ALL,
    IDC_SCRIPT_PROFILE,
    IDC_SCRIPT_TYPE,
    IDC_SCRIPT_ADDR,
    IDC_SCRIPT_REG,
    IDC_SCRIPT_DATA,
    IDC_SCRIPT_READ_LEN,
    IDC_SCRIPT_DELAY,
    IDC_SCRIPT_PEC,
    IDC_SCRIPT_CALC_PEC,
    IDC_SCRIPT_PEC_TEXT,
    IDC_SCRIPT_ADD,
    IDC_SCRIPT_UPDATE,
    IDC_SCRIPT_DELETE,
    IDC_SCRIPT_UP,
    IDC_SCRIPT_DOWN
};

std::string WideToAnsiLossy(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back((ch >= 0 && ch <= 0x7F) ? static_cast<char>(ch) : '?');
    }
    return out;
}

bool IsPmbusScriptProfile(mfc_tool::core::SmbusScriptProfile profile) {
    return profile == mfc_tool::core::SmbusScriptProfile::PmbusBase ||
           profile == mfc_tool::core::SmbusScriptProfile::PmbusCrps ||
           profile == mfc_tool::core::SmbusScriptProfile::PmbusTiUcd90xxx;
}

std::vector<std::wstring> TypeNamesForProfile(mfc_tool::core::SmbusScriptProfile profile) {
    std::vector<std::wstring> names = {L"Comment", L"Pause"};
    if (profile == mfc_tool::core::SmbusScriptProfile::SmbusUbm) {
        const std::vector<std::wstring> tail = {
            L"ReadByte",
            L"ReadWord",
            L"Read32",
            L"BlockRead",
            L"WriteByte",
            L"WriteWord",
            L"BlockWrite",
            L"BadChecksumWrite"
        };
        names.insert(names.end(), tail.begin(), tail.end());
        return names;
    }
    if (IsPmbusScriptProfile(profile)) {
        const std::vector<std::wstring> tail = {L"SendByte", L"WriteByte", L"WriteWord", L"ReadByte", L"ReadWord", L"Read32",
                                                L"BlockWrite", L"BlockRead", L"ProcessCall", L"BlockWriteReadProcessCall",
                                                L"BadPecWriteByte", L"BusRecover"};
        names.insert(names.end(), tail.begin(), tail.end());
        return names;
    }
    const std::vector<std::wstring> tail = {
        L"QuickWrite",
        L"QuickRead",
        L"SendByte",
        L"ReceiveByte",
        L"WriteByte",
        L"WriteWord",
        L"ReadByte",
        L"ReadWord",
        L"Read32",
        L"BlockWrite",
        L"BlockRead",
        L"ProcessCall",
        L"BlockWriteReadProcessCall",
        L"BadPecWriteByte",
        L"BusRecover"
    };
    names.insert(names.end(), tail.begin(), tail.end());
    return names;
}

int ParseDelayMsForScript(const std::wstring& text, bool required) {
    if (text.empty()) {
        if (required) {
            throw std::invalid_argument("Pause requires delay 1..10000 ms.");
        }
        return 0;
    }
    const int value = mfc_tool::core::ParseInt(text);
    if (value < 1 || value > 10000) {
        throw std::invalid_argument("Delay must be 1..10000 ms.");
    }
    return value;
}

} // namespace

BEGIN_MESSAGE_MAP(CScriptTab, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_SCRIPT_LOAD, &CScriptTab::OnLoad)
    ON_BN_CLICKED(IDC_SCRIPT_SAVE, &CScriptTab::OnSave)
    ON_BN_CLICKED(IDC_SCRIPT_SAVE_AS, &CScriptTab::OnSaveAs)
    ON_CBN_SELCHANGE(IDC_SCRIPT_PROFILE, &CScriptTab::OnProfileChanged)
    ON_CBN_SELCHANGE(IDC_SCRIPT_TYPE, &CScriptTab::OnTypeChanged)
    ON_BN_CLICKED(IDC_SCRIPT_CALC_PEC, &CScriptTab::OnCalcPec)
    ON_BN_CLICKED(IDC_SCRIPT_ADD, &CScriptTab::OnAdd)
    ON_BN_CLICKED(IDC_SCRIPT_UPDATE, &CScriptTab::OnUpdateRow)
    ON_BN_CLICKED(IDC_SCRIPT_DELETE, &CScriptTab::OnDeleteRow)
    ON_BN_CLICKED(IDC_SCRIPT_UP, &CScriptTab::OnMoveUp)
    ON_BN_CLICKED(IDC_SCRIPT_DOWN, &CScriptTab::OnMoveDown)
    ON_BN_CLICKED(IDC_SCRIPT_SELECT_ALL, &CScriptTab::OnSelectAll)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_SCRIPT_LIST, &CScriptTab::OnListChanged)
END_MESSAGE_MAP()

BOOL CScriptTab::Create(CWnd* parent, const RECT& rect, UINT id) {
    return CWnd::CreateEx(0, AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                                                 reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr),
                          L"ScriptTab", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          rect, parent, id);
}

void CScriptTab::Bind(std::function<void(const std::wstring&)> logger,
                      std::function<void()> persist_settings,
                      std::function<void(bool)> dirty_changed) {
    log_ = std::move(logger);
    persist_settings_ = std::move(persist_settings);
    dirty_changed_ = std::move(dirty_changed);
    UpdateDirtyUi();
}

int CScriptTab::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    ui_font_.CreatePointFont(90, L"Segoe UI");
    path_label_.Create(L"Path", WS_CHILD | WS_VISIBLE, CRect(), this);
    path_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(), this, IDC_SCRIPT_PATH);
    dirty_label_.Create(L"Unsaved changes", WS_CHILD, CRect(), this);
    load_btn_.Create(L"Load", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_SCRIPT_LOAD);
    save_btn_.Create(L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_SCRIPT_SAVE);
    save_as_btn_.Create(L"Save As", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_SCRIPT_SAVE_AS);
    select_all_btn_.Create(L"Select All", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(), this, IDC_SCRIPT_SELECT_ALL);

    list_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL, CRect(), this, IDC_SCRIPT_LIST);
    list_.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
    list_.InsertColumn(0, L"#", LVCFMT_RIGHT, 46);
    list_.InsertColumn(1, L"Profile", LVCFMT_LEFT, 136);
    list_.InsertColumn(2, L"Type", LVCFMT_LEFT, 132);
    list_.InsertColumn(3, L"Addr", LVCFMT_LEFT, 66);
    list_.InsertColumn(4, L"Reg", LVCFMT_LEFT, 66);
    list_.InsertColumn(5, L"Data", LVCFMT_LEFT, 160);
    list_.InsertColumn(6, L"Read", LVCFMT_RIGHT, 50);
    list_.InsertColumn(7, L"Delay", LVCFMT_RIGHT, 56);
    list_.InsertColumn(8, L"PEC", LVCFMT_LEFT, 42);
    list_.InsertColumn(9, L"Summary", LVCFMT_LEFT, 360);

    profile_label_.Create(L"Profile", WS_CHILD | WS_VISIBLE, CRect(), this);
    profile_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_SCRIPT_PROFILE);
    type_label_.Create(L"Type", WS_CHILD | WS_VISIBLE, CRect(), this);
    type_combo_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, CRect(), this, IDC_SCRIPT_TYPE);
    addr_label_.Create(L"Addr", WS_CHILD | WS_VISIBLE, CRect(), this);
    addr_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_SCRIPT_ADDR);
    reg_label_.Create(L"Reg", WS_CHILD | WS_VISIBLE, CRect(), this);
    reg_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_SCRIPT_REG);
    data_label_.Create(L"Data", WS_CHILD | WS_VISIBLE, CRect(), this);
    data_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_SCRIPT_DATA);
    read_len_label_.Create(L"Read", WS_CHILD | WS_VISIBLE, CRect(), this);
    read_len_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_SCRIPT_READ_LEN);
    delay_label_.Create(L"Delay", WS_CHILD | WS_VISIBLE, CRect(), this);
    delay_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(), this, IDC_SCRIPT_DELAY);
    pec_check_.Create(L"PEC", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(), this, IDC_SCRIPT_PEC);
    calc_pec_btn_.Create(L"Calc PEC", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_SCRIPT_CALC_PEC);
    pec_label_.Create(L"PEC", WS_CHILD | WS_VISIBLE, CRect(), this);
    pec_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(), this, IDC_SCRIPT_PEC_TEXT);
    add_btn_.Create(L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_SCRIPT_ADD);
    update_btn_.Create(L"Update", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_SCRIPT_UPDATE);
    delete_btn_.Create(L"Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_SCRIPT_DELETE);
    up_btn_.Create(L"Up", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_SCRIPT_UP);
    down_btn_.Create(L"Down", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(), this, IDC_SCRIPT_DOWN);

    SetChildFonts();
    PopulateProfileCombo();
    PopulateTypeCombo(mfc_tool::core::SmbusScriptProfile::PmbusCrps);
    select_all_btn_.SetCheck(BST_CHECKED);
    {
        auto row = mfc_tool::core::MakeDefaultSmbusScriptRow(mfc_tool::core::SmbusScriptProfile::PmbusCrps);
        profile_combo_.SelectString(-1, mfc_tool::core::SmbusScriptProfileText(row.profile).c_str());
        type_combo_.SelectString(-1, mfc_tool::core::SmbusScriptCommandTypeText(row.command_type).c_str());
        addr_edit_.SetWindowTextW(mfc_tool::core::FormatSmbusScriptHexByte(row.address).c_str());
        reg_edit_.SetWindowTextW(mfc_tool::core::FormatSmbusScriptHexByte(row.command).c_str());
        read_len_edit_.SetWindowTextW(L"1");
        delay_edit_.SetWindowTextW(L"10");
        pec_check_.SetCheck(BST_CHECKED);
    }
    UpdateEditorMode();
    PopulateList();
    UpdateDirtyUi();
    return 0;
}

void CScriptTab::SetChildFonts() {
    CWnd* controls[] = {
        &path_label_, &path_edit_, &dirty_label_, &load_btn_, &save_btn_, &save_as_btn_, &select_all_btn_, &list_,
        &profile_label_, &profile_combo_, &type_label_, &type_combo_, &addr_label_, &addr_edit_,
        &reg_label_, &reg_edit_, &data_label_, &data_edit_, &read_len_label_, &read_len_edit_,
        &delay_label_, &delay_edit_, &pec_check_, &calc_pec_btn_, &pec_label_, &pec_edit_,
        &add_btn_, &update_btn_, &delete_btn_, &up_btn_, &down_btn_
    };
    for (CWnd* control : controls) {
        control->SetFont(&ui_font_);
    }
}

void CScriptTab::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    if (::IsWindow(list_.GetSafeHwnd())) {
        LayoutControls(CRect(0, 0, cx, cy));
    }
}

void CScriptTab::LayoutControls(const CRect& r) {
    const int margin = 8;
    const int gap = 6;
    const int row = 24;
    const int left = r.left + margin;
    const int right = r.right - margin;
    const int top = r.top + margin;
    const int bottom = r.bottom - margin;
    const int bottom_buttons_y = bottom - row;
    const int edit2_y = bottom_buttons_y - row - gap;
    const int edit1_y = edit2_y - row - gap;
    const int list_y = top + row + gap;
    const int list_h = (std::max)(120, edit1_y - gap - list_y);
    const int load_w = 70;
    const int save_w = 70;
    const int save_as_w = 86;
    const int dirty_w = (std::max)(128, mfc_tool::ui::MeasureTextWidth(*this, L"Unsaved changes", 12));
    const int pec_check_w = (std::max)(58, mfc_tool::ui::MeasureButtonMinWidth(pec_check_, 16));
    const int calc_pec_w = 86;
    const int pec_label_w = 28;
    int x = left;

    mfc_tool::ui::SafeMoveWindow(path_label_, x, top + 4, 38, 18);
    x += 38 + gap;
    mfc_tool::ui::SafeMoveWindow(path_edit_, x, top, (std::max)(120, right - x - dirty_w - load_w - save_w - save_as_w - gap * 4), row);
    x += (std::max)(120, right - x - dirty_w - load_w - save_w - save_as_w - gap * 4) + gap;
    mfc_tool::ui::SafeMoveWindow(dirty_label_, x, top + 4, dirty_w, 18);
    x = right - load_w - save_w - save_as_w - gap * 2;
    mfc_tool::ui::SafeMoveWindow(load_btn_, x, top, load_w, row);
    x += load_w + gap;
    mfc_tool::ui::SafeMoveWindow(save_btn_, x, top, save_w, row);
    x += save_w + gap;
    mfc_tool::ui::SafeMoveWindow(save_as_btn_, x, top, save_as_w, row);

    mfc_tool::ui::SafeMoveWindow(list_, left, list_y, right - left, list_h);

    x = left;
    mfc_tool::ui::SafeMoveWindow(profile_label_, x, edit1_y + 4, 48, 18);
    x += 48 + gap;
    mfc_tool::ui::SafeMoveWindow(profile_combo_, x, edit1_y, 170, 300);
    x += 170 + gap;
    mfc_tool::ui::SafeMoveWindow(type_label_, x, edit1_y + 4, 34, 18);
    x += 34 + gap;
    mfc_tool::ui::SafeMoveWindow(type_combo_, x, edit1_y, 214, 300);
    x += 214 + gap;
    mfc_tool::ui::SafeMoveWindow(addr_label_, x, edit1_y + 4, 36, 18);
    x += 36 + gap;
    mfc_tool::ui::SafeMoveWindow(addr_edit_, x, edit1_y, 72, row);
    x += 72 + gap;
    mfc_tool::ui::SafeMoveWindow(reg_label_, x, edit1_y + 4, 28, 18);
    x += 28 + gap;
    mfc_tool::ui::SafeMoveWindow(reg_edit_, x, edit1_y, 72, row);
    x += 72 + gap;
    mfc_tool::ui::SafeMoveWindow(read_len_label_, x, edit1_y + 4, 36, 18);
    x += 36 + gap;
    mfc_tool::ui::SafeMoveWindow(read_len_edit_, x, edit1_y, 54, row);
    x += 54 + gap;
    {
        const int delay_label_w = (std::max)(42, mfc_tool::ui::MeasureControlTextWidth(delay_label_, 8));
        mfc_tool::ui::SafeMoveWindow(delay_label_, x, edit1_y + 4, delay_label_w, 18);
        x += delay_label_w + gap;
    }
    mfc_tool::ui::SafeMoveWindow(delay_edit_, x, edit1_y, 58, row);

    x = left;
    {
        const int data_label_w = (std::max)(64, mfc_tool::ui::MeasureControlTextWidth(data_label_, 8));
        mfc_tool::ui::SafeMoveWindow(data_label_, x, edit2_y + 4, data_label_w, 18);
        x += data_label_w + gap;
    }
    {
        const int min_pec_text_w = 100;
        const int tools_w = pec_check_w + gap + calc_pec_w + gap + pec_label_w + gap + min_pec_text_w;
        const int available_w = (std::max)(0, right - x);
        int data_w = available_w - tools_w - gap;
        if (data_w < 140) {
            data_w = (std::max)(80, available_w / 2);
        }
        mfc_tool::ui::SafeMoveWindow(data_edit_, x, edit2_y, data_w, row);
        x += data_w + gap;
    }
    mfc_tool::ui::SafeMoveWindow(pec_check_, x, edit2_y + 2, pec_check_w, row);
    x += pec_check_w + gap;
    mfc_tool::ui::SafeMoveWindow(calc_pec_btn_, x, edit2_y, calc_pec_w, row);
    x += calc_pec_w + gap;
    mfc_tool::ui::SafeMoveWindow(pec_label_, x, edit2_y + 4, pec_label_w, 18);
    x += pec_label_w + gap;
    mfc_tool::ui::SafeMoveWindow(pec_edit_, x, edit2_y, (std::max)(60, right - x), row);

    x = left;
    mfc_tool::ui::SafeMoveWindow(select_all_btn_, x, bottom_buttons_y + 2, 96, row);
    x += 96 + gap;
    mfc_tool::ui::SafeMoveWindow(add_btn_, x, bottom_buttons_y, 66, row);
    x += 66 + gap;
    mfc_tool::ui::SafeMoveWindow(update_btn_, x, bottom_buttons_y, 76, row);
    x += 76 + gap;
    mfc_tool::ui::SafeMoveWindow(delete_btn_, x, bottom_buttons_y, 76, row);
    x += 76 + gap;
    mfc_tool::ui::SafeMoveWindow(up_btn_, x, bottom_buttons_y, 58, row);
    x += 58 + gap;
    mfc_tool::ui::SafeMoveWindow(down_btn_, x, bottom_buttons_y, 70, row);
}

void CScriptTab::PopulateProfileCombo() {
    profile_combo_.ResetContent();
    for (const auto& name : mfc_tool::core::SmbusScriptProfileNames()) {
        profile_combo_.AddString(name.c_str());
    }
    profile_combo_.SelectString(-1, L"PMBus-CRPS");
}

void CScriptTab::PopulateTypeCombo(mfc_tool::core::SmbusScriptProfile profile) {
    type_combo_.ResetContent();
    for (const auto& name : TypeNamesForProfile(profile)) {
        type_combo_.AddString(name.c_str());
    }
    type_combo_.SelectString(-1, L"ReadByte");
}

void CScriptTab::LoadState(const mfc_tool::core::AppState& state) {
    document_.path = state.script_path;
    document_.rows.clear();
    SetPathText();
    PopulateList();
    SetDirty(false);
}

void CScriptTab::SaveState(mfc_tool::core::AppState* state) const {
    if (state != nullptr) {
        state->script_path = document_.path;
    }
}

void CScriptTab::SetPathText() {
    if (::IsWindow(path_edit_.GetSafeHwnd())) {
        path_edit_.SetWindowTextW(document_.path.c_str());
    }
}

void CScriptTab::PopulateList(int select_index) {
    populating_ = true;
    list_.SetRedraw(FALSE);
    list_.DeleteAllItems();
    for (size_t i = 0u; i < document_.rows.size(); ++i) {
        const int item = list_.InsertItem(static_cast<int>(i), std::to_wstring(i + 1u).c_str());
        SetListRow(item, i);
    }
    mfc_tool::ui::AutoSizeListColumns(list_, 10, {74, 136, 160, 66, 66, 160, 50, 78, 42, 360});
    list_.SetRedraw(TRUE);
    list_.Invalidate(FALSE);
    populating_ = false;
    if (select_index >= 0 && select_index < list_.GetItemCount()) {
        SelectListItem(select_index);
    } else if (list_.GetItemCount() == 0) {
        ClearEditors();
    } else {
        LoadSelectedRowToEditors();
    }
}

void CScriptTab::SetListRow(int item, size_t row_index) {
    if (item < 0 || row_index >= document_.rows.size()) {
        return;
    }
    const auto& row = document_.rows[row_index];
    list_.SetItemText(item, 0, std::to_wstring(row_index + 1u).c_str());
    list_.SetItemData(item, static_cast<DWORD_PTR>(row_index));
    list_.SetCheck(item, row.selected ? TRUE : FALSE);
    list_.SetItemText(item, 1, mfc_tool::core::SmbusScriptProfileText(row.profile).c_str());
    list_.SetItemText(item, 2, mfc_tool::core::SmbusScriptRowTypeText(row).c_str());
    list_.SetItemText(item, 3, row.address >= 0 ? mfc_tool::core::FormatSmbusScriptHexByte(row.address).c_str() : L"");
    list_.SetItemText(item, 4, row.command >= 0 ? mfc_tool::core::FormatSmbusScriptHexByte(row.command).c_str() : L"");
    list_.SetItemText(item, 5, mfc_tool::core::FormatSmbusScriptData(row.data).c_str());
    list_.SetItemText(item, 6, row.read_length > 0 ? std::to_wstring(row.read_length).c_str() : L"");
    list_.SetItemText(item, 7, row.delay_ms > 0 ? std::to_wstring(row.delay_ms).c_str() : L"");
    list_.SetItemText(item, 8, row.pec ? L"Y" : L"");
    list_.SetItemText(item, 9, mfc_tool::core::SmbusScriptRowSummary(row).c_str());
}

void CScriptTab::SelectListItem(int item) {
    const int count = list_.GetItemCount();
    if (item < 0 || item >= count) {
        ClearEditors();
        return;
    }
    const bool previous_populating = populating_;
    populating_ = true;
    list_.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    list_.SetItemState(item, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    list_.EnsureVisible(item, FALSE);
    populating_ = previous_populating;
    LoadSelectedRowToEditors();
}

void CScriptTab::ClearEditors() {
    addr_edit_.SetWindowTextW(L"");
    reg_edit_.SetWindowTextW(L"");
    data_edit_.SetWindowTextW(L"");
    read_len_edit_.SetWindowTextW(L"");
    delay_edit_.SetWindowTextW(L"");
    pec_check_.SetCheck(BST_UNCHECKED);
    pec_edit_.SetWindowTextW(L"");
    UpdateEditorMode();
}

void CScriptTab::UpdateEditorMode() {
    CString text;
    type_combo_.GetWindowTextW(text);
    const std::wstring type_text = text.GetString();
    const BOOL is_comment = type_text == L"Comment" ? TRUE : FALSE;
    const BOOL is_pause = type_text == L"Pause" ? TRUE : FALSE;
    const BOOL is_command = (!is_comment && !is_pause) ? TRUE : FALSE;
    const BOOL has_data_payload =
        (type_text == L"WriteByte" ||
         type_text == L"WriteWord" ||
         type_text == L"BlockWrite" ||
         type_text == L"ProcessCall" ||
         type_text == L"BlockWriteReadProcessCall" ||
         type_text == L"BadPecWriteByte" ||
         type_text == L"BadChecksumWrite") ? TRUE : FALSE;

    data_label_.SetWindowTextW(is_comment ? L"Comment" : has_data_payload ? L"Data Bytes" : L"Data");
    delay_label_.SetWindowTextW(is_command ? L"Post Delay" : L"Delay");
    mfc_tool::ui::SafeEnableWindow(addr_edit_, is_command);
    mfc_tool::ui::SafeEnableWindow(reg_edit_, is_command);
    mfc_tool::ui::SafeEnableWindow(data_edit_, is_comment || is_command);
    mfc_tool::ui::SafeEnableWindow(read_len_edit_, is_command);
    mfc_tool::ui::SafeEnableWindow(delay_edit_, is_pause || is_command);
    mfc_tool::ui::SafeEnableWindow(pec_check_, is_command);
    mfc_tool::ui::SafeEnableWindow(calc_pec_btn_, is_command || is_pause);
    if (::IsWindow(list_.GetSafeHwnd())) {
        CRect rc;
        GetClientRect(&rc);
        LayoutControls(rc);
    }
}

int CScriptTab::SelectedRowIndex() const {
    POSITION pos = list_.GetFirstSelectedItemPosition();
    if (pos == nullptr) {
        return -1;
    }
    const int item = const_cast<CListCtrl&>(list_).GetNextSelectedItem(pos);
    return item >= 0 ? static_cast<int>(list_.GetItemData(item)) : -1;
}

void CScriptTab::LoadSelectedRowToEditors() {
    const int index = SelectedRowIndex();
    if (index < 0 || index >= static_cast<int>(document_.rows.size())) {
        return;
    }
    const auto& row = document_.rows[static_cast<size_t>(index)];
    profile_combo_.SelectString(-1, mfc_tool::core::SmbusScriptProfileText(row.profile).c_str());
    PopulateTypeCombo(row.profile);
    type_combo_.SelectString(-1, row.kind == mfc_tool::core::SmbusScriptRowKind::Comment ? L"Comment" :
                             row.kind == mfc_tool::core::SmbusScriptRowKind::Pause ? L"Pause" :
                             mfc_tool::core::SmbusScriptCommandTypeText(row.command_type).c_str());
    addr_edit_.SetWindowTextW(row.address >= 0 ? mfc_tool::core::FormatSmbusScriptHexByte(row.address).c_str() : L"");
    reg_edit_.SetWindowTextW(row.command >= 0 ? mfc_tool::core::FormatSmbusScriptHexByte(row.command).c_str() : L"");
    data_edit_.SetWindowTextW(row.kind == mfc_tool::core::SmbusScriptRowKind::Comment && row.fields.size() > 1u
                                  ? row.fields[1].c_str()
                                  : mfc_tool::core::FormatSmbusScriptData(row.data).c_str());
    read_len_edit_.SetWindowTextW(row.read_length > 0 ? std::to_wstring(row.read_length).c_str() : L"");
    delay_edit_.SetWindowTextW(row.delay_ms > 0 ? std::to_wstring(row.delay_ms).c_str() : L"");
    pec_check_.SetCheck(row.pec ? BST_CHECKED : BST_UNCHECKED);
    pec_edit_.SetWindowTextW(L"");
    UpdateEditorMode();
}

std::wstring CScriptTab::GetEditText(const CEdit& edit) const {
    CString text;
    const_cast<CEdit&>(edit).GetWindowTextW(text);
    return text.GetString();
}

mfc_tool::core::SmbusScriptRow CScriptTab::BuildRowFromEditors() const {
    CString text;
    mfc_tool::core::SmbusScriptProfile profile = mfc_tool::core::SmbusScriptProfile::PmbusCrps;
    mfc_tool::core::SmbusScriptCommandType type = mfc_tool::core::SmbusScriptCommandType::ReadByte;
    mfc_tool::core::SmbusScriptRow row;

    const_cast<CComboBox&>(profile_combo_).GetWindowTextW(text);
    mfc_tool::core::TryParseSmbusScriptProfile(text.GetString(), &profile);
    const_cast<CComboBox&>(type_combo_).GetWindowTextW(text);
    const std::wstring type_text = text.GetString();
    if (type_text == L"Comment") {
        row.kind = mfc_tool::core::SmbusScriptRowKind::Comment;
        row.profile = profile;
        row.selected = true;
        row.fields = {L"Comment", GetEditText(data_edit_)};
        return row;
    }
    if (type_text == L"Pause") {
        row.kind = mfc_tool::core::SmbusScriptRowKind::Pause;
        row.profile = profile;
        row.selected = true;
        row.delay_ms = ParseDelayMsForScript(GetEditText(delay_edit_), true);
        return row;
    }
    mfc_tool::core::TryParseSmbusScriptCommandType(type_text, &type);

    row.kind = mfc_tool::core::SmbusScriptRowKind::Command;
    row.profile = profile;
    row.command_type = type;
    row.selected = true;
    row.pec = pec_check_.GetCheck() == BST_CHECKED;
    row.address = mfc_tool::core::ParseInt(GetEditText(addr_edit_)) & 0x7F;
    {
        const std::wstring reg = GetEditText(reg_edit_);
        if (!reg.empty()) {
            row.command = mfc_tool::core::ParseInt(reg) & 0xFF;
        }
    }
    row.data = mfc_tool::core::ParseHexBytes(GetEditText(data_edit_));
    {
        const std::wstring read_len = GetEditText(read_len_edit_);
        row.read_length = read_len.empty() ? 0 : (std::max)(0, mfc_tool::core::ParseInt(read_len));
    }
    {
        const std::wstring delay = GetEditText(delay_edit_);
        row.delay_ms = ParseDelayMsForScript(delay, false);
    }
    return row;
}

void CScriptTab::LoadScriptFromPath(const std::wstring& path) {
    mfc_tool::core::SmbusScriptDocument loaded;
    std::wstring error;
    if (!mfc_tool::core::LoadSmbusScriptCsv(path, &loaded, &error)) {
        throw std::runtime_error(WideToAnsiLossy(error));
    }
    document_ = std::move(loaded);
    SetPathText();
    PopulateList();
    SetDirty(false);
    NotifyChanged();
}

void CScriptTab::SaveScriptToPath(const std::wstring& path) {
    std::wstring error;
    if (!mfc_tool::core::SaveSmbusScriptCsv(document_, path, &error)) {
        throw std::runtime_error(WideToAnsiLossy(error));
    }
    document_.path = path;
    SetPathText();
    SetDirty(false);
    NotifyChanged();
}

void CScriptTab::OnLoad() {
    if (!ConfirmDiscardUnsavedChanges(L"load another script")) {
        return;
    }
    CFileDialog dlg(TRUE, L"csv", nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    L"CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) {
        return;
    }
    try {
        LoadScriptFromPath(dlg.GetPathName().GetString());
        if (log_) {
            log_(L"Script tab loaded " + document_.path);
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Script", MB_ICONERROR | MB_OK);
    }
}

void CScriptTab::OnSave() {
    if (document_.path.empty()) {
        OnSaveAs();
        return;
    }
    try {
        SaveScriptToPath(document_.path);
        if (log_) {
            log_(L"Script tab saved " + document_.path);
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Script", MB_ICONERROR | MB_OK);
    }
}

void CScriptTab::OnSaveAs() {
    CFileDialog dlg(FALSE, L"csv", L"m032_script.csv", OFN_OVERWRITEPROMPT,
                    L"CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) {
        return;
    }
    try {
        SaveScriptToPath(dlg.GetPathName().GetString());
        if (log_) {
            log_(L"Script tab saved " + document_.path);
        }
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Script", MB_ICONERROR | MB_OK);
    }
}

void CScriptTab::OnProfileChanged() {
    CString text;
    mfc_tool::core::SmbusScriptProfile profile = mfc_tool::core::SmbusScriptProfile::PmbusCrps;
    profile_combo_.GetWindowTextW(text);
    if (mfc_tool::core::TryParseSmbusScriptProfile(text.GetString(), &profile)) {
        const auto row = mfc_tool::core::MakeDefaultSmbusScriptRow(profile);
        PopulateTypeCombo(profile);
        type_combo_.SelectString(-1, mfc_tool::core::SmbusScriptCommandTypeText(row.command_type).c_str());
        addr_edit_.SetWindowTextW(mfc_tool::core::FormatSmbusScriptHexByte(row.address).c_str());
        reg_edit_.SetWindowTextW(mfc_tool::core::FormatSmbusScriptHexByte(row.command).c_str());
        read_len_edit_.SetWindowTextW(row.read_length > 0 ? std::to_wstring(row.read_length).c_str() : L"");
        pec_check_.SetCheck(row.pec ? BST_CHECKED : BST_UNCHECKED);
        UpdateEditorMode();
    }
}

void CScriptTab::OnTypeChanged() {
    pec_edit_.SetWindowTextW(L"");
    UpdateEditorMode();
}

std::wstring CScriptTab::CalculatePecText(const mfc_tool::core::SmbusScriptRow& row) const {
    std::vector<std::uint8_t> tx_frame;
    std::wstringstream ss;

    if (row.kind == mfc_tool::core::SmbusScriptRowKind::Comment) {
        return L"Comment row";
    }
    if (row.kind == mfc_tool::core::SmbusScriptRowKind::Pause) {
        return L"Pause delay " + std::to_wstring(row.delay_ms) + L" ms";
    }
    if (row.address < 0) {
        return L"No address";
    }
    tx_frame.push_back(static_cast<std::uint8_t>(row.address << 1));
    if (row.command >= 0) {
        tx_frame.push_back(static_cast<std::uint8_t>(row.command & 0xFF));
    }
    if (row.command_type == mfc_tool::core::SmbusScriptCommandType::BlockWrite && row.data.size() <= 255u) {
        tx_frame.push_back(static_cast<std::uint8_t>(row.data.size() & 0xFFu));
    }
    tx_frame.insert(tx_frame.end(), row.data.begin(), row.data.end());
    ss << L"TX PEC=0x" << std::uppercase << std::hex;
    ss.width(2);
    ss.fill(L'0');
    ss << static_cast<unsigned int>(mfc_tool::core::PmbusComputePec(tx_frame));
    if (mfc_tool::core::SmbusScriptRowIsRead(row)) {
        std::vector<std::uint8_t> rx_seed = tx_frame;
        rx_seed.push_back(static_cast<std::uint8_t>((row.address << 1) | 1u));
        ss << L" | RX seed " << mfc_tool::core::HexDump(rx_seed);
    }
    return ss.str();
}

void CScriptTab::OnCalcPec() {
    try {
        pec_edit_.SetWindowTextW(CalculatePecText(BuildRowFromEditors()).c_str());
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Script PEC", MB_ICONERROR | MB_OK);
    }
}

void CScriptTab::OnAdd() {
    try {
        const int selected = SelectedRowIndex();
        const int at = selected >= 0 ? selected + 1 : static_cast<int>(document_.rows.size());
        document_.rows.insert(document_.rows.begin() + at, BuildRowFromEditors());
        populating_ = true;
        list_.SetRedraw(FALSE);
        list_.InsertItem(at, L"");
        for (int i = at; i < list_.GetItemCount(); ++i) {
            SetListRow(i, static_cast<size_t>(i));
        }
        list_.SetRedraw(TRUE);
        list_.Invalidate(FALSE);
        populating_ = false;
        SelectListItem(at);
        MarkDirty();
        RedrawScriptSurface();
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Script", MB_ICONERROR | MB_OK);
    }
}

void CScriptTab::OnUpdateRow() {
    try {
        const int selected = SelectedRowIndex();
        if (selected < 0 || selected >= static_cast<int>(document_.rows.size())) {
            return;
        }
        const auto previous_fields = document_.rows[static_cast<size_t>(selected)].fields;
        auto row = BuildRowFromEditors();
        row.selected = list_.GetCheck(selected) == TRUE;
        if (row.kind != mfc_tool::core::SmbusScriptRowKind::Comment && previous_fields.size() > 10u) {
            row.fields.resize(11u);
            row.fields[10] = previous_fields[10];
        }
        document_.rows[static_cast<size_t>(selected)] = row;
        populating_ = true;
        SetListRow(selected, static_cast<size_t>(selected));
        populating_ = false;
        SelectListItem(selected);
        MarkDirty();
        RedrawScriptSurface();
    } catch (const std::exception& e) {
        ::MessageBoxW(m_hWnd, AnsiToWide(e.what()).c_str(), L"Script", MB_ICONERROR | MB_OK);
    }
}

void CScriptTab::OnDeleteRow() {
    const int selected = SelectedRowIndex();
    if (selected < 0 || selected >= static_cast<int>(document_.rows.size())) {
        return;
    }
    document_.rows.erase(document_.rows.begin() + selected);
    populating_ = true;
    list_.SetRedraw(FALSE);
    list_.DeleteItem(selected);
    for (int i = selected; i < list_.GetItemCount(); ++i) {
        SetListRow(i, static_cast<size_t>(i));
    }
    list_.SetRedraw(TRUE);
    list_.Invalidate(FALSE);
    populating_ = false;
    SelectListItem((std::min)(selected, static_cast<int>(document_.rows.size()) - 1));
    MarkDirty();
    RedrawScriptSurface();
}

void CScriptTab::OnMoveUp() {
    const int selected = SelectedRowIndex();
    if (selected <= 0 || selected >= static_cast<int>(document_.rows.size())) {
        return;
    }
    std::swap(document_.rows[static_cast<size_t>(selected)], document_.rows[static_cast<size_t>(selected - 1)]);
    populating_ = true;
    list_.SetRedraw(FALSE);
    SetListRow(selected - 1, static_cast<size_t>(selected - 1));
    SetListRow(selected, static_cast<size_t>(selected));
    list_.SetRedraw(TRUE);
    list_.Invalidate(FALSE);
    populating_ = false;
    SelectListItem(selected - 1);
    MarkDirty();
    RedrawScriptSurface();
}

void CScriptTab::OnMoveDown() {
    const int selected = SelectedRowIndex();
    if (selected < 0 || selected + 1 >= static_cast<int>(document_.rows.size())) {
        return;
    }
    std::swap(document_.rows[static_cast<size_t>(selected)], document_.rows[static_cast<size_t>(selected + 1)]);
    populating_ = true;
    list_.SetRedraw(FALSE);
    SetListRow(selected, static_cast<size_t>(selected));
    SetListRow(selected + 1, static_cast<size_t>(selected + 1));
    list_.SetRedraw(TRUE);
    list_.Invalidate(FALSE);
    populating_ = false;
    SelectListItem(selected + 1);
    MarkDirty();
    RedrawScriptSurface();
}

void CScriptTab::OnSelectAll() {
    const bool checked = select_all_btn_.GetCheck() == BST_CHECKED;
    all_selected_ = checked;
    populating_ = true;
    list_.SetRedraw(FALSE);
    for (size_t i = 0u; i < document_.rows.size(); ++i) {
        document_.rows[i].selected = checked;
        if (i < static_cast<size_t>(list_.GetItemCount())) {
            list_.SetCheck(static_cast<int>(i), checked ? TRUE : FALSE);
        }
    }
    list_.SetRedraw(TRUE);
    list_.Invalidate(FALSE);
    populating_ = false;
    MarkDirty();
    RedrawScriptSurface();
}

void CScriptTab::OnListChanged(NMHDR* pNMHDR, LRESULT* pResult) {
    if (pResult != nullptr) {
        *pResult = 0;
    }
    if (populating_) {
        return;
    }
    const auto* item = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if (item == nullptr) {
        return;
    }
    if ((item->uChanged & LVIF_STATE) != 0u) {
        const int idx = item->iItem;
        const bool checkbox_changed = ((item->uOldState ^ item->uNewState) & LVIS_STATEIMAGEMASK) != 0u;
        if (checkbox_changed && idx >= 0 && idx < static_cast<int>(document_.rows.size())) {
            document_.rows[static_cast<size_t>(idx)].selected = list_.GetCheck(idx) == TRUE;
            MarkDirty();
        }
        if ((item->uNewState & LVIS_SELECTED) != 0u) {
            LoadSelectedRowToEditors();
        }
    }
}

bool CScriptTab::HasUnsavedChanges() const {
    return dirty_;
}

bool CScriptTab::ConfirmDiscardUnsavedChanges(const std::wstring& action_text) const {
    if (!dirty_) {
        return true;
    }
    const std::wstring message = L"Script has unsaved changes.\r\nContinue to " + action_text + L" and discard them?";
    return ::MessageBoxW(m_hWnd, message.c_str(), L"Unsaved Script", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES;
}

void CScriptTab::NotifyChanged() {
    if (persist_settings_) {
        persist_settings_();
    }
}

void CScriptTab::SetDirty(bool dirty) {
    if (dirty_ == dirty) {
        return;
    }
    dirty_ = dirty;
    UpdateDirtyUi();
    if (dirty_changed_) {
        dirty_changed_(dirty_);
    }
}

void CScriptTab::MarkDirty() {
    SetDirty(true);
}

void CScriptTab::UpdateDirtyUi() {
    if (::IsWindow(save_btn_.GetSafeHwnd())) {
        save_btn_.SetWindowTextW(dirty_ ? L"Save *" : L"Save");
    }
    if (::IsWindow(dirty_label_.GetSafeHwnd())) {
        dirty_label_.SetWindowTextW(dirty_ ? L"Unsaved changes" : L"");
        dirty_label_.ShowWindow(dirty_ ? SW_SHOW : SW_HIDE);
    }
    RedrawScriptSurface();
}

void CScriptTab::RedrawScriptSurface() {
    if (::IsWindow(list_.GetSafeHwnd())) {
        list_.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }
    if (::IsWindow(GetSafeHwnd())) {
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

std::wstring CScriptTab::AnsiToWide(const char* text) {
    if (text == nullptr) {
        return L"";
    }
    std::wstring out;
    while (*text != '\0') {
        out.push_back(static_cast<unsigned char>(*text));
        ++text;
    }
    return out;
}
