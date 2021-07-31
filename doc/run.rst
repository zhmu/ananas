.. include:: /global.rst

.. Running Ananas

================
 Running Ananas
================

Our emulator of choice is Qemu; you can run your freshly-baked |ananas| image as follows:


.. code-block:: console

    $ qemu-system-x86_64 -drive id=disk,file=ananas.img,if=none -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0 -soundhw hda -serial stdio

Have fun playing around!
