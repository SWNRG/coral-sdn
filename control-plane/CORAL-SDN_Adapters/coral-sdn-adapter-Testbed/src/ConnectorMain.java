import java.util.*;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintStream;
import java.net.ServerSocket;
import java.net.Socket;

import com.fazecast.jSerialComm.SerialPort;

public class ConnectorMain {
	//public static int MAX_MOTES = 0;
	public static boolean sending = true;  // Semaphore for sending from southbound serial socket threads
	
	private static final int PORT = 8999;  // Northbound Server Port
	private static String SERVER;
	
	private static final int S_PORT = 9000; // Southbound Server Port
	static ServerSocket ssock = null;
	static ArrayList<SouthboundAPI> southbound = new ArrayList<SouthboundAPI>();
	
	static Thread nbTherad;
	static ArrayList<Thread> sbTherad = new ArrayList<Thread>();

	public static void delay(int ms){
		try {
			Thread.sleep(ms);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

	public static void main(String[] args) {
		if(args.length > 0){
			SERVER = args[0];			
		}else{
			SERVER = "localhost";		
		}
		
		// Northbound Listener Starting 
		// ================================================================
		System.out.println("Connector NorthBound Listening starting...");
		NorthboundAPI northbound = new NorthboundAPI(SERVER,PORT);
		nbTherad = new Thread(northbound);
		nbTherad.start();	

		delay(5000);
	
		// Southbound Listener Starting	
		// ================================================================
		System.out.println("Connector Southbound Listening starting...");	
		System.out.println("Detecting ports...");

		try {
			ssock = new ServerSocket(S_PORT);
	
			while(true){
				try {				
					System.out.println("Waiting for Southbound socket ...");
					Socket csock = null; 
					csock = ssock.accept();
					System.out.println("New southbound socket connected!!!");
					
					SouthboundAPI mote = new SouthboundAPI(csock);
					southbound.add(mote);
					 				 
					Thread thread = new Thread(mote);
					thread.start();
					sbTherad.add(thread);
					
					delay(500);								
				} catch(NullPointerException e) {
					System.out.println("Southbound reading problem: "+e);
				} catch (IOException e) {
					System.out.println("Southbound Socket disconnected");
				}
			} // END southbound while true 	
		} catch (IOException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
		}	
	}
}
