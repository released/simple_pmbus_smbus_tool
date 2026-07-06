#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "bridge_commands.h"
#include "../hid/hid_bridge_client.h"
#include "../hid/hid_scan.h"

namespace mfc_tool::core {

class BridgeService {
public:
    BridgeService() = default;

    std::vector<mfc_tool::hid::DeviceInfo> ScanDevices(std::uint16_t vid, std::uint16_t pid) const;

    void Connect(std::uint16_t vid, std::uint16_t pid, const std::wstring& path, int timeout_ms);
    void Disconnect();
    [[nodiscard]] bool IsConnected() const noexcept;

    std::vector<std::uint8_t> Ping(const std::vector<std::uint8_t>& data);
    std::vector<std::uint8_t> GetInfo();
    std::vector<std::uint8_t> ResetMcu();

    std::vector<std::uint8_t> I2cMasterInit(int port, int sda_pin, int scl_pin, int baud);
    std::vector<std::uint8_t> I2cMasterBusStatus(int port, bool recover_if_needed);
    std::vector<std::uint8_t> I2cMasterWrite(int port, int addr7, const std::vector<std::uint8_t>& data);
    std::vector<std::uint8_t> I2cMasterRead(int port, int addr7, int length);
    std::vector<std::uint8_t> I2cMasterWriteThenReadRaw(int port, int addr7, bool repeated_start,
                                                        const std::vector<std::uint8_t>& write_data, int read_length);
    std::vector<std::uint8_t> I2cMasterPmbusBlockReadRaw(int port, int addr7,
                                                         const std::vector<std::uint8_t>& write_data,
                                                         bool expect_pec, int max_block_length);
    std::vector<std::uint8_t> I2cMasterGroupWrite(int port, const std::vector<std::uint8_t>& segment_blob, int segment_count);
    std::vector<std::uint8_t> I2cMasterSmbusQuick(int port, int addr7, bool read_bit);
    std::vector<std::uint8_t> I2cMasterWriteThenRead(int port, int addr7, bool repeated_start,
                                                     const std::vector<std::uint8_t>& write_data, int read_length);
    std::vector<std::uint8_t> I2cSlaveInit(int port, int sda_pin, int scl_pin, int addr7, int baud);
    std::vector<std::uint8_t> I2cSlaveSetTx(int port, const std::vector<std::uint8_t>& data);
    std::vector<std::uint8_t> I2cSlaveGetRx(int port, int max_len);
    std::vector<std::uint8_t> I2cDeinit(int port);

private:
    std::vector<std::uint8_t> Tx(std::uint8_t cmd, const std::vector<std::uint8_t>& payload);
    std::vector<std::uint8_t> I2cMasterStageFetchRx(int port, int offset, int length);
    void I2cMasterStageClear(int port);
    void I2cMasterStageAppend(int port, const std::uint8_t* data, std::size_t length);
    std::vector<std::uint8_t> I2cMasterExecStageWrite(int port, int addr7);
    std::vector<std::uint8_t> I2cMasterExecStageWriteRead(int port, int addr7, bool repeated_start, int read_length);
    std::vector<std::uint8_t> I2cMasterExecGroupWrite(int port, int segment_count);

private:
    mfc_tool::hid::HidBridgeClient client_;
};

} // namespace mfc_tool::core
