#!/bin/bash

# delete organizer_on.txt if exist
if ls ./result/*.txt 1> /dev/null 2>&1;then
	rm ./result/*.txt
fi

declare -a number_of_process=( 100 200 300 500 1000 )
# declare -a number_of_process=(1000 2000 5000 10000 15000 20000 30000 40000 50000 75000 100000)
number_of_vm=60

# ( 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 )

for comp_per_comm in 0.2
do
	# wihthout organization
	printf "####### ORGANIZATION OFF # comp/comm=%.1f #######\n" $comp_per_comm
	counter=1
	for i in "${number_of_process[@]}"
	do
		./build/hpvc ./cluster2.xml ./deploymentOff.xml $number_of_vm $i $comp_per_comm 0.1 > ./build/off.txt 2>/dev/null;

		is_ok=$(cat ./build/off.txt | grep "successfully" | wc -l)
		if [ $is_ok = 1 ]; then

			slow_down=$(cat ./build/off.txt | grep "slow down" | awk '{ print $7 }')
			simulation_time=$(cat ./build/off.txt | grep "Time" | awk '{ print $3 }')
			
			printf "%s) %s\t %s\t %s\t %s\n" $counter $number_of_vm	$i	$slow_down	$simulation_time # to wath on screan
			echo $number_of_vm	$i	$slow_down	$simulation_time >> ./result/organizer_off_${comp_per_comm}.txt; # to save in file
		else
			echo $counter ")" $number_of_vm $i NAN NAN # to wath in screan
		fi
		counter=$((counter+1))
	done

	for alpha in 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9
	do
		# wiht organization
		printf "####### ORGANIZATION ON # comp/comm=%.1f # alpha=%.1f #######\n" $comp_per_comm $alpha
		counter=1
		for i in "${number_of_process[@]}"
		do
			./build/hpvc ./cluster2.xml ./deploymentOn.xml $number_of_vm $i $comp_per_comm $alpha > ./build/on.txt 2>/dev/null;

			is_ok=$(cat ./build/on.txt | grep "successfully" | wc -l)
			if [ $is_ok = 1 ]; then

				migration=$(cat ./build/on.txt | grep "migration" | awk '{ print $5 }')
				slow_down=$(cat ./build/on.txt | grep "slow down" | awk '{ print $7 }')
				simulation_time=$(cat ./build/on.txt | grep "Time" | awk '{ print $3 }')
				
				printf "%s) %s\t %s\t %s\t %s\t%s\n" $counter $number_of_vm $i	$slow_down $simulation_time $migration # to wath on screan
				echo $number_of_vm	$i	$slow_down	$simulation_time	$migration >> ./result/organizer_on_${comp_per_comm}_${alpha}.txt; # to save in file
			else
				echo $counter ")" $number_of_vm $i NAN NAN NAN # to wath in screan
			fi
			counter=$((counter+1))
		done
	done

done



