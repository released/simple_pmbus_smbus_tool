#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mfc_tool::core {

enum class PmbusTransactionType {
    SendByte = 0,
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
};

enum class PmbusDataFormat {
    None = 0,
    RawByte,
    RawWord,
    RawDword,
    Linear11,
    Linear16Vout,
    VoutModeAwareWord,
    BlockAscii,
    StatusByte,
    StatusWord,
    Capability,
    QueryResult,
    PmbusRevision,
    Percent0p1,
    AppProfileSupport,
    RawBlock,
};

enum class PmbusExtendedCommandType {
    ManufacturerSpecific = 0,
    PmbusFuture,
};

struct PmbusCommandPreset {
    std::uint8_t code;
    const wchar_t* name;
    PmbusTransactionType preferred_txn;
    PmbusDataFormat format;
};

const std::vector<PmbusCommandPreset>& PmbusCommandPresets();
const PmbusCommandPreset* FindPmbusCommandPresetByCode(std::uint8_t code);
const PmbusCommandPreset* FindPmbusCommandPresetByName(const std::wstring& name);

std::wstring PmbusTransactionTypeText(PmbusTransactionType type);
std::uint32_t PmbusTransactionTypeMask(PmbusTransactionType type);
bool PmbusTransactionAllowed(std::uint8_t code,
                             PmbusTransactionType type,
                             bool extended_mode,
                             std::wstring* allowed_text = nullptr);
std::uint8_t PmbusComputePec(const std::vector<std::uint8_t>& bytes);
std::uint8_t PmbusComputePec(const std::uint8_t* data, size_t len);

double PmbusLinear11ToDouble(std::uint16_t raw);
double PmbusLinear16ToDouble(std::uint16_t raw, std::int8_t exponent);
bool TryParseVoutModeExponent(std::uint8_t raw, std::int8_t* exponent);

std::wstring DecodePmbusPayload(const PmbusCommandPreset* preset,
                                const std::vector<std::uint8_t>& payload,
                                std::uint8_t vout_mode,
                                bool pec_enabled,
                                bool pec_ok);

} // namespace mfc_tool::core
