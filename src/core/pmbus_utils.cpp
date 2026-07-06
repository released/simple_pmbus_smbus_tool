#include "pmbus_utils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <iomanip>
#include <limits>
#include <sstream>

#include "text_utils.h"

namespace mfc_tool::core {
namespace {

constexpr std::uint8_t kStatusByteBits[8] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
};

constexpr const wchar_t* kStatusByteNames[8] = {
    L"NONE_OF_ABOVE",
    L"CML",
    L"TEMPERATURE",
    L"VIN_UV",
    L"IOUT_OC",
    L"VOUT_OV",
    L"OFF",
    L"BUSY",
};

constexpr const wchar_t* kStatusWordHighNames[8] = {
    L"UNKNOWN",
    L"OTHER",
    L"FANS",
    L"POWER_GOOD#",
    L"MFR",
    L"INPUT",
    L"IOUT/POUT",
    L"VOUT",
};

constexpr std::uint8_t kVoutModeRelativeMask = 0x80u;
constexpr std::uint8_t kVoutModeFormatMask = 0x60u;
constexpr std::uint8_t kVoutModeParameterMask = 0x1Fu;
constexpr std::uint8_t kVoutModeFormatULinear16 = 0x00u;
constexpr std::uint8_t kVoutModeFormatVid = 0x20u;
constexpr std::uint8_t kVoutModeFormatDirect = 0x40u;
constexpr std::uint8_t kVoutModeFormatIeeeHalf = 0x60u;

const wchar_t* DupWide(const std::wstring& text) {
    const size_t len = text.size();
    wchar_t* copy = new wchar_t[len + 1u];
    wmemcpy(copy, text.c_str(), len);
    copy[len] = L'\0';
    return copy;
}

struct PmbusContractPresetEntry {
    std::uint8_t code;
    const wchar_t* name;
    PmbusTransactionType preferred_txn;
    PmbusDataFormat format;
    std::uint32_t allowed_mask;
};

#include "pmbus_contract.generated.inc"

const PmbusContractPresetEntry* FindPmbusContractPresetEntryByCode(std::uint8_t code) {
    size_t i = 0u;
    for (i = 0u; i < kPmbusContractPresetEntryCount; ++i) {
        if (kPmbusContractPresetEntries[i].code == code) {
            return &kPmbusContractPresetEntries[i];
        }
    }
    return nullptr;
}

std::vector<PmbusCommandPreset> BuildPmbusCommandPresets() {
    std::vector<PmbusCommandPreset> out;
    size_t i = 0u;

    out.reserve(kPmbusContractPresetEntryCount);
    for (i = 0u; i < kPmbusContractPresetEntryCount; ++i) {
        const auto& entry = kPmbusContractPresetEntries[i];
        out.push_back({entry.code, DupWide(entry.name), entry.preferred_txn, entry.format});
    }
    return out;
}

const std::vector<PmbusCommandPreset> kPresets = BuildPmbusCommandPresets();

std::wstring JoinNames(const std::vector<std::wstring>& names) {
    std::wstringstream ss;
    size_t i = 0u;
    for (i = 0u; i < names.size(); ++i) {
        if (i > 0u) {
            ss << L", ";
        }
        ss << names[i];
    }
    return ss.str();
}

std::wstring FormatRawByte(std::uint8_t raw) {
    std::wstringstream ss;
    ss << L"0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
       << static_cast<unsigned int>(raw);
    return ss.str();
}

std::wstring FormatRawWord(std::uint16_t raw) {
    std::wstringstream ss;
    ss << L"0x" << std::uppercase << std::hex << std::setw(4) << std::setfill(L'0')
       << static_cast<unsigned int>(raw);
    return ss.str();
}

std::wstring FormatRawDword(std::uint32_t raw) {
    std::wstringstream ss;
    ss << L"0x" << std::uppercase << std::hex << std::setw(8) << std::setfill(L'0')
       << static_cast<unsigned long>(raw);
    return ss.str();
}

std::wstring DecodeCapabilityByte(std::uint8_t raw) {
    std::vector<std::wstring> fields;
    const std::uint8_t speed = static_cast<std::uint8_t>((raw >> 5) & 0x03u);
    std::wstring speed_text = L"MaxBus=Reserved";

    if ((raw & 0x80u) != 0u) {
        fields.emplace_back(L"PEC");
    } else {
        fields.emplace_back(L"No PEC");
    }

    switch (speed) {
    case 0x00u:
        speed_text = L"MaxBus=100kHz";
        break;
    case 0x01u:
        speed_text = L"MaxBus=400kHz";
        break;
    case 0x02u:
        speed_text = L"MaxBus=1MHz";
        break;
    default:
        break;
    }
    fields.push_back(speed_text);
    fields.push_back((raw & 0x10u) != 0u ? L"SMBALERT#" : L"No SMBALERT#");
    fields.push_back((raw & 0x08u) != 0u ? L"HalfFloat" : L"Linear/Direct");
    fields.push_back((raw & 0x04u) != 0u ? L"AVSBus" : L"No AVSBus");

    return FormatRawByte(raw) + L" | " + JoinNames(fields);
}

std::wstring DecodeQueryByte(std::uint8_t raw) {
    std::vector<std::wstring> fields;
    std::uint8_t format = 0u;
    std::wstring format_text;

    if ((raw & 0x80u) == 0u) {
        return FormatRawByte(raw) + L" | Not supported";
    }

    fields.emplace_back(L"Supported");
    fields.push_back((raw & 0x40u) != 0u ? L"Write" : L"NoWrite");
    fields.push_back((raw & 0x20u) != 0u ? L"Read" : L"NoRead");

    format = static_cast<std::uint8_t>((raw >> 2) & 0x07u);
    switch (format) {
    case 0x00u:
        format_text = L"Fmt=LINEAR11/16";
        break;
    case 0x01u:
        format_text = L"Fmt=Signed16";
        break;
    case 0x02u:
        format_text = L"Fmt=HalfFloat";
        break;
    case 0x03u:
        format_text = L"Fmt=Direct";
        break;
    case 0x04u:
        format_text = L"Fmt=Unsigned8";
        break;
    case 0x05u:
        format_text = L"Fmt=VID";
        break;
    case 0x06u:
        format_text = L"Fmt=MfrSpecific";
        break;
    case 0x07u:
        format_text = L"Fmt=NonNumeric";
        break;
    default:
        format_text = L"Fmt=Unknown";
        break;
    }
    fields.push_back(format_text);
    return FormatRawByte(raw) + L" | " + JoinNames(fields);
}

std::wstring DecodeRevisionByte(std::uint8_t raw) {
    const unsigned int part1 = static_cast<unsigned int>((raw >> 4) & 0x0Fu);
    const unsigned int part2 = static_cast<unsigned int>(raw & 0x0Fu);
    std::wstringstream ss;
    ss << FormatRawByte(raw)
       << L" | PartI=1." << part1
       << L", PartII=1." << part2;
    return ss.str();
}

std::wstring DecodeAsciiBlock(const std::vector<std::uint8_t>& payload) {
    std::wstring text;
    for (std::uint8_t byte : payload) {
        if (byte >= 32u && byte <= 126u) {
            text.push_back(static_cast<wchar_t>(byte));
        } else {
            text.push_back(L'.');
        }
    }
    return L"\"" + text + L"\"";
}

std::wstring DecodeAppProfileBlock(const std::vector<std::uint8_t>& payload) {
    std::wstringstream ss;
    size_t i = 0u;
    bool first = true;

    if (payload.empty()) {
        return L"No profiles";
    }

    if ((payload.size() % 2u) != 0u) {
        return L"Malformed APP_PROFILE_SUPPORT: " + HexDump(payload);
    }

    for (i = 0u; i < payload.size(); i += 2u) {
        std::uint8_t id = payload[i];
        std::uint8_t rev = payload[i + 1u];
        std::wstring name;
        unsigned int major = static_cast<unsigned int>((rev >> 4) & 0x0Fu);
        unsigned int minor = static_cast<unsigned int>(rev & 0x0Fu);

        if (!first) {
            ss << L" | ";
        }
        first = false;

        switch (id) {
        case 0x00u:
            name = L"None";
            break;
        case 0x01u:
            name = L"Server AC-DC";
            break;
        case 0x02u:
            name = L"DC-DC Microprocessor";
            break;
        case 0x03u:
            name = L"DC-DC General";
            break;
        default:
            {
                std::wstringstream tmp;
                tmp << L"Profile 0x" << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
                    << static_cast<unsigned int>(id);
                name = tmp.str();
            }
            break;
        }

        if (id == 0x00u && rev == 0x00u) {
            ss << L"No application profiles";
        } else {
            ss << name << L" rev " << std::dec << major << L"." << minor;
        }
    }
    return ss.str();
}

} // namespace

