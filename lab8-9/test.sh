#!/bin/bash

for i in 2 5 10 20 
do
    for j in 10 100 500 1000
    do
        ./lab8 $j s > tmp.log &
        sleep $i
        echo "Killing $j threads for $i seconds"
        kill -2 $!
        sleep 5
        cat tmp.log
    done
done
rm tmp.log