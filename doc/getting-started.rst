.. include:: /global.rst

.. Getting started

=================
 Getting started
=================

Compiler
=========

|ananas| is written mainly in C++, with pieces of C and assembly where
required.  The only supported compiler is the `Clang <https://clang.llvm.org>`_
compiler, which is part of the `LLVM <https://www.llvm.org>`_ framework. |ananas|
aims to use the latest C++ standard for optimal expressiveness.

Source code organisation
=========================

|ananas| source code is hosted on `GitHub <https://github.com>`_. The `main
repository <https://github.com/zhmu/ananas>`_ contains several external
subrepositories to allow easy synchronisation with upstream sources.

You can obtain everything you need using the following command:

.. parsed-literal::
   ~$ git clone --recursive |ananas-git-url|

.. _docker-containers:

Docker containers
==================

|ananas| development is typically performed from `Docker
<https://www.docker.com>`_ containers. The main advantage this brings is a
single, stable build environment that works on any modern system.

There are two containers used:

- ``clang-debian9`` contains Clang/LLVM binaries
- ``ananas-build`` inherits from ``clang-debian9`` and provides a complete
  environment needed for building |ananas|

Setup
------

First, the ``clang-debian9`` container is to be created. You can build this by
executing the following commands:

.. code-block:: console

   ~/ananas$ cd ci/llvm
   ~/ananas/ci/llvm$ ./go.sh

You can then proceed to build the ``ananas-build`` container as follows:

.. code-block:: console

   ~/ananas$ cd ci/buildimage
   ~/ananas/ci/buildimage$ docker build -t ananas-build .

Building
---------

Assuming your current working directory is the root of your |ananas| source
code repository, you can build the entire operating system by:

.. code-block:: console

   ~/ananas$ docker run -ti -v $PWD:/work -w /work ananas-build
   build@...:/work$ ./build.sh -a

The first line *activates* the docker container - all other lines will be
executed within that container.

You can create a bootable x86_64 disk image by appending ``-i image.img``.
Such an image can be used in emulators like Qemu, VirtualBox or copied to an
USB storage device.

Build output
=============

After building, the output is available in the aptly-named ``output``
directory. ``output/output`` contains the final output image results: all
binaries, runtime configuration files and the like will be stored there.

``output/work`` is used for all intermediate results: build management files,
object files and the binaries themselves. These are:

Prequisites
------------

- ``crt``: C runtime, used for ``crt0.o`` and similar startup objects
- ``include``: a pseudo-target (does not compile anything) to get include files
  properly installed
- ``kernel``: kernel, with both multi-boot image ``kernel`` as well as the ELF
  file ``kernel.elf``

Libraries
----------

- ``libc``: Standard C library, based on PDCLIB
- ``libcxx``: LLVM's libcxx C++ standard library
- ``libcxxabi``: ABI-specific parts of LLVM's libcxxabi library
- ``libm``: Math library
- ``libsyscall``: contains wrappers to directly invoke system calls
- ``libunwind``: C++ exception unwinding library
- ``pthread``: threading library

Userland
---------

- ``rtld``: ELF Run Time LoaDer, used to dynamically load shared libraries
- ``sysutils``: |ananas|-specific implementation of ``init``, ``ps`` and the like

