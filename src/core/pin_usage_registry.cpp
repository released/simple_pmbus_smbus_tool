#include "pin_usage_registry.h"

#include <algorithm>
#include <sstream>

#include "board_i2c_catalog.h"

namespace mfc_tool::core {
namespace {

std::wstring JoinPins(const std::set<int>& pins) {
    if (pins.empty()) {
        return L"-";
    }
    std::wstringstream ss;
    bool first = true;
    for (int pin : pins) {
        if (!first) {
            ss << L", ";
        }
        first = false;
        ss << board_i2c::PinCodeText(pin);
    }
    return ss.str();
}

std::wstring JoinOwners(const std::vector<std::wstring>& owners) {
    std::wstringstream ss;
    for (size_t i = 0; i < owners.size(); ++i) {
        if (i > 0) {
            ss << L", ";
        }
        ss << owners[i];
    }
    return ss.str();
}

} // namespace

std::set<int> PinUsageRegistry::NormalizePins(const std::vector<int>& pins) {
    std::set<int> out;
    for (int p : pins) {
        if (p >= 0 && p <= board_i2c::MakePin(board_i2c::kPortF, 15)) {
            out.insert(p);
        }
    }
    return out;
}

void PinUsageRegistry::SetClaim(const std::wstring& owner, const std::vector<int>& pins) {
    std::set<int> normalized = NormalizePins(pins);
    if (normalized.empty()) {
        claims_.erase(owner);
    } else {
        claims_[owner] = std::move(normalized);
    }
    if (active_.find(owner) == active_.end()) {
        active_[owner] = false;
    }
}

void PinUsageRegistry::Release(const std::wstring& owner) {
    claims_.erase(owner);
    active_[owner] = false;
}

void PinUsageRegistry::SetActive(const std::wstring& owner, bool active) {
    active_[owner] = active;
}

bool PinUsageRegistry::IsActive(const std::wstring& owner) const {
    auto it = active_.find(owner);
    return it != active_.end() ? it->second : false;
}

bool PinUsageRegistry::AnyActive(std::initializer_list<std::wstring> owners) const {
    for (const auto& owner : owners) {
        if (IsActive(owner)) {
            return true;
        }
    }
    return false;
}

bool PinUsageRegistry::AnyActive(const std::vector<std::wstring>& owners) const {
    for (const auto& owner : owners) {
        if (IsActive(owner)) {
            return true;
        }
    }
    return false;
}

bool PinUsageRegistry::AnyActiveExcept(const std::vector<std::wstring>& owners,
                                       const std::set<std::wstring>& exclude_owners) const {
    for (const auto& owner : owners) {
        if (exclude_owners.find(owner) != exclude_owners.end()) {
            continue;
        }
        if (IsActive(owner)) {
            return true;
        }
    }
    return false;
}

void PinUsageRegistry::SetLabel(const std::wstring& owner, const std::wstring& label) {
    labels_[owner] = label;
}

std::wstring PinUsageRegistry::OwnerLabel(const std::wstring& owner) const {
    auto it = labels_.find(owner);
    if (it == labels_.end()) {
        return owner;
    }
    return it->second;
}

std::set<int> PinUsageRegistry::Occupied(bool active_only, const std::set<std::wstring>& exclude_owners) const {
    std::set<int> out;
    for (const auto& [owner, pins] : claims_) {
        if (exclude_owners.find(owner) != exclude_owners.end()) {
            continue;
        }
        if (active_only && !IsActive(owner)) {
            continue;
        }
        out.insert(pins.begin(), pins.end());
    }
    return out;
}

bool PinUsageRegistry::AnyPinOccupied(const std::vector<int>& pins, const std::set<std::wstring>& exclude_owners) const {
    const std::set<int> occupied = Occupied(true, exclude_owners);
    for (int pin : pins) {
        if (occupied.find(pin) != occupied.end()) {
            return true;
        }
    }
    return false;
}

std::map<int, std::vector<std::wstring>> PinUsageRegistry::OwnerMap(bool active_only) const {
    std::map<int, std::vector<std::wstring>> out;
    for (const auto& [owner, pins] : claims_) {
        if (active_only && !IsActive(owner)) {
            continue;
        }
        for (int pin : pins) {
            out[pin].push_back(owner);
        }
    }
    for (auto& [pin, owners] : out) {
        (void)pin;
        std::sort(owners.begin(), owners.end());
    }
    return out;
}

std::map<std::wstring, std::set<int>> PinUsageRegistry::ClaimsByOwner(bool active_only) const {
    std::map<std::wstring, std::set<int>> out;
    for (const auto& [owner, pins] : claims_) {
        if (active_only && !IsActive(owner)) {
            continue;
        }
        out[owner] = pins;
    }
    return out;
}

std::vector<OwnerRow> PinUsageRegistry::OwnerRows() const {
    std::set<std::wstring> owners;
    for (const auto& [owner, _] : claims_) { (void)_; owners.insert(owner); }
    for (const auto& [owner, _] : active_) { (void)_; owners.insert(owner); }
    for (const auto& [owner, _] : labels_) { (void)_; owners.insert(owner); }

    std::vector<OwnerRow> out;
    for (const auto& owner : owners) {
        auto it = claims_.find(owner);
        std::set<int> pins = it == claims_.end() ? std::set<int>{} : it->second;
        OwnerRow row;
        row.owner = owner;
        row.label = OwnerLabel(owner);
        row.active = IsActive(owner) ? L"ON" : L"OFF";
        row.pins = (IsActive(owner) && !pins.empty()) ? JoinPins(pins) : L"-";
        out.push_back(std::move(row));
    }
    return out;
}

std::vector<PinRow> PinUsageRegistry::PinRows() const {
    std::vector<PinRow> out;
    auto owner_map = OwnerMap(true);
    for (const auto& [pin, owners] : owner_map) {
        std::vector<std::wstring> labels;
        labels.reserve(owners.size());
        for (const auto& owner : owners) {
            labels.push_back(OwnerLabel(owner));
        }
        PinRow row;
        row.pin = board_i2c::PinCodeText(pin);
        row.owners = JoinOwners(labels);
        row.conflict = owners.size() > 1 ? L"YES" : L"";
        out.push_back(std::move(row));
    }
    return out;
}

} // namespace mfc_tool::core
