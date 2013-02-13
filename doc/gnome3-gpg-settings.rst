==========================
GnuPG settings for GNOME 3
==========================

In the article `GnuPG settings`_, I wrote how I disable GNOME-keyrings for SSH.

It was for GNOME 2.  The old days was good, we just disabled GNOME-keyrings
interference to SSH and customizing our desktop was easy for GNU and UNIX users.

.. _GnuPG settings: gpg-settings


GNOME keyrings in GNOME 3
=========================

It seems that it is more integrated into the desktop.
It is difficult to kill it.  It would be possible to kill it simply,
but then, I can't use, say, wi-fi access (which needs to access "secrets")
any more.

We can't use GNOME configuration tool to disable interference by
GNOME keyrings any more.  It seems that desktop should not have
customization these days.


GNOME-SESSION-PROPERTIES
========================

After struggling some hours, I figured out it is GNOME-SESSION-PROPERTIES
to disable the interference.  Invoking::

 $ gnome-session-properties

and at the tab of "Startup Programs", I removed radio check buttons
for "GPG Password Agent" and "SSH Key Agent".


Now, I use gpg-agent for GnuPG Agent and SSH agent with Gnuk Token.
