import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.HashMap;

import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;


public class NorthboundAPI implements Runnable {
	private int PORT;  // Northbound Server Port
	private String SERVER;
	private static Socket socket;
	
	static HashMap<String,String> LC_NID=new HashMap<String,String>();  
	
	NorthboundAPI(String server, int port){
		this.SERVER = server;
		this.PORT = port;
	} 

	//Waiting to receive data from Northbound 
	public void run(){ 
		while(true){			
			BufferedReader in;
			String msgStr = new String();
			try {
				System.out.println("Trying to connect to CORAL-SDN server:["+SERVER+"] port:["+PORT+"]...");
				socket = new Socket(SERVER, PORT);				
				in = new BufferedReader(new InputStreamReader(socket.getInputStream(),"UTF-8"));
				System.out.println("Connected to CORAL-SDN Server!!!");		
							
			    while((msgStr=in.readLine()) != null){
					System.out.println("["+System.currentTimeMillis()+"] Received from Northbound (CORAL-SDN): "+msgStr);						
					// Format JSON packet
					try{
						JSONParser parser = new JSONParser();
						JSONObject msg = new JSONObject();
						msg = (JSONObject) parser.parse(msgStr);
						//System.out.println("LC ="+ msg.get("LC_id"));
						//System.out.println("Msg="+ msg.get("Msg"));
						JSONObject packet = new JSONObject();
						packet = (JSONObject) msg.get("Msg");
						for(SouthboundAPI sb : ConnectorMain.southbound){ 
							if(packet.get("BID").equals(sb.nodeId))
								sb.send(packet);
						}																							
					} catch (ParseException e) {
						if(msgStr.length()>4){
							System.out.println("JSON Parsing message from CORAL-SDN controller Error");
							e.printStackTrace();
						}
					}  
				} // End reading while true
			}
			catch (IOException e) {
				socket = null;
				System.out.println("Disconnected from CORAL-SDN server!!!");
			} 
			try {
				Thread.sleep(500);
				System.out.println("Thread sleep 500ms wating for server!!!");
			} catch (InterruptedException e) {
				System.out.println("Thread sleep error!!!");
			}
		} // While TRUE	
	}
	
	public static void send(JSONObject msg){
		while(ConnectorMain.sending==false);
		ConnectorMain.sending=false;
		try {			
			System.out.println("Sending Message to Northbound (CORAL-SDN): "+msg);
			if (socket != null){	
				PrintStream write = new PrintStream(socket.getOutputStream());          
				write.println(msg);
				write.flush();				
			}
		} catch (IOException e) {
			e.printStackTrace();
		} 
		ConnectorMain.sending=true;
	}	
}
