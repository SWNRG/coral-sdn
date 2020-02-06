#!/bin/bash
make clean	TARGET=z1
make udp-client	TARGET=z1
make udp-client.upload TARGET=z1 MOTES=/dev/ttyUSB1
make login TARGET=z1 MOTES=/dev/ttyUSB1
