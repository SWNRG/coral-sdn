import java.io.BufferedReader;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Scanner;

import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;

import com.fazecast.jSerialComm.SerialPort;

import java.io.OutputStream;


public class SouthboundAPI implements Runnable {
	SerialPort motePort; 
	String portName;
	String nodeId = "01.00";
//	OutputStream a;
	
	SouthboundAPI(String portName){
		this.portName = portName;
	}
	//Waiting to receive data from Southbound SERIAL ports
	public void run(){ 	
		System.out.println("Opening port:"+ portName);
		motePort = SerialPort.getCommPort(portName);
		motePort.closePort();
		motePort.setBaudRate(115200);
		//motePort.setParity(1);
		motePort.setComPortTimeouts(SerialPort.TIMEOUT_SCANNER, 0, 0);

		if(motePort.openPort()==true){
//			a = motePort.getOutputStream();			
			System.out.print("Bound Rate:"+ motePort.getBaudRate());
			System.out.print(" Parity:"+ motePort.getParity());
			System.out.println(" Write Timeout:"+ motePort.getWriteTimeout());
			
			// Send New Node request {"PTY":"NN"}
			JSONObject nnmsg = new JSONObject();
			nnmsg.put("PTY","BR"); // Detect border router
			send(nnmsg);
			Scanner scanner = new Scanner(motePort.getInputStream());
			while(scanner.hasNextLine() ){
				try{
					String msgStr = scanner.nextLine();
					if(msgStr.charAt(0)=='{'){
						System.out.println("Received from serial (CONTIKI): "+msgStr);
						try{
							JSONParser parser = new JSONParser();
							JSONObject msg = new JSONObject();
							msg = (JSONObject) parser.parse(msgStr);	
							if(msg.get("PTY") != null)
								if(msg.get("PTY").equals("BR")){
									nodeId = (String)msg.get("BID");
								}
							JSONObject sendmsg = new JSONObject();
							sendmsg.put("LC_id", nodeId);
							sendmsg.put("Msg", msg);
							NorthboundAPI.send(sendmsg); // Sending to Northbound
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
	/*		try {
				a.close();  // close output stream
			} catch (IOException e) {
				System.out.println("Error: Could not close Output stream to southbound");
			} */
			scanner.close();
			motePort.closePort();
			System.out.println("Exiting Closing port: " + portName);
		}
		else{
			System.out.println("Cannot open port:" + portName);
		}								
	}
	
	public void send(JSONObject msg){		
		System.out.println("["+System.currentTimeMillis()+"] Sending to serial (CONTIKI): "+msg);
		try{
			OutputStream a = motePort.getOutputStream();			
			
//todel			String msgstr = msg.toJSONString()+"\n";
			String msgstr = msg.toJSONString();
			a.write(msgstr.getBytes());
			a.flush();
		/* todel	int z1=0,z2=0;
			int x = 10;
			for(z1=0;z1<500;z1++)
			for(z2=0;z2<1000;z2++) x = x/10;*/
		}
		catch(Exception e){
			System.out.println("Error sending to serial port :"+portName+"!!!");
		}
	}		
}


