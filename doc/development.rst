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

For JTAG/SWD debugger, we can use OpenOCD somehow.

Note that ST-Link/V2 was *not* supported by OpenOCD 0.5.0.

It is supported by version 0.6 or later somehow, but still, you can't
enable protection of flash ROM with OpenOCD using ST-Link/V2.


GNU Toolchain
-------------

You need GNU toolchain and newlib for 'arm-none-eabi' target.

There is "gcc-arm-embedded" project.  See:
https://launchpad.net/gcc-arm-embedded/

It is based on GCC 4.8 (as of December, 2013).  We are using "-O3 -Os"
for compiler option.


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

Then, we will have "gnuk.elf" under src/build directory.
