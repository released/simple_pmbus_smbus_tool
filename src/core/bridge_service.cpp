#include "bridge_service.h"

#include <algorithm>

namespace mfc_tool::core {
namespace {

constexpr std::size_t kI2cStageChunkMax = 56u;
constexpr std::size_t kI2cShortWriteMax = 53u;
constexpr int kI2cShortReadMax = 57;

void AppendLe16(std::vector<std::uint8_t>* out, std::uint16_t v) {
    out->push_back(static_cast<std::uint8_t>(v & 0xFF));
    out->push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void AppendLe32(std::vector<std::uint8_t>* out, std::uint32_t v) {
    out->push_back(static_cast<std::uint8_t>(v & 0xFF));
    out->push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out->push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out->push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

} // namespace

std::vector<mfc_tool::hid::DeviceInfo> BridgeService::ScanDevices(std::uint16_t vid, std::uint16_t pid) const {
    return mfc_tool::hid::EnumerateHidDevices(vid, pid, true);
}

void BridgeService::Connect(std::uint16_t vid, std::uint16_t pid, const std::wstring& path, int timeout_ms) {
    client_.Connect(vid, pid, path, timeout_ms);
}

void BridgeService::Disconnect() {
    client_.Disconnect();
}

bool BridgeService::IsConnected() const noexcept {
    return client_.IsConnected();
}

std::vector<std::uint8_t> BridgeService::Tx(std::uint8_t cmd, const std::vector<std::uint8_t>& payload) {
    return client_.Transact(cmd, payload);
}

std::vector<std::uint8_t> BridgeService::Ping(const std::vector<std::uint8_t>& data) { return Tx(CMD_PING, data); }
std::vector<std::uint8_t> BridgeService::GetInfo() { return Tx(CMD_GET_INFO, {}); }
std::vector<std::uint8_t> BridgeService::ResetMcu() { return Tx(CMD_RESET_MCU, {}); }

std::vector<std::uint8_t> BridgeService::I2cMasterInit(int port, int sda_pin, int scl_pin, int baud) {
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(port));
    p.push_back(static_cast<std::uint8_t>(sda_pin));
    p.push_back(static_cast<std::uint8_t>(scl_pin));
    AppendLe32(&p, static_cast<std::uint32_t>(baud));
    return Tx(CMD_I2C_INIT_MASTER, p);
}

std::vector<std::uint8_t> BridgeService::I2cMasterBusStatus(int port, bool recover_if_needed) {
    return Tx(CMD_I2C_MASTER_BUS_STATUS, {
        static_cast<std::uint8_t>(port),
        recover_if_needed ? 1u : 0u
    });
}

std::vector<std::uint8_t> BridgeService::I2cMasterWrite(int port, int addr7, const std::vector<std::uint8_t>& data) {
    if (data.size() > 255u || data.size() > kI2cShortWriteMax) {
        I2cMasterStageClear(port);
        if (!data.empty()) {
            I2cMasterStageAppend(port, data.data(), data.size());
        }
        return I2cMasterExecStageWrite(port, addr7);
    }
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(port));
    p.push_back(static_cast<std::uint8_t>(addr7 & 0x7F));
    p.push_back(static_cast<std::uint8_t>(std::min<std::size_t>(data.size(), 255)));
    p.insert(p.end(), data.begin(), data.begin() + std::min<std::size_t>(data.size(), 255));
    return Tx(CMD_I2C_MASTER_WRITE, p);
}

std::vector<std::uint8_t> BridgeService::I2cMasterRead(int port, int addr7, int length) {
    return Tx(CMD_I2C_MASTER_READ, {
        static_cast<std::uint8_t>(port),
        static_cast<std::uint8_t>(addr7 & 0x7F),
        static_cast<std::uint8_t>(std::clamp(length, 0, 255))
    });
}

std::vector<std::uint8_t> BridgeService::I2cMasterWriteThenReadRaw(int port, int addr7, bool repeated_start,
                                                                   const std::vector<std::uint8_t>& write_data, int read_length) {
    if (write_data.size() <= kI2cShortWriteMax && read_length <= kI2cShortReadMax) {
        const std::size_t write_n = std::min<std::size_t>(write_data.size(), kI2cShortWriteMax);
        std::vector<std::uint8_t> p;
        std::vector<std::uint8_t> rx;
        int count = 0;

        p.push_back(static_cast<std::uint8_t>(port));
        p.push_back(static_cast<std::uint8_t>(addr7 & 0x7F));
        p.push_back(repeated_start ? 1u : 0u);
        p.push_back(static_cast<std::uint8_t>(write_n));
        p.push_back(static_cast<std::uint8_t>(std::clamp(read_length, 0, kI2cShortReadMax)));
        p.insert(p.end(), write_data.begin(), write_data.begin() + write_n);
        rx = Tx(CMD_I2C_MASTER_WRITE_READ, p);
        count = rx.empty() ? 0 : static_cast<int>(rx[0]);
        if (count <= 0) {
            return {};
        }
        return std::vector<std::uint8_t>(rx.begin() + 1,
                                         rx.begin() + 1 + (std::min)(static_cast<std::size_t>(count),
                                                                      rx.size() > 1 ? rx.size() - 1 : static_cast<std::size_t>(0)));
    }

    I2cMasterStageClear(port);
    if (!write_data.empty()) {
        I2cMasterStageAppend(port, write_data.data(), write_data.size());
    }

    {
        std::vector<std::uint8_t> p;
        std::vector<std::uint8_t> exec_rx;
        std::vector<std::uint8_t> out;
        int total_len = 0;
        int fetched = 0;

        p.push_back(static_cast<std::uint8_t>(port));
        p.push_back(static_cast<std::uint8_t>(addr7 & 0x7F));
        p.push_back(repeated_start ? 1u : 0u);
        AppendLe16(&p, static_cast<std::uint16_t>(std::clamp(read_length, 0, 0xFFFF)));
        exec_rx = Tx(CMD_I2C_MASTER_EXEC_STAGE_WRITE_READ, p);
        if (exec_rx.size() < 2u) {
            throw mfc_tool::hid::BridgeException("invalid staged I2C write-read response");
        }
        total_len = static_cast<int>(exec_rx[0] | (exec_rx[1] << 8));
        out.reserve(static_cast<std::size_t>(total_len));
        while (fetched < total_len) {
            const int want = (std::min)(total_len - fetched, static_cast<int>(kBridgeMaxPayload - 3));
            std::vector<std::uint8_t> chunk = I2cMasterStageFetchRx(port, fetched, want);
            if (chunk.size() < 3u) {
                throw mfc_tool::hid::BridgeException("invalid staged I2C fetch response");
            }
            const int reported_total = static_cast<int>(chunk[0] | (chunk[1] << 8));
            const int count = static_cast<int>(chunk[2]);
            if (reported_total != total_len) {
                throw mfc_tool::hid::BridgeException("staged I2C fetch length mismatch");
            }
            if (count <= 0 || static_cast<std::size_t>(count + 3) > chunk.size()) {
                throw mfc_tool::hid::BridgeException("staged I2C fetch returned no data");
            }
            out.insert(out.end(), chunk.begin() + 3, chunk.begin() + 3 + count);
            fetched += count;
        }
        return out;
    }
}

std::vector<std::uint8_t> BridgeService::I2cMasterWriteThenRead(int port, int addr7, bool repeated_start,
                                                                 const std::vector<std::uint8_t>& write_data, int read_length) {
    std::vector<std::uint8_t> raw = I2cMasterWriteThenReadRaw(port, addr7, repeated_start, write_data, read_length);
    if (raw.size() > 255u) {
        throw mfc_tool::hid::BridgeException("I2C read length exceeds legacy count-prefixed response");
    }
    std::vector<std::uint8_t> out;
    out.reserve(raw.size() + 1u);
    out.push_back(static_cast<std::uint8_t>(raw.size()));
    out.insert(out.end(), raw.begin(), raw.end());
    return out;
}

std::vector<std::uint8_t> BridgeService::I2cMasterPmbusBlockReadRaw(int port, int addr7,
                                                                    const std::vector<std::uint8_t>& write_data,
                                                                    bool expect_pec, int max_block_length) {
    std::vector<std::uint8_t> p;
    const std::size_t write_n = (std::min)(write_data.size(), static_cast<std::size_t>(255));

    if (write_n == 0u) {
        throw mfc_tool::hid::BridgeException("PMBus block read requires at least one command byte");
    }

    p.push_back(static_cast<std::uint8_t>(port));
    p.push_back(static_cast<std::uint8_t>(addr7 & 0x7F));
    p.push_back(static_cast<std::uint8_t>(write_n));
    p.push_back(expect_pec ? 1u : 0u);
    p.push_back(static_cast<std::uint8_t>(std::clamp(max_block_length, 1, static_cast<int>(kBridgeMaxPayload - (expect_pec ? 2 : 1)))));
    p.insert(p.end(), write_data.begin(), write_data.begin() + write_n);
    return Tx(CMD_I2C_MASTER_PMBUS_BLOCK_READ, p);
}

std::vector<std::uint8_t> BridgeService::I2cMasterGroupWrite(int port,
                                                             const std::vector<std::uint8_t>& segment_blob,
                                                             int segment_count) {
    if (segment_count <= 0 || segment_count > 255) {
        throw mfc_tool::hid::BridgeException("invalid I2C group-write segment count");
    }
    if (segment_blob.empty()) {
        throw mfc_tool::hid::BridgeException("I2C group-write requires at least one segment");
    }

    I2cMasterStageClear(port);
    I2cMasterStageAppend(port, segment_blob.data(), segment_blob.size());
    return I2cMasterExecGroupWrite(port, segment_count);
}

std::vector<std::uint8_t> BridgeService::I2cMasterSmbusQuick(int port, int addr7, bool read_bit) {
    return Tx(CMD_I2C_MASTER_SMBUS_QUICK, {
        static_cast<std::uint8_t>(port),
        static_cast<std::uint8_t>(addr7 & 0x7F),
        read_bit ? 1u : 0u
    });
}

std::vector<std::uint8_t> BridgeService::I2cMasterStageFetchRx(int port, int offset, int length) {
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(port));
    AppendLe16(&p, static_cast<std::uint16_t>(std::clamp(offset, 0, 0xFFFF)));
    p.push_back(static_cast<std::uint8_t>(std::clamp(length, 0, static_cast<int>(kBridgeMaxPayload - 3))));
    return Tx(CMD_I2C_MASTER_STAGE_FETCH_RX, p);
}

