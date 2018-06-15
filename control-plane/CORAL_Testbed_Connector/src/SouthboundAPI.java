import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.util.HashMap;
import java.util.Scanner;

import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;

import com.fazecast.jSerialComm.SerialPort;

import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;

public class SouthboundAPI implements Runnable {
	String nodeId = "01.00";	
	Socket csock = null; 
	
	SouthboundAPI(Socket csock){	
		this.csock = csock; 	
	}
	
	//Waiting to receive data from Southbound IP ports
	public void run(){ 	
		System.out.println("Opening southbound IP connection");
			
		BufferedReader in;
		String msgStr = new String();	
		try {
			in = new BufferedReader(new InputStreamReader(csock.getInputStream(),"UTF-8"));
			while((msgStr = in.readLine()) != null){			
				try{
					if(msgStr.charAt(0)=='{'){
						System.out.println("Received from (Local controller): "+msgStr);
						try{
							JSONParser parser = new JSONParser();
							JSONObject msg = new JSONObject();
							msg = (JSONObject) parser.parse(msgStr);
							JSONObject packet = new JSONObject();
							packet = (JSONObject) msg.get("Msg");	// Unwrap
							if(packet.get("PTY") != null)
								if(packet.get("PTY").equals("NN")){
									nodeId = (String)packet.get("NID");
								}						
							NorthboundAPI.send(msg); // Sending to Northbound
						} catch (ParseException e) {
							System.out.println("JSON Parsing from Southbound Error");
							e.printStackTrace();
						}  
					}
				}
				catch(Exception e){
					System.out.println("Error reading port");
				}
			}  // End of While
			in.close();
			csock.close();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		System.out.println("Exiting Closing IP connection: ");							
	}
	
	public void send(JSONObject msg){		
		try {
			if (csock != null ){
				System.out.println("Sending Message:"+msg);
				PrintStream write = new PrintStream(csock.getOutputStream());          
				write.println(msg);
				write.flush();	
			}
		} catch (IOException e) {
			e.printStackTrace();
		}  
	}		
}


