#include "hid_scan.h"

#include <windows.h>
#include <cfgmgr32.h>
#include <hidsdi.h>
#include <setupapi.h>

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <tuple>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

namespace mfc_tool::hid {
namespace {

struct DevInfoSetDeleter {
    void operator()(HDEVINFO handle) const {
        if (handle != INVALID_HANDLE_VALUE) {
            SetupDiDestroyDeviceInfoList(handle);
        }
    }
};

using DevInfoSetPtr = std::unique_ptr<std::remove_pointer<HDEVINFO>::type, DevInfoSetDeleter>;

std::wstring QueryWideString(HANDLE device, BOOLEAN(__stdcall* query_fn)(HANDLE, PVOID, ULONG)) {
    wchar_t buffer[256] = {};
    if (!query_fn(device, buffer, static_cast<ULONG>(sizeof(buffer)))) {
        return L"";
    }
    return std::wstring(buffer);
}

bool QueryCaps(HANDLE device, DeviceInfo* info) {
    if (info == nullptr) {
        return false;
    }
    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(device, &preparsed) || preparsed == nullptr) {
        return false;
    }

    HIDP_CAPS caps = {};
    NTSTATUS status = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    if (status != HIDP_STATUS_SUCCESS) {
        return false;
    }

    info->usage_page = caps.UsagePage;
    info->usage = caps.Usage;
    info->input_report_len = caps.InputReportByteLength;
    info->output_report_len = caps.OutputReportByteLength;
    info->feature_report_len = caps.FeatureReportByteLength;
    return true;
}

bool QueryAttributes(HANDLE device, DeviceInfo* info) {
    if (info == nullptr) {
        return false;
    }
    HIDD_ATTRIBUTES attributes = {};
    attributes.Size = sizeof(attributes);
    if (!HidD_GetAttributes(device, &attributes)) {
        return false;
    }

    info->vendor_id = attributes.VendorID;
    info->product_id = attributes.ProductID;
    return true;
}

} // namespace

std::vector<DeviceInfo> EnumerateHidDevices(std::uint16_t vid, std::uint16_t pid, bool filter_vid_pid) {
    std::vector<DeviceInfo> result;

    GUID hid_guid = {};
    HidD_GetHidGuid(&hid_guid);

    DevInfoSetPtr dev_info(
        SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)
    );
    if (dev_info.get() == INVALID_HANDLE_VALUE) {
        return result;
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA interface_data = {};
        interface_data.cbSize = sizeof(interface_data);
        if (!SetupDiEnumDeviceInterfaces(dev_info.get(), nullptr, &hid_guid, index, &interface_data)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) {
                break;
            }
            continue;
        }

        DWORD required_bytes = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info.get(), &interface_data, nullptr, 0, &required_bytes, nullptr);
        if (required_bytes == 0) {
            continue;
        }

        std::vector<std::uint8_t> detail_buffer(required_bytes);
        auto* detail_data = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detail_buffer.data());
        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(dev_info.get(), &interface_data, detail_data, required_bytes, nullptr, nullptr)) {
            continue;
        }

        std::wstring path = detail_data->DevicePath;
        HANDLE device = CreateFileW(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (device == INVALID_HANDLE_VALUE) {
            continue;
        }

        DeviceInfo info = {};
        info.path = path;

        bool attr_ok = QueryAttributes(device, &info);
        bool caps_ok = QueryCaps(device, &info);
        info.manufacturer = QueryWideString(device, HidD_GetManufacturerString);
        info.product = QueryWideString(device, HidD_GetProductString);
        info.serial_number = QueryWideString(device, HidD_GetSerialNumberString);

        CloseHandle(device);

        if (!attr_ok || !caps_ok) {
            continue;
        }

        if (filter_vid_pid && (info.vendor_id != vid || info.product_id != pid)) {
            continue;
        }

        result.push_back(std::move(info));
    }

    std::sort(result.begin(), result.end(), [](const DeviceInfo& a, const DeviceInfo& b) {
        return std::tie(
            a.vendor_id,
            a.product_id,
            a.usage_page,
            a.usage,
            a.manufacturer,
            a.product,
            a.path
        ) < std::tie(
            b.vendor_id,
            b.product_id,
            b.usage_page,
            b.usage,
            b.manufacturer,
            b.product,
            b.path
        );
    });

    return result;
}

std::optional<DeviceInfo> SelectPreferredDevice(const std::vector<DeviceInfo>& devices) {
    for (const auto& d : devices) {
        if (d.usage_page == 0xFF00 && d.usage == 0x0001) {
            return d;
        }
    }
    if (!devices.empty()) {
        return devices.front();
    }
    return std::nullopt;
}

} // namespace mfc_tool::hid
