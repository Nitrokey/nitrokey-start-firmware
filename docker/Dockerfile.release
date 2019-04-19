FROM debian:latest
LABEL Description="Image for building gnuK"

RUN apt update -y && apt install -y make gcc-arm-none-eabi && apt clean

# takes 100 MB of space more
RUN apt install -y git && apt clean

WORKDIR /gnuk/

CMD ["/bin/sh", "-c", "cd /gnuk/src && ./configure $GNUK_CONFIG && cd /gnuk/regnual && make clean && make && cd /gnuk/src && make clean && ./configure $GNUK_CONFIG && make && mkdir -p /gnuk/release/`git describe --long`/ && cp /gnuk/src/build/gnuk.bin /gnuk/regnual/regnual.bin /gnuk/release/`git describe --long`/ -v"]
