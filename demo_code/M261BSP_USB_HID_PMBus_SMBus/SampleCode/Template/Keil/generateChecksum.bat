@echo off
setlocal ENABLEDELAYEDEXPANSION
pushd "%~dp0"

REM ============================================================
REM M263KIAAE APROM IAP layout
REM   Bootloader : 0x00000000 - 0x0000FFFF
REM   Application: 0x00010000 - 0x0007FFFF
REM   CRC32 word : 0x0007FFFC - 0x0007FFFF
REM ============================================================
set "APP_HEX=.\obj\Template.hex"
set "BOOT_HEX=..\..\ISP_HID\KEIL\obj\ISP_HID.hex"
set "SREC_EXE=.\srec_cat.exe"

set "APP_START=0x00010000"
set "APP_CRC_ADDR=0x0007FFFC"
set "APP_END_EXCL=0x00080000"
set "FLASH_START=0x00000000"
set "OUT_BLK=16"

set "APP_DIR=.\obj"
set "APP_HEX_BAK=%APP_DIR%\Template_backup.hex"
set "APP_HEX_WITH_CRC=%APP_DIR%\Template_CRC.hex"
set "APP_BIN_WITH_CRC=%APP_DIR%\Template.bin"
set "MERGED_HEX=.\boot_app.hex"
set "MERGED_BIN=.\boot_app.bin"

if not exist "%SREC_EXE%" (
  where srec_cat >nul 2>&1
  if errorlevel 1 (
    echo [ERROR] srec_cat.exe not found. Put srec_cat.exe in this Keil folder or add it to PATH.
    set "ERR=1"
    goto fail
  )
  set "SREC_EXE=srec_cat"
)

if not exist "%APP_DIR%" mkdir "%APP_DIR%" >nul 2>&1

if not exist "%APP_HEX%" (
  echo [ERROR] APP hex not found: "%APP_HEX%"
  set "ERR=2"
  goto fail
)

echo.
echo [1/6] Backup original APP hex
"%SREC_EXE%" "%APP_HEX%" -Intel ^
  -crop %APP_START% %APP_END_EXCL% ^
  -o "%APP_HEX_BAK%" -Intel
if errorlevel 1 (
  set "ERR=11"
  goto fail
)

echo.
echo [2/6] Generate APP hex with CRC32 at %APP_CRC_ADDR%
"%SREC_EXE%" "%APP_HEX%" -Intel ^
  -crop %APP_START% %APP_CRC_ADDR% ^
  -fill 0xFF %APP_START% %APP_CRC_ADDR% ^
  -crc32-l-e %APP_CRC_ADDR% ^
  -crop %APP_START% %APP_END_EXCL% ^
  -o "%APP_HEX_WITH_CRC%" -Intel -Output_Block_Size=%OUT_BLK%
if errorlevel 1 (
  set "ERR=12"
  goto fail
)

echo.
echo [3/6] CRC32 word
"%SREC_EXE%" "%APP_HEX_WITH_CRC%" -Intel ^
  -crop %APP_CRC_ADDR% %APP_END_EXCL% ^
  -Output - -HEX_Dump
if errorlevel 1 (
  set "ERR=13"
  goto fail
)

echo.
echo [4/6] Generate padded APP binary with CRC
"%SREC_EXE%" "%APP_HEX_WITH_CRC%" -Intel ^
  -crop %APP_START% %APP_END_EXCL% ^
  -offset -%APP_START% ^
  -o "%APP_BIN_WITH_CRC%" -binary
if errorlevel 1 (
  set "ERR=14"
  goto fail
)

echo.
echo [5/6] Overwrite APP hex with CRC version
"%SREC_EXE%" "%APP_HEX_WITH_CRC%" -Intel ^
  -crop %APP_START% %APP_END_EXCL% ^
  -o "%APP_HEX%" -Intel -Output_Block_Size=%OUT_BLK%
if errorlevel 1 (
  set "ERR=15"
  goto fail
)

echo.
echo [6/6] Merge bootloader and APP when bootloader hex exists
if exist "%BOOT_HEX%" (
  "%SREC_EXE%" "%BOOT_HEX%" -Intel ^
    "%APP_HEX%" -Intel ^
    -o "%MERGED_HEX%" -Intel -Output_Block_Size=%OUT_BLK%
  if errorlevel 1 (
    set "ERR=16"
    goto fail
  )

  "%SREC_EXE%" "%MERGED_HEX%" -Intel ^
    -fill 0xFF %FLASH_START% %APP_END_EXCL% ^
    -crop %FLASH_START% %APP_END_EXCL% ^
    -o "%MERGED_BIN%" -binary
  if errorlevel 1 (
    set "ERR=17"
    goto fail
  )
) else (
  echo [WARN] Bootloader hex not found: "%BOOT_HEX%"
  echo [WARN] Skipped boot_app.hex and boot_app.bin generation.
)

echo.
echo Done.
echo APP backup : "%APP_HEX_BAK%"
echo APP hex    : "%APP_HEX%"
echo APP bin    : "%APP_BIN_WITH_CRC%"
if exist "%MERGED_HEX%" echo MERGED hex : "%MERGED_HEX%"
if exist "%MERGED_BIN%" echo MERGED bin : "%MERGED_BIN%"
popd
exit /b 0

:fail
echo [ERROR] generateChecksum.bat failed, code %ERR%.
popd
exit /b %ERR%
