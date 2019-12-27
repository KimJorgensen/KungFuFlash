# Kung Fu Flash Firmware
The initial firmware installation can be done via the SWD interface (J4) using a ST-Link V2 programmer or via the USB port.
The Kung Fu Flash cartridge should not be connected to the Commodore 64 before this process has been completed.

## SWD
Connect the ST-Link V2 programmer to J4 (3.3V, GND, SWDIO, and SWCLK) and connect BOOT0 to 3.3V.

Rename the `KungFuFlash_v1.xx.upd` file to `KungFuFlash_v1.xx.bin` and use the STM32 ST-LINK Utility from ST to program the firmware.

All wires to J4 should be disconnected once the firmware has been successfully installed.

## USB
Add a jumper to short J1 and a jumper to short BOOT0 and +3.3V on J4. This will power the board from USB and ensure that it is started in DFU mode.

Use the DfuSE software from ST or [dfu-util](http://dfu-util.sourceforge.net/) to install the firmware:

`dfu-util -a 0 -s 0x08000000 -D KungFuFlash_v1.xx.upd`

Both jumpers should be removed after the firmware has been installed.
