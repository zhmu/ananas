.. include:: /global.rst

.. Quickstart

=================
 Quickstart
=================

If you just want to try the latest |ananas| development version, the following
steps will bring you to a bootable |ananas| image.

You must have a working Docker environment to be able to build |ananas|.

Step 0: Obtain the source code
------------------------------

.. parsed-literal::
   ~$ git clone --recursive |ananas-git-url|

Step 1: Setup the build environment
------------------------------------

As illustrated in :ref:`docker-containers`, execute the following commands:

.. code-block:: console

   ~/ananas$ cd ci/llvm
   ~/ananas/ci/llvm$ ./build.sh
   ~/ananas/ci/llvm$ cd ../buildimage
   ~/ananas/ci/buildimage$ docker build -t ananas-build .

This may take a while, as it builds LLVM/Clang and also CMake to ensure
correct versions are available in the resulting container.

Step 2: Build the operating system
-----------------------------------

We start a Docker instance and build an image containing everything:

.. code-block:: console

   ~/ananas$ docker run -ti -v $PWD:/work -w /work ananas-build
   build@...:/work$ ./build.sh -a -i ananas.img

Step 3: Run the image
----------------------

Our emulator of choice is Qemu; you can run your freshly-baked |ananas| image as follows:


.. code-block:: console

    $ qemu-system-x86_64 -drive id=disk,file=ananas.img,if=none -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0 -soundhw hda -serial stdio

Have fun playing around!
