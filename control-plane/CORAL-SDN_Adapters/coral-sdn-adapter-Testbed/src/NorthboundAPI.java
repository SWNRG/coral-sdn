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
				System.out.println("Trying to connect to CORAL server:["+SERVER+"] port:["+PORT+"]...");
				socket = new Socket(SERVER, PORT);				
				in = new BufferedReader(new InputStreamReader(socket.getInputStream(),"UTF-8"));
				System.out.println("Connected to CORAL Server!!!");		
							
			    while((msgStr=in.readLine()) != null){
					System.out.println("Received from Northbound (CORAL): "+msgStr);						
					// Format JSON packet
					try{
						JSONParser parser = new JSONParser();
						JSONObject msg = new JSONObject();
						msg = (JSONObject) parser.parse(msgStr);

						JSONObject packet = new JSONObject();
						packet = (JSONObject) msg.get("Msg");
						for(SouthboundAPI sb : ConnectorMain.southbound){ 
							System.out.println("NID="+packet.get("NID")+" SB="+sb.nodeId);
							if(packet.get("NID").equals(sb.nodeId)){
								sb.send(msg); //send full msg with LC
								System.out.println("SSSENDING NID="+packet.get("NID")+" SB="+sb.nodeId);
							}
						}																							
					} catch (ParseException e) {
						System.out.println("JSON Parsing from Northbound Error");
						e.printStackTrace();
					}  
				} // End reading while true
			}
			catch (IOException e) {
				socket = null;
				System.out.println("Disconnected from CORAL server!!!");
			} 
			try {
				Thread.sleep(5000);
			} catch (InterruptedException e) {
				System.out.println("Thread sleep error!!!");
			}
		} // While TRUE	
	}
	
	public static void send(JSONObject msg){
		while(ConnectorMain.sending==false);
		ConnectorMain.sending=false;
		try {			
			System.out.println("Sending Message to Northbound (CORAL): "+msg);
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
