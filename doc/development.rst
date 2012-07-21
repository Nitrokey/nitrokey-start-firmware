Development Environment
=======================


Hardware
--------

JTAG debugger or SWD debugger is required.


GNU Toolchain
-------------

You need GNU toolchain and newlib for 'arm-none-eabi' target.

See http://github.com/esden/summon-arm-toolchain/ (which includes fix
of binutils-2.21.1) for preparation of GNU Toolchain for
'arm-none-eabi' target.  This is for GCC 4.5.

Note that we need to link correct C library (for string functions).
For this purpose, our src/Makefile.in contains following line:

	MCFLAGS= -mcpu=$(MCU) -mfix-cortex-m3-ldrd

This should not be needed (as -mcpu=cortex-m3 means
-mfix-cortex-m3-ldrd), but it is needed for the configuration of
patch-gcc-config-arm-t-arm-elf.diff in summon-arm-toolchain in practice.

In ChibiOS_2.0.8/os/ports/GCC/ARM/rules.mk, it specifies
-mno-thumb-interwork option.  This means that you should not link C
library which contains ARM (not Thumb) code.

Recently, there is "gcc-arm-embedded" project.  See:
https://launchpad.net/gcc-arm-embedded/

It is based on GCC 4.6.   For version 4.6-2012-q2-update, you'd
need "-O3 -s" instead of "-O2" and it will be slightly better.



Building Gnuk
-------------

Change directory to ``src``:

  $ cd gnuk-VERSION/src

Then, run ``configure``:

  $ ./configure --vidpid=<VID:PID>

Here, you need to specify USB vendor ID and product ID.  For FSIJ's,
it's: --vidpid=234b:0000 .  Please read section 'USB vendor ID and
product ID' above.

Type:

  $ make

Then, we will have "gnuk.elf".
