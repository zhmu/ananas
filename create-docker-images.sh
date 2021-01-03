#!/bin/sh

docker 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Docker not found; please install and re-run"
    exit 1
fi

set +e

BUILD_DIR=`mktemp -d`
#trap "rm -rf ${BUILD_DIR}" EXIT
echo "Preparing build directory; ${BUILD_DIR}"
cp ci/toolchain/Dockerfile ${BUILD_DIR}
mkdir -p ${BUILD_DIR}/work/src
for p in binutils-gdb gcc gmp mpc mpfr; do
    cp -r "external/$p" ${BUILD_DIR}/work/src;
done
cp -r conf/settings.sh ${BUILD_DIR}/work
cp -r ci/toolchain/build-toolchain.sh ${BUILD_DIR}/work

docker build -t ananas-toolchain:latest -f ${BUILD_DIR}/Dockerfile ${BUILD_DIR}
