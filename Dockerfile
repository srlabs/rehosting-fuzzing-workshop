FROM ubuntu:24.04 AS wokshop

ENV LLVM_VERSION=19
ENV GCC_VERSION=11
ARG DEBIAN_FRONTEND=noninteractive
ENV NO_ARCH_OPT=1
ENV IS_DOCKER=1

RUN apt-get update && apt-get full-upgrade -y && \
    apt-get install -y wget ca-certificates apt-utils && \
    apt-get install -y gawk gcc-9-arm-linux-gnueabi qemu-user qemu-utils \
      qemu-system-arm qemu-block-extra binwalk wget bison python3 \
      build-essential llvm-dev clang ninja-build libglib2.0-dev pkg-config \
      python3-pip git binwalk file unzip joe vim bash-completion less libtool \
      libtool-bin apt-transport-https gcc-13 g++-13 gcc-13-plugin-dev

RUN apt clean -y

ENV LLVM_CONFIG=llvm-config-${LLVM_VERSION}
ENV AFL_SKIP_CPUFREQ=1
ENV AFL_TRY_AFFINITY=1
ENV AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

RUN git clone --depth=1 -b dev https://github.com/AFLplusplus/AFLplusplus /afl++
RUN cd /afl++ && make && \
    cd qemu_mode && export CPU_TARGET=arm && ./build_qemu_support.sh && \
    cd .. && make -C custom_mutators/custom_send_tcp && make install

COPY ./FW_EBM68_300610244384.zip ./FW_EBM68_300610244384.zip
COPY ./glibc-2.30.tar.gz ./glibc-2.30.tar.gz

RUN echo "set encoding=utf-8" > /root/.vimrc && \
    echo ". /etc/bash_completion" >> ~/.bashrc && \
    echo 'alias joe="joe --wordwrap --joe_state -nobackup"' >> ~/.bashrc && \
    echo "export PS1='"'[WORKSHOP \h] \w \$ '"'" >> ~/.bashrc