void BridgeService::I2cMasterStageClear(int port) {
    Tx(CMD_I2C_MASTER_STAGE_CLEAR, {static_cast<std::uint8_t>(port)});
}

void BridgeService::I2cMasterStageAppend(int port, const std::uint8_t* data, std::size_t length) {
    std::size_t offset = 0u;
    while (offset < length) {
        const std::size_t chunk_n = (std::min)(length - offset, kI2cStageChunkMax);
        std::vector<std::uint8_t> p;
        p.push_back(static_cast<std::uint8_t>(port));
        p.push_back(static_cast<std::uint8_t>(chunk_n & 0xFFu));
        p.insert(p.end(), data + offset, data + offset + chunk_n);
        Tx(CMD_I2C_MASTER_STAGE_APPEND, p);
        offset += chunk_n;
    }
}

std::vector<std::uint8_t> BridgeService::I2cMasterExecStageWrite(int port, int addr7) {
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(port));
    p.push_back(static_cast<std::uint8_t>(addr7 & 0x7F));
    return Tx(CMD_I2C_MASTER_EXEC_STAGE_WRITE, p);
}

std::vector<std::uint8_t> BridgeService::I2cMasterExecStageWriteRead(int port, int addr7, bool repeated_start, int read_length) {
    std::vector<std::uint8_t> p;
    std::vector<std::uint8_t> exec_rx;
    std::vector<std::uint8_t> raw;
    int total_len = 0;
    int fetched = 0;

    p.push_back(static_cast<std::uint8_t>(port));
    p.push_back(static_cast<std::uint8_t>(addr7 & 0x7F));
    p.push_back(repeated_start ? 1u : 0u);
    AppendLe16(&p, static_cast<std::uint16_t>(std::clamp(read_length, 0, 0xFFFF)));
    exec_rx = Tx(CMD_I2C_MASTER_EXEC_STAGE_WRITE_READ, p);
    if (exec_rx.size() < 2u) {
        throw mfc_tool::hid::BridgeException("invalid staged I2C write-read response");
    }
    total_len = static_cast<int>(exec_rx[0] | (exec_rx[1] << 8));
    raw.reserve(static_cast<std::size_t>(total_len));
    while (fetched < total_len) {
        const int want = (std::min)(total_len - fetched, static_cast<int>(kBridgeMaxPayload - 3));
        std::vector<std::uint8_t> chunk = I2cMasterStageFetchRx(port, fetched, want);
        if (chunk.size() < 3u) {
            throw mfc_tool::hid::BridgeException("invalid staged I2C fetch response");
        }
        const int reported_total = static_cast<int>(chunk[0] | (chunk[1] << 8));
        const int count = static_cast<int>(chunk[2]);
        if (reported_total != total_len) {
            throw mfc_tool::hid::BridgeException("staged I2C fetch length mismatch");
        }
        if (count <= 0 || static_cast<std::size_t>(count + 3) > chunk.size()) {
            throw mfc_tool::hid::BridgeException("staged I2C fetch returned no data");
        }
        raw.insert(raw.end(), chunk.begin() + 3, chunk.begin() + 3 + count);
        fetched += count;
    }
    if (raw.size() > 255u) {
        throw mfc_tool::hid::BridgeException("I2C staged read length exceeds legacy count-prefixed response");
    }
    std::vector<std::uint8_t> out;
    out.reserve(raw.size() + 1u);
    out.push_back(static_cast<std::uint8_t>(raw.size()));
    out.insert(out.end(), raw.begin(), raw.end());
    return out;
}

