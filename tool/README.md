Before using tools located here please install missing dependencies, as presented in either
- requirements.txt (e.g. with Pip),
- Pipfile (e.g. with pipenv).

Detailed instructions below.

#### Solution one:
```
apt install python3-cffi python3-usb
```

#### Solution two, using `pipenv`:
While being in the `nitrokey-start-firmware/tool` directory:
```
git pull # to update the repository
# in case Pip is not installed
# sudo apt install python3-pip 
pip3 install pipenv
pipenv install --three
```

#### Solution three, using `pip`:
While being in the `nitrokey-start-firmware/tool` directory:
```
git pull # to update the repository
# in case Pip is not installed
# sudo apt install python3-pip 
pip3 install -r requirements.txt
```

