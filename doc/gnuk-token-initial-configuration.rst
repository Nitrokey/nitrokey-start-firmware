===================================
Initial Configuration of Gnuk Token
===================================

Conditions
==========

I assume you are using GNU/Linux.


Preparation
===========

We need to kill ``scdaemon`` before configuring Gnuk Token. ::

  $ gpg-connect-agent "SCD KILLSCD" "SCD BYE" /bye


Serial Number (optional)
========================

In the file ``GNUK_SERIAL_NUMBER``, each line has email and 6-byte serial number.

The tool ``../tool/gnuk_put_binary.py`` examines  environment variable of ``EMAIL``, and writes serial number to Gnuk Token. ::

  $ ../tool/gnuk_put_binary.py -s ../GNUK_SERIAL_NUMBER 
  Writing serial number
  Token: FSIJ Gnuk (0.12-38FF6A06) 00 00
  ATR: 3B DA 11 FF 81 B1 FE 55 1F 03 00 31 84 73 80 01 40 00 90 00 24


The tool ``../tool/gnuk_put_binary.py`` is for PC/SC Lite.  Use
``../tool/gnuk_put_binary_libusb.py`` instead, if you don't use
PC/SC Lite but use libusb directly.
