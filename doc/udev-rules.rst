===============================================
Device Configuration for Gnuk Token with libusb
===============================================

In order to use Gnuk Token with libusb, configuration of device is
needed for permissions.  Note that this is not needed for the case of
PC/SC Lite, as it has its own device configuration.


Patching 60-gnupg.rules
=======================

In case of Debian, there is a file /lib/udev/rules.d/60-gnupg.rules.
This would be the place we need to change::

    --- /lib/udev/rules.d/60-gnupg.rules.orig	2012-06-24 21:51:26.000000000 +0900
    +++ /lib/udev/rules.d/60-gnupg.rules	2012-07-13 17:18:55.149587687 +0900
    @@ -10,4 +10,7 @@
     ATTR{idVendor}=="04e6", ATTR{idProduct}=="5115", ENV{ID_SMARTCARD_READER}="1", ENV{ID_SMARTCARD_READER_DRIVER}="gnupg"
     ATTR{idVendor}=="20a0", ATTR{idProduct}=="4107", ENV{ID_SMARTCARD_READER}="1", ENV{ID_SMARTCARD_READER_DRIVER}="gnupg"
     
    +# Gnuk
    +ATTR{idVendor}=="234b", ATTR{idProduct}=="0000", ENV{ID_SMARTCARD_READER}="1", ENV{ID_SMARTCARD_READER_DRIVER}="gnupg"
    +
     LABEL="gnupg_rules_end"



Have a another configuration for reGNUal
========================================

For reGNUal (upgrade feature of Gnuk),
I also have a file /etc/udev/rules.d/92-gnuk.rules::

   # For updating firmware, permission settings are needed.
   
   SUBSYSTEMS=="usb", ATTRS{idVendor}=="234b", ATTRS{idProduct}=="0000", \
       ENV{ID_USB_INTERFACES}=="*:ff0000:*", GROUP="pcscd"


Configuration for ST-Link/V2
============================

This is for development, but I also have a file
/etc/udev/rules.d/10-stlink.rules::

    ACTION=="add", SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="3748", GROUP="tape", MODE="664", SYMLINK+="stlink"

