#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "hid_types.h"

namespace mfc_tool::hid {

class BridgeException : public std::runtime_error {
public:
    explicit BridgeException(const std::string& msg) : std::runtime_error(msg) {}
};

class BridgeStatusException : public BridgeException {
public:
    explicit BridgeStatusException(std::uint8_t status);
    [[nodiscard]] std::uint8_t status() const noexcept { return status_; }

private:
    std::uint8_t status_ = 0;
};

class HidBridgeClient {
public:
    HidBridgeClient() = default;
    ~HidBridgeClient();

    HidBridgeClient(const HidBridgeClient&) = delete;
    HidBridgeClient& operator=(const HidBridgeClient&) = delete;

    void Connect(std::uint16_t vid, std::uint16_t pid, const std::wstring& path, int timeout_ms);
    void Disconnect();

    [[nodiscard]] bool IsConnected() const noexcept;
    [[nodiscard]] const DeviceInfo& CurrentDevice() const noexcept { return device_; }

    std::vector<std::uint8_t> Transact(std::uint8_t cmd, const std::vector<std::uint8_t>& payload);

private:
    void OpenHandle(const std::wstring& path);
    void PopulateDeviceMeta();

    std::vector<std::uint8_t> BuildWriteReport(const std::vector<std::uint8_t>& frame) const;
    std::vector<std::uint8_t> ReadFrame(std::uint8_t cmd, std::uint8_t seq, int timeout_ms);

    static std::string LastErrorMessage(const char* prefix);

private:
    void* handle_ = nullptr;
    DeviceInfo device_ = {};
    int timeout_ms_ = 2000;
    std::uint8_t seq_ = 1;
};

} // namespace mfc_tool::hid
