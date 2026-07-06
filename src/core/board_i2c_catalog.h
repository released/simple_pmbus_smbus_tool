#pragma once

#include <string>
#include <vector>

namespace mfc_tool::core::board_i2c {

enum BoardPortCode {
    kPortA = 0,
    kPortB = 1,
    kPortC = 2,
    kPortF = 5,
};

constexpr int MakePin(BoardPortCode port, int pin) {
    return (static_cast<int>(port) << 4) | (pin & 0x0F);
}

struct PinPair {
    int i2c_port;
    int sda_pin;
    int scl_pin;
    const wchar_t* label;
    const wchar_t* ini_name;
};

inline const std::vector<PinPair>& AllPinPairs() {
    static const std::vector<PinPair> pairs = {
        {0, MakePin(kPortB, 4),  MakePin(kPortB, 5),  L"I2C0  PB.4=SDA / PB.5=SCL",   L"I2C0_PB4_PB5"},
        {0, MakePin(kPortF, 2),  MakePin(kPortF, 3),  L"I2C0  PF.2=SDA / PF.3=SCL",   L"I2C0_PF2_PF3"},
        {0, MakePin(kPortA, 4),  MakePin(kPortA, 5),  L"I2C0  PA.4=SDA / PA.5=SCL",   L"I2C0_PA4_PA5"},
        {0, MakePin(kPortC, 0),  MakePin(kPortC, 1),  L"I2C0  PC.0=SDA / PC.1=SCL",   L"I2C0_PC0_PC1"},
        {1, MakePin(kPortB, 2),  MakePin(kPortB, 3),  L"I2C1  PB.2=SDA / PB.3=SCL",   L"I2C1_PB2_PB3"},
        {1, MakePin(kPortB, 0),  MakePin(kPortB, 1),  L"I2C1  PB.0=SDA / PB.1=SCL",   L"I2C1_PB0_PB1"},
        {1, MakePin(kPortA, 6),  MakePin(kPortA, 7),  L"I2C1  PA.6=SDA / PA.7=SCL",   L"I2C1_PA6_PA7"},
        {1, MakePin(kPortA, 2),  MakePin(kPortA, 3),  L"I2C1  PA.2=SDA / PA.3=SCL",   L"I2C1_PA2_PA3"},
        {1, MakePin(kPortF, 1),  MakePin(kPortF, 0),  L"I2C1  PF.1=SDA / PF.0=SCL",   L"I2C1_PF1_PF0"},
        {1, MakePin(kPortC, 4),  MakePin(kPortC, 5),  L"I2C1  PC.4=SDA / PC.5=SCL",   L"I2C1_PC4_PC5"},
        {1, MakePin(kPortB, 10), MakePin(kPortB, 11), L"I2C1  PB.10=SDA / PB.11=SCL", L"I2C1_PB10_PB11"},
    };
    return pairs;
}

inline const PinPair* DefaultPinPair(int i2c_port) {
    for (const auto& pair : AllPinPairs()) {
        if (pair.i2c_port == i2c_port) {
            return &pair;
        }
    }
    return nullptr;
}

inline const PinPair* FindPinPairByIniName(const std::wstring& ini_name) {
    for (const auto& pair : AllPinPairs()) {
        if (ini_name == pair.ini_name) {
            return &pair;
        }
    }
    return nullptr;
}

inline const PinPair* FindPinPair(int i2c_port, int sda_pin, int scl_pin) {
    for (const auto& pair : AllPinPairs()) {
        if (pair.i2c_port == i2c_port && pair.sda_pin == sda_pin && pair.scl_pin == scl_pin) {
            return &pair;
        }
    }
    return nullptr;
}

inline std::wstring PortLabel(int i2c_port) {
    return i2c_port == 1 ? L"I2C1" : L"I2C0";
}

inline std::wstring PinCodeText(int pin_code) {
    const int port = (pin_code >> 4) & 0x0F;
    const int pin = pin_code & 0x0F;
    const wchar_t* port_name = L"P?";

    switch (port) {
    case kPortA:
        port_name = L"PA";
        break;
    case kPortB:
        port_name = L"PB";
        break;
    case kPortC:
        port_name = L"PC";
        break;
    case kPortF:
        port_name = L"PF";
        break;
    default:
        break;
    }

    return std::wstring(port_name) + L"." + std::to_wstring(pin);
}

} // namespace mfc_tool::core::board_i2c
