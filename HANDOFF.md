# HANDOFF.md

## Current State

The PC GUI builds as `build\PmbusSmbusHidTool.exe` and exposes `PMBus`, `SMBus`, `I2C`, and `Script` tabs with M032 EVB I2C port and pin-pair selection where bus ownership is needed.

Firmware upload was split out to the sibling `simple_programming_tool` workspace. This workspace should stay focused on PMBus/SMBus validation and generic I2C bridge testing.

## Implemented

- HID defaults: VID `0x0416`, PID `0x5020`.
- PMBus/SMBus GUI scope is master mode only.
- I2C port selection supports I2C0/I2C1 and M032 pin-pair dropdowns.
- I2C tab supports master write/read/write-read and simple slave TX/RX validation.
- Script editing is centralized in the `Script` tab using the M032 CSV format.
- PMBus and SMBus tabs have script `Load`/`Run`, row checkboxes, select-all, response panels, progress, repeat controls, and stop/cancel behavior.
- M032 bridge firmware supports the PMBus/SMBus/I2C HID command protocol and reports `m032-pmbus-smbus-bridge/1.1.3`.
- The FW Upload tab, Nuvoton I2C ISP PC helper, and M031 ISP target samples were moved to `simple_programming_tool`.
- The PMBus/SMBus bridge firmware no longer accepts the FW Upload final-byte NACK staged-write exception.

## Important Files

- PC entry: `src\main.cpp`, `src\app.cpp`, `src\main_frame.cpp`
- PMBus tab: `src\ui\pmbus_tab.cpp`
- SMBus tab: `src\ui\smbus_tab.cpp`
- I2C tab: `src\ui\i2c_tab.cpp`
- Script tab: `src\ui\script_tab.cpp`
- Script parser/serializer: `src\core\smbus_script.cpp`
- I2C pin catalog: `src\core\board_i2c_catalog.h`
- HID command constants: `src\core\bridge_commands.h`
- Firmware HID dispatcher: `demo_code\M031BSP_USB_HID_PMBus_SMBus\SampleCode\Template\hid_tool_api.c`
- Firmware I2C bridge: `demo_code\M031BSP_USB_HID_PMBus_SMBus\SampleCode\Template\m031_bridge_i2c.c`
- Firmware version: `demo_code\M031BSP_USB_HID_PMBus_SMBus\SampleCode\Template\bridge_version.h`

## Verification Done

- PC Release x64 build after FW Upload removal and README inventory update: PASS, build version `1.0.0.247`, 0 warnings, 0 errors.
- PMBus contract validation: PASS.
- Firmware ARM GCC syntax-only check for `m031_bridge_i2c.c` and `hid_tool_api.c`: PASS.
- Keil full M032 bridge firmware build: NOT RUN, Keil `UV4`/`armclang` command line is not available in this shell.
- Hardware PMBus/SMBus/I2C validation: use the documented button/script/manual smoke tests.

## Next Recommended Work

1. Build and flash the M032 bridge firmware version `1.1.3`.
2. Launch `build\PmbusSmbusHidTool.exe`, connect HID, and verify `Get Info` reports `m032-pmbus-smbus-bridge/1.1.3`.
3. Re-run PMBus Base, M-CRPS, TI UCD90xxx, SMBus Generic, SMBus UBM button/script smoke tests.
4. Re-run generic I2C master/slave smoke tests.
5. Use `simple_programming_tool` for Nuvoton target firmware upload work.
