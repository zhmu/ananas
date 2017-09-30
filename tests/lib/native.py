#!/usr/bin/env python3

import os
import shutil
import subprocess

class native:
	def __init__(self, tmp_dir):
		self._tmp_dir = tmp_dir
		self._current_test = None
		self._timeout = 5 # seconds

	def prepare(self, test):
		self._current_test = test

		# copy test
		shutil.copy(os.path.join('build', 'bin', test['exe']), self._tmp_dir)

		# copy all input files XXX we copy them to the root for now!
		for i in test['input-files']:
			with open(os.path.join(self._tmp_dir, i['filename']), 'wt') as f:
				f.write(i['content'])

	def launch(self):
		prev_dir = os.getcwd()
		os.chdir(self._tmp_dir)

		try:
			p = subprocess.run([ os.path.join('.', self._current_test['exe']) ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=self._timeout)
		except subprocess.TimeoutExpired as e:
			# XXX how to deal with this? do we still have output?
			print('Timeout!')

		output = [ ]
		for s in p.stdout.split(b'\n'):
			output.append(s)
			if s.startswith(b'END:') or s.startswith(b'ABORT:'):
				break

		# XXX a bit kludgy
		if p.returncode < 0:
			output.append(b'user land exception:')

		os.chdir(prev_dir)
		return output

	def cleanup(self):
		os.remove(os.path.join(self._tmp_dir, self._current_test['exe']))
		for i in self._current_test['input-files']:
			os.remove(os.path.join(self._tmp_dir, i['filename']))
		self._current_test = None
