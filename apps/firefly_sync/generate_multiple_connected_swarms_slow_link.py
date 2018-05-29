import random

# Parameters
#---------------------
# S - number of firefly in swarms
# N - number of fireflies in each swarm
# K - connectivity within a swarm, a firefly can see flashes from its K nearest neighbours
# L - number of "link" fireflies that connect all the swarms
# sPeriod - The period of each firefly in a swarm 
# LPeriod - The period of each link firefly should be an integer multiple of the sPeriod 

S=5
N=20
K=6
L=2 
sPeriod=25000
LPeriod=3*sPeriod

print '<GraphInstance id=\"firefly_forest\" graphTypeId=\"firefly_sync\">'
print '<DeviceInstances sorted=\'1\'>'
for s in range(S):
	for n in range(N):	
		print '\t<DevI id=\"firefly_s'+str(s)+'_'+str(n)+'\" type=\"firefly\">'
		print '\t\t<P>\"start_phase\": '+str(random.randint(0,sPeriod))+', \"period\": '+str(sPeriod)+' </P>'	
		print '\t</DevI>'

for i in range(L):
	print '\t<DevI id=\"firefly_link_'+str(i)+'\" type=\"firefly\">'
	print '\t\t<P>\"start_phase\": '+str(random.randint(0,LPeriod))+', \"period\": '+str(LPeriod)+' </P>'	
	print '\t</DevI>'

print '</DeviceInstances>'


print '<EdgeInstances sorted=\'1\'>'
for s in range(S):
	for dev in range(N):
		print '\t<EdgeI path=\"firefly_s'+str(s)+'_'+str(dev)+':tick_in-firefly_s'+str(s)+'_'+str(dev)+':tick_out\"/>'
		for i in range(N):
			if i != dev:
				if (i >= dev - K/2) and (i <= dev + K/2):
					print '\t\t<EdgeI path="firefly_s'+str(s)+'_'+str(dev)+':flash_in-firefly_s'+str(s)+'_'+str(i)+':flash_out"/>'
for l in range(L):
	print '\t<EdgeI path=\"firefly_link_'+str(l)+':tick_in-firefly_link_'+str(l)+':tick_out\"/>'
	for s in range(S):
		print '\t\t<EdgeI path="firefly_link_'+str(l)+':flash_in-firefly_s'+str(s)+'_'+str(l)+':flash_out"/>'
		print '\t\t<EdgeI path="firefly_s'+str(s)+'_'+str(l)+':flash_in-firefly_link_'+str(l)+':flash_out"/>'

print '</EdgeInstances>'

print '</GraphInstance>'
print '</Graphs>'
