# Commands

Copy-paste friendly commands used in the workshop

## Prerequisites

1. Have your favorite binary analysis tool ready!

2. Download the docker container for this workshop that has everything needed:
```
docker pull vanhauser/workshop
```

Note that this container has a AFL++ QEMU emulator compiled for ARM.
This was built like this:
```
git clone https://github.com/AFLplusplus/AFLplusplus afl++
cd afl++
git checkout dev
make
cd qemu_mode
export CPU_TARGET=arm
./build_qemu_support.sh
cd ..
sudo make install
```

## Unpack and analyze the target

```
unzip FW_EBM68_300610244384.zip
binwalk -e -M EBM68_3.0.0.6_102_44384-g304340a_370-g24e51_sec_nand_squashfs.pkgtb
cd _EBM68_3.0.0.6_102_44384-g304340a_370-g24e51_sec_nand_squashfs.pkgtb.extracted/squashfs-root/
file usr/sbin/httpd
strings usr/sbin/httpd | grep GLIBC_
```

We see the GLIBC version and the architecture.

## Prepare the fuzzing

Compile glibc-2.30.tar.gz (100% compatability with 2.28 and easier to compile)
so we can compile compatible shared libraries to preload into our fuzzing target.

```
tar xzf glibc-2.30.tar.gz
cd glibc-2.30
mkdir build
cd build
../configure --host=arm-linux-gnueabi --build=x86_64-linux-gnu   CC=arm-linux-gnueabi-gcc-9   CXX=arm-linux-gnueabi-g++-9   AR=arm-linux-gnueabi-ar   AS=arm-linux-gnueabi-as   LD=arm-linux-gnueabi-ld   RANLIB=arm-linux-gnueabi-ranlib   STRIP=arm-linux-gnueabi-strip   --prefix=`pwd`/local_install
make -j 4
make install
```

## Target analysis

### Test the target

You will see that root privileges are required (or the capability to bind to
privileged ports - or run with `-p 8080`).

from the squashfs-root:
```
sudo qemu-arm -L `pwd` usr/sbin/httpd
```
and try to connect from inside of the container:
```
curl localhost
```

1. Look at the errors
2. Find out from which library they come from
3. Create a shared library to preload to handle the errors until everything is
   clean

Bonus: Reverse-engineer the binary to uncover a debug feature :-)

Then write a shared library that hooks everything that is problematic for us.
Example solution: [lib-nonvram.c](lib-nonvram.c)

### Analyze the target

How is the connection accepted, read from and then closed?
What do we need to hook to exit the target once the connection is done?

### Result

<details>
  <summary>Spoilers :-)</summary>

For the internal debug feature:
```
sudo mkdir /jffs
touch /tmp/HTTPD_DEBUG
```
Enjoy logs in `/jffs/HTTPD_DEBUG.log` :-)
This can help analyzing the binary and fixing issues.

Connection `accept()` after a `select()`, then `shutdown()` to close.

[lib-fuzz-tcp.c](lib-fuzz-tcp.c) - Example shared library which handles all
errors plus exits when a web request is finished.

</details>

## Fuzzing the target via TCP

From the project root compile the shared library:

```
cross-compile.sh lib-fuzz-tcp.c -o _EBM68_3.0.0.6_102_44384-g304340a_370-g24e51_sec_nand_squashfs.pkgtb.extracted/squashfs-root/lib-fuzz-fuzz.so
```

Then from the squashfs-root run the fuzzer:

Create one dummy seed:
```
mkdir in
echo -en 'GET / HTTP/1.0\r\nUser-Agent: fuzzer\r\nHost: localhost\r\n\r\n' > in/in
```

```
afl-system-config

export CUSTOM_SEND_IP=127.0.0.1
export CUSTOM_SEND_PORT=80
export AFL_CUSTOM_MUTATOR_LATE_SEND=1
export AFL_CUSTOM_MUTATOR_LIBRARY=../../afl++/custom_mutators/custom_send_tcp/custom_send_tcp.so
export QEMU_LD_PREFIX=`pwd`
export AFL_PRELOAD=./lib-fuzz-tcp.so

afl-fuzz -i in -o out -Q -c 0 -- usr/sbin/httpd
```

