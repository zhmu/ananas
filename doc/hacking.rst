.. include:: /global.rst

.. Hacking

=================
 Hacking
=================

Qemu
=====

Qemu is my emulator of choice when developing |ananas|: it is reasonably fast, boot times are excellent and has decent debugging facilities.

Should you wish to build your local version of Qemu so you can instrument device code, you can use the following recipe:

.. code-block:: console

   ~$ git clone git://git.qemu-project.org/qemu.git
   ~$ cd qemu
   ~/qemu$ ./configure --prefix=~/build --target-list=x86_64-softmmu --enable-libusb --enable-debug --audio-drv-list=alsa,sdl
   ~/qemu$ make -j8
   ~/qemu$ make install

You can enable debugging, for example for HDA, by supplying ``-global
intel-hda.debug=10``.

USB
----

Qemu allows you to forward USB devices from your system to the VM. You can
enable UHCI support by supplying the following arguments to Qemu:

.. code-block:: text

   -usb -usbdevice host:1c4f:0002

Likewise, in order to enable OHCI support:

.. code-block:: text

    -device pci-ohci,id=usb -device usb-host,bus=usb.0,vendorid=0x1c4f,productid=0x0002

Where ``1c4f`` is the vendor ID and ``0002`` is the device ID of the USB device
you want to forward. You can use ``lsusb`` to see these values, and you may want to
change permissions of ``/dev/bus/usb`` to avoid running Qemu as super-user.

Kernel debugging
=================

The |ananas| kernel, as well as tools like QEmu and VirtualBox provide
so-called GDB stubs - a thin layer which allows remote debuggers to
interface with the system to obtain and modify system state.

You can enable such support in Qemu by passing the ``-s -S`` arguments. This
enables a GDB server on port 1234 and will initially halt execution.

In order to set up GDB to connect to the remote stub, create a ``.gdbinit``
file with the following content:

.. code-block:: text

   target remote :1234
   set substitute-path /work .../ananas
   display/i $pc
   break _panic

The second line is of particular interest, as it ensures filenames relative to
``/work`` within the Docker container can be properly mapped to their actual
locations on your system. Also, you may need to create a ``.gdbinit`` file in
your home directory with a ``add-auto-load-safe-path`` directive so that GDB
will load your local ``.gdinit`` file - you will receive a message upon GDB
startup if this is the case)

The fourth line places a breakpoint on the panic-handler, so that should the
kernel panic, execution automatically stops and you can inspect the system
state.

