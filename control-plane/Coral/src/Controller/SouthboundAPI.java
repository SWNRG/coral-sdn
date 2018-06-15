package Controller;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.io.Reader;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketAddress;
import java.net.SocketException;
import java.util.HashMap;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;


public class SouthboundAPI implements Runnable {
	int port; 
	static ServerSocket ssock = null;
	static Socket csock = null; 
	static HashMap<String,String> LC_NID=new HashMap<String,String>();  
	Controller controller;
	//static int UPI = 0; // 0 = SDN, 1 = UPI
	
	SouthboundAPI(int port, Controller controller) throws IOException {
		try {
			ssock = new ServerSocket(port);
			this.port = port;
			this.controller = controller;
		}
		catch (IOException e) {
			csock = null;
			System.out.println("Error: Cannot create Socket Server on Southbound port no:" + port);
			System.out.println("Port is busy ... Shuting down Server...");
			System.exit(0);
		} 
	} 

	//Waiting to receive data
	public void run(){ 
		while(true){
			BufferedReader in;
			String msgStr = new String();
			try {
				if(csock == null){
					System.out.println("Waiting for Southbound socket ...");
					csock = ssock.accept();
					System.out.println("Southbound socket (Wishful Global Controler) connected!!!");
				}			 
				else{
					System.out.println("Recovering from null pointer exception!!!");
				}
				in = new BufferedReader(new InputStreamReader(csock.getInputStream(),"UTF-8"));
					
			    while((msgStr = in.readLine()) != null){
					System.out.println("Message="+msgStr);	
					// ============= RPL ===========================================
					if(controller.DeployRouting.equals("RPL") && msgStr.length()>3){
				    	System.out.println("Received from wishful "+msgStr);
						// JSON packet {0: {'IEEE802154_RPL_Imin': 9}}
						JSONObject msg = new JSONObject();
						msg.put("RTA","RPL");
						msg.put("NID", msgStr.substring(1, msgStr.indexOf(':')));
						long curtime = System.currentTimeMillis()/1000 - CoralMain.time;
						msg.put("TIME", ""+curtime);
						msg.put("CMD", msgStr.substring(msgStr.indexOf('\'')+1,msgStr.lastIndexOf('\'')));
						msg.put("VAL", msgStr.substring(msgStr.lastIndexOf(':')+1, 
								msgStr.length()-2));
						
						NorthboundAPI.send(msg);
					} //end if(UPI==1)
					
					// ============= BCP ===========================================
					if(controller.DeployRouting.equals("BCP") && msgStr.length()>3){
				    	System.out.println("Received from wishful (BCP):"+msgStr);
						// JSON packet {"RTA":"BCP","DTP":"CHA","CMD":"PACKETSQUEUES","NID":1,"VAL": 0}
						try{
					    	JSONObject msg = new JSONObject();						
							JSONParser parser = new JSONParser();
							msg = (JSONObject) parser.parse(msgStr);
							
							JSONObject packet = new JSONObject();
							packet = (JSONObject) msg.get("Msg");	// Unwrap									

							NorthboundAPI.send(packet);						
						} catch (ParseException e) {
							System.out.println("JSON Parsing from Wishful Error");
							e.printStackTrace();
						}					
					/*	msg.put("RTA","RPL");
						msg.put("NID", msgStr.substring(1, msgStr.indexOf(':')));
						long curtime = System.currentTimeMillis()/1000 - CoralMain.time;
						msg.put("TIME", ""+curtime);
						msg.put("CMD", msgStr.substring(msgStr.indexOf('\'')+1,msgStr.lastIndexOf('\'')));
						msg.put("VAL", msgStr.substring(msgStr.lastIndexOf(':')+1, 
								msgStr.length()-2)); */					
					}
					
					// ============= SDN ===========================================
					if(controller.DeployRouting.equals("SDN")){
						
						msgStr = msgStr.replace('\'','"');
						msgStr = msgStr.replace(" ","");
						msgStr = msgStr.replace(":\"{", ":{");
						msgStr = msgStr.replace("}\"", "}");
						System.out.println("Received from wishful "+msgStr);
					
						// Format JSON packet
						try{
							JSONParser parser = new JSONParser();
							JSONObject msg = new JSONObject();
							msg = (JSONObject) parser.parse(msgStr);						

							System.out.println("Received form LC: "+msg.get("LC_id"));							
							if(msg.get("LC_id")==null){
								System.out.println("ERROR: Message not accepted from NULL LC");								
								continue;
							}
							
							JSONObject packet = new JSONObject();
							packet = (JSONObject) msg.get("Msg");	// Unwrap									
							System.out.println("Node ID: "+packet.get("NID") +" PacketType: "+packet.get("PTY"));
							System.out.println("Packet:"+packet);
						
							switch((String)packet.get("PTY")){
								case "NN": // New Node {"PTY":"NN","NID":"04.00"}
									// CoralMain.stat.incControlRecv("td"); not needed in the new type of discovery
									LC_NID.put((String)packet.get("NID"),
											(String)msg.get("LC_id"));
									System.out.println("Insert to LC array:"
											+(String)packet.get("NID")+" - "
											+(String)msg.get("LC_id"));
									// newNodeHandler(packet); comment out because of New type of discovery
									break;
								case "NB": // New Neighbor {"PTY":"NB","NID":"05.00","NBR":"02.00","RSS":83,"SSS":83,"LQI":219,"ENG":0}
									CoralMain.stat.incControlRecv("td");
									newNeighborHandler(packet);
									break;
								case "MR": // Miss Route {"PTY":"MR","NID":"03.00","DID":"01.00"}
									CoralMain.stat.incControlRecv("rt");
									tableMissHandler(packet);
									break;
								case "DT": // Data Received
									CoralMain.stat.incControlRecv("dt");
									controller.sendGraph(new JSONArray()); //Empty JSON erase flow
									recvMessageHandler(packet);
									break;
								default:
									System.out.println("Unknown Packet Type: "
											+packet.get("PTY"));
							} // End Switch
						} catch (ParseException e) {
							System.out.println("JSON Parsing from Wishful Error");
							e.printStackTrace();
						}
						System.out.println();
						controller.sendGraph(null);
					} // END if UPI = 0
				} // END while reading socket
			} catch(NullPointerException e) {
				System.out.println("wishful reading problem: "+e);
			} catch (IOException e) {
				csock = null;
				System.out.println("Southbound Socket (Wishful) disconnected");
			}
		} // END global while true 	
	}
	
