import pexpect
import sys

PROMPT = 'gpg/card> '

child = pexpect.spawn('gpg --card-edit')
child.logfile = sys.stdout.buffer

child.expect(PROMPT)
child.sendline('admin')
child.expect('.*Admin commands are allowed.*')
child.sendline('factory-reset')
child.expect('.*Continue?.*')
child.sendline('y')
child.expect('.*Really.*')
child.sendline('yes')
child.expect(PROMPT)
child.sendline('')
child.expect(PROMPT)
child.sendline('exit')