===================================
Initial Configuration of Gnuk Token
===================================

This is optional.  You don't need to setup the serial number of Gnuk Token,
as it comes with its default serial number based on MCU's chip ID.

You can setup the serial number of Gnuk Token only once.


Conditions
==========

I assume you are using GNU/Linux.


Preparation
===========

Make sure there is no ``scdaemon`` for configuring Gnuk Token.  You can  kill ``scdaemon`` by: ::

  $ gpg-connect-agent "SCD KILLSCD" "SCD BYE" /bye


Serial Number (optional)
========================

In the file ``GNUK_SERIAL_NUMBER``, each line has email and 6-byte serial number.  The first two bytes are organization number (F5:17 is for FSIJ).  Last four bytes are number for tokens.

The tool ``../tool/gnuk_put_binary_libusb.py`` examines  environment variable of ``EMAIL``, and writes corresponding serial number to Gnuk Token. ::

  $ ../tool/gnuk_put_binary_libusb.py -s ../GNUK_SERIAL_NUMBER 
  Writing serial number
  Device:  006
  Configuration:  1
  Interface:  0
  d2 76 00 01 24 01 02 00 f5 17 00 00 00 01 00 00


The example above is the case of libusb version.

Use the tool ``../tool/gnuk_put_binary.py`` instead , for PC/SC Lite.
You need PyScard for this.
