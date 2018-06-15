import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.util.Scanner;

import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;

import com.fazecast.jSerialComm.SerialPort;

import java.io.OutputStream;


public class SouthboundAPI implements Runnable {
	SerialPort motePort; 
	String portName;
	int baudRate;
	String nodeId = "01.00";
	String serialdumpPath;

	private Process serialDumpProcess;
	private PrintWriter outputToSerial;
	
	SouthboundAPI(String portName, int baudRate, String serialdumpPath){
		this.portName = portName;
		this.baudRate = baudRate;
		this.serialdumpPath = serialdumpPath;
	}
	//Waiting to receive data from Southbound SERIAL ports
	public void run(){ 	
		System.out.println("Opening port:"+ portName);
		String SERIALDUMP_LINUX = serialdumpPath+"/contiki/tools/sky/serialdump-linux";
	    String fullCommand;
	    fullCommand = SERIALDUMP_LINUX + " " + "-b" + baudRate + " " + portName;
	    System.out.println("Executing:"+fullCommand);
	    try {
	    	String[] cmd = fullCommand.split(" ");
	    	serialDumpProcess = Runtime.getRuntime().exec(cmd);
	    	final BufferedReader input = new BufferedReader
	    			(new InputStreamReader(serialDumpProcess.getInputStream()));
	    	final BufferedReader err = new BufferedReader
	    			(new InputStreamReader(serialDumpProcess.getErrorStream()));
	    	outputToSerial = new PrintWriter(new OutputStreamWriter(serialDumpProcess.getOutputStream()));
			// Send New Node request {"PTY":"NN"}
			JSONObject nnmsg = new JSONObject();
			Thread.sleep(2000);
			nnmsg.put("PTY","NN");
			send(nnmsg);

	        try {
	        	System.out.println("Reading...");
				String msgStr="";
				while ((msgStr = input.readLine()) != null) {
				//	parseIncomingLine(msgStr);
					System.out.println("Message:"+msgStr);
					if(!msgStr.isEmpty()){
					if(msgStr.charAt(0)=='{'){
						System.out.println("Received from serial (CONTIKI): "+msgStr);
						try{
							JSONParser parser = new JSONParser();
							JSONObject msg = new JSONObject();
							msg = (JSONObject) parser.parse(msgStr);
							
							if(msg.get("PTY") != null)
								if(msg.get("PTY").equals("NN")){
									nodeId = (String)msg.get("NID");
								}
							JSONObject sendmsg = new JSONObject();
							sendmsg.put("LC_id", nodeId);
							sendmsg.put("Msg", msg);
							NorthboundAPI.send(sendmsg); // Sending to Northbound
						} catch (ParseException e) {
							System.out.println("JSON Parsing from Southbound Error");
							e.printStackTrace();
						}  
					}	}	        	  
				}
				input.close();
				System.out.println("Serialdump process shut down, exiting");
	        } catch (IOException e) {
				System.err.println("Exception when reading from serialdump");
				e.printStackTrace();
				System.exit(1);
	        }
	    } catch (Exception e) {
	      System.err.println("Exception when executing '" + fullCommand + "'");
	      System.err.println("Exiting local controller");
	      e.printStackTrace();
	      System.exit(1);
	    }				
	}
	
	public void send(JSONObject msg){		
		System.out.println("Sending to serial (CONTIKI): "+msg);
		try{
			PrintWriter outputToSerial = this.outputToSerial;
			String msgstr = msg.toJSONString();
			if(outputToSerial != null){
				outputToSerial.println(msgstr);
				outputToSerial.flush();
			}			
		}
		catch(Exception e){
			System.out.println("Error sending to serial port :"+portName+"!!!");
		}
	}	
	
	/*  public void parseIncomingLine(String line) {
		    if (line == null) {
		      System.err.println("Parsing null line");
		      return;
		    }

		    String[] components = line.split(" ");
		 
		    for (int i=0; i<components.length; i++) {
		        System.out.print(components[i]+" ");
		    }
		    System.out.println();
		 
		   }*/
}


