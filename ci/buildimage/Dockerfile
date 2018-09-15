FROM clang-debian8:staging

MAINTAINER Rink Springer <rink@rink.nu>

RUN	apt-get update && apt-get upgrade -y

# we need python for libcxx, auto* for dash/coreutils, coreutils needs gettext,
# autopoint and texinfo, dash wants gcc so that it can build 'mknodes' and
# such
RUN	apt-get install --no-install-recommends -y ninja-build gnupg wget \
	gettext autopoint texinfo autoconf automake make python \
	gcc

# Import public key required for verifying signature of cmake download.
RUN gpg --keyserver hkp://pgp.mit.edu --recv 0x2D2CEF1034921684

# Download, verify and install a more recent CMake; Debian 8 has a
# very old version
RUN mkdir /tmp/cmake-install && cd /tmp/cmake-install && \
    wget "https://cmake.org/files/v3.12/cmake-3.12.1-SHA-256.txt.asc" && \
    wget "https://cmake.org/files/v3.12/cmake-3.12.1-SHA-256.txt" && \
    gpg --verify cmake-3.12.1-SHA-256.txt.asc cmake-3.12.1-SHA-256.txt && \
    wget "https://cmake.org/files/v3.12/cmake-3.12.1-Linux-x86_64.tar.gz" && \
    ( grep "cmake-3.12.1-Linux-x86_64.tar.gz" cmake-3.12.1-SHA-256.txt | \
      sha256sum -c - ) && \
    tar xzf cmake-3.12.1-Linux-x86_64.tar.gz -C /usr/local --strip-components=1 && \
    cd / && rm -rf /tmp/cmake-install

