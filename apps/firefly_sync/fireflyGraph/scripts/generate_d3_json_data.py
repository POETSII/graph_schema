import sys, getopt
import xml.etree.ElementTree
import re

def main(argv):
	inFileName=''
	try:
		opts, args = getopt.getopt(argv, "h:i", ["input="])
	except getopt.GetoptError:
		print 'Usage python generate_d3_json_data.py --input graph_instance.xml'
		sys.exit()
	for opt, arg in opts:
		if opt == '-h':
			print 'Usage python generate_d3_json_data.py --input graph_instance.xml'
			sys.exit()
		if opt in ("-i", "--input"):
			inFileName = arg
	assert(inFileName != '')

	graphversion = '{https://poets-project.org/schemas/virtual-graph-schema-v2}'

	root = xml.etree.ElementTree.parse(inFileName).getroot()

	o = '{\n'
	o+= '\t\"nodes\": [\n'
	for gi in root.findall(graphversion+'GraphInstance'):
		for devI in gi.findall(graphversion+'DeviceInstances'):	
			for dev in devI.findall(graphversion+'DevI'):	
				fname = dev.get('id')
				o+= '\t\t{\"id\": \"'+fname+'\", \"group\": 1},\n'
	o = o[:-2] #removing the last comma
	o+= '\n\t],\n'

	o+= '\t\"links\": [\n'
	for gi in root.findall(graphversion+'GraphInstance'):
		for edgeI in gi.findall(graphversion+'EdgeInstances'):	
			for edge in edgeI.findall(graphversion+'EdgeI'):	
				pathstr = edge.get('path')
				regex = re.compile(r'(firefly_s[0-9]+_[0-9]+):flash_in\-(firefly_s[0-9]+_[0-9]+):flash_out')
				m = regex.search(pathstr)
				if m:
					o+= '\t\t{\"source\": \"'+ str(m.group(1)) +'\", \"target\": \"'+str(m.group(2))+'\", \"value\":2},\n'
	o = o[:-2] #remove the last comma
	o+= '\n\t]\n'

	o+= '}\n'
	print o


if __name__== "__main__":
	main(sys.argv[1:])