	public static void send(JSONObject packet){
		if(Controller.DeployRouting.equals("RPL")){ //RPL
			try {			
				if (csock != null){	
					System.out.println("Sending Message to southbound:"+packet);
					PrintStream write = new PrintStream(csock.getOutputStream());          
					write.println(packet);
					//write.flush();				
				}
			} catch (IOException e) {
				e.printStackTrace();
			} 		
		}		
		if(Controller.DeployRouting.equals("SDN")){  //SDN
			try {
				String LC = LC_NID.get(packet.get("NID"));
				JSONObject msg = new JSONObject();
				msg.put("LC_id", LC);  // I had 0 here
				msg.put("Msg", packet);
				if (csock != null && LC != null){
					System.out.println("Sending Message:"+msg);
					PrintStream write = new PrintStream(csock.getOutputStream());          
					write.println(msg+"\n");
					write.flush();	
				}
			} catch (IOException e) {
				e.printStackTrace();
			}  
		}
	}
	
	// Handlers
	// ==============================================
	private void newNodeHandler(JSONObject packet) {
		System.out.println("Responding to New Node arrival...");	
		controller.addNewNode(packet);
	}
	
	private void newNeighborHandler(JSONObject packet) {
		System.out.println("Responding to New Neighbor arrival...");
		controller.addNeighbor(packet);
	}
	
	private void tableMissHandler(JSONObject packet) {
		System.out.println("Responding to Table Miss request...");
	    controller.findRoute(packet);
	}
	
	private void recvMessageHandler(JSONObject packet){
		System.out.println("Data packet arrived...");
		controller.recvMessage(packet);
	}

}

/*  HashMap<Integer,String> hm=new HashMap<Integer,String>();  
  
  hm.put(100,"Amit");  
  hm.put(101,"Vijay");  
  hm.put(102,"Rahul");  
  
  for(Map.Entry m:hm.entrySet()){  
   System.out.println(m.getKey()+" "+m.getValue()); 
*/