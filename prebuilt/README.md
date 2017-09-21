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

Similarly for later releases - RTM.4 is based on GNUK 1.2.4.

Firmware upgrade instructions
-------

### Requirements

For the upgrading process you need python and the module `python-pyusb`. If you want to test your NK Start after the upgrade (see below) you need the module `python-pytest` as well. You can install these things through your package management (especially on Linux, e. g. `apt-get update && apt-get install python-pyusb python-pytest`) or you can create a virtual python environment and use `pip install` to make sure you have everything needed.

To set up such a virtual python environment following steps are needed (`python`, `virtualenv` and `python-pip` have to be already installed):

```
virtualenv env3 --python python3
. env3/bin/activate
pip3 install pytest 
pip3 install pyusb
```
You should set up this virtual environment in the `nitrokey-start-firmware` folder (see below).

### Upgrading the device
Please download firmware repository (branch gnuk1.2-regnual-fix) and enter the folder `nitrokey-start-firmware` by typing the following commands:

```
git clone -b gnuk1.2-regnual-fix https://github.com/Nitrokey/nitrokey-start-firmware.git
cd nitrokey-start-firmware
```

To make sure firmware is changed on device you can save current version to file: `gpg2 --card-status > before.status`. Since gpg2 claims the device please reinsert it to make it free to use by GNUK.

Depending on you current firmware you have to choose the right prebuilts:
- If your LED flashes green on operation please type `RTM=RTM.1_to_RTM.4` in your console
- If your LED flashes red then please type `RTM=RTM.4` in your console

Now we can proceed with the actual upgrade:
```
./tools/upgrade_by_passwd.py prebuilt/$RTM/regnual.bin prebuilt/$RTM/gnuk.bin
```

Please provide the admin-PIN of your NK Start and wait for the results. To check that firmware version was actually changed please run:
```
gpg2 --card-status > after.status
diff before.status after.status
```

### Testing the device

It is possible to test the device after flashing. Please make sure python 3 and python-pytest are installed (see requirements above). Now please run `pytest -vx tests/test_*` to test the firmware.
