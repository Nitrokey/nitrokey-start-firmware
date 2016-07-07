============================
Generating 2048-bit RSA keys
============================

In this section, we describe how to generate 2048-bit RSA keys.


Key length of RSA
=================

In 2005, NIST (National Institute of Standards and Technology, USA)
issued the first revision of NIST Special Publication 800-57, 
"Recommendation for Key Management".

In 800-57, NIST advises that 1024-bit RSA keys will no longer be
viable after 2010 and advises moving to 2048-bit RSA keys.  NIST
advises that 2048-bit keys should be viable until 2030.

As of 2016, GnuPG's default for generating RSA key is 2048-bit.

Some people have preference on RSA 4096-bit keys, considering
"longer is better".

However, "longer is better" is not always true.  When it's long, it
requires more computational resource, memory, and storage.  Further,
it consumes more power for nomal usages.  These days, many people has
enough computational resource, that would be true, but less is better
for power consumption, isn't it?

For security, the key length is just a single factor.  We had and will have
algorithm issues, too.  It is true that it's difficult to update
our public keys, but this problem wouldn't be solved by just having
longer keys.

We deliberately recommend use of RSA 2048-bit keys for Gnuk,
considering device computation power and host software constraints.

Thus, the key size is 2048-bit in the examples below.


Generating keys on host PC
==========================

Here is the example session to generate main key and a subkey for encryption.

I invoke GnuPG with ``--quick-gen-key`` option. ::

  $ gpg --quick-gen-key "Niibe Yutaka <gniibe@fsij.org>"
  About to create a key for:
      "Niibe Yutaka <gniibe@fsij.org>"

  Continue? (Y/n) y

It askes passphrase for this **key on host PC**.
Note that this is a passphrase for the key on host PC.
It is different thing to the passphrase of Gnuk Token.
We enter two same inputs two times
(once for passphrase input, and another for confirmation),
<PASSWORD-KEY-ON-PC>.

Then, GnuPG generate keys.  It takes some time.  ::
  
  We need to generate a lot of random bytes. It is a good idea to perform
  some other action (type on the keyboard, move the mouse, utilize the
  disks) during the prime generation; this gives the random number
  generator a better chance to gain enough entropy.
  gpg: key 76A9392B02CD15D1 marked as ultimately trusted
  gpg: revocation certificate stored as '/home/gniibe.gnupg/openpgp-revocs.d/36CE0B8408CFE5CD07F94ACF76A9392B02CD15D1.rev'
  public and secret key created and signed.

  gpg: checking the trustdb
  gpg: marginals needed: 3  completes needed: 1  trust model: pgp
  gpg: depth: 0  valid:   1  signed:   0  trust: 0-, 0q, 0n, 0m, 0f, 1u
  pub   rsa2048 2016-06-20 [S]
        36CE0B8408CFE5CD07F94ACF76A9392B02CD15D1
  uid           [ultimate] Niibe Yutaka <gniibe@fsij.org>
  sub   rsa2048 2016-06-20 []

Done.

Then, we create authentication subkey.
Authentication subkey is not that common,
but very useful (for SSH authentication).
As it is not that common, we need ``--expert`` option for GnuPG. ::

  gpg (GnuPG) 2.1.13; Copyright (C) 2016 Free Software Foundation, Inc.
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.

  Secret key is available.

  sec  rsa2048/76A9392B02CD15D1
       created: 2016-06-20  expires: never       usage: SC  
       trust: ultimate      validity: ultimate
  ssb  rsa2048/4BD1EB26F0E607E6
       created: 2016-06-20  expires: never       usage: E   
  [ultimate] (1). Niibe Yutaka <gniibe@fsij.org>
  
  gpg> 

Here, it displays that there are main key and a subkey.
It prompts sub-command with ``gpg>`` .

Here, we enter ``addkey`` sub-command.

  gpg> addkey
    
GnuPG asks kind of key.  We select ``RSA (set your own capabilities)``. ::

  Please select what kind of key you want:
     (3) DSA (sign only)
     (4) RSA (sign only)
     (5) Elgamal (encrypt only)
     (6) RSA (encrypt only)
     (7) DSA (set your own capabilities)
     (8) RSA (set your own capabilities)
    (10) ECC (sign only)
    (11) ECC (set your own capabilities)
    (12) ECC (encrypt only)
    (13) Existing key
  Your selection? 8

And select ``Authenticate`` for the capabilities for this key.
Initially, it's ``Sign`` and  ``Encrypt``.
I need to deselect ``Sign`` and ``Encrypt``, and select ``Authenticate``.
To do that, I enter ``s``, ``e``, and ``a``.  ::

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

Then, it askes the passphrase, it is the passphrase of **key on host PC**.
It's the one we entered above as <PASSWORD-KEY-ON-PC>.

Then, GnuPG generate the key. ::

  We need to generate a lot of random bytes. It is a good idea to perform
  some other action (type on the keyboard, move the mouse, utilize the
  disks) during the prime generation; this gives the random number
  generator a better chance to gain enough entropy.

  sec  rsa2048/76A9392B02CD15D1
       created: 2016-06-20  expires: never       usage: SC  
       trust: ultimate      validity: ultimate
  ssb  rsa2048/4BD1EB26F0E607E6
       created: 2016-06-20  expires: never       usage: E   
  ssb  rsa2048/F3BA52C64012198D
       created: 2016-06-20  expires: never       usage: A   
  [ultimate] (1). Niibe Yutaka <gniibe@fsij.org>

  gpg> 

We save the key (to the storage of the host PC). ::

  gpg> save
  $ 

Now, we have three keys (one primary key for signature and certification,
subkey for encryption, and another subkey for authentication).


Publishing public key
=====================

We make a file for the public key by ``--export`` option of GnuPG. ::

  $ gpg --armor --output <YOUR-KEY>.asc --export <YOUR-KEY-ID>

We can publish the file by web server.  Or we can publish the key
to a keyserver, by invoking GnuPG with ``--send-keys`` option.  ::

  $ gpg --keyserver pool.sks-keyservers.net --send-keys <YOUR-KEY-ID>

Here, pool.sks-keyservers.net is a keyserver, which is widely used.


Backup the private key
======================

There are some ways to back up private key, such that backup .gnupg
directory entirely, or use of paperkey, etc.
Here, we describe backup by ASCII file.
ASCII file is good, because it has less risk on transfer.
Binary file has a risk to be modified on transfer.

Note that the key on host PC is protected by passphrase (which
is <PASSWORD-KEY-ON-PC> in the example above).  Using the key
from the backup needs this passphrase.  It is common that
people will forget passphrase for backup.  Never forget it.
You have been warned.

To make ASCII backup for private key,
invokde GnuPG with ``--armor`` option and ``--export-secret-keys``
specifying the key identifier. ::

  $ gpg --armor --output <YOUR-SECRET>.asc --export-secret-keys <YOUR-KEY-ID>

From the backup,
we can recover privet key by invoking GnuPG with ``--import`` option. ::

  $ gpg --import <YOUR-SECRET>.asc
