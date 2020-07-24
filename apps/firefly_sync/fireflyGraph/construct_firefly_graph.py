import sys, getopt
import xml.etree.ElementTree
import re
import os, shutil
import subprocess

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

	temporary_dir = './_tmp'
	print 'creating temp directory '+temporary_dir
	if os.path.exists(temporary_dir):
		print 'removing old build @ ' + temporary_dir
		shutil.rmtree(temporary_dir)
	os.mkdir(temporary_dir)
	subprocess.call(['chmod', '0777', temporary_dir])
	
	cpy_cmd = 'cp ./index.html '+temporary_dir+'/;'
	cpy_cmd += 'cp '+inFileName+' '+temporary_dir+'/;'
	cpy_cmd += 'cp ./scripts/launch_webserver.sh '+temporary_dir+'/;'
	cpy_cmd += 'cp ./scripts/rt_log_filter.py '+temporary_dir+'/;'
	cpy_cmd += 'cp ./scripts/refresh_log.php '+temporary_dir+'/;'
	os.system(cpy_cmd)
	subprocess.call(['chmod', '-R 7777', temporary_dir])

	print 'generating data.json for d3.js plot'
	json_gen = 'python ./scripts/generate_d3_json_data.py --input '+inFileName+' > '+temporary_dir+'/data.json'
	os.system(json_gen)
	

	print 'launching webserver...'
	serve_cmd = 'cd '+temporary_dir+'; ./launch_webserver.sh '
	os.system(serve_cmd)

if __name__== "__main__":
	main(sys.argv[1:])
