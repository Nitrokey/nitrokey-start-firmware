
Prebuilt binaries
========

Here are stored firmwares used to flash devices in first (RTM.1, GNUK v1.0.4), 
second (RTM.2, v1.2.2) and third (RTM.3, v1.2.3) series.  Both HEX and BIN files are usable for
direct flashing (HEX files have checksum control though). Only BIN files however could
be used for regnual upgrade.  Since LEDs are swapped in RTM.2 (red instead of green) using it with
regnual upgrade will result in not working LED (it should be reversible though). For this please use files from
RTM.1_to_RTM.2_upgrade/ directory.  Please unpack .hex.gz files before using or
they might be interpreted as binary by flashing software.


Binary version (.bin) for RTM.1 is provided only for testing purposes and it is a
result of a conversion hex->bin using the following command:
```
srec_cat nitrokey-start-firmware-1.0.4-1a.hex -intel -offset -0x08000000 -o nitrokey-start-firmware-1.0.4-1a.bin -binary
```
Please use .hex file for flashing.


Output of sha512sum utility is located in checksums.sha512 file.


Binaries based on GNUK 1.2.3 are available under RTM.3/ and RTM.1_to_RTM.3_upgrade/ directories for devices with red and green LED respectively. Devices upgraded earlier with RTM.1_to_RTM.2_upgrade/ firmware can be safely upgraded with RTM.1_to_RTM.3_upgrade/ binaries. 


Firmware upgrade instructions
-------

Please download firmware repository, eg. with command:
```
git clone -b gnuk1.2-regnual-fix
https://github.com/Nitrokey/nitrokey-start-firmware.git
```
enter `prebuilt`, and choose binary depending on your current firmware:
- if your LED flashes green on operation please choose RTM.1->RTM.3
- if your LED flashes red then please choose RTM.3

To make sure firmware is changed on device you can save current version
to file: `gpg2 --card-status > before.status` . Since gpg2 claims the
device please reinsert it to make it free to use by GNUK.

Then please enter `tool` directory in firmware's repository and run:
```
./upgrade_by_passwd.py ../<PATH>/regnual.bin ../<PATH/gnuk.bin
```

Please provide PIN and wait for the results. To check firmware version change please run:
```
gpg2 --card-status > after.status
diff before.status after.status
```

It is possible to test the device after flashing. Please enter `./tests/` directory
and run `py.test -v test_*`. Py.test 3+ and Python 3+ are needed to run
them. A `virtualenv` tool might be useful to help setting up environment.
