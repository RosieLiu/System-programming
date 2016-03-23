#! /bin/bash

NUM_PLAYERS=500
NUM_HOPS=500

./ringmaster $NUM_PLAYERS $NUM_HOPS &
# ./ringmaster 1 2 &

sleep 2

for (( i=0; i<$NUM_PLAYERS; i++ ))
do
    ./player $i &
done

wait
