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
that your secret will be readily extracted by DfuSe.

For JTAG debugger, Olimex JTAG-Tiny is good and supported well.  For
SWD debugger, ST-Link/V2 would be good, and it is supported by
tool/stlinkv2.py.


OpenOCD
-------

For JTAG/SWD debugger, we can use OpenOCD.

Note that ST-Link/V2 is *not* supported by OpenOCD 0.5.0.  It is
supported by version 0.6 or later.


GNU Toolchain
-------------

You need GNU toolchain and newlib for 'arm-none-eabi' target.

There is "gcc-arm-embedded" project.  See:
https://launchpad.net/gcc-arm-embedded/

It is based on GCC 4.6.  You'd need "-O3 -Os" instead of "-O2" and it
will be slightly better.

Note that we need to link correct C library (for string functions).
For this purpose, our src/Makefile.in contains following line:

	MCFLAGS= -mcpu=$(MCU) -mfix-cortex-m3-ldrd

This should not be needed (as -mcpu=cortex-m3 means
-mfix-cortex-m3-ldrd), but it was needed for the configuration of
patch-gcc-config-arm-t-arm-elf.diff in summon-arm-toolchain in practice.


Building Gnuk
-------------

Change directory to ``src``:

  $ cd gnuk-VERSION/src

Then, run ``configure``:

  $ ./configure --vidpid=<VID:PID>

Here, you need to specify USB vendor ID and product ID.  For FSIJ's,
it's: --vidpid=234b:0000 .  Please read the section 'USB vendor ID and
product ID' in README.

Type:

  $ make

Then, we will have "gnuk.elf".
