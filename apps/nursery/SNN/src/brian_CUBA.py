from brian2 import *
import matplotlib.pyplot as plt

import sys

def poets_set_neuron_model_name(dst, name):
    dst.add_attribute("poets_neuron_model_name")
    dst.poets_neuron_model_name=name

def poets_set_synapse_weight_name(dst, name):
    dst.add_attribute("poets_synapse_weight_name")
    dst.poets_synapse_weight_name=name


def build_CUBA():
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
    poets_set_neuron_model_name(P, "CUBA")

    we = (60*0.27/10)*mV # excitatory synaptic weight (voltage)
    wi = (-20*4.5/10)*mV # inhibitory synaptic weight
    Ce = Synapses(P, P, on_pre='ge += we')
    Ci = Synapses(P, P, on_pre='gi += wi')
    Ce.connect('i<3200', p=0.02)
    Ci.connect('i>=3200', p=0.02)
    poets_set_synapse_weight_name(Ce, "we")
    poets_set_synapse_weight_name(Ci, "wi")


    s_mon = SpikeMonitor(P)

    M = StateMonitor(P, 'v', record=True)

    ns=locals()

    return (M,P,Ce,Ci,ns)

if __name__=="__main__":

    set_device('cpp_standalone', directory='./wibble', debug=True)

    (M,P,Ce,Ci) =build_CUBA()

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