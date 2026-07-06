#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mfc_tool::core {

enum class SmbusScriptRowKind {
    Comment = 0,
    Pause,
    Command,
    Unknown
};

enum class SmbusScriptCommandType {
    QuickWrite = 0,
    QuickRead,
    SendByte,
    ReceiveByte,
    WriteByte,
    WriteWord,
    ReadByte,
    ReadWord,
    Read32,
    BlockWrite,
    BlockRead,
    ProcessCall,
    BlockWriteReadProcessCall,
    BusRecover,
    BadPecWriteByte,
    BadChecksumWrite
};

enum class SmbusScriptProfile {
    SmbusGeneric = 0,
    SmbusUbm,
    PmbusBase,
    PmbusCrps,
    PmbusTiUcd90xxx
};

struct SmbusScriptRow {
    SmbusScriptRowKind kind = SmbusScriptRowKind::Unknown;
    SmbusScriptProfile profile = SmbusScriptProfile::PmbusCrps;
    SmbusScriptCommandType command_type = SmbusScriptCommandType::ReadByte;
    bool selected = true;
    bool pec = false;
    int address = -1;
    int command = -1;
    int delay_ms = 0;
    int read_length = 0;
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> response;
    std::vector<std::wstring> fields;
};

struct SmbusScriptDocument {
    std::wstring path;
    std::vector<SmbusScriptRow> rows;
};

std::vector<std::wstring> SmbusScriptCommandTypeNames();
std::wstring SmbusScriptCommandTypeText(SmbusScriptCommandType type);
bool TryParseSmbusScriptCommandType(const std::wstring& text, SmbusScriptCommandType* out);
std::vector<std::wstring> SmbusScriptProfileNames();
std::wstring SmbusScriptProfileText(SmbusScriptProfile profile);
bool TryParseSmbusScriptProfile(const std::wstring& text, SmbusScriptProfile* out);

bool LoadSmbusScriptCsv(const std::wstring& path, SmbusScriptDocument* out, std::wstring* error);
bool SaveSmbusScriptCsv(const SmbusScriptDocument& doc, const std::wstring& path, std::wstring* error);
SmbusScriptRow MakeDefaultSmbusScriptRow(SmbusScriptProfile profile);

std::wstring FormatSmbusScriptHexByte(int value);
std::wstring FormatSmbusScriptData(const std::vector<std::uint8_t>& data);
std::wstring SmbusScriptRowTypeText(const SmbusScriptRow& row);
std::wstring SmbusScriptRowSummary(const SmbusScriptRow& row);
bool SmbusScriptRowIsRead(const SmbusScriptRow& row);
bool SmbusScriptRowIsWrite(const SmbusScriptRow& row);

} // namespace mfc_tool::core
