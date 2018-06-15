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

import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;


public class NorthboundAPI implements Runnable {
	int port; 
	static ServerSocket ssock = null;
	static Socket csock = null; 
	static HashMap<String,String> LC_NID=new HashMap<String,String>();  
	Controller controller;
	
	NorthboundAPI(int port, Controller controller) throws IOException {
		try {
			ssock = new ServerSocket(port);
			this.port = port;
			this.controller = controller;
		}
		catch (IOException e) {
			csock = null;
			System.out.println("Error: Cannot create Socket Server on Northbound port no:" + port);
			System.out.println("Port is busy ... Shuting down Server...");
			System.exit(0);
		} 
	} 

	//Waiting to receive data from Northbound 
	public void run(){ 
		while(true){			
			BufferedReader in;
			String msgStr = new String();
			try {
				System.out.println("Waiting for Northbound socket...");
				csock = ssock.accept();
				System.out.println("Northbound socket connected!!!");

				in = new BufferedReader(
						new InputStreamReader(csock.getInputStream(),"UTF-8"));
				
			    while((msgStr=in.readLine()) != null){
					System.out.println("Received from Northbound: "+msgStr);
					// Format JSON packet
					try{
						// Receiving: {"CMD":"???","NID":123,"TYP":"???","VAL":123} 
						JSONParser parser = new JSONParser();
						JSONObject msg = new JSONObject();
						msg = (JSONObject) parser.parse(msgStr);
						
						switch((String)msg.get("CMD")){
						case "Start": // Button
							// {"CMD":"Start","SINK":"????","TCT":0,"ACK":0,"RET":1}
							controller.setSinkId((String)msg.get("SINK"));
							controller.setTcType(((Long)msg.get("TCT")).intValue());
							controller.setTcAck(((Long)msg.get("ACK")).intValue());
							controller.setRetDelay(((Long)msg.get("RET")).intValue());
							controller.setRouting(((Long)msg.get("ROUT")).intValue());
							controller.start = true;
							controller.topologyControl();
							controller.readMap();
							controller.readCoordinates();
							//controller.topologyDiscovery();
							break;
						case "Update": // Button
							// {"CMD":"Update","SINK":"????","TCT":0,"ACK":0,"RET":1} 
							controller.setSinkId((String)msg.get("SINK"));
							controller.setTcType(((Long)msg.get("TCT")).intValue());
							controller.setTcAck(((Long)msg.get("ACK")).intValue());
							controller.setRetDelay(((Long)msg.get("RET")).intValue());
							controller.setRouting(((Long)msg.get("ROUT")).intValue());
							controller.setLQEA(((Long)msg.get("LQEA")).intValue());
							break;
						case "Topology": // (Node-RED Shift-Click)
							// {"CMD":"Topology","NID":"????"}   
							controller.topologyDiscovery((String)msg.get("NID"));
							break;	
						case "Message":  // (Node-RED Ctrl-Click)
							// {"CMD":"Message","NID":"????"} 
							controller.sendMessage((String)msg.get("NID"));
							break;	
						case "Save":     // Button  SAVE
							// {"CMD":"Save","FILE":"filename} 
							controller.save((String)msg.get("FILE"));
							break;
						case "Clear":    // Button STOP/CLEAR
							// {"CMD":"Clear"} 
							controller.start = false;
							controller.clear();
							break;
						case "ClearRoutes": // Button  CLEAR ROUTES
							// {"CMD":"ClearRoutes"} 
							controller.clearRoutes();
							break;
						case "DeployRouting": // Button  CLEAR ROUTES							
							// {"CMD":"DeployRouting","VAL":"SDN"}
							controller.DeployRouting = (String)msg.get("VAL");
							System.out.println("Deploy routing type = "+controller.DeployRouting);
							break;
						default:
							System.out.println("UPI");
							JSONObject sendmsg = new JSONObject();
							JSONObject upi = new JSONObject();
							
							// Sending UPI = {0: {'IEEE802154_RPL_Imin': 9}}
							upi.put(msg.get("CMD"), msg.get("VAL"));
							sendmsg.put("NID", msg.get("NID")); 
							sendmsg.put("TYP","set_parameters");  
							sendmsg.put("CMD", upi); 
							//sendmsg.put(msg.get("NID"), upi);
							SouthboundAPI.send(sendmsg);
						}									
					} catch (ParseException e) {
						System.out.println("JSON Parsing from Northbound Error");
						e.printStackTrace();
					}  
					controller.sendGraph(null);
				} // End while true
			}
			catch (IOException e) {
				csock = null;
				System.out.println("Northbound socket disconnected");
			} 
		}	
	}
	
	public static void send(JSONObject msg){
		try {			
			System.out.println("Sending Message to northbound:"+msg);
			if (csock != null){	
				PrintStream write = new PrintStream(csock.getOutputStream());          
				write.println(msg);
				write.flush();				
			}
		} catch (IOException e) {
			e.printStackTrace();
		}  
	}
}
	