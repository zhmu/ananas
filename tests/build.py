#!/usr/bin/env python3

import glob
import os
import shutil
import subprocess
import sys

sys.path.append('lib')
import lib

def filename_to_obj(f, path):
	f = f.replace(os.sep, '-') # replace / -> -
	f = os.path.splitext(f)[0] + '.o' # replace extension -> .o
	return os.path.join(path, f)

framework_src = glob.glob("framework/*.cpp")
tests = lib.find_tests('tests')

# create a clean build/ directory
if os.path.isdir('build'):
	shutil.rmtree('build')
os.makedirs('build')
os.makedirs('build/obj')
os.makedirs('build/bin')

# write the makefile
bin_dir = 'bin'
obj_dir = 'obj'
framework_lib = os.path.join(obj_dir, 'libframework.a')
with open('build/Makefile', 'wt') as f:
	test_exe = {}
	for t in tests:
		exe = lib.test_to_exe(t)
		test_exe[exe] = t

	f.write('TARGET:\tall\n')
	f.write('R=\t..\n')
	f.write('\n')
	f.write('CXXFLAGS+=-std=c++11 -I$R/framework\n')
	f.write('CXXFLAGS+=-O3 -Wall -Werror\n')
	f.write('CXXFLAGS+=-g\n')
	f.write('\n')
	f.write('all:\t%s\n' % ' '.join([ os.path.join(bin_dir, exe) for exe in test_exe.keys() ]))
	f.write('\n')
	f.write('%s:\t%s\n' % (framework_lib, ' '.join([ filename_to_obj(src, obj_dir) for src in framework_src ])))
	f.write('\t\t\t$(AR) rcs %s %s\n' % (framework_lib, ' '.join([ filename_to_obj(src, obj_dir) for src in framework_src ])))
	f.write('\n')

	for s in framework_src:
		f.write('%s:\t$R/%s\n' % (filename_to_obj(s, obj_dir), s))
		f.write('\t\t\t$(CXX) $(CXXFLAGS) -c -o %s $R/%s\n' % (filename_to_obj(s, obj_dir), s))
		f.write('\n')

	for exe, src in test_exe.items():
		f.write('%s:\t%s $R/tests/%s\n' % (os.path.join(bin_dir, exe), os.path.join(obj_dir, 'libframework.a'), src))
		f.write('\t\t\t$(CXX) $(CXXFLAGS) -o %s $R/tests/%s obj/libframework.a\n' % (os.path.join(bin_dir, exe), src))
		f.write('\n')

	f.write('clean:\n')
	f.write('\trm -f %s/* %s/*\n' % (obj_dir, bin_dir))

# build everything
prevdir = os.getcwd()
os.chdir('build')
try:
	subprocess.run([ "make" ])
finally:
	os.chdir(prevdir)
