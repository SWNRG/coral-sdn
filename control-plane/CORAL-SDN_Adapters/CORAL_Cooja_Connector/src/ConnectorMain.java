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
		System.out.println("Connector NorthBound Listening starting...");
		NorthboundAPI northbound = new NorthboundAPI(SERVER,PORT);
		nbTherad = new Thread(northbound);
		nbTherad.start();	

		delay(5000);
/*		try {
			Thread.sleep(5000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
*/		
		// Southbound Listener Starting	
		System.out.println("Connector SouthBound Listening starting...");	
		System.out.println("Detecting ports...");
		
		// READING Ports from a text file
		String rfileName = "softLinkCreationScript";
		System.out.println("Reading file: "+rfileName+"...");
		try {
			 FileReader fileReader = new FileReader(rfileName);
			 BufferedReader br =new BufferedReader(fileReader);
			 String line;
			 while((line=br.readLine())!=null){
				 String portname = line.substring(line.lastIndexOf('/')+1,line.length());
				 
				 if(portname.length() > 5 && portname.substring(0,5).equals("rm090")) {
					 System.out.println(portname);
					 
					 SouthboundAPI mote = new SouthboundAPI(portname);
					 southbound.add(mote);
					 				 
					 Thread thread = new Thread(mote);
					 thread.start();
					 sbTherad.add(thread);

					 delay(500);
					 
					 //sbTherad[MAX_MOTES] = new Thread(mote);
					 //sbTherad[MAX_MOTES].start();
					 //MAX_MOTES++;
				 }
			 }
			 br.close();
			 System.out.println("Finish reading ports text file !!!");
		 }
		 catch(IOException ex) {
			 System.out.println("Error reading file: " + rfileName);
		 }	
	}
}
