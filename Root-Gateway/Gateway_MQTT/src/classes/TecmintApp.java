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


public class TecmintApp {
	
	static final String[] TYPES = {"1","2"};
    static final int QOS = 2;
    static final String CLIENTID = "sample";
    
    
    //SerialPort porta = Serial.getInstance().getSerialPort();
	
	public static boolean AcceptType(String Type)
	{
		for(String t : TYPES)
		{	
			if(t.equals(Type)) return true;
		}
		return false;
	}


	public static void main(String[] args) throws IOException  {
		Runtime rt = Runtime.getRuntime();	
		HashMap<String, String> dic = new HashMap<String, String>();
		dic.put(TYPES[0], "humidity");
		dic.put(TYPES[1], "temperature");
		
		String BROKER = "tcp://127.0.0.1:"+args[1];
		
		final Process proc =  rt.exec("make login MOTES=/dev/ttyUSB0");
		
		System.out.println("Listening using : "+args[0]);
		System.out.println("MQTT Port : "+args[1]);
		
		final BufferedReader stdInput = new BufferedReader(new InputStreamReader(proc.getInputStream()));
		final BufferedWriter stdOupit = new BufferedWriter(new OutputStreamWriter(proc.getOutputStream()));
		final BufferedReader stdInputIn = new BufferedReader(new InputStreamReader(System.in));
		
        Thread threadMQTT = new Thread() {
            public void run() {
				try {
				
					MemoryPersistence persistence = new MemoryPersistence();
				    MqttClient sampleClient = new MqttClient(BROKER, CLIENTID, persistence);
			        MqttConnectOptions connOpts = new MqttConnectOptions();
			        connOpts.setCleanSession(true);
			        sampleClient.connect(connOpts);
					
			        
					String s = null;
		
					while((s = stdInput.readLine()) != null ){
						String[] infos = s.split(" ");
						
						if(infos[0].equals("#")) {
							  if(infos.length == 4 && AcceptType( infos[2])){
								    String node_id = infos[1];
								    if(node_id.equals("stop")) break;

								    String type = dic.get(infos[2]);
								    String data = infos[3];
								    
							    	//System.out.println("Node id : "+infos[0]+"\tData : "+infos[2]+"\tType : "+infos[1]);
							    	MqttMessage message = new MqttMessage(data.getBytes());
							        message.setQos(QOS);
							        sampleClient.publish(type, message);
							        sampleClient.publish("node"+node_id+"/"+type, message);
							        sampleClient.publish("node"+node_id, message);
							    }
							    else if(infos[1].equals("stop")) {
							    	break;
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
        
        Thread threadConsole = new Thread() {
            public void run() {
    			String s = null;
    			try {
					while((s = stdInputIn.readLine()) != null) {
						String[] infos = s.split(" ");
						if(infos.length == 1)
						{
							if(infos[0].equals("nothing"))
							{
								proc.getOutputStream().write(("4"+"\n").getBytes());
							    proc.getOutputStream().flush();
							}
							else if(infos[0].equals("onChange"))
							{
								proc.getOutputStream().write(("1"+"\n").getBytes());
							    proc.getOutputStream().flush();
							}
							else if(infos[0].equals("periodical"))
							{
								proc.getOutputStream().write(("0"+"\n").getBytes());
							    proc.getOutputStream().flush();
							}
							else if(infos[0].equals("stopTemp"))
							{
								proc.getOutputStream().write(("2"+"\n").getBytes());
							    proc.getOutputStream().flush();
							}
							else if(infos[0].equals("stopHum"))
							{
								proc.getOutputStream().write(("3"+"\n").getBytes());
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
