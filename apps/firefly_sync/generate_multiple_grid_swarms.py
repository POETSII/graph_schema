import random

# Parameters
#---------------------
# S - number of firefly in swarms
# N - number of fireflies in each swarm
# L - number of "link" fireflies that connect all the swarms
# sPeriod - The period of each firefly in a swarm 
# LPeriod - The period of each link firefly should be an integer multiple of the sPeriod 

S=1
N=2
M=2
L=0 
sPeriod=5000
LPeriod=3
sNudge=int(round(0.30*sPeriod))
sFlashzone=int(round(0.01*sPeriod))

print '<GraphInstance id=\"firefly_forest\" graphTypeId=\"firefly_sync\">'
print '<DeviceInstances sorted=\'1\'>'
for s in range(S):
	for n in range(N):	
		for m in range(M):
			print '\t<DevI id=\"firefly_s'+str(s)+'_'+str(n*M)+str(m)+'\" type=\"firefly\">'
			print '\t\t<P>\"start_phase\": '+str(random.randint(0,sPeriod))+', \"period\": '+str(sPeriod)+', \"phase_nudge\":'+str(sNudge)+', \"flashzone\":'+str(sFlashzone)+', \"slowflash_period\":'+str(LPeriod)+' </P>'	
			print '\t</DevI>'

for i in range(L):
	print '\t<DevI id=\"firefly_s999_'+str(i)+'\" type=\"firefly\">'
	print '\t\t<P>\"start_phase\": '+str(random.randint(0,LPeriod*sPeriod))+', \"period\": '+str(LPeriod*sPeriod)+', \"phase_nudge\":'+str(sNudge)+', \"flashzone\":'+str(sFlashzone)+', \"slowflash_period\":'+str(LPeriod)+' </P>'	
	print '\t</DevI>'

print '</DeviceInstances>'


print '<EdgeInstances sorted=\'1\'>'
for s in range(S):
	for n in range(N):
		for m in range(M):

			print '\t<EdgeI path=\"firefly_s'+str(s)+'_'+str(n*M)+str(m)+':tick_in-firefly_s'+str(s)+'_'+str(n*M)+str(m)+':tick_out\"/>'

			#North boundary
			if n != (N-1):
				print '\t\t<EdgeI path="firefly_s'+str(s)+'_'+str(n*M)+str(m)+':flash_in-firefly_s'+str(s)+'_'+str((n+1)*M)+str(m)+':flash_out"/>'
				                                                                                                               
			#East boundary
			if m != (M-1): 
				print '\t\t<EdgeI path="firefly_s'+str(s)+'_'+str(n*M)+str(m)+':flash_in-firefly_s'+str(s)+'_'+str(n*M)+str(m+1)+':flash_out"/>'
                                                                                                                       
			#South boundary
			if n != 0:            
				print '\t\t<EdgeI path="firefly_s'+str(s)+'_'+str(n*M)+str(m)+':flash_in-firefly_s'+str(s)+'_'+str((n-1)*M)+str(m)+':flash_out"/>'

			#West boundary
			if m != 0:
				print '\t\t<EdgeI path="firefly_s'+str(s)+'_'+str(n*M)+str(m)+':flash_in-firefly_s'+str(s)+'_'+str(n*M)+str(m-1)+':flash_out"/>'
				

for l in range(L):
	print '\t<EdgeI path=\"firefly_s999_'+str(l)+':tick_in-firefly_s999_'+str(l)+':tick_out\"/>'
	for s in range(S):
		print '\t\t<EdgeI path="firefly_s999_'+str(l)+':flash_in-firefly_s'+str(s)+'_'+str(l)+str(l)+':slowflash_out"/>'
		print '\t\t<EdgeI path="firefly_s'+str(s)+'_'+str(l)+str(l)+':flash_in-firefly_s999_'+str(l)+':flash_out"/>'

print '</EdgeInstances>'

print '</GraphInstance>'
print '</Graphs>'
