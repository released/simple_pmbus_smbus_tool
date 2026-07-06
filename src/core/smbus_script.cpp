#include "smbus_script.h"

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "text_utils.h"

namespace mfc_tool::core {
namespace {

std::wstring Trim(const std::wstring& text) {
    size_t begin = 0u;
    size_t end = text.size();

    while (begin < end && iswspace(text[begin])) {
        ++begin;
    }
    while (end > begin && iswspace(text[end - 1u])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::wstring LowerCompact(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        if (iswspace(ch) || ch == L'_' || ch == L'-') {
            continue;
        }
        out.push_back(static_cast<wchar_t>(towlower(ch)));
    }
    return out;
}

std::vector<std::wstring> SplitCsvLine(const std::wstring& line) {
    std::vector<std::wstring> fields;
    std::wstring field;
    bool quoted = false;
    size_t i = 0u;

    for (i = 0u; i < line.size(); ++i) {
        const wchar_t ch = line[i];
        if (quoted) {
            if (ch == L'"') {
                if ((i + 1u) < line.size() && line[i + 1u] == L'"') {
                    field.push_back(L'"');
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field.push_back(ch);
            }
        } else if (ch == L'"') {
            quoted = true;
        } else if (ch == L',') {
            fields.push_back(field);
            field.clear();
        } else {
            field.push_back(ch);
        }
    }
    fields.push_back(field);
    return fields;
}

std::wstring QuoteCsvField(const std::wstring& field) {
    bool quote = false;
    std::wstring out;
    size_t i = 0u;

    for (wchar_t ch : field) {
        if (ch == L',' || ch == L'"' || ch == L'\r' || ch == L'\n') {
            quote = true;
            break;
        }
    }
    if (!quote) {
        return field;
    }

    out.push_back(L'"');
    for (i = 0u; i < field.size(); ++i) {
        if (field[i] == L'"') {
            out.push_back(L'"');
        }
        out.push_back(field[i]);
    }
    out.push_back(L'"');
    return out;
}

std::wstring JoinCsvFields(const std::vector<std::wstring>& fields) {
    std::wstring out;
    size_t i = 0u;

    for (i = 0u; i < fields.size(); ++i) {
        if (i > 0u) {
            out.push_back(L',');
        }
        out += QuoteCsvField(fields[i]);
    }
    return out;
}

int HexDigit(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') {
        return static_cast<int>(ch - L'0');
    }
    if (ch >= L'a' && ch <= L'f') {
        return static_cast<int>(10 + ch - L'a');
    }
    if (ch >= L'A' && ch <= L'F') {
        return static_cast<int>(10 + ch - L'A');
    }
    return -1;
}

std::vector<std::uint8_t> ParseDataToken(std::wstring token) {
    std::vector<std::uint8_t> out;
    std::wstring hex;
    size_t i = 0u;

    token = Trim(token);
    if (token.size() >= 2u && token[0] == L'0' && (token[1] == L'x' || token[1] == L'X')) {
        token = token.substr(2u);
    }
    for (i = 0u; i < token.size(); ++i) {
        if (iswspace(token[i]) || token[i] == L'_') {
            continue;
        }
        if (HexDigit(token[i]) < 0) {
            throw std::invalid_argument("invalid script hex data");
        }
        hex.push_back(token[i]);
    }
    if (hex.empty()) {
        return out;
    }
    if ((hex.size() % 2u) != 0u) {
        hex.insert(hex.begin(), L'0');
    }
    for (i = 0u; i < hex.size(); i += 2u) {
        out.push_back(static_cast<std::uint8_t>((HexDigit(hex[i]) << 4) | HexDigit(hex[i + 1u])));
    }
    return out;
}

std::vector<std::uint8_t> ParseDataField(const std::wstring& field) {
    std::vector<std::uint8_t> out;
    std::wstring token;
    size_t i = 0u;
    bool split = false;

    for (wchar_t ch : field) {
        if (iswspace(ch) || ch == L';' || ch == L':' || ch == L'|') {
            split = true;
            break;
        }
    }
    if (!split) {
        return ParseDataToken(field);
    }

    for (i = 0u; i <= field.size(); ++i) {
        const wchar_t ch = (i < field.size()) ? field[i] : L' ';
        if (iswspace(ch) || ch == L';' || ch == L':' || ch == L'|') {
            std::vector<std::uint8_t> bytes = ParseDataToken(token);
            out.insert(out.end(), bytes.begin(), bytes.end());
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    return out;
}

int ParseScriptInt(const std::wstring& field, int fallback) {
    const std::wstring text = Trim(field);
    if (text.empty()) {
        return fallback;
    }
    return ParseInt(text);
}

SmbusScriptRow ParseRow(const std::vector<std::wstring>& fields) {
    SmbusScriptRow row;
    SmbusScriptCommandType type = SmbusScriptCommandType::ReadByte;
    size_t i = 0u;

    row.fields = fields;
    if (fields.empty()) {
        row.kind = SmbusScriptRowKind::Unknown;
        return row;
    }

    if (LowerCompact(fields[0]) == L"comment") {
        row.kind = SmbusScriptRowKind::Comment;
        return row;
    }
    if (LowerCompact(fields[0]) == L"pause") {
        row.kind = SmbusScriptRowKind::Pause;
        if (fields.size() > 1u) {
            row.delay_ms = (std::max)(0, ParseScriptInt(fields[1], 0));
        }
        return row;
    }
    if (!TryParseSmbusScriptCommandType(fields[0], &type)) {
        row.kind = SmbusScriptRowKind::Unknown;
        return row;
    }

    row.kind = SmbusScriptRowKind::Command;
    row.command_type = type;
    if (fields.size() > 1u) {
        row.address = ParseScriptInt(fields[1], -1) & 0x7F;
    }
    if (fields.size() > 2u && !Trim(fields[2]).empty()) {
        row.command = ParseScriptInt(fields[2], -1) & 0xFF;
    }
    for (i = 3u; i < fields.size(); ++i) {
        std::vector<std::uint8_t> bytes = ParseDataField(fields[i]);
        row.data.insert(row.data.end(), bytes.begin(), bytes.end());
    }
    return row;
}

SmbusScriptRow ParseM032Row(const std::vector<std::wstring>& fields) {
    SmbusScriptRow row;
    SmbusScriptProfile profile = SmbusScriptProfile::PmbusCrps;
    SmbusScriptCommandType type = SmbusScriptCommandType::ReadByte;
    const std::wstring kind = fields.empty() ? L"" : LowerCompact(fields[0]);

    row.fields = fields;
    if (kind == L"comment") {
        row.kind = SmbusScriptRowKind::Comment;
        row.fields = {L"Comment", fields.size() > 10u ? fields[10] : L""};
        return row;
    }
    if (kind == L"pause") {
        row.kind = SmbusScriptRowKind::Pause;
        row.selected = fields.size() > 1u ? (ParseScriptInt(fields[1], 1) != 0) : true;
        if (fields.size() > 2u && TryParseSmbusScriptProfile(fields[2], &profile)) {
            row.profile = profile;
        }
        row.delay_ms = fields.size() > 8u ? (std::max)(0, ParseScriptInt(fields[8], 0)) : 0;
        return row;
    }
    if (kind == L"command") {
        row.kind = SmbusScriptRowKind::Command;
        row.selected = fields.size() > 1u ? (ParseScriptInt(fields[1], 1) != 0) : true;
        if (fields.size() > 2u && TryParseSmbusScriptProfile(fields[2], &profile)) {
            row.profile = profile;
        }
        if (fields.size() > 3u && TryParseSmbusScriptCommandType(fields[3], &type)) {
            row.command_type = type;
        }
        row.address = fields.size() > 4u && !Trim(fields[4]).empty() ? (ParseScriptInt(fields[4], -1) & 0x7F) : -1;
        row.command = fields.size() > 5u && !Trim(fields[5]).empty() ? (ParseScriptInt(fields[5], -1) & 0xFF) : -1;
        row.data = fields.size() > 6u ? ParseDataField(fields[6]) : std::vector<std::uint8_t>{};
        row.read_length = fields.size() > 7u && !Trim(fields[7]).empty() ? (std::max)(0, ParseScriptInt(fields[7], 0)) : 0;
        row.delay_ms = fields.size() > 8u && !Trim(fields[8]).empty() ? (std::max)(0, ParseScriptInt(fields[8], 0)) : 0;
        row.pec = fields.size() > 9u ? (ParseScriptInt(fields[9], 0) != 0) : false;
        return row;
    }
    row.kind = SmbusScriptRowKind::Unknown;
    return row;
}

std::vector<std::wstring> SerializeM032Row(const SmbusScriptRow& row) {
    std::vector<std::wstring> fields;
    const std::wstring comment = row.fields.size() > 10u ? row.fields[10] : L"";

    if (row.kind == SmbusScriptRowKind::Comment) {
        fields = {L"Comment", L"", L"", L"", L"", L"", L"", L"", L"", L"", row.fields.size() > 1u ? row.fields[1] : L""};
        return fields;
    }
    if (row.kind == SmbusScriptRowKind::Unknown) {
        fields = {L"Comment", L"", L"", L"", L"", L"", L"", L"", L"", L"", JoinCsvFields(row.fields)};
        return fields;
    }
    if (row.kind == SmbusScriptRowKind::Pause) {
        fields = {
            L"Pause",
            row.selected ? L"1" : L"0",
            SmbusScriptProfileText(row.profile),
            L"Pause",
            L"",
            L"",
            L"",
            L"",
            std::to_wstring((std::max)(0, row.delay_ms)),
            row.pec ? L"1" : L"0",
            comment
        };
        return fields;
    }
    if (row.kind == SmbusScriptRowKind::Command) {
        fields = {
            L"Command",
            row.selected ? L"1" : L"0",
            SmbusScriptProfileText(row.profile),
            SmbusScriptCommandTypeText(row.command_type),
            row.address >= 0 ? FormatSmbusScriptHexByte(row.address) : L"",
            row.command >= 0 ? FormatSmbusScriptHexByte(row.command) : L"",
            FormatSmbusScriptData(row.data),
            row.read_length > 0 ? std::to_wstring(row.read_length) : L"",
            row.delay_ms > 0 ? std::to_wstring(row.delay_ms) : L"",
            row.pec ? L"1" : L"0",
            comment
        };
        return fields;
    }
    return row.fields;
}

std::wstring NarrowBytesToWide(const std::string& text) {
    std::wstring out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
        if (ch == 0xEFu || ch == 0xBBu || ch == 0xBFu) {
            continue;
        }
        out.push_back((ch < 0x80u) ? static_cast<wchar_t>(ch) : L'?');
    }
    return out;
}

std::string WideToNarrowAscii(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back((ch >= 0 && ch <= 0x7F) ? static_cast<char>(ch) : '?');
    }
    return out;
}

} // namespace

std::vector<std::wstring> SmbusScriptCommandTypeNames() {
    return {
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
        L"BusRecover",
        L"BadPecWriteByte",
        L"BadChecksumWrite"
    };
}

std::wstring SmbusScriptCommandTypeText(SmbusScriptCommandType type) {
    switch (type) {
    case SmbusScriptCommandType::QuickWrite: return L"QuickWrite";
    case SmbusScriptCommandType::QuickRead: return L"QuickRead";
    case SmbusScriptCommandType::SendByte: return L"SendByte";
    case SmbusScriptCommandType::ReceiveByte: return L"ReceiveByte";
    case SmbusScriptCommandType::WriteByte: return L"WriteByte";
    case SmbusScriptCommandType::WriteWord: return L"WriteWord";
    case SmbusScriptCommandType::ReadByte: return L"ReadByte";
    case SmbusScriptCommandType::ReadWord: return L"ReadWord";
    case SmbusScriptCommandType::Read32: return L"Read32";
    case SmbusScriptCommandType::BlockWrite: return L"BlockWrite";
    case SmbusScriptCommandType::BlockRead: return L"BlockRead";
    case SmbusScriptCommandType::ProcessCall: return L"ProcessCall";
    case SmbusScriptCommandType::BlockWriteReadProcessCall: return L"BlockWriteReadProcessCall";
    case SmbusScriptCommandType::BusRecover: return L"BusRecover";
    case SmbusScriptCommandType::BadPecWriteByte: return L"BadPecWriteByte";
    case SmbusScriptCommandType::BadChecksumWrite: return L"BadChecksumWrite";
    default: return L"ReadByte";
    }
}

bool TryParseSmbusScriptCommandType(const std::wstring& text, SmbusScriptCommandType* out) {
    const std::wstring key = LowerCompact(text);
    const auto names = SmbusScriptCommandTypeNames();
    size_t i = 0u;

    for (i = 0u; i < names.size(); ++i) {
        if (key == LowerCompact(names[i])) {
            if (out != nullptr) {
                *out = static_cast<SmbusScriptCommandType>(i);
            }
            return true;
        }
    }
    if (key == L"blockwritereadprocesscall" || key == L"blockprocesscall") {
        if (out != nullptr) {
            *out = SmbusScriptCommandType::BlockWriteReadProcessCall;
        }
        return true;
    }
    if (key == L"busrecover" || key == L"recover") {
        if (out != nullptr) {
            *out = SmbusScriptCommandType::BusRecover;
        }
        return true;
    }
    if (key == L"badpecwritebyte" || key == L"badpecwrite" || key == L"forcebadpecwritebyte") {
        if (out != nullptr) {
            *out = SmbusScriptCommandType::BadPecWriteByte;
        }
        return true;
    }
    if (key == L"badchecksumwrite" || key == L"badubmchecksumwrite" || key == L"ubmbadchecksumwrite") {
        if (out != nullptr) {
            *out = SmbusScriptCommandType::BadChecksumWrite;
        }
        return true;
    }
    return false;
}

std::vector<std::wstring> SmbusScriptProfileNames() {
    return {
        L"SMBus-Generic",
        L"SMBus-UBM",
        L"PMBus-Base",
        L"PMBus-CRPS",
        L"PMBus-TI-UCD90xxx"
    };
}

std::wstring SmbusScriptProfileText(SmbusScriptProfile profile) {
    switch (profile) {
    case SmbusScriptProfile::SmbusGeneric: return L"SMBus-Generic";
    case SmbusScriptProfile::SmbusUbm: return L"SMBus-UBM";
    case SmbusScriptProfile::PmbusBase: return L"PMBus-Base";
    case SmbusScriptProfile::PmbusCrps: return L"PMBus-CRPS";
    case SmbusScriptProfile::PmbusTiUcd90xxx: return L"PMBus-TI-UCD90xxx";
    default: return L"PMBus-CRPS";
    }
}

bool TryParseSmbusScriptProfile(const std::wstring& text, SmbusScriptProfile* out) {
    const std::wstring key = LowerCompact(text);
    if (key == L"smbusgeneric" || key == L"generic" || key == L"smbus") {
        if (out != nullptr) {
            *out = SmbusScriptProfile::SmbusGeneric;
        }
        return true;
    }
    if (key == L"smbusubm" || key == L"ubm" || key == L"ubmcontroller") {
        if (out != nullptr) {
            *out = SmbusScriptProfile::SmbusUbm;
        }
        return true;
    }
    if (key == L"pmbusbase" || key == L"base" || key == L"pmbus") {
        if (out != nullptr) {
            *out = SmbusScriptProfile::PmbusBase;
        }
        return true;
    }
    if (key == L"pmbuscrps" || key == L"crps" || key == L"mcrps" || key == L"pmbusmcrps") {
        if (out != nullptr) {
            *out = SmbusScriptProfile::PmbusCrps;
        }
        return true;
    }
    if (key == L"pmbustiucd90xxx" || key == L"pmbustiudc90xxx" || key == L"tiucd90xxx" ||
        key == L"tiudc90xxx" || key == L"ucd90xxx" || key == L"udc90xxx" || key == L"ti") {
        if (out != nullptr) {
            *out = SmbusScriptProfile::PmbusTiUcd90xxx;
        }
        return true;
    }
    return false;
}

bool LoadSmbusScriptCsv(const std::wstring& path, SmbusScriptDocument* out, std::wstring* error) {
    std::ifstream file;
    std::string line;
    bool first_data_row = true;
    bool m032_format = false;

    if (out == nullptr) {
        if (error != nullptr) {
            *error = L"output document is null";
        }
        return false;
    }

    file.open(std::filesystem::path(path), std::ios::binary);
    if (!file.is_open()) {
        if (error != nullptr) {
            *error = L"failed to open script: " + path;
        }
        return false;
    }

    out->path = path;
    out->rows.clear();
    while (std::getline(file, line)) {
        std::wstring wide = NarrowBytesToWide(line);
        if (!wide.empty() && wide.back() == L'\r') {
            wide.pop_back();
        }
        if (wide.empty()) {
            continue;
        }
        try {
            std::vector<std::wstring> fields = SplitCsvLine(wide);
            if (first_data_row) {
                first_data_row = false;
                if (!fields.empty() && LowerCompact(fields[0]) == L"kind") {
                    m032_format = true;
                    continue;
                }
            }
            out->rows.push_back(m032_format ? ParseM032Row(fields) : ParseRow(fields));
        } catch (const std::exception& ex) {
            if (error != nullptr) {
                *error = L"failed to parse script row " + std::to_wstring(out->rows.size() + 1u) +
                         L": " + std::wstring(ex.what(), ex.what() + strlen(ex.what()));
            }
            out->rows.clear();
            return false;
        }
    }
    return true;
}

bool SaveSmbusScriptCsv(const SmbusScriptDocument& doc, const std::wstring& path, std::wstring* error) {
    std::ofstream file;
    size_t i = 0u;

    file.open(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        if (error != nullptr) {
            *error = L"failed to save script: " + path;
        }
        return false;
    }

    file << "Kind,Selected,Profile,Type,Address,Command,Data,ReadLength,DelayMs,PEC,Comment\r\n";
    for (i = 0u; i < doc.rows.size(); ++i) {
        const std::wstring line = JoinCsvFields(SerializeM032Row(doc.rows[i]));
        file << WideToNarrowAscii(line) << "\r\n";
    }
    return true;
}

SmbusScriptRow MakeDefaultSmbusScriptRow(SmbusScriptProfile profile) {
    SmbusScriptRow row;
    row.kind = SmbusScriptRowKind::Command;
    row.profile = profile;
    row.selected = true;
    row.pec = profile == SmbusScriptProfile::PmbusBase ||
              profile == SmbusScriptProfile::PmbusCrps ||
              profile == SmbusScriptProfile::PmbusTiUcd90xxx;
    row.address = profile == SmbusScriptProfile::SmbusGeneric ? 0x5A : 0x5A;
    row.command = 0x98;
    row.command_type = SmbusScriptCommandType::ReadByte;
    row.read_length = 1;
    row.delay_ms = 10;
    if (profile == SmbusScriptProfile::SmbusUbm) {
        row.command = 0x00;
        row.command_type = SmbusScriptCommandType::ReadByte;
        row.pec = false;
    }
    return row;
}

std::wstring FormatSmbusScriptHexByte(int value) {
    std::wstringstream ss;
    ss << L"0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
       << static_cast<unsigned int>(value & 0xFF);
    return ss.str();
}

std::wstring FormatSmbusScriptData(const std::vector<std::uint8_t>& data) {
    std::wstringstream ss;
    size_t i = 0u;

    ss << std::uppercase << std::hex << std::setfill(L'0');
    for (i = 0u; i < data.size(); ++i) {
        if (i > 0u) {
            ss << L' ';
        }
        ss << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return ss.str();
}

std::wstring SmbusScriptRowTypeText(const SmbusScriptRow& row) {
    switch (row.kind) {
    case SmbusScriptRowKind::Comment: return L"Comment";
    case SmbusScriptRowKind::Pause: return L"Pause";
    case SmbusScriptRowKind::Command: return SmbusScriptCommandTypeText(row.command_type);
    default: return row.fields.empty() ? L"Unknown" : row.fields[0];
    }
}

std::wstring SmbusScriptRowSummary(const SmbusScriptRow& row) {
    std::wstringstream ss;

    if (row.kind == SmbusScriptRowKind::Pause) {
        ss << L"Delay " << row.delay_ms << L" ms";
        return ss.str();
    }
    if (row.kind == SmbusScriptRowKind::Comment) {
        if (row.fields.size() > 1u) {
            return row.fields[1];
        }
        return L"";
    }
    if (row.kind == SmbusScriptRowKind::Command) {
        ss << SmbusScriptProfileText(row.profile)
           << L" "
           << SmbusScriptCommandTypeText(row.command_type)
           << L" addr=" << FormatSmbusScriptHexByte(row.address);
        if (row.command >= 0) {
            ss << L" cmd=" << FormatSmbusScriptHexByte(row.command);
        }
        if (!row.data.empty()) {
            ss << L" data=" << FormatSmbusScriptData(row.data);
        }
        if (row.read_length > 0) {
            ss << L" read=" << row.read_length;
        }
        if (row.pec) {
            ss << L" PEC";
        }
        if (row.delay_ms > 0) {
            ss << L" delay=" << row.delay_ms << L"ms";
        }
        return ss.str();
    }
    return JoinCsvFields(row.fields);
}

bool SmbusScriptRowIsRead(const SmbusScriptRow& row) {
    if (row.kind != SmbusScriptRowKind::Command) {
        return false;
    }
    switch (row.command_type) {
    case SmbusScriptCommandType::QuickRead:
    case SmbusScriptCommandType::ReceiveByte:
    case SmbusScriptCommandType::ReadByte:
    case SmbusScriptCommandType::ReadWord:
    case SmbusScriptCommandType::Read32:
    case SmbusScriptCommandType::BlockRead:
    case SmbusScriptCommandType::ProcessCall:
    case SmbusScriptCommandType::BlockWriteReadProcessCall:
        return true;
    default:
        return false;
    }
}

bool SmbusScriptRowIsWrite(const SmbusScriptRow& row) {
    if (row.kind != SmbusScriptRowKind::Command) {
        return false;
    }
    switch (row.command_type) {
    case SmbusScriptCommandType::QuickWrite:
    case SmbusScriptCommandType::SendByte:
    case SmbusScriptCommandType::WriteByte:
    case SmbusScriptCommandType::WriteWord:
    case SmbusScriptCommandType::BlockWrite:
    case SmbusScriptCommandType::ProcessCall:
    case SmbusScriptCommandType::BlockWriteReadProcessCall:
    case SmbusScriptCommandType::BadPecWriteByte:
    case SmbusScriptCommandType::BadChecksumWrite:
        return true;
    default:
        return false;
    }
}

} // namespace mfc_tool::core
