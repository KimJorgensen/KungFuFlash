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

The diagnostic tool is started by pressing the menu button for 2 seconds (until the LED turns off) and is intended to debug stability problems on some C64 models.
For reference the phi2 clock frequency should be around 985248 Hz for PAL and around 1022727 Hz for NTSC.

As of firmware v1.44, the diagnostic tool allows adjusting a phi2 clock offset to address any stability issues.
Some C64 breadbin/longboards may require a small negative phi2 offset and C64C/shortboards may require a small positive offset.

In the diagnostic tool the following buttons on the cartridge can be used:

* Reset: Exit diagnostic tool without saving offset

* Menu: Increase offset
* Menu, hold for 2 seconds: Reset offset (without saving)

* Special: Decrease offset
* Special, hold for 2 seconds: Save offset and exit

* Special + Menu, hold for 2 seconds: Reset and save offset. This combo will also work without starting the diagnostic tool first

The phi2 offset is saved to the KungFuFlash.dat file and is reset if the file is not found on the SD card.
