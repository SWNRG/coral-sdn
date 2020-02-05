#! /bin/bash

make clean
make cc1200-demo
sudo make cc1200-demo.upload
make login
