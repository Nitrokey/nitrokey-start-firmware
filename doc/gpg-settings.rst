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
  default-preference-list SHA512 SHA384 SHA256 AES256 AES192 AES CAST5 ZLIB BZIP2 ZIP Uncompressed
  
  default-key 0x4ca7babe


Let gpg-agent manage SSH key
============================

I deactivate seahose-agent.  Also, I deactivate gnome-keyring managing SSH key. ::

  $ gconftool-2 --type bool --set /apps/gnome-keyring/daemon-components/ssh false

Then, I create ``.gnupg/gpg-agent.conf`` file with the following content. ::

  enable-ssh-support


References
==========

* `Creating a new GPG key`_
* `Use OpenPGP Keys for OpenSSH, how to use gpg with ssh`_

.. _Creating a new GPG key: http://keyring.debian.org/creating-key.html
.. _Use OpenPGP Keys for OpenSSH, how to use gpg with ssh: http://www.programmierecke.net/howto/gpg-ssh.html
