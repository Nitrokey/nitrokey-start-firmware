======================================
Using Gnuk Token with another computer
======================================

This document describes how you can use Gnuk Token on another PC (which is not the one you generate your keys).

Note that the Token only brings your secret keys, while ``.gnupg`` directory contains keyrings and trustdb, too.

.. BREAK

Fetch the public key and connect it to the Token
================================================

Using the Token, we need to put the public key and the secret key reference (to the token) in ``.gnupg``.

To do that, invoke GnuPG with ``--card-edit`` option. ::

  $ gpg --card-edit
  gpg: detected reader `FSIJ Gnuk (0.12-37006A06) 00 00'
  Application ID ...: D276000124010200F517000000010000
  Version ..........: 2.0
  Manufacturer .....: FSIJ
  Serial number ....: 00000001
  Name of cardholder: Yutaka Niibe
  Language prefs ...: ja
  Sex ..............: male
  URL of public key : http://www.gniibe.org/gniibe.asc
  Login data .......: gniibe
  Signature PIN ....: not forced
  Key attributes ...: 2048R 2048R 2048R
  Max. PIN lengths .: 127 127 127
  PIN retry counter : 3 3 3
  Signature counter : 6
  Signature key ....: 1241 24BD 3B48 62AF 7A0A  42F1 00B4 5EBD 4CA7 BABE
        created ....: 2010-10-15 06:46:33
  Encryption key....: 42E1 E805 4E6F 1F30 26F2  DC79 79A7 9093 0842 39CF
        created ....: 2010-10-15 06:46:33
  Authentication key: B4D9 7142 C42D 6802 F5F7  4E70 9C33 B6BA 5BB0 65DC
        created ....: 2010-10-22 06:06:36
  General key info..: [none]
  
  gpg/card> 

It says, there is no key info related to this token on your PC (``[none]``).

Fetch the public key from URL specified in the Token. ::

  gpg/card> fetch
  gpg: requesting key 4CA7BABE from http server www.gniibe.org
  gpg: key 4CA7BABE: public key "NIIBE Yutaka <gniibe@fsij.org>" imported
  gpg: no ultimately trusted keys found
  gpg: Total number processed: 1
  gpg:               imported: 1  (RSA: 1)
  
  gpg/card> 

Good.  The public key is now in ``.gnupg``.  We can examine by ``gpg --list-keys``.

However, the secret key reference (to the token) is not in ``.gnupg`` yet.

It will be generated when I do ``--card-status`` by GnuPG with correspoinding public key in ``.gnupg``, or just type return at the ``gpg/card>`` prompt. ::

  gpg/card> 
  
  Application ID ...: D276000124010200F517000000010000
  Version ..........: 2.0
  Manufacturer .....: FSIJ
  Serial number ....: 00000001
  Name of cardholder: Yutaka Niibe
  Language prefs ...: ja
  Sex ..............: male
  URL of public key : http://www.gniibe.org/gniibe.asc
  Login data .......: gniibe
  Signature PIN ....: not forced
  Key attributes ...: 2048R 2048R 2048R
  Max. PIN lengths .: 127 127 127
  PIN retry counter : 3 3 3
  Signature counter : 6
  Signature key ....: 1241 24BD 3B48 62AF 7A0A  42F1 00B4 5EBD 4CA7 BABE
        created ....: 2010-10-15 06:46:33
  Encryption key....: 42E1 E805 4E6F 1F30 26F2  DC79 79A7 9093 0842 39CF
        created ....: 2010-10-15 06:46:33
  Authentication key: B4D9 7142 C42D 6802 F5F7  4E70 9C33 B6BA 5BB0 65DC
        created ....: 2010-10-22 06:06:36
  General key info..: 
  pub  2048R/4CA7BABE 2010-10-15 NIIBE Yutaka <gniibe@fsij.org>
  sec>  2048R/4CA7BABE  created: 2010-10-15  expires: never     
                        card-no: F517 00000001
  ssb>  2048R/084239CF  created: 2010-10-15  expires: never     
                        card-no: F517 00000001
  ssb>  2048R/5BB065DC  created: 2010-10-22  expires: never     
                        card-no: F517 00000001
  
  gpg/card> 

OK, now I can use the Token on this computer.


Update trustdb for the key on Gnuk Token
========================================

Yes, I can use the Token by the public key and the secret key reference to the card.  More, I need to update the trustdb.

To do that I do: ::

  $ gpg --edit-key 4ca7babe
  gpg (GnuPG) 1.4.11; Copyright (C) 2010 Free Software Foundation, Inc.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.
  
  Secret key is available.
  
  pub  2048R/4CA7BABE  created: 2010-10-15  expires: never       usage: SC  
                       trust: unknown       validity: unknown
  sub  2048R/084239CF  created: 2010-10-15  expires: never       usage: E   
  sub  2048R/5BB065DC  created: 2010-10-22  expires: never       usage: A   
  [ unknown] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ unknown] (2)  NIIBE Yutaka <gniibe@debian.org>
  
  gpg> 

See, the key is ``unknown`` state.  Add trust for that. ::

  gpg> trust
  pub  2048R/4CA7BABE  created: 2010-10-15  expires: never       usage: SC  
                       trust: unknown       validity: unknown
  sub  2048R/084239CF  created: 2010-10-15  expires: never       usage: E   
  sub  2048R/5BB065DC  created: 2010-10-22  expires: never       usage: A   
  [ unknown] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ unknown] (2)  NIIBE Yutaka <gniibe@debian.org>
  
  Please decide how far you trust this user to correctly verify other users' keys
  (by looking at passports, checking fingerprints from different sources, etc.)

    1 = I don't know or won't say
    2 = I do NOT trust
    3 = I trust marginally
    4 = I trust fully
    5 = I trust ultimately
    m = back to the main menu
  
  Your decision? 5
  Do you really want to set this key to ultimate trust? (y/N) y
  
  pub  2048R/4CA7BABE  created: 2010-10-15  expires: never       usage: SC  
                       trust: ultimate      validity: unknown
  sub  2048R/084239CF  created: 2010-10-15  expires: never       usage: E   
  sub  2048R/5BB065DC  created: 2010-10-22  expires: never       usage: A   
  [ unknown] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ unknown] (2)  NIIBE Yutaka <gniibe@debian.org>
  Please note that the shown key validity is not necessarily correct
  unless you restart the program.
  
  $ 

Next time I invoke GnuPG, it will be ``ultimate`` key.  Let's see: ::

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
  [ultimate] (2)  NIIBE Yutaka <gniibe@debian.org>
  
  gpg> quit
  $ 
