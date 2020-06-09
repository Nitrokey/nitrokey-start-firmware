Prebuilt binaries
========

Here are stored firmwares used to flash devices in all (RTM.1->RTM.4) series.  Both HEX and BIN files are usable for direct flashing (HEX files have checksum control though). Only BIN files however could be used for regnual upgrade.  Since LEDs are swapped in RTM.2 (red instead of green) using it with regnual upgrade will result in not working LED (it should be reversible though). For this please use files from RTM.1_to_RTM.2_upgrade/ directory.  Please unpack .hex.gz files before using or they might be interpreted as binary by flashing software.

Binary version (.bin) for RTM.1 is provided only for testing purposes and it is a result of a conversion hex->bin using the following command:
```
srec_cat nitrokey-start-firmware-1.0.4-1a.hex -intel -offset -0x08000000 -o nitrokey-start-firmware-1.0.4-1a.bin -binary
```
Please use .hex file for flashing.

Output of sha512sum utility is located in checksums.sha512 file.

Binaries based on GNUK 1.2.3 are available under RTM.3/ and RTM.1_to_RTM.3_upgrade/ directories for devices with red and green LED respectively. Devices upgraded earlier with RTM.1_to_RTM.2_upgrade/ firmware can be safely upgraded with RTM.1_to_RTM.3_upgrade/ binaries. 

Similarly for later releases:
- RTM.4 is based on GNUK 1.2.4.
- RTM.5 is based on GNUK 1.2.6 with not yet released (2017-10-31) fix for key generation.
- RTM.6 is based on GNUK 1.2.10.
- RTM.7 is based on GNUK 1.2.14.
- RTM.8 is based on GNUK 1.2.15.

Firmware upgrade instructions
-------

To upgrade your "Nitrokey Start" key you will need to install [nitro-python](https://github.com/Nitrokey/nitro-python).

With `nitro-python` installed you can first check if your "Nitrokey Start" key is recognized using:
```bash
nitrokey start list
```
This is also where you can see the current firmware version installed on your key.
To run the upgrade simply run:
```bash
nitrokey start update
```

### Testing the device

**Warning:** please do not use production devices (as in populated with user data, which should not be lost) to run the tests. The tests might remove or replace all data on the tested device.

It is possible to test the device after flashing. Please make sure python 3 and python-pytest are installed (see requirements above). Now please run 
```
cd ../tests/
pytest -vx test_*
```
to test the firmware.
