#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "hid_types.h"

namespace mfc_tool::hid {

std::vector<DeviceInfo> EnumerateHidDevices(std::uint16_t vid, std::uint16_t pid, bool filter_vid_pid);
std::optional<DeviceInfo> SelectPreferredDevice(const std::vector<DeviceInfo>& devices);

} // namespace mfc_tool::hid
