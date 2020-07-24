import random

# Parameters
#---------------------
# N1 - number of fireflies in swarm 1
# N2 - number of fireflies in swarm 2
# L - number of "link" fireflies that connect the two swarms
# K - connectivity, a firefly can see flashes from its K nearest neighbours
# sPeriod - The sPeriod of each loop

N1=10
N2=10
L=1 
K=8
sPeriod=360000
LPeriod=3*sPeriod

print '<GraphInstance id=\"firefly_forest\" graphTypeId=\"firefly_sync\">'
print '<DeviceInstances sorted=\'1\'>'
for i in range(N1):
	print '\t<DevI id=\"firefly_s1_'+str(i)+'\" type=\"firefly\">'
	print '\t\t<P>\"start_phase\": '+str(random.randint(0,sPeriod))+', \"period\": '+str(sPeriod)+' </P>'	
	print '\t</DevI>'

for i in range(N2):
	print '\t<DevI id=\"firefly_s2_'+str(i)+'\" type=\"firefly\">'
	print '\t\t<P>\"start_phase\": '+str(random.randint(0,sPeriod))+', \"period\": '+str(sPeriod)+' </P>'	
	print '\t</DevI>'

for i in range(L):
	print '\t<DevI id=\"firefly_link_'+str(i)+'\" type=\"firefly\">'
	print '\t\t<P>\"start_phase\": '+str(random.randint(0,LPeriod))+', \"period\": '+str(LPeriod)+' </P>'	
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

for l in range(L):
	print '\t<EdgeI path=\"firefly_link_'+str(l)+':tick_in-firefly_link_'+str(l)+':tick_out\"/>'
	print '\t\t<EdgeI path="firefly_link_'+str(l)+':flash_in-firefly_s1_'+str(l)+':flash_out"/>'
	print '\t\t<EdgeI path="firefly_link_'+str(l)+':flash_in-firefly_s2_'+str(l)+':flash_out"/>'
	print '\t\t<EdgeI path="firefly_s1_'+str(l)+':flash_in-firefly_link_'+str(l)+':flash_out"/>'
	print '\t\t<EdgeI path="firefly_s2_'+str(l)+':flash_in-firefly_link_'+str(l)+':flash_out"/>'

print '</EdgeInstances>'

print '</GraphInstance>'
print '</Graphs>'
