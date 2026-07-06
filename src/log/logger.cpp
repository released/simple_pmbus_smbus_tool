#include "logger.h"

#include <cstdio>
#include <sstream>

namespace mfc_tool::log {

void Logger::AddLine(const std::wstring& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.push_back(line);
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.clear();
}

std::wstring Logger::GetAllText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::wstringstream ss;
    for (const auto& line : lines_) {
        ss << line << L"\r\n";
    }
    return ss.str();
}

bool Logger::SaveToFile(const std::wstring& path, std::wstring* error) const {
    std::lock_guard<std::mutex> lock(mutex_);
    FILE* fp = nullptr;
    errno_t ec = _wfopen_s(&fp, path.c_str(), L"wt, ccs=UTF-8");
    if (ec != 0 || fp == nullptr) {
        if (error) {
            *error = L"Cannot open file: " + path;
        }
        return false;
    }

    for (const auto& line : lines_) {
        if (fwprintf(fp, L"%ls\n", line.c_str()) < 0) {
            fclose(fp);
            if (error) {
                *error = L"Write log failed: " + path;
            }
            return false;
        }
    }
    fclose(fp);
    return true;
}

} // namespace mfc_tool::log
