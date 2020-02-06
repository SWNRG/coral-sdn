# CORAL-SDN Infrastructure plane

The *CORAL-SDN Infrastructure plane* covering the dual network stacks that implement the control and data communication channels, among the neighbor nodes and the Border Router.
The *Infrastructure plane* is composed of the multi-hop WSN motes. These motes are either a border router or regular IoT motes. All motes contain two radio interfaces, operated by the two \textit{CORAL-SDN} network stacks (i.e., the control network and the data network stacks). 
Both of them are implemented using the C programming language, for the Contiki-OS 3.0, and are embedded into the IoT devices' firmware. 
In the context of this work, we had to employ the following facilities to implement mote devices with dual-radio interfaces:

*Contiki fork with dual-radio features: Since the Contiki-OS does not support a dual network stack operation in its standard version, we used as a basis a relevant forked version of it [here](https://github.com/clovervnd/Dual-radio-simulation). Moreover, we had to ameliorate the Contiki core network modules to enable the two network stacks.
*Zolertia RE-Mote devices upgrade: Although the Zolertia RE-Mote devices contain two radio interfaces, in their standard version, they are not designed to operate at the same time. Applying an upgrade suggested by Zoleria [RE-Mote 2.4GHz dual antenna](https://github.com/Zolertia/Resources/wiki/RE-Mote-2.4GHz-dual-antenna), allowed these motes to become capable of using both radio interfaces concurrently.