=============================================
Key import from PC to Gnuk Token (no removal)
=============================================

This document describes how I put my **keys on PC** to the Token
without removing keys from PC.

The difference is only the last step.
I don't save changes on PC after keytocard.

For the steps before the last step, please see `keytocard with removing keys on PC`_.

.. _keytocard removing keys: gnuk-keytocard

Here is the session log of the last step.

Lastly, I quit GnuPG.  Note that I **don't** save changes. ::

  gpg> quit
  Save changes? (y/N) n
  Quit without saving? (y/N) y
  $ 

All keys are imported to Gnuk Token now.
Still, secret keys are available on PC, too.
