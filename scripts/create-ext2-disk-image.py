#!/usr/bin/env python3

import math
import os
import shutil
import subprocess
import sys

root_img = 'root.img' # temporary, will be removed after use
boot_img = 'boot.img' # temporary, will be removed after use
root_size_mb = None # None to autodetect
boot_size_mb = None # None to autodetect
kernel_fname = 'kernel'
kernel_cmdline = 'root=ext2:slice0'
root_skipped_files = [ kernel_fname, 'toolchain.txt' ]

SECTOR_SIZE = 512

def mb_to_sectors(n):
	return (n * 1024 * 1024) // SECTOR_SIZE

def create_empty_image(fname, length_in_sectors):
	with open(fname, 'wb') as f:
		os.ftruncate(f.fileno(), length_in_sectors * SECTOR_SIZE)

def get_offset_to_next_partition(p):
	return p['offset'] + p['length']

def run_command(cmd):
	subprocess.run(cmd, check=True)

def build_root_image(img, size_mb, root_path):
	# create a temporary copy, with only the stuff we need
	temp_dir = 'root_tmp'
	temp_size_kb = 0
	for r, dirs, files in os.walk(root_path):
		prefix = os.path.relpath(r, root_path)
		dest_path = os.path.join(temp_dir, prefix)
		if not os.path.isdir(dest_path):
			os.makedirs(dest_path)
		for f in files:
			if f not in root_skipped_files:
				src_file = os.path.join(r, f)
				shutil.copy(src_file, dest_path)
				temp_size_kb += math.ceil(os.path.getsize(src_file) / 1024) # round up

	# if we do not have a size given, yield what we calculated
	if not size_mb:
		size_mb = math.ceil(temp_size_kb / 1024)
		size_mb += 4 # 4MB slack for ext2 structures etc

	# create the raw image; it needs to have the correct length
	create_empty_image(img, mb_to_sectors(size_mb))

	# now build a filesystem and conveniently fill it with our stuff
	cmd = [ 'mkfs.ext2', ]
	cmd += [ '-i', '8192' ] # inode block size
	cmd += [ '-L', 'root' ] # label
	cmd += [ '-d', temp_dir ]
	cmd += [ img ]
	run_command(cmd)

	shutil.rmtree(temp_dir)
	return size_mb

if len(sys.argv) not in [ 3, 4 ]:
	print('usage: %s disk.img path [syslinux_root]' % sys.argv[0])
	print()
	print('This will create a disk image in disk.img with the content from path')
	print('If [syslinux_root] is set, the image will be bootable')
	quit(1)

disk_img, source_path = sys.argv[1:3]
syslinux_root = None
if len(sys.argv) > 3:
	syslinux_root = sys.argv[3]

# create the root image itself
root_size_mb = build_root_image(root_img, root_size_mb, source_path)

# construct the partition map; we need this to determine the length of the
# disk image
next_offset = 64 # XXX seems like a reasonable default
partitions = [ { 'offset': next_offset, 'length': mb_to_sectors(root_size_mb), 'image': root_img, 'type': 'ext2', 'boot': False } ]
next_offset = get_offset_to_next_partition(partitions[-1]) + 1
if syslinux_root:
	if not boot_size_mb:
		# determine the boot partition size - we'll use the kernel as a baseline plus 1MB
		boot_size_mb = int(math.ceil(os.path.getsize(os.path.join(source_path, kernel_fname)) / (1024 * 1024))) + 1
	partitions.append({ 'offset': next_offset, 'length': mb_to_sectors(boot_size_mb), 'image': boot_img, 'type': 'fat32', 'boot': True })

image_length_in_sectors = get_offset_to_next_partition(partitions[-1])

# create the raw disk; we'll use the raw image size as a baseline and
# append suitable slack
create_empty_image(disk_img, image_length_in_sectors + 1)
if syslinux_root:
	# throw away the boot image as mkfs.fat utterly refuses to overwrite it
	if os.path.exists(boot_img):
		os.unlink(boot_img)

	# build a filesystem for the boot image
	cmd = [ 'mkfs.fat', '-C' ]
	cmd += [ '-n', 'BOOT' ] # label
	cmd += [ '-S', '512' ] # sector size
	cmd += [ boot_img ]
	cmd += [ str(boot_size_mb * 1024) ]
	run_command(cmd)

	# create a configuration file; this uses the multiboot-module to load our kernel
	with open('syslinux.cfg', 'wt') as f:
		f.write('PROMPT 0\n')
		f.write('DEFAULT ananas\n')
		f.write('LABEL ananas\n')
		f.write('KERNEL mboot.c32\n')
		f.write('APPEND %s %s\n' % (kernel_fname, kernel_cmdline))

	# copy kernel and other stuff
	syslinux_usr_share = os.path.join(syslinux_root, 'usr', 'share', 'syslinux')
	for f in [ os.path.join(syslinux_usr_share, 'mboot.c32'), os.path.join(syslinux_usr_share, 'libcom32.c32'), os.path.join(source_path, kernel_fname), 'syslinux.cfg' ]:
		cmd = [ 'mcopy', '-i', boot_img, f, '::/' ]
		run_command(cmd)

	# put syslinux on there
	cmd = [ os.path.join(syslinux_root, 'usr', 'bin', 'syslinux'), boot_img ]
	run_command(cmd)

cmd = [ 'parted', '-s', disk_img, 'mklabel', 'msdos' ]
run_command(cmd)
for n, p in enumerate(partitions):
	cmd = [ 'parted', '-s', disk_img, 'unit', 's', 'mkpart', 'primary', p['type'], str(p['offset']), str(p['offset'] + p['length']) ]
	run_command(cmd)
	if p['boot']:
		cmd = [ 'parted', '-s', disk_img, 'set', str(n + 1), 'boot', 'on' ]
		run_command(cmd)

# copy the images into the disk image
with open(disk_img, 'r+b') as img:
	for p in partitions:
		with open(p['image'], 'rb') as src:
			src.seek(0, 2)
			source_len = src.tell()
			src.seek(0)
			if source_len != p['length'] * SECTOR_SIZE:
				raise Exception('source image length mismatch')

			img.seek(p['offset'] * SECTOR_SIZE)
			while True:
				buf = src.read(4096)
				if not buf:
					break
				img.write(buf)

	if syslinux_root:
		# place the syslinux MBR onto the disk
		with open(os.path.join(syslinux_root, 'usr', 'share', 'syslinux', 'mbr.bin'), 'rb') as mbrf:
			mbr = mbrf.read()
			img.seek(0)
			img.write(mbr)

# throw away the root/boot images; no longer need it
os.unlink(root_img)
if syslinux_root:
	os.unlink(boot_img)
