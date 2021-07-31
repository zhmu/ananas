.. include:: /global.rst

.. Building

==========
 Building
==========

Compiler
=========

|ananas| is written mainly in C++, with pieces of C and assembly where
required.  The only supported compiler is the `GCC <https://gcc.gnu.org>`_
compiler. |ananas| aims to use the latest C++ standard for optimal
expressiveness.

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

Setup
------

First, ``ananas-toolchain`` container is to be created. This will contain a
basic binutils and gcc, but nothing more: mainly, there are no libraries
installed.  This makes the toolchain container reusable and removes the need to
rebuild the entire toolchain.

You can create the toolchain container as follows:

.. code-block:: console

   ~/ananas$ scripts/create-docker-toolchain.sh

Building
---------

Assuming your current working directory is the root of your |ananas| source
code repository, you can build the entire operating system by:

.. code-block:: console

   ~/ananas$ docker run -ti --rm -v $PWD:/work -w /work ananas-toolchain
   build@...:/work$ ./build.py -a

The first line *activates* the docker container - the next lines will be
executed within that container. The second line builds all supported items.

This will create a bootable x86_64 disk image in ``ananas.img``. This image can
be used in emulators like Qemu, VirtualBox or copied to an USB storage device to
boot on real hardware.

Build output
=============

All build output is present in the ``build`` directory: ``build/output``
contains the final image results: all binaries, runtime configuration files and
the like will be stored there.

``build/work`` is used for all intermediate results: build management files,
object files and the binaries themselves.
