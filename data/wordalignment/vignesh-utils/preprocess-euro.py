"""
This program takes an English or Czech newsletter dataset and
removes lines with only numbers in it"""

import sys
import re

if len(sys.argv) < 3:
	print ' Usage : python preprocess-euro.py <ipfilename> <opfilename>'

ipfilename = sys.argv[1]
contents = open(ipfilename).readlines()
opfilename = sys.argv[2]
opfile = open(opfilename, 'w')
for line in contents:
	lineprocessed = re.findall('^[0-9]+[0-9]*\s*.\s*\n$', line)
	if lineprocessed == []:
		opfile.write(line) # Carriage return is present in the line alread
	else:
		print 'Number line encountered'	
		continue

	

opfile.close()