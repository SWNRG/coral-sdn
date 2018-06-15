package Controller;
import java.util.ArrayList;
import java.util.HashMap;

import org.json.simple.JSONObject;

public class Statistics {
	private HashMap<String,Integer> control_sent = new HashMap<String,Integer>();  // <td || rt || dt, num>
	private HashMap<String,Integer> control_recv = new HashMap<String,Integer>();
	// Topology discovery START STOP time
	private long td_start_time;  
	private long td_stop_time;   
	// Message Delivery START STOP time
	private HashMap<String,Long> msg_start_time = new HashMap<String,Long>(); //NodeId,Time
	private HashMap<String,Long> msg_stop_time = new HashMap<String,Long>();

	Statistics(){
		this.setControlSent("td", 0);  // td = Topology Discovery control packets
		this.setControlSent("rt", 0);  // rt = Routing control packets
		this.setControlSent("dt", 0);  // dt = Data control packets
		this.setControlRecv("td", 0);
		this.setControlRecv("rt", 0);
		this.setControlRecv("dt", 0);
		reset();
	}
	
	public void reset(){
		clearControlSent("td");
		clearControlRecv("td");
		clearControlSent("rt");
		clearControlRecv("rt");
		clearControlSent("dt");
		clearControlRecv("dt");
		clearTdTime();
		clearMsgTime();
	}
	
	// Topology Discovery Timers
	public void setTdStartTimer(){
		td_start_time = System.currentTimeMillis();
	}
	public void setTdStopTimer(){
		td_stop_time = System.currentTimeMillis();
	}	
	public double getTdTime(){  // return topology discovery time in seconds
		return (td_stop_time-td_start_time)/1000;
	}
	public void clearTdTime(){
		this.td_start_time = 0;
		this.td_stop_time = 0;
	}
	public void sendTdStopTimer(int nodes, int links){
	    // GRAPH 1
		//{"DTP":"CH1","TIME":00,"NODES":00}
		System.out.println("Sending to TD timer graph nodes"+nodes);
		JSONObject pkt = new JSONObject();
		pkt.put("DTP","CH1");
		pkt.put("TIME", td_stop_time);
		pkt.put("NODES", nodes);
		NorthboundAPI.send(pkt);
		
	    // GRAPH 2
		// {"DTP":"CH2","TIME":00,"SENT":4,"RECV":5,"TOTAL":9}
		System.out.println("Sending to TD control packets graph");
		JSONObject pkt2 = new JSONObject();
		pkt2.put("DTP","CH1");
		pkt2.put("TIME", td_stop_time);
		pkt2.put("SENT", control_sent.get("td"));
		pkt2.put("RECV", control_recv.get("td"));		
		pkt2.put("TOTAL", control_sent.get("td")+control_recv.get("td"));
		NorthboundAPI.send(pkt2);
		
		// MESSAGE LOG
		//{"DTP":"LOG", "TIME":1488877483786, "MSG":"MESSAGE1"}
		JSONObject json = new JSONObject();
		json.put("DTP", "LOG");
		json.put("TIME", td_stop_time);
		json.put("MSG", "TD:[Time:"+CoralMain.stat.getTdTime()+"sec] [Sent:"+control_sent.get("td")+"] [Recv:"+control_recv.get("td")+"] [Nodes:"+nodes+"] [Links:"+links+"]");		
		NorthboundAPI.send(json);
	}
	
	// MESSAGE Delay Timers
	public void setMsgStartTimer(String nid){
		msg_start_time.put(nid,System.currentTimeMillis());
	}
	public void setMsgStopTimer(String nid){
		msg_stop_time.put(nid,System.currentTimeMillis());
	}	
	public double getMsgTime(String nid){  // return message delivery time in miliseconds
		if(msg_start_time.containsKey(nid)&& msg_stop_time.containsKey(nid))
			return (msg_stop_time.get(nid)-msg_start_time.get(nid));
		else
			return -1;
	}
	public boolean existsMsgTime(String nid){
		if(msg_start_time.containsKey(nid)&& msg_stop_time.containsKey(nid))
			return true;
		else
			return false;		
	}
	public void clearMsgTime(){
		msg_start_time.clear();
		msg_stop_time.clear();			
	}
	
	// Sent Control Messages Counter
	public int getControlSent(String key) {
		return control_sent.get(key);
	}
	public void setControlSent(String key, int value) {
		this.control_sent.put(key, value);
	}
	public void incControlSent(String key) {
		this.control_sent.put(key, control_sent.get(key)+1);
	}
	public void clearControlSent(String key) {
		this.control_sent.put(key,0);
	}

	// Receive Control Messages Counter
	public int getControlRecv(String key) {
		return control_recv.get(key);
	}
	public void setControlRecv(String key, int value) {
		this.control_recv.put(key, value);
	}
	public void incControlRecv(String key) {
		this.control_recv.put(key, control_recv.get(key)+1);
	}
	public void clearControlRecv(String key) {
		this.control_recv.put(key,0);
	}
}
