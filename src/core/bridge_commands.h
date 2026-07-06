#pragma once

#include <cstdint>
#include <string>

namespace mfc_tool::core {

constexpr uint8_t kBridgeMagic = 0xA5;
constexpr uint8_t kBridgeReportSize = 64;
constexpr uint8_t kBridgeHeaderSize = 6;
constexpr uint8_t kBridgeMaxPayload = kBridgeReportSize - kBridgeHeaderSize;

constexpr uint8_t CMD_PING = 0x01;
constexpr uint8_t CMD_GET_INFO = 0x02;
constexpr uint8_t CMD_RESET_MCU = 0x03;

constexpr uint8_t CMD_I2C_INIT_MASTER = 0x20;
constexpr uint8_t CMD_I2C_MASTER_WRITE = 0x21;
constexpr uint8_t CMD_I2C_MASTER_READ = 0x22;
constexpr uint8_t CMD_I2C_INIT_SLAVE = 0x23;
constexpr uint8_t CMD_I2C_SLAVE_SET_TX = 0x24;
constexpr uint8_t CMD_I2C_SLAVE_GET_RX = 0x25;
constexpr uint8_t CMD_I2C_DEINIT = 0x26;
constexpr uint8_t CMD_I2C_MASTER_WRITE_READ = 0x27;
constexpr uint8_t CMD_I2C_MASTER_STAGE_CLEAR = 0x28;
constexpr uint8_t CMD_I2C_MASTER_STAGE_APPEND = 0x29;
constexpr uint8_t CMD_I2C_MASTER_EXEC_STAGE_WRITE = 0x2A;
constexpr uint8_t CMD_I2C_MASTER_EXEC_STAGE_WRITE_READ = 0x2B;
constexpr uint8_t CMD_I2C_MASTER_STAGE_FETCH_RX = 0x2C;
constexpr uint8_t CMD_I2C_MASTER_PMBUS_BLOCK_READ = 0x2D;
constexpr uint8_t CMD_I2C_MASTER_GROUP_WRITE = 0x2E;
constexpr uint8_t CMD_I2C_MASTER_BUS_STATUS = 0x2F;
constexpr uint8_t CMD_I2C_MASTER_SMBUS_QUICK = 0x35;

inline std::wstring StatusText(uint8_t status) {
    switch (status) {
    case 0x00: return L"OK";
    case 0x01: return L"BAD_MAGIC";
    case 0x02: return L"BAD_COMMAND";
    case 0x03: return L"BAD_PAYLOAD";
    case 0x04: return L"IO_ERROR";
    case 0x05: return L"TIMEOUT";
    case 0x06: return L"NOT_READY";
    case 0x07: return L"BUSY";
    case 0x08: return L"UNSUPPORTED";
    default: return L"UNKNOWN";
    }
}

} // namespace mfc_tool::core
