#===- llvm/utils/docker/debian9/build/Dockerfile -------------------------===//
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===----------------------------------------------------------------------===//
# Stage 1. Check out LLVM source code and run the build.
FROM launcher.gcr.io/google/debian9:latest as builder
LABEL maintainer "LLVM Developers"
# Install build dependencies of llvm.
# First, Update the apt's source list and include the sources of the packages.
RUN grep deb /etc/apt/sources.list | \
    sed 's/^deb/deb-src /g' >> /etc/apt/sources.list
# Install compiler, python and git.
RUN apt-get update && \
    apt-get install -y --no-install-recommends ca-certificates gnupg \
           build-essential python wget git unzip ninja-build cmake && \
    rm -rf /var/lib/apt/lists/*

ADD checksums /tmp/checksums
ADD scripts /tmp/scripts
ADD patches /tmp/patches

# Checkout the source code.
ARG checkout_args
RUN /tmp/scripts/git-clone.sh ${checkout_args}
# Apply any patches we may have
ARG checkout_args
RUN /tmp/scripts/apply-patches.sh ${checkout_args}
# Run the build. Results of the build will be available at /tmp/clang-install/.
ARG buildscript_args
RUN /tmp/scripts/build_install_llvm.sh --to /tmp/clang-install ${buildscript_args}


# Stage 2. Produce a minimal release image with build results.
FROM launcher.gcr.io/google/debian9:latest
LABEL maintainer "LLVM Developers"
# Install packages for minimal useful image.
RUN apt-get update && \
    apt-get install -y --no-install-recommends binutils && \
    rm -rf /var/lib/apt/lists/*
# Copy build results of stage 1 to /usr/local.
COPY --from=builder /tmp/clang-install/ /usr/local/
