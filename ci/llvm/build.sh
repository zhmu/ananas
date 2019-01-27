#!/bin/sh
# Originally based on llvm/utils/docker/build_docker_image.sh

DOCKER_TAG="ananas-llvm:latest"
BUILDSCRIPT_ARGS="-i install -- -DLLVM_ENABLE_PROJECTS=clang;lld -DLLVM_TARGETS_TO_BUILD=X86 -DCMAKE_BUILD_TYPE=Release -DBOOTSTRAP_CMAKE_BUILD_TYPE=Release -DCLANG_ENABLE_BOOTSTRAP=ON -DCLANG_BOOTSTRAP_TARGETS=install-clang;install-clang-headers -DLLVM_ENABLE_PROJECTS=clang;lld"

docker 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Docker binary cannot be found. Please install Docker to use this script."
    exit 1
fi

set -e

SOURCE_DIR=`dirname $0`
ROOT=`realpath ${SOURCE_DIR}/../..`
if [ ! -d "$ROOT/external/llvm" ]; then
  echo "external/llvm not found in ${ROOT}"
  exit 1
fi

# create a temporary folder to place the temporary results
BUILD_DIR=`mktemp -d`
trap "rm -rf $BUILD_DIR" EXIT
echo "Preparing temporary directory for the build: $BUILD_DIR ..."
cp -r "$SOURCE_DIR/Dockerfile" "$BUILD_DIR"
cp -r "$SOURCE_DIR/scripts" "$BUILD_DIR/scripts"
mkdir "$BUILD_DIR/source"
cp -r "$ROOT/external/llvm" "$BUILD_DIR/source/llvm"
# re-organise source directory so we can find our enabled projects; they must
# be relative to the root
mv "$BUILD_DIR/source/llvm/tools/clang" "$BUILD_DIR/source"
mv "$BUILD_DIR/source/llvm/tools/lld" "$BUILD_DIR/source"

mkdir "$BUILD_DIR/checksums"

echo "Building ${DOCKER_TAG}"
docker build -t ${DOCKER_TAG} \
  --build-arg "buildscript_args=$BUILDSCRIPT_ARGS" \
  -f "$BUILD_DIR/Dockerfile" \
  "$BUILD_DIR"
echo "Done"
