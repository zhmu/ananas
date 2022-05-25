#!/bin/sh

docker 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Docker not found; please install and re-run"
    exit 1
fi

set +e

D=`dirname $0`
ROOT=`realpath $D/..`

BUILD_DIR=`mktemp -d`
trap "rm -rf ${BUILD_DIR}" EXIT
echo "Preparing build directory; ${BUILD_DIR}"
cp ${ROOT}/ci/toolchain/Dockerfile ${BUILD_DIR}
mkdir -p ${BUILD_DIR}/work/src
for p in binutils-gdb gcc-12.1.0 gmp mpc mpfr; do
    cp -r "${ROOT}/external/$p" ${BUILD_DIR}/work/src;
done
cp -r ${ROOT}/conf/settings.sh ${BUILD_DIR}/work
cp -r ${ROOT}/ci/toolchain/build-toolchain.sh ${BUILD_DIR}/work
cp -r ${ROOT}/ci/toolchain/*.py ${BUILD_DIR}/work

docker build -t ananas-toolchain:latest -f ${BUILD_DIR}/Dockerfile ${BUILD_DIR}
