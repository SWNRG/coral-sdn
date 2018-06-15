import java.util.*;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintStream;
import java.net.ServerSocket;
import java.net.Socket;

import com.fazecast.jSerialComm.SerialPort;

public class AgentMain {
	//public static int MAX_MOTES = 0;
	public static boolean sending = true;  // Semaphore for sending from southbound serial socket threads
	
	private static int PORT;  // Northbound Server Port
	private static String SERVER;
	private static int baudRate;
	
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
			PORT = Integer.parseInt(args[1]);
			baudRate = Integer.parseInt(args[3]);
		}else{
			SERVER = "localhost";
			PORT = 9000;
			baudRate = 115200;
		}
		
		// Northbound Listener Starting 
		System.out.println("Connector NorthBound Listening on SERVER:"+SERVER+" PORT:"+PORT+" starting...");
		NorthboundAPI northbound = new NorthboundAPI(SERVER,PORT);
		nbTherad = new Thread(northbound);
		nbTherad.start();	

		delay(2000);
/*		try {
			Thread.sleep(5000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
*/		
		// Southbound Listener Starting	
		System.out.println("Connector SouthBound Listening starting...");	
		System.out.println("Detecting ports...");
		
		if(args.length > 2){
			System.out.println("Opening port:"+args[2]);
			SouthboundAPI mote = new SouthboundAPI(args[2], baudRate);
			southbound.add(mote);
			 				 
			Thread thread = new Thread(mote);
			thread.start();
			sbTherad.add(thread);
		}
		else{
			// READING Ports from a text file
			String rfileName = "softLinkCreationScript";
			System.out.println("Reading file: "+rfileName+"...");
			try {
				 FileReader fileReader = new FileReader(rfileName);
				 BufferedReader br =new BufferedReader(fileReader);
				 String line;
				 while((line=br.readLine())!=null){
					 String portname = line.substring(line.lastIndexOf('/')+1,line.length());
					 
					 if(portname.length() > 4){ // && portname.substring(0,5).equals("ttyUS")) {
						 System.out.println(portname);
						 
						 SouthboundAPI mote = new SouthboundAPI(portname, baudRate);
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
}
