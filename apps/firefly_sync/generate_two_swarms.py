import random

# Parameters
#---------------------
# N1 - number of fireflies in swarm 1
# N2 - number of fireflies in swarm 2
# K - connectivity, a firefly can see flashes from its K nearest neighbours
# period - The period of each loop

N1=10
N2=10
K=8
period=720000

print '<GraphInstance id=\"firefly_forest\" graphTypeId=\"firefly_sync\">'
print '<DeviceInstances sorted=\'1\'>'
for i in range(N1):
	print '\t<DevI id=\"firefly_s1_'+str(i)+'\" type=\"firefly\">'
	print '\t\t<P>\"start_phase\": '+str(random.randint(0,period))+' </P>'	
	print '\t</DevI>'

for i in range(N2):
	print '\t<DevI id=\"firefly_s2_'+str(i)+'\" type=\"firefly\">'
	print '\t\t<P>\"start_phase\": '+str(random.randint(0,period))+' </P>'	
	print '\t</DevI>'

print '</DeviceInstances>'


print '<EdgeInstances sorted=\'1\'>'
for dev in range(N1):
	print '\t<EdgeI path=\"firefly_s1_'+str(dev)+':tick_in-firefly_s1_'+str(dev)+':tick_out\"/>'
	for i in range(N1):
		if i != dev:
			if (i >= dev - K/2) and (i <= dev + K/2):
				print '\t\t<EdgeI path="firefly_s1_'+str(dev)+':flash_in-firefly_s1_'+str(i)+':flash_out"/>'

for dev in range(N2):
	print '\t<EdgeI path=\"firefly_s2_'+str(dev)+':tick_in-firefly_s2_'+str(dev)+':tick_out\"/>'
	for i in range(N2):
		if i != dev:
			if (i >= dev - K/2) and (i <= dev + K/2):
				print '\t\t<EdgeI path="firefly_s2_'+str(dev)+':flash_in-firefly_s2_'+str(i)+':flash_out"/>'

print '</EdgeInstances>'

print '</GraphInstance>'
print '</Graphs>'
