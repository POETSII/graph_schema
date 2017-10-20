import random

# Parameters
#---------------------
# N - number of fireflies
# K - connectivity, a firefly can see flashes from its K nearest neighbours
# period - The period of each loop

N=100
K=4
period=720000

print '<GraphInstance id=\"firefly_forest\" graphTypeId=\"firefly_sync\">'
print '<DeviceInstances sorted=\'1\'>'
for i in range(N):
	print '\t<DevI id=\"firefly_'+str(i)+'\" type=\"firefly\">'
	print '\t\t<P>\"start_phase\": '+str(random.randint(0,period))+' </P>'	
	print '\t</DevI>'
print '</DeviceInstances>'


print '<EdgeInstances sorted=\'1\'>'
for dev in range(N):
	print '\t<EdgeI path=\"firefly_'+str(dev)+':tick_in-firefly_'+str(dev)+':tick_out\"/>'
	for i in range(N):
		if i != dev:
			if (i >= dev - K/2) and (i <= dev + K/2):
				print '\t\t<EdgeI path="firefly_'+str(dev)+':flash_in-firefly_'+str(i)+':flash_out"/>'

print '</EdgeInstances>'

print '</GraphInstance>'
print '</Graphs>'
