#!/usr/bin/env python3

import os
import shutil
import subprocess
import threading

class qemu:
	def __init__(self, src_image, dst_image):
		self._src_image = src_image
		self._dst_image = dst_image
		self._mount_dir = '/mnt'

	# prepares a disk image for a given test
	# XXX we should do all this stuff in userland, but that is for later
	def prepare(self, test):
		shutil.copy(self._src_image, self._dst_image)
		if subprocess.call([ 'sudo', 'mount', '-t', 'vfat', '-o', 'loop,offset=1048576,uid=1000', self._dst_image, self._mount_dir ]) != 0:
			raise Exception("unable to mount '%s'" % self._dst_image)

		# copy the test to there
		test_dir = os.path.join(self._mount_dir, 'tests')
		if not os.path.isdir(test_dir):
			os.makedirs(test_dir)
		shutil.copy(os.path.join('build', 'bin', test['exe']), test_dir)

		# copy all input files XXX we copy them to the root for now!
		for i in test['input-files']:
			with open(os.path.join(self._mount_dir, i['filename']), 'wt') as f:
				f.write(i['content'])

		# create the grub config
		with open(os.path.join(self._mount_dir, 'boot', 'grub', 'grub.cfg'), 'wt') as f:
			f.write('set timeout=0\n')
			f.write('set default=0\n')
			f.write('menuentry "Ananas ATF" {\n')
			f.write('  set root="hd0,msdos1"\n')
			f.write('  multiboot /kernel root=fatfs:slice0 init=/tests/%s\n' % (test['exe']))
			f.write('  boot\n')
			f.write('}\n')

		# and clean up
		if subprocess.call([ 'sudo', 'umount', self._mount_dir ]) != 0:
			raise Exception("unable to umount '%s'" % self._mount_dir)

	def launch(self):
		qemu_path = "/home/rink/build/bin/qemu-system-x86_64"
		qemu_args = [ '-nographic', '-drive', 'id=disk,file=%s,format=raw,if=none' % self._dst_image, '-device', 'ahci,id=ahci', '-device', 'ide-drive,drive=disk,bus=ahci.0' ]
		qemu_timeout = 5 # seconds

		p = subprocess.Popen([ qemu_path ] + qemu_args, stdout=subprocess.PIPE)
		kill_fn = lambda p: p.kill()
		timer = threading.Timer(qemu_timeout, kill_fn, [ p ])
		output = [ ]
		timed_out = True
		try:
			timer.start()
			for s in p.stdout:
				output.append(s)
				if s.startswith(b'END:') or s.startswith(b'ABORT:'):
					break

			# we got what we came for - destroy the output
			timed_out = False
			p.kill()
		finally:
			timer.cancel()

		return output

	def cleanup(self):
		pass

