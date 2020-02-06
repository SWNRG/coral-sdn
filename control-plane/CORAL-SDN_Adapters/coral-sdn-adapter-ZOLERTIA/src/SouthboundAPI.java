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
	int baudRate;
	String nodeId = null;
//	OutputStream a;
	
	SouthboundAPI(String portName, int baudRate){
		this.portName = portName;
		this.baudRate = baudRate;
	}
	//Waiting to receive data from Southbound SERIAL ports
	public void run(){ 
		System.out.println("Opening port:"+ portName);
		motePort = SerialPort.getCommPort(portName);
		motePort.closePort();
		motePort.setBaudRate(baudRate);
		motePort.setNumDataBits(8);
		motePort.setParity(SerialPort.NO_PARITY);
		motePort.setNumStopBits(SerialPort.ONE_STOP_BIT);
		motePort.setComPortTimeouts(SerialPort.TIMEOUT_SCANNER, 0, 0);
		//motePort.setFlowControl(SerialPort.FLOW_CONTROL_RTS_ENABLED|SerialPort.FLOW_CONTROL_CTS_ENABLED);
		
		if(motePort.openPort()==true){
			//motePort.setFlowControl(SerialPort.FLOW_CONTROL_DTR_ENABLED);
//			a = motePort.getOutputStream();			
			System.out.println("Bound Rate:"+ motePort.getBaudRate());
			System.out.println("Databits:"+ motePort.getNumDataBits());
			System.out.println("Parity:"+ motePort.getParity());
			System.out.println("StopBits:"+ motePort.getNumStopBits());
			System.out.println("Write Timeout:"+ motePort.getWriteTimeout());
			System.out.println("FlowControl:"+ motePort.getFlowControlSettings());
			// Send New Node request {"PTY":"NN"}
			JSONObject nnmsg = new JSONObject();
			nnmsg.put("PTY","NN");
			send(nnmsg);
			Scanner scanner = new Scanner(motePort.getInputStream());
		
			while(scanner.hasNextLine()){
				try{
					String msgStr = scanner.nextLine();	
					if(msgStr.charAt(0)=='{'){
						System.out.println("Received from serial (CONTIKI): "+msgStr);
						try{
							JSONParser parser = new JSONParser();
							JSONObject msg = new JSONObject();
							msg = (JSONObject) parser.parse(msgStr);	
							
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
					}	
				}
				catch(Exception e){
					System.out.println("Error reading port");
				}
			}  // End of While  */
			scanner.close();
			motePort.closePort();
			System.out.println("Exiting Closing port: " + portName);
			System.out.println("ExitWhile");
		}
		else{
			System.out.println("Cannot open port:" + portName);
		}								
	}
	
	public void send(JSONObject msg){		
		System.out.println("Sending to serial (CONTIKI): "+msg);
		try{
			OutputStream a = motePort.getOutputStream();			
			
			String msgstr = msg.toJSONString()+"\n";
			a.write(msgstr.getBytes());
			a.flush();
		}
		catch(Exception e){
			System.out.println("Error sending to serial port :"+portName+"!!!");
		}
	}		
}


