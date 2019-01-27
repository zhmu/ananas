FROM ananas-llvm:latest

MAINTAINER Rink Springer <rink@rink.nu>

RUN	apt-get update && apt-get upgrade -y

# we need python for libcxx, auto* for dash/coreutils, coreutils needs gettext,
# autopoint and texinfo, dash want to build C code so that it can build
# 'mknodes' and such (we install build-essential and gcc for that)
# we use python3 for all our scripts, so add that
# parted, dosfstools and syslinux is needed to build bootable images
RUN	apt-get install --no-install-recommends -y ninja-build wget \
	gettext autopoint texinfo autoconf automake make python python3 \
	gcc build-essential parted dosfstools syslinux syslinux-common

# Compile CMake ourselves; the version provided in stretch is too old for us
RUN	mkdir /tmp/cmake-build && cd /tmp/cmake-build && \
	wget "https://cmake.org/files/v3.12/cmake-3.12.2.tar.gz" && \
	tar xzf cmake-3.12.2.tar.gz && \
	cd cmake-3.12.2 && ./bootstrap -- -DCMAKE_BUILD_TYPE:STRING=Release && \
	make -j16 && make install && \
	cd / && rm -rf /tmp/cmake-build

# Create an unprivileged build user; we expect that the uid/gid matches the
# work directory on the host system (inspired by
# https://gist.github.com/alkrauss48/2dd9f9d84ed6ebff9240ccfa49a80662)
RUN	groupadd -g 1000 -r build && \
	useradd -r -u 1000 -g build -d /home/build -s /sbin/nologin -c "Build user" build

ENV	home=/home/build

# Run things as this user
USER	build
