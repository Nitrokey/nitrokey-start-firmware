Here are stored firmwares used to flash devices in first (RTM.1, GNUK v1.0.4), 
second (RTM.2, v1.2.2) and third (RTM.3, v1.2.3) series.  Both HEX and BIN files are usable for
direct flashing (HEX files have checksum control though). Only BIN files however could
be used for regnual upgrade.  Since LEDs are swapped in RTM.2 (red instead of green) using it with
regnual upgrade will result in not working LED. For this please use files from
RTM.1_to_RTM.2_upgrade/ directory.  Please unpack .hex.gz files before using or
they might be interpreted as binary by flashing software.

Binary version for RTM.1 is provided only for testing purposes and it is a
result of a conversion hex->bin using the following command:
```
srec_cat nitrokey-start-firmware-1.0.4-1a.hex -intel -offset -0x08000000 -o nitrokey-start-firmware-1.0.4-1a.bin -binary
```
Please use .hex file for flashing.

Output of sha512sum utility is located in checksums.sha512 file.

Binaries based on GNUK 1.2.3 are available under RTM.3/ and RTM.1_to_RTM.3_upgrade/ directories for devices with red and green LED respectively. Devices upgraded earlier with RTM.1_to_RTM.2_upgrade/ firmware can be safely upgraded with RTM.1_to_RTM.3_upgrade/ binaries. 