const std::vector<PmbusCommandPreset>& PmbusCommandPresets() {
    return kPresets;
}

const PmbusCommandPreset* FindPmbusCommandPresetByCode(std::uint8_t code) {
    const PmbusCommandPreset* result = nullptr;
    const auto it = std::find_if(kPresets.begin(), kPresets.end(),
                                 [code](const PmbusCommandPreset& preset) {
                                     return preset.code == code;
                                 });
    if (it != kPresets.end()) {
        result = &(*it);
    }
    return result;
}

const PmbusCommandPreset* FindPmbusCommandPresetByName(const std::wstring& name) {
    const PmbusCommandPreset* result = nullptr;
    const auto it = std::find_if(kPresets.begin(), kPresets.end(),
                                 [&name](const PmbusCommandPreset& preset) {
                                     return name == preset.name;
                                 });
    if (it != kPresets.end()) {
        result = &(*it);
    }
    return result;
}

std::wstring PmbusTransactionTypeText(PmbusTransactionType type) {
    switch (type) {
    case PmbusTransactionType::SendByte: return L"Send Byte";
    case PmbusTransactionType::ReceiveByte: return L"Receive Byte";
    case PmbusTransactionType::WriteByte: return L"Write Byte";
    case PmbusTransactionType::WriteWord: return L"Write Word";
    case PmbusTransactionType::ReadByte: return L"Read Byte";
    case PmbusTransactionType::ReadWord: return L"Read Word";
    case PmbusTransactionType::Read32: return L"Read 32";
    case PmbusTransactionType::BlockWrite: return L"Block Write";
    case PmbusTransactionType::BlockRead: return L"Block Read";
    case PmbusTransactionType::ProcessCall: return L"Process Call";
    case PmbusTransactionType::BlockWriteReadProcessCall: return L"Block Wr/Rd ProcCall";
    default: return L"Unknown";
    }
}

