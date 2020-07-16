from brian2 import *
import matplotlib.pyplot as plt

import sys

set_device('cpp_standalone', directory='./wibble', debug=True)

sys.stderr.write("Setting up\n")

taum = 20*ms
taue = 5*ms
taui = 10*ms
Vt = -50*mV
Vr = -60*mV
El = -49*mV

eqs = '''
dv/dt  = (ge+gi-(v-El))/taum : volt (unless refractory)
dge/dt = -ge/taue : volt
dgi/dt = -gi/taui : volt
'''

P = NeuronGroup(4000, eqs, threshold='v>Vt', reset='v = Vr', refractory=5*ms,
                method='euler')
P.v = 'Vr + rand() * (Vt - Vr)'
P.ge = 0*mV
P.gi = 0*mV

we = (60*0.27/10)*mV # excitatory synaptic weight (voltage)
wi = (-20*4.5/10)*mV # inhibitory synaptic weight
Ce = Synapses(P, P, on_pre='ge += we')
Ci = Synapses(P, P, on_pre='gi += wi')
Ce.connect('i<3200', p=0.02)
Ci.connect('i>=3200', p=0.02)

s_mon = SpikeMonitor(P)

M = StateMonitor(P, 'v', record=True)

sys.stderr.write("Running\n")
run(1 * second)

print(M.v.shape)
print(M.v[0:10,0:10])
plt.imshow(M.v.transpose() / mV)

sys.stderr.write("Shwowing\n")
plt.colorbar()
plt.show()

sys.stderr.write("Reformatting\n")
#plot(s_mon.t/ms, s_mon.i, '.k')

spike_trains=s_mon.spike_trains()
ts=[
    spike_trains[i] for i in range(len(spike_trains))
]
#for (t,i) in zip(s_mon.t,s_mon.i):
#    ts[i].append(t)

sys.stderr.write("Plotting\n")

plt.eventplot(ts)

xlabel('Time (ms)')
ylabel('Neuron index')

sys.stderr.write("Showing\n")
show()