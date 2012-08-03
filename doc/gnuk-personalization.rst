=============================
Personalization of Gnuk Token
=============================


Personalize your Gnuk Token
===========================

Invoke GnuPG with the option ``--card-edit``.  ::

  $ gpg --card-edit
  gpg: detected reader `FSIJ Gnuk (0.12-34006E06) 00 00'
  Application ID ...: D276000124010200F517000000010000
  Version ..........: 2.0
  Manufacturer .....: FSIJ
  Serial number ....: 00000001
  Name of cardholder: [not set]
  Language prefs ...: [not set]
  Sex ..............: unspecified
  URL of public key : [not set]
  Login data .......: [not set]
  Signature PIN ....: forced
  Key attributes ...: 2048R 2048R 2048R
  Max. PIN lengths .: 127 127 127
  PIN retry counter : 3 3 3
  Signature counter : 0
  Signature key ....: [none]
  Encryption key....: [none]
  Authentication key: [none]
  General key info..: [none]

It shows the status of the card (as same as the output of ``gpg --card-status``).  It shows token's name and its USB serial string (0.12-34006E06) from PC/SC-lite.

Then, GnuPG enters its own command interaction mode.  The prompt is ``gpg/card>``.

Firstly, I change PIN of card user from factory setting (of "123456").  Note that, only changing PIN of user enables "admin less mode" of Gnuk.  Admin password will become same one of user's. ::

  gpg/card> passwd
  gpg: OpenPGP card no. D276000124010200F517000000010000 detected
  
  Please enter the PIN
  Enter PIN: 123456
             
  New PIN
  Enter New PIN: <PASSWORD-OF-GNUK>
                 
  New PIN
  Repeat this PIN: <PASSWORD-OF-GNUK>
  PIN changed.

Secondly, enabling admin command, I put name of mine.  Note that I input user's PIN (which I set above) here, because it is "admin less mode". ::

  gpg/card> admin
  Admin commands are allowed
  
  gpg/card> name
  Cardholder's surname: Niibe
  Cardholder's given name: Yutaka
  gpg: 3 Admin PIN attempts remaining before card is permanently locked
  
  Please enter the Admin PIN
  Enter Admin PIN: <PASSWORD-OF-GNUK>

Thirdly, I put some other informations, such as language, sex, login, and URL.  URL specifies the place where I put my public keys. ::

  gpg/card> lang
  Language preferences: ja
  
  gpg/card> sex
  Sex ((M)ale, (F)emale or space): m
  
  gpg/card> url
  URL to retrieve public key: http://www.gniibe.org/gniibe.asc
  
  gpg/card> login
  Login data (account name): gniibe

Since I don't force PIN input everytime, toggle it to non-force-pin-for-signature. ::

  gpg/card> forcesig

Lastly, I setup reset code.  This is optional. ::

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
