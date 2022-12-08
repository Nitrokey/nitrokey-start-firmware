# Release

Notes for the release process.

## Tools

### Test Update

Update the device to the built firmware using the latest nitropy:
```shell
make update UPDATETOOL="pipx run pynitrokey start update"
````

Remove `UPDATETOOL` to just use `nitropy`, which is the default tool name.

### Test Flashing


Flash the chosen firmware file:
```shell
make flash FLASH_FILE=../prebuilt/RTM.10/gnuk.hex
```

Remove `FLASH_FILE` to flash the last built firmware.

### Troubleshooting

In case debug adapter is responding that the target device is not reachable, try to bombard it with requests - this helps:
```bash
while true; do make flash; sleep 0.05; done
```

This could happen, when the `RDP` flag is set by the firmware, and after that the debug adapter can have bad time reconnecting to it.

## Tests

1. Test direct flashing
2. Tests update firmware from:
   - HW3/RTM.10
   - HW4/RTM.11 (optional)
   - HW5/RTM.12

Execute tests for the given commit, coming from the merged GNUK release.
Do so with using pytest reporter for a nice output:

```shell
# setup
pip install pytest-reporter
pip install pytest-reporter-html1
# execution
pytest  --template=html1/index.html --report=reports/report-rtm10-to-rtm13.rc3-hw3-updated.html -v
pytest  --template=html1/index.html --report=reports/report-rtm10-to-rtm13.rc3-hw3-updated-ident.html -v test_multiple_identities.py
```
