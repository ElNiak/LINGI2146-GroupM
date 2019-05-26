package classes;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.HashMap;

import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;

/**
 * Read output data from the root node + send the data sensor to the MQTT broker	
 * 
 */
public class TecmintApp {

	static final String[] TYPES = {"1","2"}; //1 = humidité, 2 = temperature
    static final int QOS = 2; //Quality of service
    static final String CLIENTID = "sample"; //Name of the client
    
	public static boolean AcceptType(String Type)
	{
		//Type == 1 or 2
		for(String t : TYPES)
		{	
			if(t.equals(Type)) return true;
		}
		return false;
	}


	public static void main(String[] args) throws IOException  {
		Runtime rt = Runtime.getRuntime();	
		HashMap<String, String> dic = new HashMap<String, String>(); //Dictionary to map integer type to their string type
		dic.put(TYPES[0], "humidity"); 
		dic.put(TYPES[1], "temperature");
		
		String BROKER = "tcp://127.0.0.1:"+args[0];

		final Process proc =  rt.exec(args[1]); //execute ./serial dump with the corresponding port to communicate with the port

		System.out.println("Listening using : "+args[1]); 
		System.out.println("MQTT Port : "+args[0]);
		
		final BufferedReader stdInput = new BufferedReader(new InputStreamReader(proc.getInputStream())); //Read the ouptut of the root
		final BufferedWriter stdOupit = new BufferedWriter(new OutputStreamWriter(proc.getOutputStream())); //Send data to the stream
		final BufferedReader stdInputIn = new BufferedReader(new InputStreamReader(System.in)); //Console to type command line
		
		TopicListener listener = new TopicListener(proc.getOutputStream());
		
		//This thread listen to the root and send the data to the MQTT broker
        Thread threadMQTT = new Thread() {
            public void run() {
				try {
					
					MemoryPersistence persistence = new MemoryPersistence();
				    MqttClient sampleClient = new MqttClient(BROKER, CLIENTID, persistence);
			        MqttConnectOptions connOpts = new MqttConnectOptions();
			        connOpts.setCleanSession(true);
			        sampleClient.setCallback(listener); //Callback used to count the number of MQTT subrscriber
			       
			        sampleClient.connect(connOpts);
			        sampleClient.subscribe("$SYS/broker/log/#"); //Topic used to count the number of MQTT subrscriber
			        
					String s = null;
					
					//Read each new data from the root
					while((s = stdInput.readLine()) != null ){
						System.out.println(s);
						
						String[] infos = s.split(" ");
						
						if(infos[0].equals("#")) { //row starting by # advertises a sensor data 
							  if(infos.length == 4 && AcceptType( infos[2])){
								    String node_id = infos[1]; //Id of the sender mote

								    String type = dic.get(infos[2]); //Convert integer type to string type
								    String data = infos[3]; //get data
								    
							    	MqttMessage message = new MqttMessage(data.getBytes());
							        message.setQos(QOS);
							        sampleClient.publish(type, message); //send data to the topic "temperature" or "humidity"
							        sampleClient.publish("node"+node_id+"/"+type, message); //send data to the topic used to listen to a specific mote and a specific metric
							        sampleClient.publish("node"+node_id, message);//send data to the topic used to listen to a specific mote
							    }
						}
						
					}
					sampleClient.disconnect();
					System.exit(0);
				} catch (MqttException e) {
					System.out.println("Error when using MQTT");
					e.printStackTrace();
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
            }
        };
        
        //Console of the gateway, listen to input command user
        Thread threadConsole = new Thread() {
            public void run() {
    			String s = null;
    			System.out.println("\nCommand possible : \n"
    					+ "onChange\t : to send data only when a change occurs\n"
    					+ "periodical\t : to send data ever x seconds\n"
    					+ "nothing\t\t : to stop the transfert of data\n");
    			try {
					while((s = stdInputIn.readLine()) != null) {
						/* Behaviour of the sensor node :
						   - 0 : data is sent periodically (every x seconds)
						   - 1 : data is sent only when there is a change
						   - 4 : data is not sent since there are no subscribers at all.
						*/
						String[] infos = s.split(" ");
						if(infos.length == 1)
						{
							//Stop the transfer of data
							if(infos[0].equals("nothing"))
							{
								//If nothing => send 4
								proc.getOutputStream().write(("4"+"\n").getBytes());
							    proc.getOutputStream().flush();
							}
							//Send data only if a change appears
							else if(infos[0].equals("onChange"))
							{
								//If onChange => send 1
								proc.getOutputStream().write(("1"+"\n").getBytes());
							    proc.getOutputStream().flush();
							}
							//Send data each x second
							else if(infos[0].equals("periodical"))
							{
								//If Periodical => send 0
								proc.getOutputStream().write(("0"+"\n").getBytes());
							    proc.getOutputStream().flush();
							}
						}
					}
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				} 
            }
        };
        

        threadMQTT.start();
        threadConsole.start();
      
       
        
	}	
	
	
}
