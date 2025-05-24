#!/bin/sh
test -z "$CC" && CC=`which arm-linux-gnueabi-gcc-9 2>/dev/null`
test -z "$CC" && CC=`which arm-linux-gnueabi-gcc 2>/dev/null`
test -z "$CC" && { echo Error: install arm-linux-gnueabi-gcc; exit 1; }
test -z "$1" -o "$1" = "-h" && { echo "Syntax: $0  -o foo.so foo.c"; exit 1; }

"$CC" -mfloat-abi=soft -nostdlib -shared -fPIC \
  -Wl,--dynamic-linker=/lib/ld-linux.so.3 \
  -Wl,-rpath=/lib \
  -I./glibc-2.30/build/local_install/include \
  -L./glibc-2.30/build/local_install/lib \
  $*
