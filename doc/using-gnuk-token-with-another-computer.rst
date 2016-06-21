======================================
Using Gnuk Token with another computer
======================================

This document describes how you can use Gnuk Token
on another PC (which is not the one you generate your keys).

Note that the Token only brings your secret keys,
while ``.gnupg`` directory contains keyrings and trustdb, too.


Fetch the public key and connect it to the Token
================================================

In order to use the Token, we need to put the public key and the secret
key references (to the token) under ``.gnupg`` directory.

To do that, invoke GnuPG with ``--card-edit`` option. ::

  Reader ...........: 234B:0000:FSIJ-1.2.0-87193059:0
  Application ID ...: D276000124010200FFFE871930590000
  Version ..........: 2.0
  Manufacturer .....: unmanaged S/N range
  Serial number ....: 87193059
  Name of cardholder: Yutaka Niibe
  Language prefs ...: ja
  Sex ..............: male
  URL of public key : http://www.gniibe.org/gniibe-20150813.asc
  Login data .......: gniibe
  Signature PIN ....: not forced
  Key attributes ...: ed25519 cv25519 ed25519
  Max. PIN lengths .: 127 127 127
  PIN retry counter : 3 3 3
  Signature counter : 0
  Signature key ....: 249C B377 1750 745D 5CDD  323C E267 B052 364F 028D
        created ....: 2015-08-12 07:10:48
  Encryption key....: E228 AB42 0F73 3B1D 712D  E50C 850A F040 D619 F240
        created ....: 2015-08-12 07:10:48
  Authentication key: E63F 31E6 F203 20B5 D796  D266 5F91 0521 FAA8 05B1
        created ....: 2015-08-12 07:16:14
  General key info..: [none]
  
  gpg/card> 

Here, the secret key references (to the token) are created under ``.gnupg/private-keys-v1.d`` directory.  It can be also created when I do ``--card-status`` by GnuPG.

Still, it says that there is no key info related to this token on my PC (``[none]`` for General key info), because I don't have the public key on this PC yet.

So, I fetch the public key from URL specified in the Token. ::

  gpg/card> fetch
  gpg: requesting key E267B052364F028D from http server www.gniibe.org
  gpg: key E267B052364F028D: public key "NIIBE Yutaka <gniibe@fsij.org>" imported
  gpg: Total number processed: 1
  gpg:               imported: 1
  gpg: marginals needed: 3  completes needed: 1  trust model: pgp
  gpg: depth: 0  valid:   6  signed:   0  trust: 0-, 0q, 0n, 0m, 0f, 6u
  
  gpg/card> 

Good.  The public key is now under ``.gnupg`` directory.  We can examine by ``gpg --list-keys``.

When I type return at the ``gpg/card>`` prompt, now, I can see: ::

  Reader ...........: 234B:0000:FSIJ-1.2.0-87193059:0
  Application ID ...: D276000124010200FFFE871930590000
  Version ..........: 2.0
  Manufacturer .....: unmanaged S/N range
  Serial number ....: 87193059
  Name of cardholder: Yutaka Niibe
  Language prefs ...: ja
  Sex ..............: male
  URL of public key : http://www.gniibe.org/gniibe-20150813.asc
  Login data .......: gniibe
  Signature PIN ....: not forced
  Key attributes ...: ed25519 cv25519 ed25519
  Max. PIN lengths .: 127 127 127
  PIN retry counter : 3 3 3
  Signature counter : 0
  Signature key ....: 249C B377 1750 745D 5CDD  323C E267 B052 364F 028D
        created ....: 2015-08-12 07:10:48
  Encryption key....: E228 AB42 0F73 3B1D 712D  E50C 850A F040 D619 F240
        created ....: 2015-08-12 07:10:48
  Authentication key: E63F 31E6 F203 20B5 D796  D266 5F91 0521 FAA8 05B1
        created ....: 2015-08-12 07:16:14
  General key info..: pub  ed25519/E267B052364F028D 2015-08-12 NIIBE Yutaka <gniibe@fsij.org>
  sec>  ed25519/E267B052364F028D  created: 2015-08-12  expires: never     
                                  card-no: FFFE 87193059
  ssb>  cv25519/850AF040D619F240  created: 2015-08-12  expires: never     
                                  card-no: FFFE 87193059
  ssb>  ed25519/5F910521FAA805B1  created: 2015-08-12  expires: never     
                                  card-no: FFFE 87193059

    
  gpg/card> 

Note that, it displays the information about "General key info".

OK, now I can use the Token on this computer.


Update trustdb for the key on Gnuk Token
========================================

Yes, I can use the Token by the public key and the secret
key references to the card.  More, I need to update the trustdb.

To do that, I do: ::

  $ ./gpg --edit-key E267B052364F028D
  gpg (GnuPG) 2.1.13; Copyright (C) 2016 Free Software Foundation, Inc.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.

  Secret key is available.
  
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       card-no: FFFE 87193059
       trust: unknown       validity: unknown
  ssb  cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
       card-no: FFFE 87193059
  ssb  ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
       card-no: FFFE 87193059
  [ unknown] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ unknown] (2)  NIIBE Yutaka <gniibe@debian.org>

See, the key is ``unknown`` state.  Add trust for that, because it's the key under my control. ::

  gpg> trust
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       card-no: FFFE 87193059
       trust: unknown       validity: unknown
  ssb  cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
       card-no: FFFE 87193059
  ssb  ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
       card-no: FFFE 87193059
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
  
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       card-no: FFFE 87193059
       trust: ultimate      validity: unknown
  ssb  cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
       card-no: FFFE 87193059
  ssb  ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
       card-no: FFFE 87193059
  [ unknown] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ unknown] (2)  NIIBE Yutaka <gniibe@debian.org>
  Please note that the shown key validity is not necessarily correct
  unless you restart the program.
  
  gpg> 

And I quit from gpg.  Then, when I invoke GnuPG, it will be ``ultimate`` key.  Let's see: ::

  $ ./gpg --edit-key E267B052364F028D
  gpg (GnuPG) 2.1.13; Copyright (C) 2016 Free Software Foundation, Inc.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.

  Secret key is available.

  gpg: checking the trustdb
  gpg: marginals needed: 3  completes needed: 1  trust model: pgp
  gpg: depth: 0  valid:   7  signed:   0  trust: 0-, 0q, 0n, 0m, 0f, 7u
  sec  ed25519/E267B052364F028D
       created: 2015-08-12  expires: never       usage: SC  
       card-no: FFFE 87193059
       trust: ultimate      validity: ultimate
  ssb  cv25519/850AF040D619F240
       created: 2015-08-12  expires: never       usage: E   
       card-no: FFFE 87193059
  ssb  ed25519/5F910521FAA805B1
       created: 2015-08-12  expires: never       usage: A   
       card-no: FFFE 87193059
  [ultimate] (1). NIIBE Yutaka <gniibe@fsij.org>
  [ultimate] (2)  NIIBE Yutaka <gniibe@debian.org>

  gpg> quit
  $

OK, all set.  I'm ready to use my Gnuk Token on this PC.
