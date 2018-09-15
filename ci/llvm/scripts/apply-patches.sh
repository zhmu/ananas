#!/usr/bin/env bash

set -e

SRC_DIR=/tmp/clang-build/src
PATCH_DIR=/tmp/patches

set -x

ls $SRC_DIR
ls $SRC_DIR/clang
for p in clang-ananas-c++; do
	echo "Applying patch $p"
	(cd $SRC_DIR && patch -p0 < $PATCH_DIR/$p.diff)
done
