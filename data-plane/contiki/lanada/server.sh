#!/bin/bash
make clean	TARGET=z1
make udp-server	TARGET=z1
make udp-server.upload TARGET=z1 MOTES=/dev/ttyUSB0
make login TARGET=z1 MOTES=/dev/ttyUSB0
