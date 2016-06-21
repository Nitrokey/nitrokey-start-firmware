===================================
Initial Configuration of Gnuk Token
===================================

This is optional step.

You don't need to setup the serial number of Gnuk Token,
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

Note that this is completely optional step.  I don't know anyone other than me, do this.  Even for me, I only do that for a single device among multiple devices I use.  I do that to test the feature.

In the file ``GNUK_SERIAL_NUMBER``, each line has email and 6-byte serial number.  The first two bytes are organization number (F5:17 is for FSIJ).  Last four bytes are number for tokens.

The tool ``../tool/gnuk_put_binary_libusb.py`` examines  environment variable of ``EMAIL``, and writes corresponding serial number to Gnuk Token. ::

  $ ../tool/gnuk_put_binary_libusb.py -s ../GNUK_SERIAL_NUMBER 
  Writing serial number
  Device:  
  Configuration:  1
  Interface:  0
  d2 76 00 01 24 01 02 00 f5 17 00 00 00 01 00 00
