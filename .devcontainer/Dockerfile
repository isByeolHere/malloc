# FROM ubuntu:latest
FROM --platform=linux/amd64 ubuntu:latest

ENV TZ=Asia/Seoul LANG=ko_KR.UTF-8 LANGUAGE=ko_KR.UTF-8

RUN dpkg --add-architecture i386 && \
    apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    locales tzdata build-essential gdb vim curl git wget \
    cmake make gcc sudo valgrind \
    libc6-dev-i386 gcc-multilib g++-multilib \
    && locale-gen ko_KR.UTF-8 && update-locale LANG=ko_KR.UTF-8

RUN useradd -m -s /bin/bash jungle && usermod -aG sudo jungle
RUN echo "jungle ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/jungle

USER jungle
WORKDIR /home/jungle