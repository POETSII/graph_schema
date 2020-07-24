import sys, getopt
import xml.etree.ElementTree
import re
import os, shutil

def main(argv):
	inFileName=''
	try:
		opts, args = getopt.getopt(argv, "h:i", ["input="])
	except getopt.GetoptError:
		print 'Usage python execute_firefly_graph.py --input graph_instance.xml'
		sys.exit()
	for opt, arg in opts:
		if opt == '-h':
			print 'Usage python execute_firefly_graph.py --input graph_instance.xml'
			sys.exit()
		if opt in ("-i", "--input"):
			inFileName = arg
	assert(inFileName != '')

	temp_dir ='_tmp'

	build_cmd = 'cd '+temp_dir+'; /vagrant/build_tinsel_elf.sh ../' + inFileName
	#exec_cmd = 'cd '+temp_dir+'; /vagrant/execute_tinsel_elf.sh tinsel.elf | python -u ../scripts/rt_log_filter.py'  
	exec_cmd = 'cd '+temp_dir+'; /vagrant/execute_tinsel_elf.sh tinsel.elf | grep -oEi \'firefly_s[0-9]+_[0-9]+\' --line-buffered | python -u ../scripts/rt_log_filter.py > log.csv'  
	#exec_cmd = 'cd '+temp_dir+'; /vagrant/execute_tinsel_elf.sh tinsel.elf | grep -c -oEi \'firefly_s[0-9]+_[0-9]+\''  
	#exec_cmd = 'cd '+temp_dir+'; /vagrant/execute_tinsel_elf.sh tinsel.elf'  
	
	print 'Building the ELF'
	os.system(build_cmd)
	print 'Executing the ELF on the POETS hardware'
	os.system(exec_cmd)

if __name__== "__main__":
	main(sys.argv[1:])
