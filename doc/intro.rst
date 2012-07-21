Introduction
============


What's Gnuk?
------------

Gnuk is an implementation of USB cryptographic token for GNU Privacy
Guard.  Gnuk supports OpenPGP card protocol version 2, and it runs on
STM32F103 processor.


Cryptographic token and feature of Gnuk
---------------------------------------

Cryptographic token is a store of private keys and it computes cryptographic functions on the device.


Development Environment
-----------------------

See :doc:`development` for development environment for Gnuk.  It builds on Free Software.


Prerequisites
-------------

* GNU Privacy Guard (GnuPG)

* libusb

* [Optional] PC/SC lite (pcscd, libccid)

* SSH: openssh

* Web: scute, firefox


Usage
-----

* Sign with GnuPG
* Decrypt with GnuPG
* Use with OpenSSH
* Use with Firefox for X.509 client certificate authentication
