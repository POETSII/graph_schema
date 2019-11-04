#!/bin/bash

#------------------------------------------------------------------------------------------------------------------
#This script has written to automatically run different number of nodes (neurons) and edges (synapse weights)
#User can choose the model and number of fanin and fanout for each neuron array of neuron number is declared 
#at the begining of the file that represent the repeating of the simulation and hardware running for different 
#network scale. This run.sh file is using when we want to report time after running on FPGA and only on multi-bix repository.
# if we run this file on other ripo we need to use "pts-xmlc ${model}_${i}_${fanin}_${fanout}.xml" without "--timer .."
#for both Izhekevich and LIF models.  
#-------------------------------------------------------------------------------------------------------------------

declare -a Neuron_n=( 100 500 1000 10000 100000 200000)

#declare -a Neuron_n=( 100)
#Neuron_n = (100, 200, 500, 1000, 10000, 100000, 200000)

#echo enter 1 for izikevich and 2 for LIF neuron model:
read -p "enter 1 for "Izikevich" and 2 for "LIF" neuron model:" b
echo you entered model $b

if [[ $b -eq 1 ]]
then
       model=izk
       echo $model      
elif [[ $b -eq 2 ]];then
        model=lif
        echo $model
else
        echo only 1 or 2 for choosing the model run agin the run.sh 
	exit 1
fi

echo enter the fanin and fanout:
read -p "enter fanin bumber:" fanin
echo fanainis : $fanin
read -p "enter the fanout number:" fanout
echo fanout is: $fanout
DIR="results"
if [ "$(ls -A $DIR)" ] ; then
rm -r ${DIR}/*
fi

for i in ${Neuron_n[@]}
do 
ne="$(($i*8/10))"
ni= "$(($i*2/10))"
mkdir ${DIR}/${model}_$i

if [ [model=izk] ]; then	
echo $DIR
python3.6 create_clocked_izhikevich_fix_instance.py clocked_izhikevich_fix_graph_type.xml $ne $ni 20 10 $fanin $fanout >> ${DIR}/${model}_${i}/${model}_${i}_${fanin}_${fanout}.xml    
wait
echo " ${DIR}/${model}_${i}/${model}_${i}_${fanin}_${fanout}.xml"
cd ${DIR}/${model}_$i
pts-xmlc ${model}_${i}_${fanin}_${fanout}.xml --timers -x 1 -y 1
wait
pts-serve --headless true >> hdOut_${model}_${i}_${fanin}_${fanout}.txt
wait
sed -n /Time/p hdOut_${model}_${i}_${fanin}_${fanout}.txt > rTime.txt
rm code.v data.v tinsel.elf 
cd ../../	

elif [ [model=lif] ]; then	
	{#mkdir results/izk_$i
 	python3.6  create_clocked_LIF_instance.py clocked_LIF_graph_type.xml $ne $ni 20 10 $fanin $fanout >> ${DIR}/${model}_${i}/${model}_${i}_${fanin}_${fanout}.xml    
	wait
	cd ${DIR}/${model}_$i
	pts-xmlc ${model}_${i}_${fanin}_${fanout}.xml --timers -x 1 -y 1
	wait
	pts-serve --headless true >> hdOut_${model}_${i}_${fanin}_${fanout}.txt
	wait
	sed -n /Time/p hdOut_${model}_${i}_${fanin}_${fanout}.txt > rTime.txt
	rm code.v data.v tinsel.elf 
	cd ../../}
fi	
done
echo all done












