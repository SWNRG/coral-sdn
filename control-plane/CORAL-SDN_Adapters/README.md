# CORAL-SDN Adapter
The CORAL-SDN Adapter is responsible for communicating control messages between the CORAL-SDN-Controller and the Border Router (BR). 
Physically it is located close to the latter and enables the former to be off-site. 
Depending on the data-plane environment (i.e., the Border Router type) we provide different adapters. 

**coral-sdn-adapter-COOJA**: connects the CORAL-SDN Controller with the COOJA simulator pty2serial interface. The runtime environment with a python script detects available PTY and assigns automatically for deployment simplicity.
**coral-sdn-adapter-RM090**: for RM090 motes 
**coral-sdn-adapter-ZOLERTIA**: for ZOLERTIA motes