std::uint32_t PmbusTransactionTypeMask(PmbusTransactionType type) {
    return 1u << static_cast<unsigned int>(type);
}

namespace {

std::uint32_t PmbusAllowedMaskForStandardCommand(std::uint8_t code) {
    const PmbusContractPresetEntry* entry = FindPmbusContractPresetEntryByCode(code);
    if (entry == nullptr) {
        return 0u;
    }
    return entry->allowed_mask;
}
std::wstring PmbusAllowedMaskText(std::uint32_t mask) {
    std::vector<std::wstring> names;
    int i = 0;
    for (i = 0; i <= static_cast<int>(PmbusTransactionType::BlockWriteReadProcessCall); ++i) {
        const PmbusTransactionType type = static_cast<PmbusTransactionType>(i);
        if ((mask & PmbusTransactionTypeMask(type)) != 0u) {
            names.push_back(PmbusTransactionTypeText(type));
        }
    }
    if (names.empty()) {
        return L"Manufacturer defined / no strict check";
    }
    return JoinNames(names);
}

} // namespace

bool PmbusTransactionAllowed(std::uint8_t code,
                             PmbusTransactionType type,
                             bool extended_mode,
                             std::wstring* allowed_text) {
    std::uint32_t mask = 0u;
    if (extended_mode) {
        mask = PmbusTransactionTypeMask(PmbusTransactionType::WriteByte) |
               PmbusTransactionTypeMask(PmbusTransactionType::WriteWord) |
               PmbusTransactionTypeMask(PmbusTransactionType::ReadByte) |
               PmbusTransactionTypeMask(PmbusTransactionType::ReadWord) |
               PmbusTransactionTypeMask(PmbusTransactionType::Read32) |
               PmbusTransactionTypeMask(PmbusTransactionType::BlockWrite) |
               PmbusTransactionTypeMask(PmbusTransactionType::BlockRead) |
               PmbusTransactionTypeMask(PmbusTransactionType::ProcessCall) |
               PmbusTransactionTypeMask(PmbusTransactionType::BlockWriteReadProcessCall);
    } else if (code == 0xFEu || code == 0xFFu) {
        mask = 0u;
    } else {
        mask = PmbusAllowedMaskForStandardCommand(code);
    }
    if (allowed_text != nullptr) {
        *allowed_text = PmbusAllowedMaskText(mask);
    }
    if (mask == 0u) {
        return true;
    }
    return (mask & PmbusTransactionTypeMask(type)) != 0u;
}

