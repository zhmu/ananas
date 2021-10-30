.. include:: /global.rst

.. Internals

===========
 Internals
===========

Headers
========

Note: this is a work-in-progress!

The ``include/ananas/`` directory contains Ananas-specific header files that are shared between the kernel and userland. From here, only other files from the ``include/ananas/`` directory and its subdirectories can be included and most types are prefixed with two underscores (__) to hide them from userland.

Types like ``size_t`` are not prefixed as these are compiler-supplied and having the underscore causes problems with libgcc.

The ``include/sys/`` directory contains POSIX-specific headers that are shared between the kernel and userland. The most interesting one is ``sys/types.h`` which publishes the kernel types from ``ananas/types.h``. Again, these files can also be used by the kernel.

The reason why both ``include/ananas/`` and ``include/sys/`` exist is that we can put whatever we need in the former, but a lot of software is picky considering the latter. It also allows us to distinguish between |ananas|-specific features.

TODO: ``include/`` itself should be moved to the C library later on. ``sys/`` and ``include/`` need to be cleaned up.
