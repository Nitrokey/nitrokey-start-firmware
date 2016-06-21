==========================================
Set up your passphrase for your Gnuk Token
==========================================

Terminology
===========

In the OpenPGPcard specification, there are two passwords: one is
user-password and another is admin-password.  In the specification,
user-password is refered as PW1, and admin-password is refered as PW3.
Besides, there is reset code, which enable a user to reset PW1.

Note that people sometimes use different words than "password" to
refer same thing, in GnuPG and its applications.  For example, the
output explained above includes the word "PIN" (Personal
Identification Number), and the helper program for input is named
"pinentry".  Note that it is OK (and recommended) to include
characters other than digits for the case of OpenPGPcard.

Besides, some people sometimes prefer the word "passphrase" to
"password", as it can encourage to have longer string, but it means
same thing and it just refer user-password or admin-password.


Set up PW1 and PW3
==================

Invoke GnuPG with the option ``--card-edit``.  ::

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

It shows the status of the card (as same as the output of ``gpg --card-status``).

Then, GnuPG enters its own command interaction mode.  The prompt is ``gpg/card>``.

Firstly, I change PIN of card user from factory setting (of "123456").
Note that, by only changing user's PIN, it enables "admin less mode" of Gnuk.
"Admin less mode" means that admin password will become same one of user's.
That is, PW1 = PW3.
Note that *the length of PIN should be more than (or equals to) 8* for
"admin less mode".  ::

  gpg/card> passwd
  gpg: OpenPGP card no. D276000124010200FFFE871930590000 detected
  
  Please enter the PIN
  Enter PIN: 123456
             
  New PIN
  Enter New PIN: <PASSWORD-OF-GNUK>
                 
  New PIN
  Repeat this PIN: <PASSWORD-OF-GNUK>
  PIN changed.

The "admin less mode" is Gnuk only feature, not defined in the
OpenPGPcard specification.  By using "admin less mode", it will be
only a sigle password for user to memorize, and it will be easier if a token
is used by an individual.

(If you want normal way ("admin full mode" in Gnuk's term),
that is, user-password *and* admin-password independently,
please change admin-password at first.
Then, the token works as same as OpenPGPcard specification
with regards to PW1 and PW3.)


Set up of reset code (optional)
===============================

Lastly, we can setup reset code, entering admin mode.

Having reset code, we can unblock the token when the token will be blocked
(by wrong attempts to entering passphrase).  Note that this is optional step.

When reset code is known to someone, that person can try to guess your passphrase of PW1 more times by unblocking the token.  So, I don't use this feature by myself.

If we do, here is the interaction. ::

  gpg/card> admin
  Admin commands are allowed
  
  gpg/card> passwd
  gpg: OpenPGP card no. D276000124010200FFFE871930590000 detected
  
  1 - change PIN
  2 - unblock PIN
  3 - change Admin PIN
  4 - set the Reset Code
  Q - quit
  
  Your selection? 4
  gpg: 3 Admin PIN attempts remaining before card is permanently locked
  
  Please enter the Admin PIN
  Enter Admin PIN: <PASSWORD-OF-GNUK>
  
  New Reset Code
  Enter New PIN: <RESETCODE-OF-GNUK>
  
  New Reset Code
  Repeat this PIN: <RESETCODE-OF-GNUK>
  Reset Code set.
  
  1 - change PIN
  2 - unblock PIN
  3 - change Admin PIN
  4 - set the Reset Code
  Q - quit
  
  Your selection? q

Then, I quit. ::

  gpg/card> quit

That's all in this step.
