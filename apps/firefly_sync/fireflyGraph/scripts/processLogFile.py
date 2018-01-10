import sys, getopt
import xml.etree.ElementTree
import re
import csv
import matplotlib.pyplot as plt

def main(argv):
	inFileName=''
	try:
		opts, args = getopt.getopt(argv, "h:i", ["input="])
	except getopt.GetoptError:
		print 'Usage python processLogFile.py --input log.csv'
		sys.exit()
	for opt, arg in opts:
		if opt == '-h':
			print 'Usage python processLogFile.py --input log.csv'
			sys.exit()
		if opt in ("-i", "--input"):
			inFileName = arg
	assert(inFileName != '')

	occurence_dict = { } 
	with open(inFileName, 'rb') as f:
		reader = csv.reader(f)
		for event in reader:
				occurence_dict[event[0]] = int(occurence_dict.get(event[0], 1)) + 1
				
	time_str = list(occurence_dict.keys())
	#time_int = [int(x) for x in time_str]
	time_float = [float(x) for x in time_str]
	#print 'Tmin: '+ str(min(time_int)) +',  Tmax:  '+str(max(time_int))
	print 'Tmin: '+ str(min(time_float)) +',  Tmax:  '+str(max(time_float))

	#total_time = range(min(time_int), max(time_int))
	#total_time = range(min(time_float), max(time_float), 0.05)
	#occurences = [ ]
	#for t in total_time:
	#	if str(t) in occurence_dict:
	#		occurences.append(occurence_dict[str(t)])
	#	else:
	#		occurences.append(0)			 

#	occurences = [occurence_dict[x] for x in occurence_dict]
	print 'Omin: '+ str(min(occurences)) +',  Omax:  '+str(max(occurences))
	print 'Olen: '+str(len(occurences)) + ',  Tlen:  '+str(len(total_time))

	#plt.scatter(total_time, occurences, c='r')	
	#plt.plot(total_time, occurences)
	plt.plot(total_float, occurences)
	plt.axis([min(time_float), max(time_float), 1, max(occurences)])
	plt.show()

if __name__== "__main__":
	main(sys.argv[1:])
