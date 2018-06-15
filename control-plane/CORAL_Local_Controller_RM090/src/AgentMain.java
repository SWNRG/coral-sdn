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
	private static String SERIALNAME;
	private static int baudRate;
	private static String serialdumpPath;
	
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
		if(args.length > 4){
			serialdumpPath = args[4];			
		}else{
			serialdumpPath = "/users/tryfon";			
		}
		if(args.length > 3){
			baudRate = Integer.parseInt(args[3]);
		}else{
			baudRate = 115200;			
		}		
		if(args.length > 2){
			SERIALNAME = args[2];
		}else{
			SERIALNAME = "/dev/rm090";			
		}
		if(args.length > 1){
			PORT = Integer.parseInt(args[1]);
		}else{
			PORT = 9000;
		}
		if(args.length > 0){
			SERVER = args[0];
		}else{
			SERVER = "localhost";
		}
			
		// Northbound Listener Starting 
		System.out.println("Connector NorthBound Listening on SERVER:"+SERVER+" PORT:"+PORT+" starting...");
		NorthboundAPI northbound = new NorthboundAPI(SERVER,PORT);
		nbTherad = new Thread(northbound);
		nbTherad.start();	

		delay(2000);
	
		// Southbound Listener Starting	
		System.out.println("Connector SouthBound Listening starting...");	
		System.out.println("Opening port:"+SERIALNAME+" with baudrate:"+baudRate);
		SouthboundAPI mote = new SouthboundAPI(SERIALNAME, baudRate, serialdumpPath);
		southbound.add(mote);			 				 
		Thread thread = new Thread(mote);
		thread.start();
		sbTherad.add(thread);
	}
}
