#pragma once

#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mfc_tool::core {

struct OwnerRow {
    std::wstring owner;
    std::wstring label;
    std::wstring active;
    std::wstring pins;
};

struct PinRow {
    std::wstring pin;
    std::wstring owners;
    std::wstring conflict;
};

class PinUsageRegistry {
public:
    void SetClaim(const std::wstring& owner, const std::vector<int>& pins);
    void Release(const std::wstring& owner);

    void SetActive(const std::wstring& owner, bool active);
    [[nodiscard]] bool IsActive(const std::wstring& owner) const;
    [[nodiscard]] bool AnyActive(std::initializer_list<std::wstring> owners) const;
    [[nodiscard]] bool AnyActive(const std::vector<std::wstring>& owners) const;
    [[nodiscard]] bool AnyActiveExcept(const std::vector<std::wstring>& owners,
                                       const std::set<std::wstring>& exclude_owners) const;

    void SetLabel(const std::wstring& owner, const std::wstring& label);
    [[nodiscard]] std::wstring OwnerLabel(const std::wstring& owner) const;

    [[nodiscard]] std::set<int> Occupied(bool active_only, const std::set<std::wstring>& exclude_owners = {}) const;
    [[nodiscard]] bool AnyPinOccupied(const std::vector<int>& pins, const std::set<std::wstring>& exclude_owners = {}) const;
    [[nodiscard]] std::map<int, std::vector<std::wstring>> OwnerMap(bool active_only) const;
    [[nodiscard]] std::map<std::wstring, std::set<int>> ClaimsByOwner(bool active_only) const;

    [[nodiscard]] std::vector<OwnerRow> OwnerRows() const;
    [[nodiscard]] std::vector<PinRow> PinRows() const;

private:
    static std::set<int> NormalizePins(const std::vector<int>& pins);

private:
    std::map<std::wstring, std::set<int>> claims_;
    std::map<std::wstring, bool> active_;
    std::map<std::wstring, std::wstring> labels_;
};

} // namespace mfc_tool::core
