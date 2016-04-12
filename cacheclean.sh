#!/bin/bash

round=1000000
if [ "$#" -gt 0 ]
then
    echo 1 > /proc/sys/vm/drop_caches
else
for ((i = 1; i <= round; i++)); do
   echo "$i round clean"
   echo 1 > /proc/sys/vm/drop_caches
   sleep 1
done

fi

