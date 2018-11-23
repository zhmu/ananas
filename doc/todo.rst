.. include:: /global.rst

.. Further work

=============
 Further work
=============

This section contains a non-exhaustive list of items that can use improvement.
These are not in a specific order, and may range from trivial to impossible.

If you are willing to contribute in any way, please `let me know
<mailto:rink@rink.nu>`_ how I can help you!

Kernel
-------

- bio (block I/O layer): horrible, should be reimplemented using page as base
  (the Linux 2.6+ page cache rework is a good source of inspiration)
- ext2fs: add read-write support
- pit: Do not use the legacy PIT timer, use the LAPIC timers instead
- revisit thread/process locking so that userland threading can be supported without races
- add lock-based debugging to detect deadlocks/wrong locking order
- implement more STL-based algorithms/containers
- investigate whether (parts of) libcxx is usable in the kernel
- re-think tracing interface
- add support for userland system call tracing (similar to strace)

Hardware support
-----------------

- PCI interrupt routing does not work on some modern systems
- Add support for `virtio <https://www.linux-kvm.org/page/Virtio>`_.
- Add support for VGA framebuffer console
- USB attach/detach does not always work properly on real hardware

Libraries
----------

- libc: Most unicode support was either hacked in or stubbed; this could use improvement
- pthread: this is just a stub, it should become more than that
- rtld: add support for dlopen() and such

Documentation
--------------

- Write some documentation on the virtual memory (VM) subsystem

External
--------

- coreutils: replace build infrastructure with CMake-based one
- coreutils: investigate possible replacements
- dash: replace build infrastructure with CMake-based one
- llvm/clang: get llvm/clang cross-building to |ananas|
- investigate whether it makes sense to use apt/rpm/... to package things (the image is currently thrown together using ``make install``)

Miscellaneous
--------------

- buildbot: create a `buildbot <http:/buildbot.net>`_ based continous integration setup
- remove all vestiges of C usage
