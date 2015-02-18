.. -*- coding: utf-8 -*-

==============
GnuPG settings
==============

Here is my GnuPG settings.

.gnupg/gpg.conf
===============

I create ``.gnupg/gpg.conf`` file with the following content. ::

  use-agent
  personal-digest-preferences SHA256
  cert-digest-algo SHA256
  default-preference-list SHA512 SHA384 SHA256 SHA224 AES256 AES192 AES CAST5 ZLIB BZIP2 ZIP Uncompressed

  default-key 0x4ca7babe

In addition to the ``use-agent`` option, set preferences on algorithms, and specify my default key.

The ``use-agent`` option is for GnuPG 1.4.x and it means using gpg-agent if available.
If no option, GnuPG 1.4.x directly connects to Gnuk Token by itself, instead of through scdaemon.  When GnuPG 1.4.x tries to access Gnuk Token and scdaemon is running, there are conflicts.

We recommend to specify the ``use-agent`` option for GnuPG 1.4.x to access Gnuk Token through gpg-agent and scdaemon.

For GnuPG 2.0.x, gpg-agent is always used, so there is no need to specify the ``use-agent`` option, but having this option is no harm, anyway.


Let gpg-agent manage SSH key
============================

I deactivate seahorse-agent.  Also, for GNOME 2, I deactivate gnome-keyring managing SSH key. ::

  $ gconftool-2 --type bool --set /apps/gnome-keyring/daemon-components/ssh false

I edit the file /etc/X11/Xsession.options and comment out use-ssh-agent line.

Then, I create ``.gnupg/gpg-agent.conf`` file with the following content. ::

  enable-ssh-support


References
==========

* `Creating a new GPG key`_
* `Use OpenPGP Keys for OpenSSH, how to use gpg with ssh`_

.. _Creating a new GPG key: http://keyring.debian.org/creating-key.html
.. _Use OpenPGP Keys for OpenSSH, how to use gpg with ssh: http://www.programmierecke.net/howto/gpg-ssh.html
