#!/usr/bin/env bash
#===- llvm/utils/docker/scripts/git-checkout.sh -----------------===//
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===-----------------------------------------------------------------------===//

set -e

function show_usage() {
  cat << EOF
Usage: git-checkout.sh [options]

Checkout git sources into /tmp/clang-build/src. Used inside a docker container.

Available options:
  -h|--help           show this help message
  -p|--llvm-project   name of an svn project to checkout.
                      For clang, please use 'clang', not 'cfe'.
                      Project 'llvm' is always included and ignored, if
                      specified.
                      Can be specified multiple times.
EOF
}

# We always checkout llvm
LLVM_PROJECTS="llvm"

function contains_project() {
  local TARGET_PROJ="$1"
  local PROJ
  for PROJ in $LLVM_PROJECTS; do
    if [ "$PROJ" == "$TARGET_PROJ" ]; then
      return 0
    fi
  done
  return 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--llvm-project)
      shift
      PROJ="$1"
      shift

      if [ "$PROJ" == "cfe" ]; then
        PROJ="clang"
      fi

      if ! contains_project "$PROJ" ; then
        if [ "$PROJ" == "clang-tools-extra" ] && [ ! contains_project "clang" ]; then
          echo "Project 'clang-tools-extra' specified before 'clang'. Adding 'clang' to a list of projects first."
          LLVM_PROJECTS="$LLVM_PROJECTS clang"
        fi
        LLVM_PROJECTS="$LLVM_PROJECTS $PROJ"
      else
        echo "Project '$PROJ' is already enabled, ignoring extra occurrences."
      fi
      ;;
    -h|--help)
      show_usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
  esac
done

CLANG_BUILD_DIR=/tmp/clang-build

# Get the sources from git.
echo "Checking out sources from git"
mkdir -p "$CLANG_BUILD_DIR/src"
for LLVM_PROJECT in $LLVM_PROJECTS; do
  GIT_PROJECT="$LLVM_PROJECT"

  if [ "$GIT_PROJECT" != "clang-tools-extra" ]; then
    CHECKOUT_DIR="$CLANG_BUILD_DIR/src/$LLVM_PROJECT"
  else
    CHECKOUT_DIR="$CLANG_BUILD_DIR/src/clang/tools/extra"
  fi

  URL=https://github.com/zhmu/ananas-$GIT_PROJECT
  echo "Cloning $URL to $CHECKOUT_DIR"
  git clone "$URL" "$CHECKOUT_DIR"
done

echo "Done"
