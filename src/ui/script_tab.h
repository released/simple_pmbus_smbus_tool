#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <functional>
#include <string>

#include "../core/app_state.h"
#include "../core/smbus_script.h"

class CScriptTab : public CWnd {
public:
    BOOL Create(CWnd* parent, const RECT& rect, UINT id);
    void Bind(std::function<void(const std::wstring&)> logger,
              std::function<void()> persist_settings = {},
              std::function<void(bool)> dirty_changed = {});
    void LoadState(const mfc_tool::core::AppState& state);
    void SaveState(mfc_tool::core::AppState* state) const;
    bool HasUnsavedChanges() const;
    bool ConfirmDiscardUnsavedChanges(const std::wstring& action_text) const;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLoad();
    afx_msg void OnSave();
    afx_msg void OnSaveAs();
    afx_msg void OnProfileChanged();
    afx_msg void OnTypeChanged();
    afx_msg void OnCalcPec();
    afx_msg void OnAdd();
    afx_msg void OnUpdateRow();
    afx_msg void OnDeleteRow();
    afx_msg void OnMoveUp();
    afx_msg void OnMoveDown();
    afx_msg void OnSelectAll();
    afx_msg void OnListChanged(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(const CRect& r);
    void PopulateProfileCombo();
    void PopulateTypeCombo(mfc_tool::core::SmbusScriptProfile profile = mfc_tool::core::SmbusScriptProfile::PmbusCrps);
    void PopulateList(int select_index = -1);
    void SetListRow(int item, size_t row_index);
    void SelectListItem(int item);
    void ClearEditors();
    void UpdateEditorMode();
    void LoadSelectedRowToEditors();
    int SelectedRowIndex() const;
    mfc_tool::core::SmbusScriptRow BuildRowFromEditors() const;
    void LoadScriptFromPath(const std::wstring& path);
    void SaveScriptToPath(const std::wstring& path);
    void SetPathText();
    void NotifyChanged();
    void SetDirty(bool dirty);
    void MarkDirty();
    void UpdateDirtyUi();
    void RedrawScriptSurface();
    void SetChildFonts();
    std::wstring GetEditText(const CEdit& edit) const;
    static std::wstring AnsiToWide(const char* text);
    std::wstring CalculatePecText(const mfc_tool::core::SmbusScriptRow& row) const;

private:
    mfc_tool::core::SmbusScriptDocument document_;
    std::function<void(const std::wstring&)> log_;
    std::function<void()> persist_settings_;
    std::function<void(bool)> dirty_changed_;
    bool populating_ = false;
    bool all_selected_ = true;
    bool dirty_ = false;

    CFont ui_font_;
    CStatic path_label_;
    CEdit path_edit_;
    CStatic dirty_label_;
    CButton load_btn_;
    CButton save_btn_;
    CButton save_as_btn_;
    CListCtrl list_;
    CButton select_all_btn_;
    CStatic profile_label_;
    CComboBox profile_combo_;
    CStatic type_label_;
    CComboBox type_combo_;
    CStatic addr_label_;
    CEdit addr_edit_;
    CStatic reg_label_;
    CEdit reg_edit_;
    CStatic data_label_;
    CEdit data_edit_;
    CStatic read_len_label_;
    CEdit read_len_edit_;
    CStatic delay_label_;
    CEdit delay_edit_;
    CButton pec_check_;
    CButton calc_pec_btn_;
    CStatic pec_label_;
    CEdit pec_edit_;
    CButton add_btn_;
    CButton update_btn_;
    CButton delete_btn_;
    CButton up_btn_;
    CButton down_btn_;
};
