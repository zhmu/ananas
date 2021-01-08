FROM debian:10 as builder
LABEL maintainer "Rink Springer <rink@rink.nu>"

# Create an unprivileged build user; we expect that the uid/gid matches the
# work directory on the host system (inspired by
# https://gist.github.com/alkrauss48/2dd9f9d84ed6ebff9240ccfa49a80662)
RUN groupadd -g 1000 -r build && \
    useradd -r -u 1000 -g build -d /home/build -s /bin/bash -d /home/build -c "Build user" build
ENV home=/home/build

# install gcc/binutils/etc
RUN apt-get update && \
    apt-get install -y --no-install-recommends build-essential ninja-build cmake texinfo bison \
            flex automake python3 && \
    rm -rf /var/lib/apt/lists/*

ADD --chown=build:build work /work

# update permissions so we can build as non-root
RUN mkdir -p /opt/toolchain && chown build:build /opt/toolchain
RUN mkdir -p /opt/files && chown build:build /opt/files

# step 2: build the toolchain
USER build
RUN (cd /work && ./build-toolchain.sh)

# step 2: collect build results
FROM debian:10
LABEL maintainer "Rink Springer <rink@rink.nu>"

# install ninja, cmake etc
RUN apt-get update && \
    apt-get install -y --no-install-recommends ninja-build cmake \
        wget gettext autopoint texinfo autoconf automake make python \
        gcc python3 build-essential parted dosfstools syslinux syslinux-common \
        flex && \
    rm -rf /var/lib/apt/lists/*
COPY --from=builder /opt/toolchain/ /opt/toolchain/
COPY --from=builder /opt/files/ /opt/files/

# ensure the configured sysroot is available in our target container
RUN  ln -sf /work/build/output /opt/sysroot

# ensure our PATH is properly set up
RUN  echo 'export PATH=/opt/toolchain/bin:$PATH' >> /etc/bash.bashrc

# replace config.sub with our patched one
RUN  rm -f /usr/share/misc/config.sub && ln -sf /opt/files/config.sub /usr/share/misc/config.sub

RUN  groupadd -g 1000 -r build && \
     useradd -r -u 1000 -g build -d /home/build -s /bin/bash -d /home/build -c "Build user" build
ENV  home=/home/build
USER build
