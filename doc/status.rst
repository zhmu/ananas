.. include:: /global.rst

.. Current status

================
 Current status
================

Note that this list is incomplete.

Kernel features
================

- Symmetric Multiprocessor Support
- Copy-On-Write

Userland features
==================

- ELF shared library support

Hardware support
=================

- Advance Host Controller Interface (AHCI) SATA
- Generic PCI ATA (dev/ata) |deprecated|
- Intel High Definition Audio (HDA)
- PCI
- Open Host Controller Interface (OHCI) USB 1.x
- Universal Host Controller Interface (UHCI) USB 1.x
- USB hub
- USB keyboard
- USB storage (bulk-only)
- Legacy x86 keyboard
- Legacy x86 text-mode VGA console
- Legacy x86 serial port

Filesystem support
===================

- AnkhFS (read-only, similar to Linux procfs/sysfs)
- cramfs |deprecated|
- ext2fs (read-only)
- FAT16 and FAT32 (read-write)
- ISO-9660 (read-only) |deprecated|

Miscellaneous features
=======================

- In-kernel debugger KDB
- Remote GDB debugging

Removed features
=================

- Support for i386 and PowerPC architectures
- Nintendo Wii support
