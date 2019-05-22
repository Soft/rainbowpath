FROM alpine:latest

RUN addgroup -g 2000 builder && \
  adduser -u 2000 -G builder -D builder && \
  apk add \
  autoconf \
  automake \
  gcc \
  libc-dev \
  make \
  ncurses-dev \
  ncurses-static \
  pkgconf

USER builder

WORKDIR /home/builder/work
