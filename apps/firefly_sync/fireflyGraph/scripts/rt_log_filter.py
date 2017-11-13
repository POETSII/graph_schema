import sys
import time
import re

regex = re.compile(r'(firefly_s[0-9]+_[0-9]+)')

o = 'firefly\n'
logfile = open("./data_update", "wb")
i = 0
startT = time.time()

sys.stdout.write('time,firefly\n')

#for line in sys.stdin:
#	m = regex.search(line)
#	if m:
#		sys.stderr.write(str(time.time() - startT)+','+m.group(0)+'\n')

while True:
	l=sys.stdin.readline()		
	sys.stdout.write(str(time.time() - startT)+','+l)

