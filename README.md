CORAL-SDN git is under major update with the latest version (expected the controller to be upload on 8th of Feb 2020) stay tuned

# CORAL-SDN: A Software-Defined Networking Solution with Out-of-Band Control for the Internet of Things

![CORAL-SDN Architecture](/CORAL-SDN-Architecture.png)

* CORAL-SDN protocol video [here]( https://youtu.be/AaVqCTXVyQk)
* CORAL-SDN Out-of-Band Control Panorama video [here](https://youtu.be/nGNGpMxJjdE)

CORAL-SDN
The Internet of Things (IoT) is gradually incorporating multiple environmental, people, or industrial monitoring deployments with diverse communication and application requirements. 
The main routing protocols used in the IoT, such as the IPv6 Routing Protocol for Low-Power and Lossy Networks (RPL), are focusing on the many-to-one communication of resource-constraint devices over wireless multi-hop topologies, i.e., due to their legacy of the Wireless Sensor Networks (WSN).
The Software-Defined Networking (SDN) paradigm appeared as a promising approach to implement alternative routing control strategies, enriching the set of IoT applications that can be delivered, by enabling global protocol strategies and programmability of the network environment. However, SDN can be associated with significant network control overhead. 
In this paper, we propose CORAL-SDN, an open-source SDN solution for the IoT, bringing the following novelties in contrast to the related works: (i) programmable routing control with reduced control overhead through inherent protocol support of a long-range control channel; and (ii) a modular SDN controller and an OpenFlow-like protocol improving the quality of communication in a wide range of IoT scenarios through supporting two alternative topology discovery and two flow establishment mechanisms.
We carried out experiments with various topologies, network sizes and high-volume transmissions with alternative communication patterns. Our results verified the robust performance and reduced control overhead of CORAL-SDN for alternative IoT deployments, e.g., achieved up to 47% reduction in network's overall end-to-end delay time compared to the RPL and a packet delivery ratio of over 99.5%.
