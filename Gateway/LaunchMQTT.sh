#!/bin/bash

gnome-terminal -x sh -c "echo '=============================\n======= Broker Terminal =====\n============================='; 
			mosquitto -p  $1;
			exit; 
			bash"

						
gnome-terminal -x sh -c "echo '=============================\n=======     Gateway Output =====\n=============================';
			java -jar Gateway.jar './serialdump-linux $2' $1;
			exit;
			bash"

			

nb=$#
counter=1
while [ $counter -le $nb ]
do
	gnome-terminal -x sh -c "echo '====================================\n==== Subscriber on ${!counter} ====\n====================================';
				mosquitto_sub -h 127.0.0.1 -p 1889 -t ${!counter};
				exit;		
				bash"
	((counter+=1))
done

exit
