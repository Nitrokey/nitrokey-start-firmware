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


Set up PW1, PW3 and reset code
==============================

Invoke GnuPG with the option ``--card-edit``.  ::

  $ gpg --card-edit
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
  Signature counter : 0
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

It shows the status of the card (as same as the output of ``gpg --card-status``).

Then, GnuPG enters its own command interaction mode.  The prompt is ``gpg/card>``.

Firstly, I change PIN of card user from factory setting (of "123456").
Note that, by only changing user's PIN, it enables "admin less mode" of Gnuk.
"Admin less mode" means that admin password will become same one of user's.
That is, PW1 = PW3.
Note that *the length of PIN should be more than (or equals to) 8* for
"admin less mode".  ::

  gpg/card> passwd
  gpg: OpenPGP card no. D276000124010200F517000000010000 detected
  
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

Lastly, I setup reset code, entering admin mode.
Having reset code, you can unblock PIN when the token will be blocked
(by wrong attempt to entering PIN).  This is optional step. ::

  gpg/card> admin
  Admin commands are allowed
  
  gpg/card> passwd
  gpg: OpenPGP card no. D276000124010200F517000000010000 detected
  
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

That's all.
