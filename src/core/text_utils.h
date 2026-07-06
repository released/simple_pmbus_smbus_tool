#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mfc_tool::core {

int ParseInt(const std::wstring& text);
std::vector<std::uint8_t> ParseHexBytes(const std::wstring& text);
std::wstring HexDump(const std::vector<std::uint8_t>& data);
std::wstring HexDumpPtr(const std::uint8_t* data, size_t len);

} // namespace mfc_tool::core
