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

I invoke GnuPG with my key (4ca7babe).  ::

  $ gpg --edit-key 4ca7babe 
  gpg (GnuPG) 1.4.11; Copyright (C) 2010 Free Software Foundation, Inc.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.
  
  Secret key is available.
  
  pub  2048R/4CA7BABE  created: 2010-10-15  expires: never       usage: SC  
                       trust: ultimate      validity: ultimate
  sub  2048R/084239CF  created: 2010-10-15  expires: never       usage: E   
  sub  2048R/5BB065DC  created: 2010-10-22  expires: never       usage: A   
  [ultimate] (1). NIIBE Yutaka <gniibe@fsij.org>


Then, GnuPG enters its own command interaction mode.  The prompt is ``gpg>``.
To enable ``keytocard`` command, I type ``toggle`` command.  ::

  gpg> toggle
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
  ssb  2048R/084239CF  created: 2010-10-15  expires: never     
  ssb  2048R/5BB065DC  created: 2010-10-22  expires: never     
  (1)  NIIBE Yutaka <gniibe@fsij.org>

Firstly, I import my primary key into Gnuk Token.
I type ``keytocard`` command, answer ``y`` to confirm keyimport,
and type ``1`` to say it's signature key. ::

  gpg> keytocard
  Really move the primary key? (y/N) y
  Signature key ....: [none]
  Encryption key....: [none]
  Authentication key: [none]
  
  Please select where to store the key:
     (1) Signature key
     (3) Authentication key
  Your selection? 1

Then, GnuPG asks two passwords.  One is the passphrase of **keys on PC**
and another is the password of **Gnuk Token**.  Note that the password of
the token and the password of the keys on PC are different things,
although they can be same.

Here, I assume that Gnuk Token's admin password of factory setting (12345678).

I enter these passwords. ::

  You need a passphrase to unlock the secret key for
  user: "NIIBE Yutaka <gniibe@fsij.org>"
  2048-bit RSA key, ID 4CA7BABE, created 2010-10-15
  <PASSWORD-KEY-4CA7BABE>
  gpg: writing new key
  gpg: 3 Admin PIN attempts remaining before card is permanently locked
  
  Please enter the Admin PIN
  Enter Admin PIN: 12345678
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb  2048R/084239CF  created: 2010-10-15  expires: never     
  ssb  2048R/5BB065DC  created: 2010-10-22  expires: never     
  (1)  NIIBE Yutaka <gniibe@fsij.org>

The primary key is now on the Token and GnuPG says its card-no (F517 00000001),
where F517 is the vendor ID of FSIJ.

Secondly, I import my subkey of encryption.  I select key number '1'. ::

  gpg> key 1
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb* 2048R/084239CF  created: 2010-10-15  expires: never     
  ssb  2048R/5BB065DC  created: 2010-10-22  expires: never     
  (1)  NIIBE Yutaka <gniibe@fsij.org>

You can see that the subkey is marked by '*'.
I type ``keytocard`` command to import this subkey to Gnuk Token.
I select ``2`` as it's encryption key. ::

  gpg> keytocard
  Signature key ....: [none]
  Encryption key....: [none]
  Authentication key: [none]
  
  Please select where to store the key:
     (2) Encryption key
  Your selection? 2

Then, GnuPG asks the passphrase of **keys on PC** again.  I enter. ::

  You need a passphrase to unlock the secret key for
  user: "NIIBE Yutaka <gniibe@fsij.org>"
  2048-bit RSA key, ID 084239CF, created 2010-10-15
  <PASSWORD-KEY-4CA7BABE>
  gpg: writing new key
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb* 2048R/084239CF  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb  2048R/5BB065DC  created: 2010-10-22  expires: never     
  (1)  NIIBE Yutaka <gniibe@fsij.org>

The sub key is now on the Token and GnuPG says its card-no for it.
  
I type ``key 1`` to deselect key number '1'. ::

  gpg> key 1
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb  2048R/084239CF  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb  2048R/5BB065DC  created: 2010-10-22  expires: never     
  (1)  NIIBE Yutaka <gniibe@fsij.org>

Thirdly, I select sub key of authentication which has key number '2'. ::

  gpg> key 2
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb  2048R/084239CF  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb* 2048R/5BB065DC  created: 2010-10-22  expires: never     
  (1)  NIIBE Yutaka <gniibe@fsij.org>

You can see that the subkey number '2' is marked by '*'.
I type ``keytocard`` command to import this subkey to Gnuk Token.
I select ``3`` as it's authentication key. ::

  gpg> keytocard
  Signature key ....: [none]
  Encryption key....: [none]
  Authentication key: [none]
  
  Please select where to store the key:
     (3) Authentication key
  Your selection? 3

Then, GnuPG asks the passphrase of **keys on PC** again.  I enter. ::

  You need a passphrase to unlock the secret key for
  user: "NIIBE Yutaka <gniibe@fsij.org>"
  2048-bit RSA key, ID 5BB065DC, created 2010-10-22
  <PASSWORD-KEY-4CA7BABE>
  gpg: writing new key
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb  2048R/084239CF  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb* 2048R/5BB065DC  created: 2010-10-22  expires: never     
                       card-no: F517 00000001
  (1)  NIIBE Yutaka <gniibe@fsij.org>

The sub key is now on the Token and GnuPG says its card-no for it.

Lastly, I save changes of **keys on PC** and quit GnuPG. ::

  gpg> save
  $ 

All secret keys are imported to Gnuk Token now.
On PC, only references (card-no) to the Token remain
and secrets have been removed.
