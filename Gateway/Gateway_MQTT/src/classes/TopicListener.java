package classes;

import java.io.IOException;
import java.io.OutputStream;

import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttMessage;

/**
 * 
 * Listen to the number of subscriber connected to the MQTT broker
 */
public class TopicListener implements MqttCallback{
	
	public int nbConnection= 0;
	public OutputStream out;
	public boolean first = true;
	
	public TopicListener(OutputStream outputStream) {
		this.out = outputStream;
	}

	@Override
	public void deliveryComplete(IMqttDeliveryToken arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void messageArrived(String arg0, MqttMessage s) throws Exception {
		//Someone has deconnected
		if(s.toString().contains("disconnecting")) {
			
			nbConnection--;
			System.out.println("Number of clients : "+nbConnection);
			if(nbConnection == 0)
			{
				out.write(("4"+"\n").getBytes());
				out.flush();
				System.out.println("Ending the transfer of the data sensor");
			}
			
		} 
		//New subscriber has arrived
		else if(s.toString().contains("connected"))
		{
			
			nbConnection++;
			System.out.println("Number of clients : "+nbConnection);
			if(nbConnection == 1) 
			{
				if(!first)
				{
					out.write(("0"+"\n").getBytes());
					out.flush();
					System.out.println("Starting the transfer of the data sensor");
				}
				else {
					first = false;
				}
				
			} 
		}
	}

	@Override
	public void connectionLost(Throwable arg0) {
		// TODO Auto-generated method stub
		
	}	
	
}	
