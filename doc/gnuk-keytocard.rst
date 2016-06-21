================================
Key import from PC to Gnuk Token
================================

This document describes how I put my **keys on PC** to the Token,
and remove secret keys from PC.

Note that there is **no ways** to export keys from the Gnuk Token,
so please be careful.


If you want to import same keys to multiple Tokens,
please copy ``.gnupg`` directory beforehand.

In my case, I do something like following:  ::

  $ cp -a .gnupg tmp/gnuk-testing-dir

See `another document`_ to import keys to the Token from copied directory.

.. _another document: gnuk-keytocard-noremoval

After personalization, I put my keys into the Token.

Here is the session log.

I invoke GnuPG with my key (249CB3771750745D5CDD323CE267B052364F028D).  ::

  $ gpg --edit-key 249CB3771750745D5CDD323CE267B052364F028D
  gpg (GnuPG) 2.1.13; Copyright (C) 2016 Free Software Foundation, Inc.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.

  Secret key is available.

  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       trust: ultimate      validity: ultimate
  ssb  cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
  ssb  ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
  [ultimate] (1). NIIBE Yutaka <gniibe@debian.org>
  [ultimate] (2)  NIIBE Yutaka <gniibe@fsij.org>

  gpg> 


Then, GnuPG enters its own command interaction mode.  The prompt is ``gpg>``.

Firstly, I import my primary key into Gnuk Token.
I type ``keytocard`` command, answer ``y`` to confirm keyimport,
and type ``1`` to say it's signature key. ::

  gpg> keytocard
  Really move the primary key? (y/N) y
  Please select where to store the key:
     (1) Signature key
     (3) Authentication key
  Your selection? 1

Then, GnuPG asks two kinds of passphrases.  One is the passphrase of **keys on PC**
and another is the passphrase of **Gnuk Token**.  Note that the passphrase of
the token and the passphrase of the keys on PC are different things,
although they can be same.

Here, I assume that Gnuk Token's admin passphrase of factory setting (12345678).

I enter these passphrases. ::

  Please enter your passphrase, so that the secret key can be unlocked for this session
  <PASSWORD-KEY-ON-PC>
  
  Please enter the Admin PIN
  Enter Admin PIN: 12345678
  
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       trust: ultimate      validity: ultimate
  ssb  cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
  ssb  ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
  [ultimate] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ultimate] (2)  NIIBE Yutaka <gniibe@debian.org>

Secondly, I import my subkey of encryption.  I select key number '1'. ::

  gpg> key 1
  
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       trust: ultimate      validity: ultimate
  ssb* cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
  ssb  ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
  [ultimate] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ultimate] (2)  NIIBE Yutaka <gniibe@debian.org>

You can see that the subkey is marked by '*'.
I type ``keytocard`` command to import this subkey to Gnuk Token.
I select ``2`` as it's encryption key. ::

  gpg> keytocard
  Please select where to store the key:
     (2) Encryption key
  Your selection? 2

Then, GnuPG asks the passphrase of **keys on PC** again.  I enter. ::

  Please enter your passphrase, so that the secret key can be unlocked for this session
  <PASSWORD-KEY-ON-PC>
  
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       trust: ultimate      validity: ultimate
  ssb* cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
  ssb  ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
  [ultimate] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ultimate] (2)  NIIBE Yutaka <gniibe@debian.org>

The sub key is now on the Token.

I type ``key 1`` to deselect key number '1'. ::

  gpg> key 1
  
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       trust: ultimate      validity: ultimate
  ssb  cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
  ssb  ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
  [ultimate] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ultimate] (2)  NIIBE Yutaka <gniibe@debian.org>

Thirdly, I select sub key of authentication which has key number '2'. ::

  gpg> key 2
  
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       trust: ultimate      validity: ultimate
  ssb  cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
  ssb* ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
  [ultimate] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ultimate] (2)  NIIBE Yutaka <gniibe@debian.org>

You can see that the subkey number '2' is marked by '*'.
I type ``keytocard`` command to import this subkey to Gnuk Token.
I select ``3`` as it's authentication key. ::

  gpg> keytocard
  Please select where to store the key:
     (3) Authentication key
  Your selection? 3

Then, GnuPG asks the passphrase of **keys on PC** again.  I enter. ::

  Please enter your passphrase, so that the secret key can be unlocked for this session
  <PASSWORD-KEY-ON-PC>
  
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       trust: ultimate      validity: ultimate
  ssb  cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
  ssb* ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
  [ultimate] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ultimate] (2)  NIIBE Yutaka <gniibe@debian.org>

The sub key is now on the Token.

Lastly, I save changes of **keys on PC** and quit GnuPG. ::

  gpg> save
  $ 

All secret keys are imported to Gnuk Token now.
On PC, only references (card-no) to the Token remain
and secrets have been removed.
