#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace mfc_tool::log {

class Logger {
public:
    void AddLine(const std::wstring& line);
    void Clear();
    [[nodiscard]] std::wstring GetAllText() const;

    bool SaveToFile(const std::wstring& path, std::wstring* error = nullptr) const;

private:
    mutable std::mutex mutex_;
    std::vector<std::wstring> lines_;
};

} // namespace mfc_tool::log
