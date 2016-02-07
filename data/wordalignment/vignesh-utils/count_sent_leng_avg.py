from __future__ import division
import sys

input_file = sys.argv[1]
punctuations = [',','//','/','?','.','!']
totallength = 0


content = open(input_file).readlines()
for line in content:

	if line.strip(' \r\n') == '':
		continue # escaping empty lines

	# remove punctuations
	for p in punctuations:
		line = line.replace(p, '')

	# split and count the number of tokens
	tokens = line.split()
	totallength += len(tokens)

print 'Average sentence length : ' + str(totallength / len(content))
print 'Total number of sentences : ' + str(len(content))

