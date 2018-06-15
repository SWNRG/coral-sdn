package Experiment;
import org.graphstream.graph.Graph;
import org.graphstream.graph.Node;
import org.graphstream.graph.implementations.MultiGraph;
import org.json.simple.JSONObject;

import Controller.CoralMain;
import Controller.SouthboundAPI;


public class ExperimentScenario {
	protected static Graph graph = null;
	private String sinkId = "01.00"; // Receiver node

	private int PacketsInterval = 15000; // Data transmission Interval in milliseconds (e.g.15sec)
	private int NumberOfPackets = 100;   // Number of packets per node
	private String PacketPayload = "HELLO"; // Payload message in the packet !!! For bigger message change contiki
	
	public ExperimentScenario(Graph graph, String sinkId){
		this.graph = graph;
		this.sinkId = sinkId;
	}
	
	public void start(){
		System.out.println("Starting Experiment Scenario...");
		
		for(Node node : graph){  // for all nodes
			if(!node.getId().equals(sinkId)){  // If not the sink node
				sendTo(node.getId());				
			}
			delay(5000);	
		}
	}
	
	public void sendTo(String nid){	
		// Create Packet
		JSONObject pkt = new JSONObject();
		//e.g.{"PTY":"DT","NID":"05.00","DID":"01.00","PLD":"HELLO"}
		pkt.put("NID", nid);    // From Node
		pkt.put("PTY", "DT");   // Data packet
		pkt.put("DID", sinkId); // To Node
		pkt.put("PLD", PacketPayload); // !!! For bigger message change contiki
	
		Thread thread = new Thread(){
			@Override 
			public void run(){
				for(int i=0; i<NumberOfPackets; i++){
					CoralMain.stat.setMsgStartTimer(nid);
					CoralMain.stat.incControlSent("td");
					
					SouthboundAPI.send(pkt);
					delay(PacketsInterval);	
				}
			}
		};
		thread.start();
	}
	
	public static void delay(int ms){
		try {
			Thread.sleep(ms);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}
}
