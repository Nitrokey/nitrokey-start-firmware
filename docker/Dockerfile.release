FROM debian:latest
LABEL Description="Image for building gnuK"

RUN apt update -y && apt install -y make gcc-arm-none-eabi && apt clean

CMD ["/bin/sh", "-c", "cd /gnuk/src && make clean && ./configure $GNUK_CONFIG && make"]
