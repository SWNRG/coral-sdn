package Controller;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.CharBuffer;
import java.util.Iterator;

import org.graphstream.algorithm.Dijkstra;
import org.graphstream.graph.Edge;
import org.graphstream.graph.Graph;
import org.graphstream.graph.Node;
import org.graphstream.graph.implementations.MultiGraph;
import org.graphstream.stream.file.FileSinkDOT;
import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;

import Experiment.ExperimentScenario;



public class Controller implements Runnable {
	private String[] mobileNodesIDs = {"02.00"};	// MOBILE NODES LIST
	
	public static String DeployRouting = "SDN"; // SDN, RPL, BCP
	// General Configuration Options	
	private String sinkId = "01.00";
	private int staticNodes = 0;
	private int mobileNodes = 0;	
	private int sink = 0;
	
	public static boolean start = false;
	
	JSONObject coordinates = new JSONObject();
	
	// Topology Control Configuration Options
	private int tcType = 1; // 0=TC Advertisement based, 1=TC Node based 
	private int tcAck = 1;  // Neighbor discovery types 0=noAck 1=withACK
	private int retDelay = 1;  // Retransmission Delay (sec)
	// Topology Control Maintenance Options
	private int tcInterval=60; // Time interval in seconds
	private int tcCount = 0; // Counter for the JSI
	
	//JSI
	private simpleWekaClassifierExample jsiClassifier = new simpleWekaClassifierExample();
	
	// Routing Configuration Options
	private int routing = 0;  // Routing type 0=NextHopOnly  1=Total path
	private String LQEA = "RSSI_R"; // RSSI_R, RSSI_S, LQI, JSICLASSVAL
	
	// Properties
	private int timestamp = 0;
	final static boolean GUI = false; // true add graph gui info, false for only server mode
	protected final static Graph graph = new MultiGraph("CORAL SDN Network");
	boolean graphmodified = true;  // every time there is a change in the graph become true
	private static Dijkstra dijkstra = new Dijkstra(Dijkstra.Element.EDGE, null, "RSSI_S"); // Dijkstra
	
	Controller(){
      graph.setAutoCreate(true);
      graph.setStrict(false);
	}
	
	public void run(){
		if(GUI) graph.display();
		//while(true){
		//	}
	    readCoordinates();
	}
	
	private void initialize(){
		sinkId = "01.00";
		staticNodes = 0;
		mobileNodes = 0;
		sink=0;	
		//tcCount = 0; // Counter for the JSI		
	}
	
