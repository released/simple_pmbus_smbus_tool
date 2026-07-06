#pragma once

#include <map>
#include <string>

namespace mfc_tool::config {

using IniSection = std::map<std::wstring, std::wstring>;
using IniData = std::map<std::wstring, IniSection>;

class IniManager {
public:
    explicit IniManager(std::wstring path = L"");

    [[nodiscard]] const std::wstring& Path() const noexcept { return path_; }
    void SetPath(const std::wstring& path);

    bool Exists() const;

    bool Load(IniData* out, std::wstring* error = nullptr) const;
    bool Save(const IniData& data, std::wstring* error = nullptr) const;

    static std::wstring DefaultIniPath(const std::wstring& file_name);

private:
    static std::wstring ExeDirectory();

private:
    std::wstring path_;
};

} // namespace mfc_tool::config
