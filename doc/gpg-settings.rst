.. -*- coding: utf-8 -*-

==============
GnuPG settings
==============

Here is my GnuPG settings.

.gnupg/gpg.conf
===============

I create ``.gnupg/gpg.conf`` file with the following content. ::

  use-agent
  default-key 0xE267B052364F028D

In addition to the ``use-agent`` option, I specify my default key.

The ``use-agent`` option is for GnuPG 1.4.x and it means using gpg-agent if available.
If no option, GnuPG 1.4.x directly connects to Gnuk Token by itself, instead of through scdaemon.  When GnuPG 1.4.x tries to access Gnuk Token and scdaemon is running, there are conflicts.

We recommend to specify the ``use-agent`` option for GnuPG 1.4.x to access Gnuk Token through gpg-agent and scdaemon.

For GnuPG 2.0 and 2.1, gpg-agent is always used, so, there is no need to specify the ``use-agent`` option, but having this option is no harm, anyway.


Let gpg-agent manage SSH key
============================

I create ``.gnupg/gpg-agent.conf`` file with the following content. ::

  enable-ssh-support

I edit the file /etc/X11/Xsession.options and comment out use-ssh-agent line,
so that Xsession doesn't invoke original ssh-agent.  We use gpg-agent as ssh-agent.

In the files /etc/xdg/autostart/gnome-keyring-ssh.desktop,
I have a line something like: ::

    OnlyShowIn=GNOME;Unity;MATE;

I edit this line to: ::

    OnlyShowIn=

So that no desktop environment enables gnome-keyring for ssh.

References
==========

* `Creating a new GPG key`_
* `Use OpenPGP Keys for OpenSSH, how to use gpg with ssh`_

.. _Creating a new GPG key: http://keyring.debian.org/creating-key.html
.. _Use OpenPGP Keys for OpenSSH, how to use gpg with ssh: http://www.programmierecke.net/howto/gpg-ssh.html
