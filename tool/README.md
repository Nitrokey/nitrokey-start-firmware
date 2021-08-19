
### Nitrokey Start tools

This directory hosts various tools, most inherited from the upstream. These are not actively maintained. Some of them are incorporated and maintained in the [pynitrokey] tool. Using them through the latter is recommended.

[pynitrokey]: https://github.com/Nitrokey/pynitrokey/

### Setup
Before using tools located here please install missing dependencies, as presented in either
- requirements.txt (e.g. with Pip),
- Pipfile (e.g. with pipenv).

Detailed instructions below.

#### Solution one (the fastest, Ubuntu 18.04+ and derivatives):
```bash
sudo apt install python3-cffi python3-usb
```

#### Solution two, using `pipenv` (for develoment, distribution agnostic):
While being in the `nitrokey-start-firmware/tool` directory:
```bash
git pull # to update the repository
# in case Pip is not installed
# sudo apt install python3-pip 
pip3 install pipenv --user
pipenv install --three
```

#### Solution three, using `pip` (classic way):
While being in the `nitrokey-start-firmware/tool` directory:

```bash
git pull # to update the repository
# in case Pip is not installed
# sudo apt install python3-pip 
pip3 install -r requirements.txt --user
```

