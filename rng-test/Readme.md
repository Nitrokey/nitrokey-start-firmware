# RNG tests

This Makefile allows for a quick RNG output tests using standard tooling:
- rngcheck
- dieharder
- ent

## Setup

Following are setup lines required to run under Ubuntu 20.04:
```text
$ sudo apt install ent rng-tools dieharder
$ pip3 install --user -U pynitrokey
```

## Run
Reports will be produced after executing regular `make` command.

## Notes
Failing tests coming from dieharder may come from too small sample. It seems to require by design hundreds of gigabits to perform its checks. Currently, the random sample given to it is re-read multiple times for the results' calculation.

## References
- https://github.com/trezor/rng-test
- https://github.com/solokeys/solo/issues/154
- https://medium.com/unitychain/provable-randomness-how-to-test-rngs-55ac6726c5a3
- https://itechlabs.com/certification-services/rng-testing-certification/
- https://stackoverflow.com/questions/2130621/how-to-test-a-random-generator
- https://webhome.phy.duke.edu/~rgb/General/dieharder.php


### Results from Solo
As provided in:
- https://github.com/solokeys/solo/issues/154
```text

% solo key rng raw | rngtest -c 1000
rngtest 2-unofficial-mt.14
Copyright (c) 2004 by Henrique de Moraes Holschuh
This is free software; see the source for copying conditions.  There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

rngtest: starting FIPS tests...
rngtest: bits received from input: 20000032
rngtest: FIPS 140-2 successes: 999
rngtest: FIPS 140-2 failures: 1
rngtest: FIPS 140-2(2001-10-10) Monobit: 0
rngtest: FIPS 140-2(2001-10-10) Poker: 0
rngtest: FIPS 140-2(2001-10-10) Runs: 1
rngtest: FIPS 140-2(2001-10-10) Long run: 0
rngtest: FIPS 140-2(2001-10-10) Continuous run: 0
rngtest: input channel speed: (min=44.639; avg=82.640; max=19531250.000)Kibits/s
rngtest: FIPS tests speed: (min=4.994; avg=23.831; max=88.303)Mibits/s
rngtest: Program run time: 237760016 microseconds
solo key rng raw  16.45s user 3.82s system 8% cpu 3:58.20 total
rngtest -c 1000  0.80s user 0.02s system 0% cpu 3:57.76 total

% time solo key rng raw|head -c 1048576 > random-MiB
solo key rng raw  7.06s user 2.22s system 9% cpu 1:40.32 total
head -c 1048576 > random-MiB  0.03s user 0.00s system 0% cpu 1:39.89 total

% ent random-MiB
Entropy = 7.999844 bits per byte.

Optimum compression would reduce the size
of this 1048576 byte file by 0 percent.

Chi square distribution for 1048576 samples is 226.57, and randomly
would exceed this value 89.96 percent of the times.

Arithmetic mean value of data bytes is 127.4872 (127.5 = random).
Monte Carlo value for Pi is 3.141964500 (error 0.01 percent).
Serial correlation coefficient is -0.000115 (totally uncorrelated = 0.0).

% ent -b random-MiB
Entropy = 1.000000 bits per bit.

Optimum compression would reduce the size
of this 8388608 bit file by 0 percent.

Chi square distribution for 8388608 samples is 0.39, and randomly
would exceed this value 53.47 percent of the times.

Arithmetic mean value of data bits is 0.5001 (0.5 = random).
Monte Carlo value for Pi is 3.141964500 (error 0.01 percent).
Serial correlation coefficient is 0.000192 (totally uncorrelated = 0.0).

```
