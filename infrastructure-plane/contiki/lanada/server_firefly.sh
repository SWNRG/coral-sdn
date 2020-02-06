#!/bin/bash
make clean TARGET=zoul
make udp-server TARGET=zoul 
make udp-server.upload TARGET=zoul PORT=/dev/ttyUSB0
make login TARGET=zoul PORT=/dev/ttyUSB0
