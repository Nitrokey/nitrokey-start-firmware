============================
Generating 2048-bit RSA keys
============================

In this section, we describe how to generate 2048-bit RSA keys.


Key length of RSA
=================

In 2005, NIST (National Institute of Standards and Technology, USA)
has issued the first revision of NIST Special Publication 800-57, 
"Recommendation for Key Management".

In 800-57, NIST advises that 1024-bit RSA keys will no longer be
viable after 2010 and advises moving to 2048-bit RSA keys.  NIST
advises that 2048-bit keys should be viable until 2030.

As of 2010, GnuPG's default for generating RSA key is 2048-bit.

Some people have preference on RSA 4096-bit keys, considering
"longer is better".

However, "longer is better" is not always true.  When it's long, it
requires more computational resource, memory and storage, and it
consumes more power for nomal usages.  These days, many people has
enough computational resource, that would be true, but less is better
for power consumption.

For security, the key length is a single factor.  We had and will have
algorithm issues, too.  It is true that it's difficult to update
our public keys, but this problem wouldn't be solved by just have
longer keys.

We deliberately support only RSA 2048-bit keys for Gnuk, considering
device computation power and host software constraints.

Thus, the key size is 2048-bit in the examples below.

Generating keys on host PC
==========================

Here is the example session to generate main key and a subkey for encryption.

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

and select 2048-bit (as Gnuk Token only supports this). ::

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

and enter passphrase for this **key on host PC**.
Note that this is a passphrase for the key on host PC.
It is different thing to the password of Gnuk Token.

We enter two same inputs two times
(once for passphrase input, and another for confirmation). ::

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
  pub   2048R/4CA7BABE 2010-10-15
        Key fingerprint = 1241 24BD 3B48 62AF 7A0A  42F1 00B4 5EBD 4CA7 BABE
  uid                  Niibe Yutaka <gniibe@fsij.org>
  sub   2048R/084239CF 2010-10-15
  $ 

Done.

Then, we create authentication subkey.  Authentication subkey is not that common, but very useful (for SSH authentication).  As it is not that common, we need ``--expert`` option for GnuPG. ::

  $ gpg --expert --edit-key 4CA7BABE
  gpg (GnuPG) 1.4.11; Copyright (C) 2010 Free Software Foundation, Inc.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.
  
  Secret key is available.
  
  pub  2048R/4CA7BABE  created: 2010-10-15  expires: never       usage: SC  
                       trust: ultimate      validity: ultimate
  sub  2048R/084239CF  created: 2010-10-15  expires: never       usage: E   
  [ultimate] (1). Niibe Yutaka <gniibe@fsij.org>
  
  gpg> 

Here, it displays that there are main key and a subkey.
It prompts sub-command with ``gpg>`` .

Here, we enter ``addkey`` sub-command.
Then, we enter the passphrase of **key on host PC**.
It's the one we entered above as <PASSWORD-KEY-ON-PC>. ::

  gpg> addkey
  Key is protected.
    
  You need a passphrase to unlock the secret key for
  user: "Niibe Yutaka <gniibe@fsij.org>"
  2048-bit RSA key, ID 4CA7BABE, created 2010-10-15
  <PASSWORD-KEY-ON-PC>
  gpg: gpg-agent is not available in this session

GnuPG asks kind of key.  We select ``RSA (set your own capabilities)``. ::

  Please select what kind of key you want:
     (3) DSA (sign only)
     (4) RSA (sign only)
     (5) Elgamal (encrypt only)
     (6) RSA (encrypt only)
     (7) DSA (set your own capabilities)
     (8) RSA (set your own capabilities)
  Your selection? 8

And select ``Authenticate`` for the capabilities for this key.   Initially, it's ``Sign`` and  ``Encrypt``.  I need to deselect ``Sign`` and ``Encryp``, and select ``Authenticate``.  To do that, I enter ``s``, ``e``, and ``a``.  ::

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
  
  Your selection? e
  
  Possible actions for a RSA key: Sign Encrypt Authenticate 
  Current allowed actions: 

     (S) Toggle the sign capability
     (E) Toggle the encrypt capability
     (A) Toggle the authenticate capability
     (Q) Finished
  
  Your selection? a
  
  Possible actions for a RSA key: Sign Encrypt Authenticate 
  Current allowed actions: Authenticate 
  
     (S) Toggle the sign capability
     (E) Toggle the encrypt capability
     (A) Toggle the authenticate capability
     (Q) Finished

OK, we set the capability of ``Authenticate``.
We enter ``q`` to finish setting capabilities. ::

  Your selection? q

GnuPG asks bitsize and expiration, we enter 2048 for bitsize and no expiration.
Then, we confirm that we really create the key. ::

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

  pub  2048R/4CA7BABE  created: 2010-10-15  expires: never       usage: SC  
                       trust: ultimate      validity: ultimate
  sub  2048R/084239CF  created: 2010-10-15  expires: never       usage: E   
  sub  2048R/5BB065DC  created: 2010-10-22  expires: never       usage: A   
  [ultimate] (1). Niibe Yutaka <gniibe@fsij.org>

  gpg> 

We save the key (to the storage of the host PC. ::

  gpg> save
  $ 

Now, we have three keys (one primary key for signature and certification, subkey for encryption, and another subkey for authentication).


Publishing public key
=====================

We make a file for the public key by ``--export`` option of GnuPG. ::

  $ gpg --armor --output <YOUR-KEY>.asc --export <YOUR-KEY-ID>

We can publish the file by web server.  Or we can publish it
to a keyserver, by invoking GnuPG with ``--send-keys`` option.  ::

  $ gpg --keyserver pool.sks-keyservers.net --send-keys <YOUR-KEY-ID>

Here, pool.sks-keyservers.net is a keyserver, widely used.
