.. include:: /global.rst

.. Overview

==============
 Introduction
==============

The Ananas project was started around summer 2009, with a goal to create an
understandable yet usable modern operating system. Having been involved with
both the `FreeBSD <https://www.freebsd.org>`_ and `Linux
<https://www.kernel.org>`_ operating systems, the author found that they are
extremely complicated due to the vast amount of supported configurations and
features, which hampers learning and tinkering.

.. _goal:

Project goal
=============

The |ananas| project goal is the following:

"To broaden one's understanding of operating system concepts by implementing an
understandable yet usable operating system for modern computer architures."

Specifically:

- Real-life operating systems tend to support large amounts of
  obsoleted/impractical hardware configurations which convolute the system and
  make the actual underlying concepts very hard to understand.

- Do not get bogged down in compatibility: |ananas| will not claim to have any
  form of backwards compatibility and there will likely never have ABI stability.

- To live is to learn: do not be afraid to try out new things and see what they
  will or will not bring.

- Sharing is caring: |ananas| is licensed using a very liberal :ref:`license`.
  Reusing code from |ananas| is therefor recommended, as long as this license
  is honoured.

Conventions
===========

Throughout the documentation, the following definitions and notations are used.

- ``path$ command`` refers to a *command* typed by the user from a shell where
  the current working directory is *path*. Super-user privileges are not needed.
- ``build@...:path$ command`` is similar from inside a Docker container.

Requirements
============

In order to run |ananas|, your host system (either a virtual machine or
physical hardware) must confirm to the following:

- You must have an x86_64-capable CPU with No-Execute (NX) support
- `ACPI <http://www.acpi.info>`_ (Advanced Configuration & Power Interface) is required

.. _license:

License
========

Unless otherwise indicated, the |ananas| kernel, userland and accompanying
libraries and scripts are distributed using the :ref:`zlib` license.

Previous versions were licensed as 'beer-ware'. In late 2018, the decision was
made to use the better known Zlib license to avoid confusion.

The |ananas| C library is a fork of `PDCLIB
<https://rootdirectory.ddns.net/dokuwiki/doku.php?id=pdclib:start>`_, which is
licensed as *public domain*. All modifications are licensed using the
:ref:`zlib` license.

External code, such as the LLVM/Clang suite, the math library *libm*, GNU
autotools and *dash* are licensed using their own terms.

This documentation is licensed under a `Creative Commons Attribution 2.0
Generic License <https://creativecommons.org/licenses/by/2.0/>`_.

.. _zlib:

Zlib license
-------------

.. literalinclude:: ../LICENSE
