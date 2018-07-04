Development Environment
=======================


Hardware
--------

For development, it is highly recommended to have JTAG/SWD debugger.

For boards with DFU (Device Firmware Upgrade) feature (such as DfuSe),
it is possible to develop with that.  But it should be considered
*experimental* environment, and it should not be used for usual
purpose.  That's because it is basically impossible for DfuSe
implementations to disable reading-out from flash ROM.  It means
that your secrets will be readily extracted by DfuSe.

For JTAG debugger, Olimex JTAG-Tiny is good and supported well.  For
SWD debugger, ST-Link/V2 would be good, and it is supported by
tool/stlinkv2.py.


OpenOCD
-------

For JTAG/SWD debugger, we can use OpenOCD.


GNU Toolchain
-------------

You need GNU toolchain and newlib for 'arm-none-eabi' target.
In Debian, we can just apt-get packages of: gcc-arm-none-eabi, binutils-arm-none-eabi, gdb-arm-none-eabi and libnewlib-arm-none-eabi. 

For other distributiions, there is "gcc-arm-embedded" project.  See:
https://launchpad.net/gcc-arm-embedded/

We are using "-O3 -Os" for compiler option.


Building Gnuk
-------------

Change directory to ``src``: ::

  $ cd gnuk-VERSION/src

Then, run ``configure``: ::

  $ ./configure --vidpid=<VID:PID>

Here, you need to specify USB vendor ID and product ID.  For FSIJ's,
it's: --vidpid=234b:0000 .  Please read the section 'USB vendor ID and
product ID' in README.

Type: ::

  $ make

Then, we will have "gnuk.elf" under src/build directory.

If you are not the authorized vendor, please never distribute this
file of "gnuk.elf", which includes VID:PID in the image.  If you would
like to distribute the image (for example, to check if it's
reproducible or not), the file "gnuk-no-vidpid.elf" is the one with no
VID:PID.
