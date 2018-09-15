#!/usr/bin/env python3

import os
import subprocess
import sys

root_img = 'root.img' # temporary, will be removed after use
root_size_mb = 64 # XXX hardcoded for now (FIXME)

SECTOR_SIZE = 512

def mb_to_sectors(n):
	return (n * 1024 * 1024) / SECTOR_SIZE

def run_command(cmd):
	subprocess.run(cmd, check=True)

if len(sys.argv) != 3:
	print('usage: %s disk.img path' % sys.argv[0])
	print()
	print('This will create a disk image in disk.img with the content from path')
	quit(1)

disk_img, source_path = sys.argv[1:]

# create the raw image; it needs to have the correct length
with open(root_img, 'wb') as f:
	os.ftruncate(f.fileno(), root_size_mb * 1024 * 1024)

# now build a filesystem and conveniently fill it with our stuff
cmd = [ 'mkfs.ext2', ]
cmd += [ '-i', '8192' ] # inode block size
cmd += [ '-L', 'root' ] # label
cmd += [ '-d', source_path ]
cmd += [ root_img  ]
run_command(cmd)

# construct the partition map; we need this to determine the length of the
# disk image
offset = 64 # XXX seems like a reasonable default
partitions = [ (offset, mb_to_sectors(root_size_mb), root_img) ]
image_length_in_sectors = partitions[-1][0] + partitions[-1][1]

# create the raw disk; we'll use the raw image size as a baseline and
# append suitable slack
with open(disk_img, 'wb') as f:
	os.ftruncate(f.fileno(), image_length_in_sectors * SECTOR_SIZE)

cmd = [ 'parted', '-s', disk_img, 'mklabel', 'msdos' ]
run_command(cmd)
for start, length, _ in partitions:
	cmd = [ 'parted', '-s', disk_img, 'unit', 's', 'mkpart', 'primary', 'ext2', str(start), str(length) ]
	run_command(cmd)

# copy the root image into the disk image
with open(disk_img, 'r+b') as img:
	for offset, length, source in partitions:
		with open(source, 'rb') as src:
			src.seek(0, 2)
			source_len = src.tell()
			src.seek(0)
			if source_len != length * SECTOR_SIZE:
				raise Exception('source image length mismatch')

			img.seek(offset * SECTOR_SIZE)
			while True:
				buf = src.read(4096)
				if not buf:
					break
				img.write(buf)

# throw away the root image; no longer need it
os.unlink(root_img)