	public static void delay(int ms){
		try {
			Thread.sleep(ms);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

/* TOPOLOGY CONTROL **********************************************************/
	// Northbound API functions
	// ===================================================================================================

	// Configuration Methods
	// =============================================================
	public void setSinkId(String val) {
		this.sinkId = val;
		System.out.println("SinkId"+sinkId); 
	}
	public void setTcType(int val) {
		if(val == 0 || val ==1)
			this.tcType = val; // TC types 0=AdvertisementBased 1=NodeBased
		else
			this.tcType = 1;
		
		if(this.tcType == 0)
			System.out.println("Topology control Advertisement based");  
		else 
			System.out.println("Topology control Node based");							
	}	
	public void setTcAck(int val) {
		if(val == 0 || val ==1)
			this.tcAck = val; // TC Acknowledgement 0=noAck 1=withACK
		else
			this.tcAck = 1;
		
		if(this.tcAck == 0)			
			System.out.println("Topology control whithOUT acknowledgement");  
		else 
			System.out.println("Topology control WITH acknowledgement");			
	}
	public void setRetDelay(int val) {	
		if(val >=0 || val <=10)
			this.retDelay = val;
		else
			this.retDelay = 5;
		System.out.println("Retransmission Delay:" + this.retDelay); 
	}
	public void setRouting(int val) {
		switch(val){
			case 0:
				this.routing = val;
				System.out.println("Routing SET Next Hop Only");
				break;
			case 1:
				this.routing = val;
				System.out.println("Routing SET Total Path");
				break;
			default:
				this.routing = 1;
				System.out.println("Routing SET Default to Total Path");
		}	
	}
	// Link Quality Estimation Algorithm
	public void setLQEA(int val) {
		switch(val){
			case 0: 
				LQEA = "RSSI_S";
				System.out.println("Link Quality Estimation Algorithm = RSSI");  
				break;
			case 1: 
				LQEA = "RSSI_R";
				System.out.println("Link Quality Estimation Algorithm = RSSI&ENERGY");  
				break;
			case 2: 
				LQEA = "LQI";
				System.out.println("Link Quality Estimation Algorithm = LQI");  
				break;
			case 3: 
				LQEA = "JSICLASSVAL";
				System.out.println("Link Quality Estimation Algorithm = JSI Intelligent LQE");  
				break;
			default:
				LQEA = "RSSI_S";
				System.out.println("Link Quality Estimation Algorithm = RSSI");  
		}
		setDijkstra(LQEA);
	}
			
	// Commands 
	// =============================================================
	public void clear(){   // CLEAR all
		clearRoutes();
		initialize();
		CoralMain.stat.reset(); // reset all stats
		graph.clear();
		graphmodified = true;
	}	
	
	public void clearRoutes(){   // CLEAR routes
		for(Node node : graph)
			sendRemoveAllRoute(node.getId());
		CoralMain.stat.clearMsgTime();  //reset only message related stats	
		CoralMain.stat.clearControlSent("rt");
		CoralMain.stat.clearControlRecv("rt");
		CoralMain.stat.clearControlSent("dt");
		CoralMain.stat.clearControlRecv("dt");
	}
	
	public void save(String filename){    // SAVE
		JSONObject json = new JSONObject();

		json.put("Experiment Name","TOP");
		JSONObject gp = new JSONObject();		
		gp.put("Nodes",this.getNodesNum());
		gp.put("Links",this.getEdgesNum());
		gp.put("Sink",sink);
		gp.put("Static Nodes",staticNodes);
		gp.put("Mobile Nodes",mobileNodes);
		json.put("General Paramaters", gp);
		
		JSONObject tc = new JSONObject();
		if(tcType == 1)
			tc.put("TC Type", "Find Neighbors");
		else
			tc.put("TC Type", "Advertise to Neighbors");
		if(tcAck == 1)
			tc.put("Acknowledgement", "Yes");
		else
			tc.put("Acknowledgement", "No");
		tc.put("Re-transmission",retDelay);
		json.put("Topology Control", tc);
		
		JSONObject route = new JSONObject();
		if(routing == 1)
			route.put("Routing Type", "Total Path");
		else
			route.put("Routing Type", "Next Hop Only");
		json.put("Routing", route);		
		
		JSONObject stats = new JSONObject();
		stats.put("Topology Discovery Duration Time", CoralMain.stat.getTdTime());
		stats.put("Topology Discovery Send Control Messages", CoralMain.stat.getControlSent("td"));
		stats.put("Topology Discovery Recv Control Messages", CoralMain.stat.getControlRecv("td"));
		stats.put("Topology Discovery Total Control Messages", CoralMain.stat.getControlSent("td")+CoralMain.stat.getControlRecv("td"));

		stats.put("Routing Send Control Messages", CoralMain.stat.getControlSent("rt"));
		stats.put("Routing Recv Control Messages", CoralMain.stat.getControlRecv("rt"));
		stats.put("Routing Total Control Messages", CoralMain.stat.getControlSent("rt")+CoralMain.stat.getControlRecv("rt"));

		stats.put("Data Send Messages", CoralMain.stat.getControlSent("dt"));
		stats.put("Data Recv Messages", CoralMain.stat.getControlRecv("dt"));
		stats.put("Data Total Messages", CoralMain.stat.getControlSent("dt")+CoralMain.stat.getControlRecv("dt"));
		
		JSONArray nodeslist = new JSONArray();
		for(Node node : graph){
			if(CoralMain.stat.existsMsgTime(node.getId())){
				JSONObject nodeobj = new JSONObject();			
				nodeobj.put("Node Id", node.getId());
				nodeobj.put("First Packet Time", CoralMain.stat.getMsgTime(node.getId()));
				nodeslist.add(nodeobj);
			}
		}
		stats.put("Nodes Stats", nodeslist);
		json.put("Statistics", stats);		
	
		try {		
			FileWriter file = new FileWriter("./data/"+filename+".json");
			file.write(json.toJSONString());
			file.flush();
			file.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}	
	
	public void sendMessage(String nid){
		System.out.println("Inside node neighbor discovery Sending to "+nid);
		JSONObject pkt = new JSONObject();
		pkt.put("NID", nid);
		pkt.put("PTY", "DT");
		pkt.put("DID", sinkId);
		pkt.put("PLD", "HELLO"); // !!! For bigger message change contiki
		CoralMain.stat.setMsgStartTimer(nid);
		CoralMain.stat.incControlSent("td");
		//{"PTY":"DT","NID":"05.00","DID":"01.00","PLD":"HELLO"}
		SouthboundAPI.send(pkt);
		 
		//ExperimentScenario a = new ExperimentScenario(this.graph,this.sinkId);
		//a.start();	
	}
	
	public void recvMessage(JSONObject pkt){
		graphmodified = true;
		String nid = (String)pkt.get("NID");
		String sid = (String)pkt.get("SID");  //Sender ID
		CoralMain.stat.setMsgStopTimer(sid);
		String msg = (String)pkt.get("PLD");
		System.out.println("[Duration:"+CoralMain.stat.getMsgTime(sid)+"msec] Received from "+ sid +" to "+ nid +" message " + msg);

		JSONObject json = new JSONObject();
		json.put("RTA", "SDN");
		json.put("DTP", "LOG");
		json.put("TIME", System.currentTimeMillis());
		json.put("MSG", "[Duration:"+CoralMain.stat.getMsgTime(sid)+"msec] Sink received from node "+sid+" message: "+msg);		
		//{"DTP":"LOG", "TIME":1488877483786, "MSG":"MESSAGE1"}
		NorthboundAPI.send(json);
	}
	
	public void topologyDiscovery(){  // From Sink  START
		CoralMain.stat.setTdStartTimer();
		System.out.println("Topology Discovery Starting...");
		graphmodified = true;
		Node n = graph.addNode(sinkId);
		SouthboundAPI.LC_NID.put(sinkId,sinkId);
		if(GUI) n.addAttribute("ui.label",sinkId);
		sendNodeNeighborDiscovery(sinkId);		
	}
	public void topologyDiscovery(String nid){  // From Node   NODE Shift-CLICK
		CoralMain.stat.setTdStartTimer();
		System.out.println("Topology Discovery for node:"+nid);
		sendNodeNeighborDiscovery(nid);		
	}

	public void topologyDiscoveryRefresh(String nid){  // From Node   NODE Shift-CLICK
		graphRemoveNode(nid);
		graphAddNode(nid);
		sendRemoveAllRoute(nid);
		topologyDiscovery(nid);	
	}

	public void topologyControl(){  // Topology control 
		System.out.println("Starting Topology Control ...");
		tcCount = 1; // Counter for the JSI	
		CoralMain.stat.reset(); // reset all stats
		graph.clear();		    // Clear topology graph			
		topologyDiscovery();    // Start new topology discovery
		clearRoutes();			// Clear routes  I HAVE TO REMOVE THAT
		saveJSIdata(); 
		
		Thread thread = new Thread(){
			@Override 
			public void run(){
				while(true){
					try {   //Delay Trikle Timer
					    Thread.sleep(tcInterval * 1000); //1000 milliseconds is one second.
					}catch(InterruptedException ex){ 
					    Thread.currentThread().interrupt();
					}			
					tcCount = tcCount + 1;
					System.out.println("Topology Control No:"+tcCount+" refresh every:"+tcInterval+"sec");
					// Refresh only mobile nodes
					//	for(Node node : graph)
					//		for(int i=0;i<mobileNodesIDs.length;i++)
					//			if(node.getId().equals(mobileNodesIDs[i]))
					//				topologyDiscoveryRefresh(node.getId());	
					for(int i=0;i<mobileNodesIDs.length;i++)
						topologyDiscoveryRefresh(mobileNodesIDs[i]);		
					
					graphmodified = true;			
						
			//		if(start == false) // close thread
			//			return;			
				}
			}
		};
		thread.start();	
	}
	
	private void graphAddNode(String nid /* more param*/) {
		Node n = graph.addNode(nid);
		//n.addAttribute("ENG",pkt.get("ENG")); 
		n.addAttribute("ENG",0);// ??? Temporary
		if(GUI) n.addAttribute("ui.label",nid); 
		// ... add extra info about the node from the payload eg energy
		graphmodified = true;
	}
	private void graphRemoveNode(String nid){ 
		for(Node node : graph)
			if(node.getId().equals(nid)){
				System.out.println("Remove node:"+nid);
				graph.removeNode(node);
				sendRemoveAllRoute(node.getId());
				graphmodified = true;
			}
	}
	// Southbound API function
	// ===================================================================================================
	public void addNewNode(JSONObject pkt) {
		CoralMain.stat.setTdStopTimer();
		graphmodified = true;
		String nid = (String)pkt.get("NID");
		graphAddNode(nid);
		sendNodeNeighborDiscovery(nid);
		CoralMain.stat.sendTdStopTimer(getNodesNum(),getEdgesNum());  //Send to graph
	}
	private void sendNodeNeighborDiscovery(String nid){
		System.out.println("Inside node neighbor discovery Sending to "+nid);
		JSONObject pkt = new JSONObject();
		pkt.put("NID", nid);
		pkt.put("PTY", "ND");
		pkt.put("ACK", tcAck+"");
		pkt.put("TCT", tcType+"");
		pkt.put("RET", retDelay+"");
		CoralMain.stat.incControlSent("td");
		// {"PTY":"ND","NID":"05.00","TCT":"1","ACK":"1","RET":"4"}
		SouthboundAPI.send(pkt);
	}
	
	public void addNeighbor(JSONObject pkt) {
		String nid = (String)pkt.get("NID");
		String nbid = (String)pkt.get("NBR");
		if(this.tcType == 0){ // If Advertisement TC swap nid and nbid
			String tmp = nid;  
			nid = nbid;
			nbid = tmp;
		}
		// If it is a new neighbor send TC request
		if(graph.getNode(nbid) == null) { // If neighbor is NEW
			System.out.println("Inside IF node neighbor discovery Sending to "+nid);
			sendNodeNeighborDiscovery(nbid);
		}
		// Add Neighbor
		graphmodified = true;
		if(graph.getEdge(nid+"-"+nbid) == null && graph.getEdge(nbid+"-"+nid) == null){				
			if(graph.getNode(nid) != null){
				CoralMain.stat.setTdStopTimer();
				Node n = graph.addNode(nbid);  // Create New Neighbor node 
				if(GUI) n.addAttribute("ui.label",nbid);
				Edge edge = graph.addEdge(nid+"-"+nbid, nid, nbid);
				edge.addAttribute("RSSI_R", pkt.get("RSS"));
				edge.addAttribute("RSSI_S", pkt.get("SSS"));
				edge.addAttribute("LQI", pkt.get("LQI"));
				if(LQEA.equals("JSICLASSVAL")) {
					float rssi = Float.parseFloat(pkt.get("RSS").toString());
					float lqi = Float.parseFloat(pkt.get("LQI").toString());
					String jsiClass = jsiClassifier.getClassification(rssi, lqi);
					edge.addAttribute("JSICLASS", jsiClass);
					edge.addAttribute("JSICLASSVAL", jsiClassVal(jsiClass));
				}
				if(GUI) edge.addAttribute("label", "" + (int) edge.getNumber("RSSI_S")+" "+(int) edge.getNumber("LQI"));
				CoralMain.stat.sendTdStopTimer(this.getNodesNum(), this.getEdgesNum());  //Send to graph
			}
			else{
				System.out.println("Node "+nid+" does not exists in the network");
			}
		}
	}	

/* ***** ROUTING ************************************************************ */
/* ************************************************************************** */
	// Dijkstra Functions
	// ========================================================
	public void setDijkstra(String weightAttr){	
		switch (weightAttr){ // Check if weight Attribute exists
			case "RSSI_S": break;
			case "RSSI_R": break;
			case "LQI": break;
			case "JSICLASSVAL": break;
		default:
			weightAttr = "RSSI_S";	
		}
		System.out.println("Setting Dijkstra weight attribute: "+weightAttr);
		dijkstra = new Dijkstra(Dijkstra.Element.EDGE, null, weightAttr);
	} 
	
	public boolean runDijkstra(final String nid){
		if(graph.getNode(nid) == null){
			System.out.println("Cannot run dikstra node ["+nid+"] does not exists");
			return false;
		}
		dijkstra.init(graph);
		dijkstra.setSource(graph.getNode(nid));
		dijkstra.compute();
		return true;
	}
	
	// Routing Functions
	// ======================================================================
	public void findRoute(JSONObject pkt) {
		String nid = (String)pkt.get("NID");
		String did = (String)pkt.get("DID");
		System.out.println("Running Dijkstra ... ");
		if(runDijkstra(nid)==true){  // run Dijkstra
			//String nexthop = findNextHop(nid,did);
			if(routing == 0)  // Routing type 0=NextHopOnly
				sendNextHopOnly(nid,did); 
			else              // Routing type 1=Total path
				sendNextTotalPath(nid,did);
		}
	}
	
	public void sendNextHopOnly(String nid, String did){
		System.out.println("Sending Next Hop Only to node:"+nid);
		Node n = null;
		for(Node node : dijkstra.getPathNodes(graph.getNode(did))){ // Find the last in the row
			if(!node.getId().equals(nid)) // NODE not equal to NID
				n = node;  // Save to n the previous to nid		
		}
		if(n != null){  // n.getId() the first node in the path
			System.out.println("From Dijkstra result next hop:"+ n.getId());
			if(n.getId() != null){
				// Flow GUI 
				JSONArray flowlinks = new JSONArray();	    
				JSONObject linkobj = new JSONObject();
				linkobj.put("source", nid);
				linkobj.put("target", n.getId());
				flowlinks.add(linkobj);										
				sendGraph(flowlinks);
				
				String cost = findCost(did);
				sendAddRoute("AR",nid,did,n.getId(),cost,"00");
			}		
		}
	}
	public void sendNextTotalPath(String nid, String did){
		System.out.println("Sending Total Path to nodes");
		Node prevNode = null;
		JSONArray flowlinks = new JSONArray();  // Flow GUI
		for(Node node : dijkstra.getPathNodes(graph.getNode(did))){ // Loop the path from did to nid
			if(!node.getId().equals(did) && prevNode != null){ // NODE not equal to DID
				System.out.println("From Dijkstra result next hop:"+ node.getId());
				if(node.getId() != null){
					String cost = findCost(did);
					sendRemoveRoute(node.getId(), did);
					
					// Flow GUI 	    
					JSONObject linkobj = new JSONObject();
					linkobj.put("source", node.getId());
					linkobj.put("target", prevNode.getId());
					flowlinks.add(linkobj);										

					if(!node.getId().equals(nid)) 
						sendAddRoute("AD", node.getId(), did, prevNode.getId(), cost, "00");
					else // For the node nid resends immediately
						sendAddRoute("AR", node.getId(), did, prevNode.getId(), cost, "00");						
					// {"PTY":"AR","NID":"02.00","DID":"01.00","NXH":"01.00","CST":"79","SEQ":"00"}
				}
			}
			prevNode = node;
		} //  End For
		sendGraph(flowlinks);
	}	

/*	public String findNextHop(String nid, String did){
		Node n = null;
		for(Node node : dijkstra.getPathNodes(graph.getNode(did))){
			if(!node.getId().equals(nid)) //if you find
				n = node;			
		}
		if(n != null)
			return n.getId();  //return the first node in the path
		else
			return null;
	}*/
	private String findCost(String did) {
		for (Edge edge : dijkstra.getPathEdges(graph.getNode(did)))
			return edge.getAttribute("RSSI_R").toString();
		return null;
	}
	public void sendAddRoute(String addtype, String nid, String did, String nexthop, String cost, String sequence){	
		JSONObject pkt = new JSONObject();
		pkt.put("NID", nid);  	// Node Addr
		pkt.put("PTY", addtype); 	// Packet Type: AD for new add, AR replay add to route miss  
		pkt.put("DID", did);  	// Destination Addr
		pkt.put("NXH", nexthop); 	// NextHop
		pkt.put("CST", cost);	 	// Cost	
		pkt.put("SEQ", sequence);	// Sequence
		CoralMain.stat.incControlSent("rt");
		// {"PTY":"AR","NID":"02.00","DID":"01.00","NXH":"01.00","CST":"79","SEQ":"00"}
		SouthboundAPI.send(pkt);
	}
	
	public void sendRemoveRoute(String nid, String did){	
		JSONObject pkt = new JSONObject();
		pkt.put("NID", nid);
		pkt.put("PTY", "RM");
		pkt.put("DID", did);
		CoralMain.stat.incControlSent("rt");
		// {"PTY":"RM","NID":"09.00","DID":"03.00"}   
		SouthboundAPI.send(pkt);
	}
	
	public void sendRemoveAllRoute(String nid){	
		JSONObject pkt = new JSONObject();
		pkt.put("NID", nid);
		pkt.put("PTY", "RM");
		pkt.put("DID", "00.00");
		CoralMain.stat.incControlSent("rt");
		// {"PTY":"RM","NID":"09.00","DID":"00.00"}   destination 00.00 all
		SouthboundAPI.send(pkt);
	}
	
	
/* ******* GRAPH management ********************************************** */
/* *********************************************************************** */	
	private String timestamp(){
		if(timestamp>10000)
			timestamp = 0;
		timestamp++;
		return Integer.toString(timestamp);
	}
	
	public int getEdgesNum(){
		int eNum=0;
		for(Edge edge : graph.getEachEdge())
			eNum++;
		return eNum;
	}
	public int getNodesNum(){
		int nNum=0;
		for(Node node : graph)
			nNum++;
		return nNum;
	}
	public void sendGraph(JSONArray flowlinks){
		int nodes=0;
		int links=0;
		sink=0;
		JSONObject obj = new JSONObject();
		// obj.put("timestamp", timestamp() ); Legacy from save to file
		obj.put("RTA","SDN");
		obj.put("DTP","TOP");
		JSONArray nodeslist = new JSONArray();
		// NODES 
		for(Node node : graph){
			JSONObject nodeobj = new JSONObject();			
			nodeobj.put("id", node.getId());
			nodeobj.put("desc", "Node"+node.getId());
			if(node.getId().equals("01.00")){
				nodeobj.put("type", 1);
				sink++;
			}
			else
				nodeobj.put("type", 2);
			// X Y
			JSONArray xy = (JSONArray)coordinates.get("nodes");
			if(!xy.isEmpty()){
		        Iterator i = xy.iterator();
		        while (i.hasNext()) {
		        	JSONObject xynode = (JSONObject)i.next();
		        	if(xynode.get("id").toString().equals(node.getId())){
			        	nodeobj.put("realX",xynode.get("realX").toString());
			        	nodeobj.put("realY",xynode.get("realY").toString());
		        	}
		        	//System.out.println("COOOOOOOOO===="+(String) xynode.get("id")+" "+(String)xynode.get("name"));
		        }			
			}	
			else
				System.out.println("XY is NULL");
			
			nodeobj.put("class", "controler");	
				JSONObject dataobj = new JSONObject();			
				dataobj.put("energy", node.getAttribute("ENG"));
				dataobj.put("other data", 10);     
			nodeobj.put("data", dataobj);		
			nodeslist.add(nodeobj);
			nodes++;
		}
		obj.put("nodes", nodeslist);

		// LINKS
		JSONArray edgeslist = new JSONArray();
		for(Edge edge : graph.getEachEdge()){
			JSONObject edgeobj = new JSONObject(); 
			edgeobj.put("source", edge.getSourceNode().toString());
			edgeobj.put("target", edge.getTargetNode().toString());
				JSONObject dataobj = new JSONObject();			
				dataobj.put("RSSI_R", edge.getAttribute("RSSI_R"));
				dataobj.put("RSSI_S", edge.getAttribute("RSSI_S")); 
				dataobj.put("LQI", edge.getAttribute("LQI"));
				if(LQEA.equals("JSICLASSVAL")){
					dataobj.put("JSICLASS", edge.getAttribute("JSICLASS"));
					dataobj.put("JSICLASSVAL", edge.getAttribute("JSICLASSVAL"));
				}
				edgeobj.put("data", dataobj);		
			edgeslist.add(edgeobj);
			links++;
		}
		obj.put("links", edgeslist);
	
		// FLOWS
	    if(flowlinks != null){
		   JSONArray flowslist = new JSONArray();	

		    JSONObject flowobj = new JSONObject();
			flowobj.put("color", "red");
				/*JSONArray flowlinks = new JSONArray();			    
					JSONObject linkobj = new JSONObject();
					linkobj.put("source", "01.00");
					linkobj.put("target", "02.00");
					flowlinks.add(linkobj);						
				flowslist.add(linkobj);*/
			if(flowlinks!=null)
				flowobj.put("links", flowlinks);
			flowslist.add(flowobj);

			obj.put("flows", flowslist);            	            		  
	    }

		NorthboundAPI.send(obj);  // Sending json to Node-RED		
		
		// NETWORK SETUP Information
		System.out.println("Sending to Northbound initial Network setup information...");
		// {"DTP":"NET","NODES":15,"SINK":1,"STATIC":10,"MOBILE":4}
		JSONObject netJson = new JSONObject();
		netJson.put("RTA",DeployRouting);
		netJson.put("DTP","NET");
		netJson.put("NODES",nodes);
		netJson.put("LINKS",links);
		netJson.put("SINK",sink);
		netJson.put("STATIC",nodes-sink);
		netJson.put("MOBILE",0);
		NorthboundAPI.send(netJson);  // Sending json to Node-RED
	
		/* Legacy from File save
		try {		
			FileWriter file = new FileWriter("./gui/data.json");
			file.write(obj.toJSONString());
			file.flush();
			file.close();

		} catch (IOException e) {
			e.printStackTrace();
		}*/
	}
	
	public void readMap(){
		BufferedReader br = null;
		try {
			br = new BufferedReader(new FileReader("./gui/iMindsMap.json")); 
			String sCurrentLine;
			String fulljson="";

			while ((sCurrentLine = br.readLine()) != null) {
				fulljson = fulljson + sCurrentLine;
			}
			JSONParser parser = new JSONParser();
			JSONObject mapJson = new JSONObject();
			mapJson = (JSONObject) parser.parse(fulljson);
			System.out.println("Sending MAP info to Northbound"); 
			NorthboundAPI.send(mapJson);
			br.close();
		} catch (IOException e) {
			System.out.println("ERROR reading IMinds MAP!!!");
			e.printStackTrace();
		} catch (ParseException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} 

	}
	
	public void readCoordinates(){
		BufferedReader br = null;
		try {
			System.out.println("Reading Node Coordinates...");
			br = new BufferedReader(new FileReader("./gui/coordinates.json"));
			String sCurrentLine;
			String fulljson="";
			while ((sCurrentLine = br.readLine()) != null) {
				fulljson = fulljson + sCurrentLine;
			}
			JSONParser parser = new JSONParser();
			coordinates = (JSONObject) parser.parse(fulljson);
			br.close();
		} catch (IOException e) {
			System.out.println("ERROR REading Coordinates...");
			e.printStackTrace();
		} catch (ParseException e) {
			System.out.println("Parsing Coordinates file exeception");
			e.printStackTrace();
		} 
	}

	// JSI Functions
	// ===============================================================
	void saveJSIdata(){
		try {		
			for (Edge edge : graph.getEachEdge()){
				FileWriter file = new FileWriter("./jsidata/"+edge.getId(),true);
				file.write(tcCount+","+edge.getAttribute("RSSI_R").toString()+","+edge.getAttribute("LQI").toString());
				file.write(System.lineSeparator());
				file.flush();
				file.close();
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	
	private int jsiClassVal(String classTag){
		switch (classTag){		
			case "perfect": return 1; 
			case "good": return 2; 
			case "avg": return 3;
			case "bad": return 4;
			case "terrible": return 5;
		default: 
			System.out.println("Classificatio ["+classTag+"] does not exits return value 100");
			return 100;
		}
	}
	
}
