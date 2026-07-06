#include "ini_manager.h"

#include <windows.h>

#include <vector>

namespace mfc_tool::config {
namespace {

std::wstring Trim(const std::wstring& s) {
    size_t b = 0;
    while (b < s.size() && iswspace(s[b])) {
        ++b;
    }
    size_t e = s.size();
    while (e > b && iswspace(s[e - 1])) {
        --e;
    }
    return s.substr(b, e - b);
}

} // namespace

IniManager::IniManager(std::wstring path)
    : path_(std::move(path)) {
    if (path_.empty()) {
        path_ = DefaultIniPath(L"pmbus_smbus_tool.ini");
    }
}

void IniManager::SetPath(const std::wstring& path) {
    path_ = path;
}

bool IniManager::Exists() const {
    DWORD attrs = GetFileAttributesW(path_.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IniManager::Load(IniData* out, std::wstring* error) const {
    if (out == nullptr) {
        if (error) {
            *error = L"IniManager::Load out pointer is null";
        }
        return false;
    }
    out->clear();

    std::vector<wchar_t> section_names(64 * 1024, 0);
    DWORD count = GetPrivateProfileSectionNamesW(section_names.data(), static_cast<DWORD>(section_names.size()), path_.c_str());
    if (count == 0) {
        DWORD last_error = GetLastError();
        if (last_error != ERROR_SUCCESS && last_error != ERROR_FILE_NOT_FOUND) {
            if (error) {
                *error = L"GetPrivateProfileSectionNamesW failed: " + std::to_wstring(last_error);
            }
            return false;
        }
        return true;
    }

    const wchar_t* sec_ptr = section_names.data();
    while (*sec_ptr != L'\0') {
        std::wstring section = sec_ptr;
        sec_ptr += section.size() + 1;

        std::vector<wchar_t> key_values(64 * 1024, 0);
        DWORD key_count = GetPrivateProfileSectionW(section.c_str(), key_values.data(), static_cast<DWORD>(key_values.size()), path_.c_str());
        if (key_count == 0) {
            (*out)[section] = IniSection{};
            continue;
        }

        IniSection sec_map;
        const wchar_t* kv_ptr = key_values.data();
        while (*kv_ptr != L'\0') {
            std::wstring line = kv_ptr;
            kv_ptr += line.size() + 1;
            size_t pos = line.find(L'=');
            if (pos == std::wstring::npos) {
                continue;
            }
            std::wstring key = Trim(line.substr(0, pos));
            std::wstring value = Trim(line.substr(pos + 1));
            sec_map[key] = value;
        }

        (*out)[section] = std::move(sec_map);
    }

    return true;
}

bool IniManager::Save(const IniData& data, std::wstring* error) const {
    for (const auto& [section, _] : data) {
        (void)_;
        if (!WritePrivateProfileSectionW(section.c_str(), nullptr, path_.c_str())) {
            if (error) {
                *error = L"Failed to clear section: " + section;
            }
            return false;
        }
    }

    for (const auto& [section, items] : data) {
        for (const auto& [key, value] : items) {
            if (!WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), path_.c_str())) {
                if (error) {
                    *error = L"WritePrivateProfileStringW failed at [" + section + L"] " + key;
                }
                return false;
            }
        }
    }

    return true;
}

std::wstring IniManager::DefaultIniPath(const std::wstring& file_name) {
    std::wstring dir = ExeDirectory();
    if (!dir.empty() && dir.back() != L'\\') {
        dir.push_back(L'\\');
    }
    return dir + file_name;
}

std::wstring IniManager::ExeDirectory() {
    wchar_t path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n == MAX_PATH) {
        return L".";
    }

    std::wstring full(path, n);
    size_t pos = full.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L".";
    }
    return full.substr(0, pos);
}

} // namespace mfc_tool::config
