===========================
Stopping/Resetting SCDAEMON
===========================

There is a daemon named ``scdaemon`` behind gpg-agent, which handles
communication to smartcard/token.

Ideally, we don't need to care about ``scdaemon``, and it should
handle everything automatically.  But, there are some cases (because
of bugs), where we need to talk to the daemon directly, in practice.


How to communicate SCDAEMON
===========================

We have a utility to communicate with a running gpg-agent, that's
gpg-connect-agent.  We can use it to communicate with scdaemon,
as it supports sub-command "SCD", exactly for this purpose. 


Stopping SCDAEMON
=================

To stop SCDAEMON and let it exit, type::

	$ gpg-connect-agent "SCD KILLSCD" "SCD BYE" /bye

Then, you can confirm that there is no SCDAEMON any more by ``ps``
command.


Let GPG-AGENT/SCDAEMON learn
============================

To let gpg-agent/scdaemon learn from Gnuk Token, type::

	$ gpg-connect-agent learn /bye
