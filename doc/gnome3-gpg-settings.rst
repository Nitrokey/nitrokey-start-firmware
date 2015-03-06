===========================================
GnuPG settings for GNOME 3.1x and GNOME 3.0
===========================================

In the section `GnuPG settings`_, I wrote how I disable GNOME-keyrings for SSH.

It was for GNOME 2.  The old days was good, we just disabled GNOME-keyrings
interference to SSH and customizing our desktop was easy for GNU and UNIX users.

.. _GnuPG settings: gpg-settings


GNOME keyrings in GNOME 3.1x
============================

In the files /etc/xdg/autostart/gnome-keyring-ssh.desktop
and /etc/xdg/autostart/gnome-keyring-gpg.desktop,
we have a line something like: ::

    OnlyShowIn=GNOME;Unity;MATE;

Please edit this line to: ::

    OnlyShowIn=

Then, no desktop environment invokes gnome-keyring for ssh and gpg.  I think that it is The Right Thing.


GNOME keyrings in GNOME 3.0 by GNOME-SESSION-PROPERTIES
=======================================================

We can't use GNOME configuration tool (like GNOME 2) to disable interference by
GNOME keyrings in GNOME 3.0.

It is GNOME-SESSION-PROPERTIES to disable the interference.  Invoking::

 $ gnome-session-properties

and at the tab of "Startup Programs", I removed radio check buttons
for "GPG Password Agent" and "SSH Key Agent".

Then, I can use proper gpg-agent for GnuPG Agent Service and SSH Agent Service with Gnuk Token in GNOME 3.0.
