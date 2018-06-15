package Controller;
import java.io.IOException;
import java.io.PrintStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Scanner;

import org.graphstream.graph.Graph;
import org.json.simple.JSONObject;
import org.graphstream.graph.*;

import java.util.*;

public class CoralMain {
	static int nport = 8992; // Northbound Server Port
	static int sport = 8999; // Southbound Server Port

	static Thread ctTherad;
	static Thread nbTherad;
	static Thread dpTherad;
	
	public static Statistics stat = new Statistics();
	
	final static long time = System.currentTimeMillis()/1000;

	public static void main(String args[]){
		try{		
			// Starting Controller
			System.out.println("Coral Controller starting...");
			Controller controller = new Controller();
			ctTherad = new Thread(controller);
			ctTherad.start();
			
			// Northbound Listener Starting 
			System.out.println("NorthBound Coral Listening on port ["+nport+"] starting...");
			NorthboundAPI northbound = new NorthboundAPI(nport, controller);
			nbTherad = new Thread(northbound);
			nbTherad.start();	
			
			// Southbound Listener Starting
			System.out.println("SouthBound Coral Listening on port ["+sport+"] starting...");
			SouthboundAPI southbound = new SouthboundAPI(sport, controller);
			dpTherad = new Thread(southbound);
			dpTherad.start();	

		} // End of Server Socket Try
		catch (IOException e) {
			System.out.println("Coral Controler Socket closed...");
			e.printStackTrace();
		}
   }
}
