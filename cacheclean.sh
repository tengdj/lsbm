#!/bin/bash
for ((i = 1; i < 1000000; i++)); do
   echo "$i round clean"
   echo 3 > /proc/sys/vm/drop_caches
   sleep 1
done

