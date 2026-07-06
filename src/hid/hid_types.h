#pragma once

#include <cstdint>
#include <string>

namespace mfc_tool::hid {

struct DeviceInfo {
    std::wstring path;
    std::uint16_t vendor_id = 0;
    std::uint16_t product_id = 0;
    std::uint16_t usage_page = 0;
    std::uint16_t usage = 0;
    std::uint16_t input_report_len = 0;
    std::uint16_t output_report_len = 0;
    std::uint16_t feature_report_len = 0;
    std::wstring manufacturer;
    std::wstring product;
    std::wstring serial_number;

    bool IsPreferredBridge() const {
        return usage_page == 0xFF00 && usage == 0x0001;
    }
};

} // namespace mfc_tool::hid
