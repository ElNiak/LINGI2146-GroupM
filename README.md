# LINGI2146-GroupM

Note that this project has been made using contiki 2.6, so the tests have been made with only four Z1 nodes. 
We are aware that some class have been changed from 2.6 to 3 but the project was almost finished when we saw this anomaly.
Also note that our network works a lot better in simulation than on real modes where the modes have sometimes trouble to find the adresse of the root. 

How to set-up the network : 
1. Run the Root nodes (don't forget to specify the TARGET):
make clean && make RPLRoot.upload nodeid=1 nodemac=1 MOTES=/dev/ttyUSB0

2. Run the Sender nodes (don't forget to specify the TARGET):
make clean && make RPLSender.upload nodeid=X nodemac=X MOTES=/dev/ttyUSBY 

3. Launch the bash file to :
- Start the MQTT broker 
- Start the Gateway
- Start some subscibers

cd Root-Gateway
./Y.sh p sp [topics*]
where : 
Y = LaunchSimulationGatewayMQTT if you want to launch launch the simulation for simulated nodes cooja OR  Y = LaunchGatewayMQTT if you want to launch the gateway for real modes
p is the port of the broker you want to start
sp is the serial port of the root mode 
topics is a list of topics you want to subscribre

Exemple :
./LaunchGatewayMQTT.sh 1888 /dev/ttyUSB0 temperature humidity
./LaunchSimulationGatewayMQTT.sh 1886 /dev/pts/4 temperature humidity

