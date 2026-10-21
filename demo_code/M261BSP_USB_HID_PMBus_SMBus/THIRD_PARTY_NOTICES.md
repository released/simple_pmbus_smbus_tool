# Third-party components

The M261 BSP files originate from [OpenNuvoton/M261BSP](https://github.com/OpenNuvoton/M261BSP).
Its upstream README identifies the BSP as Apache-2.0; individual third-party
file headers remain authoritative. Nuvoton and Arm/CMSIS copyright and license
headers are retained. A copy of Apache License 2.0 is provided in
[LICENSE-APACHE-2.0.txt](LICENSE-APACHE-2.0.txt).

The SampleCode application and bootloader projects are modified for this tool's
HID bridge, IAP memory map and checksum/update flow. They are not an unmodified
upstream sample and still require the documented hardware acceptance.

The six precompiled Smartcard/MKROM libraries were checked byte-for-byte against
the corresponding files in the public upstream repository during publication.
They are vendor SDK dependencies, not this project's compiler intermediates;
normal clean must not delete them.

SRecord's srec_cat.exe is not redistributed here. Firmware developers need an
appropriate SRecord installation; the existing checksum scripts accept it from
PATH or their local Keil folder. Keil, Arm compiler licenses and local debugger
options are also developer-environment prerequisites, not uploaded user data.
