#include "text_utils.h"

#include <cwctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace mfc_tool::core {

int ParseInt(const std::wstring& text) {
    std::wstring s;
    for (wchar_t ch : text) {
        if (!iswspace(ch)) {
            s.push_back(ch);
        }
    }
    if (s.empty()) {
        throw std::invalid_argument("empty integer string");
    }

    int base = 10;
    size_t idx = 0;
    if (s.size() > 2 && s[0] == L'0' && (s[1] == L'x' || s[1] == L'X')) {
        base = 16;
        idx = 2;
    }

    int sign = 1;
    if (idx < s.size() && s[idx] == L'-') {
        sign = -1;
        ++idx;
    }

    long long value = 0;
    for (; idx < s.size(); ++idx) {
        wchar_t ch = s[idx];
        int digit = -1;
        if (ch >= L'0' && ch <= L'9') {
            digit = ch - L'0';
        } else if (base == 16 && ch >= L'a' && ch <= L'f') {
            digit = 10 + ch - L'a';
        } else if (base == 16 && ch >= L'A' && ch <= L'F') {
            digit = 10 + ch - L'A';
        } else {
            throw std::invalid_argument("invalid integer character");
        }
        if (digit >= base) {
            throw std::invalid_argument("invalid digit for base");
        }
        value = value * base + digit;
    }

    return static_cast<int>(value * sign);
}

std::vector<std::uint8_t> ParseHexBytes(const std::wstring& text) {
    std::wstring cleaned;
    cleaned.reserve(text.size());
    for (wchar_t ch : text) {
        if (iswspace(ch) || ch == L',' || ch == L':' || ch == L'_') {
            continue;
        }
        cleaned.push_back(ch);
    }

    if (cleaned.size() >= 2 && cleaned[0] == L'0' && (cleaned[1] == L'x' || cleaned[1] == L'X')) {
        cleaned = cleaned.substr(2);
    }

    if (cleaned.empty()) {
        return {};
    }
    if (cleaned.size() % 2 != 0) {
        throw std::invalid_argument("hex string length must be even");
    }

    std::vector<std::uint8_t> out;
    out.reserve(cleaned.size() / 2);

    auto hex_val = [](wchar_t ch) -> int {
        if (ch >= L'0' && ch <= L'9') {
            return ch - L'0';
        }
        if (ch >= L'a' && ch <= L'f') {
            return 10 + ch - L'a';
        }
        if (ch >= L'A' && ch <= L'F') {
            return 10 + ch - L'A';
        }
        return -1;
    };

    for (size_t i = 0; i < cleaned.size(); i += 2) {
        int hi = hex_val(cleaned[i]);
        int lo = hex_val(cleaned[i + 1]);
        if (hi < 0 || lo < 0) {
            throw std::invalid_argument("invalid hex character");
        }
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }

    return out;
}

std::wstring HexDump(const std::vector<std::uint8_t>& data) {
    return HexDumpPtr(data.data(), data.size());
}

std::wstring HexDumpPtr(const std::uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return L"";
    }
    std::wstringstream ss;
    ss << std::uppercase << std::hex;
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) {
            ss << L' ';
        }
        ss << std::setw(2) << std::setfill(L'0') << static_cast<unsigned int>(data[i]);
    }
    return ss.str();
}

} // namespace mfc_tool::core
