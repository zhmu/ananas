#!/usr/bin/env python3

import cgi

def _generate_css():
	return '''
	table#results {
		width: 100%;
	}
	table#results th#testTh {
		width: 20%;
	}
	table#results {
		padding: 4px;
	}
	table#results tr:hover, table#results th:hover {
		background-color: #d0d0d0;
	}
	.filter {
		text-decoration: none;
		color: #0000ff;
	}
	.filterActive {
		font-weight: bold;
	}
	.filterInactive {
		font-weight: normal;
	}
	tr.testOk {
	}
	tr.testFailure {
		outline: 1px #ff0000 solid;
	}
	'''

def _generate_filter(counts):
	return '''<div>
	filter: <a class="filter filterAll" href="#">all</a> (%s) | <a class="filter filterFailed" href="#">failed</a> (%s) | <a class="filter filterSucceeded" href="#">succeeded</a> (%s)
	</div>
	''' % (counts['total'], counts['failure'], counts['success'])

def _generate_filter_js():
	return '''
	$('.filterAll').click(function() {
		$('.filter').addClass('filterInactive');
		$('.filterAll').removeClass('filterInactive');
		$('.filterAll').addClass('filterActive');
		$('.testOk').show();
		$('.testFailure').show();
	});
	$('.filterFailed').click(function() {
		$('.filter').addClass('filterInactive');
		$('.filterFailed').removeClass('filterInactive');
		$('.filterFailed').addClass('filterActive');
		$('.testOk').hide();
		$('.testFailure').show();
	});
	$('.filterSucceeded').click(function() {
		$('.filter').addClass('filterInactive');
		$('.filterSucceeded').removeClass('filterInactive');
		$('.filterSucceeded').addClass('filterActive');
		$('.testOk').show();
		$('.testFailure').hide();
	});
	$('.filterAll').click();'''

def generate(test_plan):
	sorted_tests = sorted(test_plan.keys())

	output = '''<html>
	<head>
	<title>Ananas Test Framework Results</title>
	<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js"></script>
	<script src="https://cdn.rawgit.com/google/code-prettify/master/loader/run_prettify.js"></script>
	<style type="text/css">
	'''
	output += _generate_css()

	counts = {
		'total': len(sorted_tests),
		'success': len([ True for tid in sorted_tests if test_plan[tid]['result']['ok'] ])
	}
	counts['failure'] = counts['total'] - counts['success']

	for tnum, tid in enumerate(sorted_tests):
		t = test_plan[tid]
		if not t['result']['failure-line']:
			continue

		output += '''#source%s li:nth-child(%s) {
			outline: 1px solid #f00000;
			font-weight: bold;
		}
		''' % (tnum, t['result']['failure-line'])

	output += '''</style>
	</head>
	<body>
	'''

	output += _generate_filter(counts)

	output += '''
	<div>
	<table id="results">
		<tr>
			<th id="testTh">Test</th>
			<th>Status</th>
		</tr>
	'''

	for tnum, tid in enumerate(sorted_tests):
		t = test_plan[tid]

		with open(t['source'], 'rt') as f:
			source = ''.join(f.readlines())

		tr_class = 'testFailure'
		if t['result']['ok']:
			tr_class = 'testOk'
		output += '''
		<tr id="test%s" class="%s">
			<td>%s</td>
			<td>
				<div>%s</div>
				<div id="detail%s" style="display:none">
					<div>%s</div>
					<pre id="source%s" class="prettyprint linenums">%s</pre>
				</div>
			</td>
		</tr>
		''' % (tnum, tr_class, tid, t['summary'], tnum, t['result']['message'], tnum, cgi.escape(source))

	output += '''</table>
	</div>'''
	output += _generate_filter(counts)

	output += '''</body><script type="text/javascript">'''
	for tnum, tid in enumerate(sorted_tests):
		output += '''$('#test%s').click(function() {\n''' % tnum
		output += '''	$('#detail%s').toggle();\n''' % tnum
		output += '''});\n'''

	output += _generate_filter_js()
	output += '''</script></html>'''

	return output
