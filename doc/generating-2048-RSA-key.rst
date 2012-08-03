============================
Generating 2048-bit RSA keys
============================

This document describes how I generate 2048-bit RSA keys.

.. BREAK

Here is the log to generate signature key and encryption subkey.

I invoke GnuPG with ``--gen-key`` option. ::

  $ gpg --gen-key
  gpg (GnuPG) 1.4.11; Copyright (C) 2010 Free Software Foundation, Inc.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.

and GnuPG asks kind of key.  Select ``RSA and RSA``. ::

  Please select what kind of key you want:
     (1) RSA and RSA (default)
     (2) DSA and Elgamal
     (3) DSA (sign only)
     (4) RSA (sign only)
  Your selection? 1
  RSA keys may be between 1024 and 4096 bits long.

and select 2048-bit (as Gnuk Token only suppurt this). ::

  What keysize do you want? (2048) 
  Requested keysize is 2048 bits

and select expiration of the key. ::

  Please specify how long the key should be valid.
           0 = key does not expire
        <n>  = key expires in n days
        <n>w = key expires in n weeks
        <n>m = key expires in n months
        <n>y = key expires in n years
  Key is valid for? (0) 0
  Key does not expire at all

Confirm key types, bitsize and expiration. ::

  Is this correct? (y/N) y

Then enter user ID. ::

  You need a user ID to identify your key; the software constructs the user ID
  from the Real Name, Comment and Email Address in this form:
      "Heinrich Heine (Der Dichter) <heinrichh@duesseldorf.de>"
  
  Real name: Niibe Yutaka
  Email address: gniibe@fsij.org
  Comment: 
  You selected this USER-ID:
      "Niibe Yutaka <gniibe@fsij.org>"
  
  Change (N)ame, (C)omment, (E)mail or (O)kay/(Q)uit? o

and enter passphrase for this **key on PC**. ::

  You need a Passphrase to protect your secret key.
  <PASSWORD-KEY-ON-PC>

Then, GnuPG generate keys.  It takes some time.  ::

  We need to generate a lot of random bytes. It is a good idea to perform
  some other action (type on the keyboard, move the mouse, utilize the
  disks) during the prime generation; this gives the random number
  generator a better chance to gain enough entropy.
  ...+++++
  +++++
  We need to generate a lot of random bytes. It is a good idea to perform
  some other action (type on the keyboard, move the mouse, utilize the
  disks) during the prime generation; this gives the random number
  generator a better chance to gain enough entropy.
  ..+++++
  
  Not enough random bytes available.  Please do some other work to give
  the OS a chance to collect more entropy! (Need 15 more bytes)
  ...+++++
  gpg: key 28C0CD7C marked as ultimately trusted
  public and secret key created and signed.
  
  gpg: checking the trustdb
  gpg: 3 marginal(s) needed, 1 complete(s) needed, PGP trust model
  gpg: depth: 0  valid:   2  signed:   0  trust: 0-, 0q, 0n, 0m, 0f, 2u
  pub   2048R/28C0CD7C 2011-05-24
        Key fingerprint = 0B4D C763 D57B ADBB 1870  A978 BDEE 4A35 28C0 CD7C
  uid                  Niibe Yutaka <gniibe@fsij.org>
  sub   2048R/F01E19B7 2011-05-24
  $ 

Done.

Then, I create authentication subkey.  Authentication subkey is not that common, but very useful (say, for SSH authentication).  As it is not that common, we need ``--expert`` option for GnuPG. ::

  $ gpg --expert --edit-key 28C0CD7C
  gpg (GnuPG) 1.4.11; Copyright (C) 2010 Free Software Foundation, Inc.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.
  
  Secret key is available.
  
  pub  2048R/28C0CD7C  created: 2011-05-24  expires: never       usage: SC  
                       trust: ultimate      validity: ultimate
  sub  2048R/F01E19B7  created: 2011-05-24  expires: never       usage: E   
  [ultimate] (1). Niibe Yutaka <gniibe@fsij.org>
  
  gpg> 

