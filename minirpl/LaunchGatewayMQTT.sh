#!/bin/bash

gnome-terminal -x sh -c "echo '=============================\n======= Broker Terminal =====\n============================='; 
			mosquitto -p $1 -c mosquitto-custom.conf;
			exit; 
			bash"

						
gnome-terminal -x sh -c "echo '=============================\n=======     Gateway Output =====\n=============================';
			java -jar Gateway.jar $1 'make login MOTES=$2';
			exit;
			bash"

			
sleep 3s

nb=$#
counter=3
while [ $counter -le $nb ]
do
	gnome-terminal -x sh -c "echo '====================================\n==== Subscriber on ${!counter} ====\n====================================';
				mosquitto_sub -h 127.0.0.1 -p $1 -t ${!counter};
				exit;		
				bash"
	((counter+=1))
done

exit
