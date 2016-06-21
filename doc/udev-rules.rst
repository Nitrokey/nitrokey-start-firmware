===============================================
Device Configuration for Gnuk Token with libusb
===============================================

In order to use Gnuk Token with libusb, configuration of device is
needed for permissions.  Note that this is not needed for the case of
PC/SC Lite, as it has its own device configuration.


udev rules for Gnuk Token
=========================

In case of Debian, there is a file /lib/udev/rules.d/60-gnupg.rules
(or /lib/udev/rules.d/60-scdamon.rules for newer version),
when you install "gnupg" package (or "scdaemon" package).
This is the place we need to
change, if your installation is older than jessie.  Newer "gnupg"
package (1.4.15-1 or later) or "scdaemon" package has already
supported Gnuk Token.

If needed, please add lines for Gnuk Token to give a desktop user the
permission to use the device.  We specify USB ID of Gnuk Token (by
FSIJ)::

    --- /lib/udev/rules.d/60-gnupg.rules.orig	2012-06-24 21:51:26.000000000 +0900
    +++ /lib/udev/rules.d/60-gnupg.rules	2012-07-13 17:18:55.149587687 +0900
    @@ -10,4 +10,7 @@
     ATTR{idVendor}=="04e6", ATTR{idProduct}=="5115", ENV{ID_SMARTCARD_READER}="1", ENV{ID_SMARTCARD_READER_DRIVER}="gnupg"
     ATTR{idVendor}=="20a0", ATTR{idProduct}=="4107", ENV{ID_SMARTCARD_READER}="1", ENV{ID_SMARTCARD_READER_DRIVER}="gnupg"
     
    +# Gnuk
    +ATTR{idVendor}=="234b", ATTR{idProduct}=="0000", ENV{ID_SMARTCARD_READER}="1", ENV{ID_SMARTCARD_READER_DRIVER}="gnupg"
    +
     LABEL="gnupg_rules_end"

When we only install "gnupg2" package for 2.0 (with no "gnupg" package),
there will be no udev rules (there is a bug report #543217 for this issue).
In this case, we need something like this in /etc/udev/rules.d/60-gnuk.rules::

    SUBSYSTEMS=="usb", ATTRS{idVendor}=="234b", ATTRS{idProduct}=="0000",   \
    ENV{ID_SMARTCARD_READER}="1", ENV{ID_SMARTCARD_READER_DRIVER}="gnupg"

Usually, udev daemon automatically handles for the changes of configuration
files.  If not, please let the daemon reload rules::

  # udevadm control --reload-rules




udev rules for ST-Link/V2
=========================

For development of Gnuk, we use ST-Link/V2 as JTAG/SWD debugger.
We need to have a udev rule for ST-Link/V2.  It's like::

    ACTION=="add", SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="3748", GROUP="tape", MODE="664", SYMLINK+="stlink"

I have this in the file /etc/udev/rules.d/10-stlink.rules.