### Hint: more speed

Reverse engineer the target binary and look up a suitable address before
the `accept()` call, and set this address for `AFL_ENTRYPOINT`.

#### Fuzz with more speed

add `export AFL_ENTRYPOINT=0x190c4` => 50% speed increase!

### Hint: more instances

either spawn the process on different ports - e.g. `CUSTOM_SEND_PORT=81 ... usr/sbin/httpd -p 81` - 
or run in different namespaces (or more docker containers):

```
NS_NAME="$1"
ip netns add "$NS_NAME"
ip netns exec "$NS_NAME" ip link set lo up
ip netns exec $NS_NAME afl-fuzz -S "$NS_NAME" -Q ... 
ip netns delete "$NS_NAME"
```

## Fuzzing the target via desocketing

### Analysis

Reverse engineer the target to see what needs to be intercepted to desocket the
application. Of course `accept()`, but what else? getpeername? ... ?

Create a shared library to intercept these.

### Result

<details>
  <summary>Spoilers :-)</summary>
[lib-fuzz-desock.c](lib-fuzz-desock.c) - Example shared library which handles
all errors, exits when a web request is finished AND desockets all necessary
functions. Surprise candidate here: `fdopen` :-)
</details>

### Fuzz via desocketing

From the project root compile the shared library:

```
cross-compile.sh lib-fuzz-desock.c -o _EBM68_3.0.0.6_102_44384-g304340a_370-g24e51_sec_nand_squashfs.pkgtb.extracted/squashfs-root/lib-fuzz-desock.so
```

Then from the squashfs-root run the fuzzer (unset all previous environment
variables from TCP fuzzing!):

```
export QEMU_LD_PREFIX=`pwd`
export AFL_PRELOAD=./lib-fuzz-desock.so

afl-fuzz -i in -o out -Q -c0 -- usr/sbin/httpd
```

### Hint: more speed

Reverse engineer the target binary and look up a suitable address before
the `fgets` call, and set this address for `AFL_ENTRYPOINT`.

#### Fuzz with more speed

add `export AFL_ENTRYPOINT=0x1bb84` => 70% speed increase!


## Fuzzing in persistent mode

### Analysis

Reverse engineer the target to see if we can perform persistent fuzzing:
* Where to start the loop
* Where to end the loop
* How to inject the payload into the target

### Result

<details>
  <summary>Spoilers :-)</summary>
* Where to start: 0x195dc
* Where to end: 0x19688
* How to inject the payload: Normally we would use shared memory and copy directly to a memory reagion. Due the fgets() used we simple send via `stdin`

So we reuse the desocketing library.

</details>

### Fuzz persistently

```
export AFL_QEMU_PERSISTENT_ADDR=0x195dc
export AFL_QEMU_PERSISTENT_RET=0x19688
export AFL_QEMU_PERSISTENT_GPR=1
export QEMU_LD_PREFIX=`pwd`
export AFL_PRELOAD=./lib-fuzz-desock.so

afl-fuzz -Q -i in -o out -c 0 -- usr/sbin/httpd
```

vs. tcp without AFL_ENTRYPOINT: 1200% faster
vs. desocket with AFL_ENTRYPOINT: 750% faster

## Optional: Make the fuzzing better

1. Ensure you are not running with debug :-) `rm -f /tmp/HTTPD_DEBUG`

2. Better options for AFL++:
```
export AFL_DISABLE_TRIM=1
export AFL_FAST_CAL=1
```

3. Generate a dictionary and use it:
```
for i in `strings usr/sbin/httpd | grep -E '^[A-Z][a-zA-Z-]*:$'`; do
  echo \"$i\"
done > target.dic
```
add use it with afl-fuzz via: `-x target.dic`

4. Run multiple parallel instances!


THE END.