std::uint8_t PmbusComputePec(const std::uint8_t* data, size_t len) {
    std::uint8_t crc = 0u;
    size_t i = 0u;
    int bit = 0;

    for (i = 0u; i < len; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            if ((crc & 0x80u) != 0u) {
                crc = static_cast<std::uint8_t>((crc << 1) ^ 0x07u);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

std::uint8_t PmbusComputePec(const std::vector<std::uint8_t>& bytes) {
    return PmbusComputePec(bytes.data(), bytes.size());
}

double PmbusLinear11ToDouble(std::uint16_t raw) {
    std::int16_t signed_raw = static_cast<std::int16_t>(raw);
    std::int16_t mantissa = static_cast<std::int16_t>(signed_raw & 0x07FF);
    std::int8_t exponent = 0;
    double value = 0.0;

    if ((mantissa & 0x0400) != 0) {
        mantissa = static_cast<std::int16_t>(mantissa | static_cast<std::int16_t>(0xF800));
    }
    exponent = static_cast<std::int8_t>((signed_raw >> 11) & 0x1F);
    if ((exponent & 0x10) != 0) {
        exponent = static_cast<std::int8_t>(exponent | static_cast<std::int8_t>(0xE0));
    }
    value = static_cast<double>(mantissa);
    if (exponent >= 0) {
        value *= static_cast<double>(1 << exponent);
    } else {
        value /= static_cast<double>(1 << (-exponent));
    }
    return value;
}

double PmbusLinear16ToDouble(std::uint16_t raw, std::int8_t exponent) {
    double value = static_cast<double>(raw);
    if (exponent >= 0) {
        value *= static_cast<double>(1 << exponent);
    } else {
        value /= static_cast<double>(1 << (-exponent));
    }
    return value;
}

double PmbusIeeeHalfToDouble(std::uint16_t raw) {
    const int sign = ((raw & 0x8000u) != 0u) ? -1 : 1;
    const int exponent = static_cast<int>((raw >> 10) & 0x1Fu);
    const int fraction = static_cast<int>(raw & 0x03FFu);

    if (exponent == 0) {
        if (fraction == 0) {
            return sign < 0 ? -0.0 : 0.0;
        }
        return static_cast<double>(sign) * std::ldexp(static_cast<double>(fraction), -24);
    }
    if (exponent == 31) {
        if (fraction == 0) {
            return sign < 0 ? -std::numeric_limits<double>::infinity()
                            : std::numeric_limits<double>::infinity();
        }
        return std::numeric_limits<double>::quiet_NaN();
    }
    return static_cast<double>(sign) * std::ldexp(static_cast<double>(1024 + fraction), exponent - 25);
}

std::wstring VoutModeFormatName(std::uint8_t raw) {
    switch (raw & kVoutModeFormatMask) {
    case kVoutModeFormatULinear16:
        return L"ULINEAR16";
    case kVoutModeFormatVid:
        return L"VID";
    case kVoutModeFormatDirect:
        return L"Direct";
    case kVoutModeFormatIeeeHalf:
        return L"IEEE half";
    default:
        return L"Unknown";
    }
}

std::wstring DecodeVoutModeByte(std::uint8_t raw) {
    std::wstringstream ss;
    const bool relative = (raw & kVoutModeRelativeMask) != 0u;
    const std::uint8_t format = static_cast<std::uint8_t>(raw & kVoutModeFormatMask);
    const std::uint8_t parameter = static_cast<std::uint8_t>(raw & kVoutModeParameterMask);

    ss << L"mode=" << (relative ? L"Relative" : L"Absolute")
       << L" | fmt=" << VoutModeFormatName(raw);
    if (format == kVoutModeFormatULinear16) {
        std::int8_t exp = 0;
        if (TryParseVoutModeExponent(raw, &exp)) {
            ss << L" | exp=" << static_cast<int>(exp);
        }
    } else if (format == kVoutModeFormatVid) {
        ss << L" | vid_type=0x" << std::uppercase << std::hex << std::setw(2)
           << std::setfill(L'0') << static_cast<unsigned int>(parameter);
        if ((parameter != 0x1Eu) && (parameter != 0x1Fu)) {
            ss << L" | invalid/reserved selector";
        }
    } else {
        ss << L" | param=0x" << std::uppercase << std::hex << std::setw(2)
           << std::setfill(L'0') << static_cast<unsigned int>(parameter);
        if (parameter != 0u) {
            ss << L" | invalid parameter";
        }
    }
    return ss.str();
}

std::wstring DecodeVoutModeAwareWord(std::uint16_t raw, std::uint8_t vout_mode) {
    std::wstringstream ss;
    const std::uint8_t format = static_cast<std::uint8_t>(vout_mode & kVoutModeFormatMask);
    const std::uint8_t parameter = static_cast<std::uint8_t>(vout_mode & kVoutModeParameterMask);

    ss.setf(std::ios::fixed, std::ios::floatfield);
    ss.precision(4);
    if (format == kVoutModeFormatULinear16) {
        std::int8_t exp = 0;
        if (TryParseVoutModeExponent(vout_mode, &exp)) {
            ss << PmbusLinear16ToDouble(raw, exp)
               << L" | fmt=ULINEAR16"
               << L" | exp=" << static_cast<int>(exp)
               << L" | raw=" << FormatRawWord(raw);
            return ss.str();
        }
    } else if (format == kVoutModeFormatVid) {
        ss << L"VID raw=" << FormatRawWord(raw)
           << L" | vid_type=0x" << std::uppercase << std::hex << std::setw(2)
           << std::setfill(L'0') << static_cast<unsigned int>(parameter)
           << L" | right-justified raw code";
        if ((parameter != 0x1Eu) && (parameter != 0x1Fu)) {
            ss << L" | invalid/reserved selector";
        } else {
            ss << L" | product VID table required for volts";
        }
        return ss.str();
    } else if (format == kVoutModeFormatDirect) {
        ss << L"Direct raw=" << FormatRawWord(raw)
           << L" | use COEFFICIENTS m/b/R for numeric decode";
        if (parameter != 0u) {
            ss << L" | invalid VOUT_MODE parameter";
        }
        return ss.str();
    } else if (format == kVoutModeFormatIeeeHalf) {
        ss << PmbusIeeeHalfToDouble(raw)
           << L" | fmt=IEEE half"
           << L" | raw=" << FormatRawWord(raw);
        if (parameter != 0u) {
            ss << L" | invalid VOUT_MODE parameter";
        }
        return ss.str();
    }

    ss << FormatRawWord(raw) << L" | unsupported VOUT_MODE";
    return ss.str();
}

bool TryParseVoutModeExponent(std::uint8_t raw, std::int8_t* exponent) {
    std::uint8_t format = 0u;
    std::int8_t exp = 0;

    if (exponent == nullptr) {
        return false;
    }
    format = static_cast<std::uint8_t>(raw & kVoutModeFormatMask);
    if (format != kVoutModeFormatULinear16) {
        return false;
    }
    exp = static_cast<std::int8_t>(raw & kVoutModeParameterMask);
    if ((exp & 0x10) != 0) {
        exp = static_cast<std::int8_t>(exp | static_cast<std::int8_t>(0xE0));
    }
    *exponent = exp;
    return true;
}

std::wstring DecodePmbusPayload(const PmbusCommandPreset* preset,
                                const std::vector<std::uint8_t>& payload,
                                std::uint8_t vout_mode,
                                bool pec_enabled,
                                bool pec_ok) {
    std::wstringstream ss;

    if (preset == nullptr) {
        ss << L"Raw: " << HexDump(payload);
        if (pec_enabled) {
            ss << L" | PEC " << (pec_ok ? L"OK" : L"FAIL");
        }
        return ss.str();
    }

    if (payload.empty()) {
        ss << L"No payload";
        if (pec_enabled) {
            ss << L" | PEC " << (pec_ok ? L"OK" : L"FAIL");
        }
        return ss.str();
    }

    switch (preset->format) {
    case PmbusDataFormat::None:
        ss << L"OK";
        break;

    case PmbusDataFormat::RawByte:
        ss << FormatRawByte(payload[0]);
        if (preset->code == 0x20u) {
            ss << L" | " << DecodeVoutModeByte(payload[0]);
        }
        break;

    case PmbusDataFormat::RawWord:
    case PmbusDataFormat::StatusWord:
    case PmbusDataFormat::Linear11:
    case PmbusDataFormat::Linear16Vout:
    case PmbusDataFormat::VoutModeAwareWord:
        if (payload.size() < 2u) {
            ss << L"Payload too short";
            break;
        }
        {
            const std::uint16_t raw = static_cast<std::uint16_t>(payload[0] | (payload[1] << 8));
            if (preset->format == PmbusDataFormat::RawWord) {
                ss << FormatRawWord(raw);
            } else if (preset->format == PmbusDataFormat::Linear11) {
                ss.setf(std::ios::fixed, std::ios::floatfield);
                ss.precision(4);
                ss << PmbusLinear11ToDouble(raw) << L" | raw=" << FormatRawWord(raw);
            } else if ((preset->format == PmbusDataFormat::Linear16Vout) ||
                       (preset->format == PmbusDataFormat::VoutModeAwareWord)) {
                ss.setf(std::ios::fixed, std::ios::floatfield);
                ss.precision(4);
                ss << DecodeVoutModeAwareWord(raw, vout_mode);
            } else {
                std::vector<std::wstring> bits;
                const std::uint8_t low = payload[0];
                const std::uint8_t high = payload[1];
                int i = 0;
                for (i = 0; i < 8; ++i) {
                    if ((low & kStatusByteBits[i]) != 0u) {
                        bits.emplace_back(kStatusByteNames[i]);
                    }
                    if ((high & kStatusByteBits[i]) != 0u) {
                        bits.emplace_back(kStatusWordHighNames[i]);
                    }
                }
                ss << FormatRawWord(raw);
                if (!bits.empty()) {
                    ss << L" | " << JoinNames(bits);
                }
            }
        }
        break;

    case PmbusDataFormat::RawDword:
        if (payload.size() < 4u) {
            ss << L"Payload too short";
            break;
        }
        {
            std::uint32_t raw = static_cast<std::uint32_t>(payload[0]) |
                                (static_cast<std::uint32_t>(payload[1]) << 8) |
                                (static_cast<std::uint32_t>(payload[2]) << 16) |
                                (static_cast<std::uint32_t>(payload[3]) << 24);
            ss << FormatRawDword(raw) << L" | dec=" << std::dec << raw;
        }
        break;

    case PmbusDataFormat::StatusByte:
        {
            std::vector<std::wstring> bits;
            int i = 0;
            for (i = 0; i < 8; ++i) {
                if ((payload[0] & kStatusByteBits[i]) != 0u) {
                    bits.emplace_back(kStatusByteNames[i]);
                }
            }
            ss << FormatRawByte(payload[0]);
            if (!bits.empty()) {
                ss << std::dec << L" | " << JoinNames(bits);
            }
        }
        break;

    case PmbusDataFormat::Capability:
        ss << DecodeCapabilityByte(payload[0]);
        break;

    case PmbusDataFormat::QueryResult:
        ss << DecodeQueryByte(payload[0]);
        break;

    case PmbusDataFormat::PmbusRevision:
        ss << DecodeRevisionByte(payload[0]);
        break;

    case PmbusDataFormat::Percent0p1:
        ss.setf(std::ios::fixed, std::ios::floatfield);
        ss.precision(1);
        ss << static_cast<double>(payload[0]) / 10.0 << L"% | raw=" << FormatRawByte(payload[0]);
        break;

    case PmbusDataFormat::AppProfileSupport:
        ss << DecodeAppProfileBlock(payload);
        break;

    case PmbusDataFormat::BlockAscii:
        ss << DecodeAsciiBlock(payload);
        break;

    case PmbusDataFormat::RawBlock:
    default:
        ss << HexDump(payload);
        break;
    }

    if (pec_enabled) {
        ss << L" | PEC " << (pec_ok ? L"OK" : L"FAIL");
    }
    return ss.str();
}

} // namespace mfc_tool::core