Here, I enter ``addkey`` command.  Then, I enter the passphrase of **key on PC**, I specified above. ::

  gpg> addkey
  Key is protected.
    
  You need a passphrase to unlock the secret key for
  user: "Niibe Yutaka <gniibe@fsij.org>"
  2048-bit RSA key, ID 28C0CD7C, created 2011-05-24
  <PASSWORD-KEY-ON-PC>
  gpg: gpg-agent is not available in this session

GnuPG askes kind of key.  I select ``RSA (set your own capabilities)``. ::

  Please select what kind of key you want:
     (3) DSA (sign only)
     (4) RSA (sign only)
     (5) Elgamal (encrypt only)
     (6) RSA (encrypt only)
     (7) DSA (set your own capabilities)
     (8) RSA (set your own capabilities)
  Your selection? 8

And select ``Authenticate`` for the capabilities for this key.   Initially, it's ``Sign`` and  ``Encrypt``.  I need to deselect ``Sign`` and ``Encryp``, and select ``Authenticate``.  To do that, I enter ``s``, ``a``, and ``e``.  ::

  Possible actions for a RSA key: Sign Encrypt Authenticate 
  Current allowed actions: Sign Encrypt 
  
     (S) Toggle the sign capability
     (E) Toggle the encrypt capability
     (A) Toggle the authenticate capability
     (Q) Finished
  
  Your selection? s
  
  Possible actions for a RSA key: Sign Encrypt Authenticate 
  Current allowed actions: Encrypt 
  
     (S) Toggle the sign capability
     (E) Toggle the encrypt capability
     (A) Toggle the authenticate capability
     (Q) Finished
  
  Your selection? a
  
  Possible actions for a RSA key: Sign Encrypt Authenticate 
  Current allowed actions: Encrypt Authenticate 

     (S) Toggle the sign capability
     (E) Toggle the encrypt capability
     (A) Toggle the authenticate capability
     (Q) Finished
  
  Your selection? e
  
  Possible actions for a RSA key: Sign Encrypt Authenticate 
  Current allowed actions: Authenticate 

     (S) Toggle the sign capability
     (E) Toggle the encrypt capability
     (A) Toggle the authenticate capability
     (Q) Finished

OK, I set the capability of ``Authenticate``.  I enter ``q`` to finish setting capabilities. ::

  Your selection? q

GnuPG asks bitsize and expiration, I enter 2048 for bitsize and no expiration.  Then, I confirm that I really create the key. ::

  RSA keys may be between 1024 and 4096 bits long.
  What keysize do you want? (2048) 
  Requested keysize is 2048 bits
  Please specify how long the key should be valid.
           0 = key does not expire
        <n>  = key expires in n days
        <n>w = key expires in n weeks
        <n>m = key expires in n months
        <n>y = key expires in n years
  Key is valid for? (0) 0
  Key does not expire at all
  Is this correct? (y/N) y
  Really create? (y/N) y

Then, GnuPG generate the key. ::

  We need to generate a lot of random bytes. It is a good idea to perform
  some other action (type on the keyboard, move the mouse, utilize the
  disks) during the prime generation; this gives the random number
  generator a better chance to gain enough entropy.
  .......+++++
  +++++

  pub  2048R/28C0CD7C  created: 2011-05-24  expires: never       usage: SC  
                       trust: ultimate      validity: ultimate
  sub  2048R/F01E19B7  created: 2011-05-24  expires: never       usage: E   
  sub  2048R/B8929606  created: 2011-05-24  expires: never       usage: A   
  [ultimate] (1). Niibe Yutaka <gniibe@fsij.org>

  gpg> 

I save the key. ::

  gpg> save
  $ 

Now, we have three keys (one primary key for signature and certification, subkey for encryption, and another subkey for authentication).


Publishing public key
=====================

I make a file for my public key by ``--export`` option of GnuPG. ::

  $ gpg --armor --output gniibe.asc --export 4CA7BABE

and put it at: http://www.gniibe.org/gniibe.asc
