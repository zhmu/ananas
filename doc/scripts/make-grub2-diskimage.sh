#!/bin/sh

DEVMAP=/tmp/devmap.$$
MNT=/mnt
IMG=/tmp/disk.img
SIZE=500
UID=`id -u`

GRUB_ROOT=/home/rink/build

echo "This (Linux only!) script will create a ${SIZE} MB disk image in ${IMG} with VFAT"
echo "The image will be prepared with GRUB2 so it can be booted from"
echo -n "If this is what you want, type 'yes' after the prompt > "
read y
if [ "x$y" != "xyes" ]; then
	echo "aborting..."
	exit 1
fi

if [ ! -f ${GRUB_ROOT}/sbin/grub-install ]; then
	echo "grub-install not found"
	exit 1
fi

# create image
dd if=/dev/zero of=${IMG} bs=1M count=${SIZE}

# (n)ew (p)rimary, default start/end, change (t)ype -> 0xc (fat32), (w)rite
echo "x\nc\n1000\nh\n16\ns\n63\nr\nn\np\n\n\n\nt\nc\na\nw\n" | /sbin/fdisk ${IMG}
if [ $? -ne 0 ]; then
	echo "fdisk failure"
	exit 1
fi
sync

# use the losetup tool to turn this image into a device
DEV=`sudo /sbin/losetup -P --show -f ${IMG}`
echo $DEV|grep -q '^/dev/loop'
if [ $? -ne 0 ]; then
	echo "Not trusting loop device '$DEV'"
	exit 1
fi

sleep 1 # allow partition sync

if [ ! -b "${DEV}p1" ]; then
	echo "Cannot find ${DEV}p1, aborting"
	exit 1
fi

echo "(hd0) $DEV" > $DEVMAP

set +e
sudo mkfs.vfat -F 32 ${DEV}p1
sudo mount -o uid=$UID ${DEV}p1 $MNT
sudo ${GRUB_ROOT}/sbin/grub-install \
	--no-floppy --grub-mkdevicemap=$DEVMAP \
	--modules="biosdisk part_msdos fat normal multiboot" \
	--boot-directory=$MNT/boot \
	\
	$IMG

sudo echo 'set timeout=3
set default=0

menuentry "Ananas" {
        set root="hd0,msdos1"
        multiboot /kernel
        boot
}
' > $MNT/boot/grub/grub.cfg

sudo umount $MNT
sudo /sbin/losetup -d $DEV

rm -f $DEVMAP
