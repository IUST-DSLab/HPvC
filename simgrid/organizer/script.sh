#!/bin/bash

# delete organizer_on.txt if exist
if [ -f ./result/organizer_on.txt ];then
	rm ./result/organizer_on.txt
fi
if [ -f ./result/organizer_off.txt ];then
	rm ./result/organizer_off.txt
fi

# declare -a number_of_process=( 50000 75000 100000 )
declare -a number_of_process=(1000 2000 5000 10000 15000 20000 30000 40000 50000 75000 100000)
number_of_vm=60


# wihthout organization
echo "########## ORGANIZATION OFF ##########"
counter=1
for i in "${number_of_process[@]}"
do
	./build/hpvc ./cluster2.xml ./deploymentOff.xml $number_of_vm $i > ./build/off.txt 2>/dev/null;

	is_ok=$(cat ./build/off.txt | grep "successfully" | wc -l)
	if [ $is_ok = 1 ]; then

		slow_down=$(cat ./build/off.txt | grep "slow down" | awk '{ print $7 }')
		simulation_time=$(cat ./build/off.txt | grep "Time" | awk '{ print $3 }')
		
		printf "%s) %s\t %s\t %s\t %s\n" $counter $number_of_vm	$i	$slow_down	$simulation_time # to wath on screan
		echo $number_of_vm	$i	$slow_down	$simulation_time >> ./result/organizer_off.txt; # to save in file
	else
		echo $counter ")" $number_of_vm $i NAN NAN # to wath in screan
	fi
	counter=$((counter+1))
done


# wiht organization
echo "########## ORGANIZATION ON ##########"
counter=1
for i in "${number_of_process[@]}"
do
	./build/hpvc ./cluster2.xml ./deploymentOn.xml $number_of_vm $i > ./build/on.txt 2>/dev/null;

	is_ok=$(cat ./build/on.txt | grep "successfully" | wc -l)
	if [ $is_ok = 1 ]; then

		migration=$(cat ./build/on.txt | grep "migration" | awk '{ print $5 }')
		slow_down=$(cat ./build/on.txt | grep "slow down" | awk '{ print $7 }')
		simulation_time=$(cat ./build/on.txt | grep "Time" | awk '{ print $3 }')
		
		printf "%s) %s\t %s\t %s\t %s\t%s\n" $counter $number_of_vm $i	$slow_down $simulation_time $migration # to wath on screan
		echo $number_of_vm	$i	$slow_down	$simulation_time	$migration >> ./result/organizer_on.txt; # to save in file
	else
		echo $counter ")" $number_of_vm $i NAN NAN NAN # to wath in screan
	fi
	counter=$((counter+1))
done

