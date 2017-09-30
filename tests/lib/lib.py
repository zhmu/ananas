#!/usr/bin/python
"""
 The way this stuff is structured is as follows:

 framework/
  Everything in here is considered as part of the framework and will be placed in framework.a
  which is used by every test

 tests/
  Contains all tests, scattered in subdirectories

 The output will be placed in the output/ folder as follows:

 run.sh
   Script running every test

 build/
   Build output (Makefile etc)

"""

import os

# expects s="...", returns ..., remainder of s
def _get_quoted_string(s):
	if s[0] != '"':
		raise Exception('expected " at start of "%s"' % s)
	n = s.find('"', 1)
	if n < 0:
		raise Exception('expected " in "%s"' % s)
	return s[1:n], s[n+1:].strip()

def find_tests(test_dir):
	tests = []
	for root, dirs, files in os.walk(test_dir):
		# cut off tests/ from the root
		d = os.sep.join(root.split(os.sep)[1:])
		tests += [ os.path.join(d, f) for f in files if os.path.splitext(f)[1] == '.cpp' ]
	return tests

def make_testplan(test_dir):
	tests = find_tests(test_dir)

	test_plan = {}
	for t in tests:
		tid = test_make_identifier(t)
		t_props = {
			'source': os.path.join(test_dir, t),
			'exe': test_to_exe(t),
			'summary': None,
			'input-files': [ ]
		}

		# check test properties; these are annotated in the source
		with open(t_props['source'], 'rt') as f:
			for s in f:
				s = s.strip()
				if not s.startswith('//'):
					continue
				n = s.find(':')
				if n < 0:
					continue
				key, val = s[2:n].strip().upper(), s[n + 1:].strip()

				if key == "SUMMARY":
					t_props['summary'] = val
				elif key == "PROVIDE-FILE":
					# we expect "name" "content" here
					filename, val = _get_quoted_string(val)
					content, val = _get_quoted_string(val)
					if val:
						raise Exception("unexpected '%s'" % val)
					if os.sep in filename:
						raise Exception("rejecting filename '%s'" % filename)
					t_props['input-files'].append({
						'filename': filename,
						'content': content
					})
				else:
					raise Exception("unexpected key/val '%s', '%s'" % (key, val))

		test_plan[tid] = t_props
	return test_plan

def test_make_identifier(t):
	return os.path.splitext(t)[0] # removes extension

def test_to_exe(t):
	return os.path.splitext(t.replace(os.sep, '-'))[0] # / -> -, removes extension

def parse_test_output(tid, logging, output):
	# first analyse the results
	active = None
	success = None
	failure = None
	expecting_death = False
	died = False
	for o in output:
		if not active:
			if o.startswith(b'START:'):
				active = True
			continue

		if o.startswith(b'END:'):
			# if we didn't see any failure, this ijs good
			active = False
			success = not failure
			break
		elif o.startswith(b'ABORT:'):
			active = False
			break;
		elif o.startswith(b'FAILURE:'):
			# we skip consequent failures, the first one is of main interest
			if not failure:
				n = o.rfind(b':')
				failure = int(o[n+1:].strip())
		elif o.startswith(b'EXPECT-DEATH:'):
			expecting_death = True
		elif b'user land exception:' in o:
			# XXX this will eventually become a signal, but not now
			died = True
			break
		else:
			logging.warn('%s: unexpected line "%s"' % (tid, o))

	# next is to give a nice status
	test_ok = False
	test_result = "(unknown)"
	if active is None:
		test_result = "result parser never started"
	elif died:
		if expecting_death:
			test_result = "success (termination expected)"
			test_ok = True
		else:
			test_result = "failure (unexpected termination)"
	elif expecting_death:
		test_result = "failure (termination expected but did not happen)"
	elif active:
		test_result = "result parser never finished"
	elif failure is not None:
		test_result = "failure in line " + str(failure)
	elif success:
		test_result = "success"
		test_ok = True

	return test_ok, test_result, failure
