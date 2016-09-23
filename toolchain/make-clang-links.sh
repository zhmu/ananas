#!/bin/sh

CLANG_PATH=$1
ARCH=$2
TARGET=$3

if [ -z "$TARGET" ]; then
	echo "usage: $0 path-to-clang architecture target-path"
	echo
	echo "example architecture: 'amd64'"
	exit 1
fi

if [ "$ARCH" = "amd64" ]; then
	TRIPLET='i386-ananas-elf'
elif [ "$ARCH" = "i386" ]; then
	TRIPLET='x86_64-ananas-elf'
else
	print "error: unrecognized architecture '$ARCH'"
	exit 1
fi

if [ ! -f $CLANG_PATH/clang ]; then
	echo "error: cannot find 'clang' in '$CLANG_PATH'"
	exit 1
fi

ln -sf $CLANG_PATH/clang $TARGET/${TRIPLET}-clang
ln -sf $CLANG_PATH/llvm-ar $TARGET/${TRIPLET}-ar
ln -sf $CLANG_PATH/llvm-as $TARGET/${TRIPLET}-as
ln -sf $CLANG_PATH/lld $TARGET/${TRIPLET}-ld
ln -sf $CLANG_PATH/llvm-objdump $TARGET/${TRIPLET}-objdump
ln -sf $CLANG_PATH/llvm-size $TARGET/${TRIPLET}-size
