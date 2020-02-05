#!/bin/bash

echo "Lifetime query"
while [ 2>1 ]
do
	echo "1"
	cat ./COOJA.testlog | grep Lifetime
done