std::vector<std::uint8_t> BridgeService::I2cMasterExecGroupWrite(int port, int segment_count) {
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(port));
    p.push_back(static_cast<std::uint8_t>(segment_count & 0xFF));
    return Tx(CMD_I2C_MASTER_GROUP_WRITE, p);
}

std::vector<std::uint8_t> BridgeService::I2cSlaveInit(int port, int sda_pin, int scl_pin, int addr7, int baud) {
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(port));
    p.push_back(static_cast<std::uint8_t>(sda_pin));
    p.push_back(static_cast<std::uint8_t>(scl_pin));
    p.push_back(static_cast<std::uint8_t>(addr7 & 0x7F));
    AppendLe32(&p, static_cast<std::uint32_t>(baud));
    return Tx(CMD_I2C_INIT_SLAVE, p);
}

std::vector<std::uint8_t> BridgeService::I2cSlaveSetTx(int port, const std::vector<std::uint8_t>& data) {
    const std::size_t write_n = std::min<std::size_t>(data.size(), kBridgeMaxPayload - 2u);
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(port));
    p.push_back(static_cast<std::uint8_t>(write_n));
    p.insert(p.end(), data.begin(), data.begin() + write_n);
    return Tx(CMD_I2C_SLAVE_SET_TX, p);
}

std::vector<std::uint8_t> BridgeService::I2cSlaveGetRx(int port, int max_len) {
    return Tx(CMD_I2C_SLAVE_GET_RX, {
        static_cast<std::uint8_t>(port),
        static_cast<std::uint8_t>(std::clamp(max_len, 0, static_cast<int>(kBridgeMaxPayload - 1u)))
    });
}

std::vector<std::uint8_t> BridgeService::I2cDeinit(int port) {
    return Tx(CMD_I2C_DEINIT, {static_cast<std::uint8_t>(port)});
}

} // namespace mfc_tool::core
