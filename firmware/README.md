# Kung Fu Flash Firmware
The initial firmware installation can be done via the SWD interface (J4) using a ST-Link V2 programmer or via the USB port.
The Kung Fu Flash cartridge should not be connected to the Commodore 64 before this process has been completed.

## SWD (Recommended)
Connect the ST-Link V2 programmer to J4 (3.3V, GND, SWDIO, and SWCLK) and connect BOOT0 to 3.3V.

Rename the `KungFuFlash_v1.xx.upd` file to `KungFuFlash_v1.xx.bin` and use the STM32 ST-LINK Utility from ST to program the firmware.

All wires to J4 should be disconnected once the firmware has been successfully installed.

## USB
Add a jumper to short J1 and a jumper to short BOOT0 and +3.3V on J4. This will power the board from USB and ensure that it is started in DFU mode.

Use the DfuSE software from ST or [dfu-util](http://dfu-util.sourceforge.net/) to install the firmware:

`dfu-util -a 0 -s 0x08000000 -D KungFuFlash_v1.xx.upd`

Both jumpers should be removed after the firmware has been installed.

## Diagnostic
Introduced in firmware v1.27

The diagnostic tool is started by pressing the menu button for 2 seconds (until the LED turns off) and is intended to help debug stability problems on some C64 models.
The tool can also be started in a USB only mode if Kung Fu Flash refuses to start at all. In this mode debug data is send via USB only and the C64 should start normally (boot to Basic). This mode is started by pressing both the special and menu button for 2 seconds, then releasing the menu button followed by the special button.

For reference the phi2 clock frequency should be around 985248 Hz for PAL and around 1022727 Hz for NTSC.
