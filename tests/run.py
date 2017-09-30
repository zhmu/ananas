#!/usr/bin/env python3

import os
import logging
import sys
import shutil
import subprocess

sys.path.append('lib')
import lib
import native
import qemu
import make_report

logging.basicConfig(format='%(asctime)s - %(module)s::%(funcName)s - %(message)s', datefmt='%Y-%d-%m %H:%M:%S', level=logging.DEBUG)

is_native = False

# create a fresh results folder
result_dir = 'results'
if os.path.exists(result_dir):
	shutil.rmtree(result_dir)
os.makedirs(result_dir)

src_disk_image = 'images/disk.img'
tmp_image = '/tmp/atf-disk.img'
if is_native:
	backend = native.native('tmp')
else:
	backend = qemu.qemu(src_disk_image, tmp_image)

test_dir = 'tests'
logger = logging.getLogger('main')

test_plan = lib.make_testplan(test_dir)
tests_to_run = sorted(test_plan.keys())

for tid in tests_to_run:
	t = test_plan[tid]
	logger.info('Preparing "%s"' % tid)
	backend.prepare(t)

	logging.info('Executing "%s"' % tid)
	output = backend.launch()
	backend.cleanup()

	# store results of this run
	test_result_path = os.path.join(result_dir, tid)
	os.makedirs(test_result_path)
	with open(os.path.join(test_result_path, 'output'), 'wb') as f:
		f.write(b''.join(output))

	test_ok, test_result, failure = lib.parse_test_output(tid, logging, output)
	t['result'] = {
		'ok': test_ok,
		'message': test_result,
		'failure-line': failure
	}

output = make_report.generate(test_plan)
if is_native:
	output_fname = 'report-native.html'
else:
	output_fname = 'report-qemu.html'
with open(output_fname , 'wt') as f:
	f.write(output)
