=============================================
Key import from PC to Gnuk Token (no removal)
=============================================

This document describes how I put my **keys on PC** to the Token without removing keys from PC.

The difference is just not-to-save changes after key imports.

.. BREAK

After personalization, I put my keys into the Token.

Here is the log.

I invoke GnuPG with my key (4ca7babe) and with ``--homedir`` option to specify the directory which contains my secret keys.  ::

  $ gpg --homedir=/home/gniibe/tmp/gnuk-testing-dir --edit-key 4ca7babe 
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
  gpg: detected reader `FSIJ Gnuk (0.12-38FF6A06) 00 00'
  Signature key ....: [none]
  Encryption key....: [none]
  Authentication key: [none]
  
  Please select where to store the key:
     (1) Signature key
     (3) Authentication key
  Your selection? 1

Then, GnuPG asks two passwords.  One is the passphrase of **keys on PC** and another is the password of **Gnuk Token**.  Note that the password of the token and the password of the keys on PC are different things, although they can be same.

I enter these passwords. ::

  You need a passphrase to unlock the secret key for
  user: "NIIBE Yutaka <gniibe@fsij.org>"
  2048-bit RSA key, ID 4CA7BABE, created 2010-10-15
  <PASSWORD-KEY-4CA7BABE>
  gpg: writing new key
  gpg: 3 Admin PIN attempts remaining before card is permanently locked
  
  Please enter the Admin PIN
  Enter Admin PIN: <PASSWORD-GNUK>
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb  2048R/084239CF  created: 2010-10-15  expires: never     
  ssb  2048R/5BB065DC  created: 2010-10-22  expires: never     
  (1)  NIIBE Yutaka <gniibe@fsij.org>

The primary key is now on the Token and GnuPG says its card-no (F517 00000001) , where F517 is the vendor ID of FSIJ.

Secondly, I import my subkey of encryption.  I select key number '1'. ::

  gpg> key 1
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb* 2048R/084239CF  created: 2010-10-15  expires: never     
  ssb  2048R/5BB065DC  created: 2010-10-22  expires: never     
  (1)  NIIBE Yutaka <gniibe@fsij.org>

You can see that the subkey is marked by '*'.
I type ``keytocard`` command to import this subkey to Gnuk Token.  I select ``2`` as it's encryption key. ::

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

Thirdly, I select sub key of suthentication which has key number '2'. ::

  gpg> key 2
  
  sec  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb  2048R/084239CF  created: 2010-10-15  expires: never     
                       card-no: F517 00000001
  ssb* 2048R/5BB065DC  created: 2010-10-22  expires: never     
  (1)  NIIBE Yutaka <gniibe@fsij.org>

You can see that the subkey number '2' is marked by '*'.
I type ``keytocard`` command to import this subkey to Gnuk Token.  I select ``3`` as it's authentication key. ::

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

Lastly, I quit GnuPG.  Note that I **don't** save changes. ::

  gpg> quit
  Save changes? (y/N) n
  Quit without saving? (y/N) y
  $ 

All keys are imported to Gnuk Token now.
