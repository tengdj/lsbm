#!/bin/sh

usage()
{
	echo "Usage: `echo $0| awk -F/ '{print $NF}'`  [-option]"
	echo "[option]:"
	echo "  -f  "
	echo "          standard way to clean up the cache"
	echo "  -b  "
	echo "          standard way to clean up the cache and wait"
	echo "  -g  hosts_file"
	echo "          clean up cache for all the hosts in the given file"
	echo
	echo "  e.g cache-cleanup.sh -s 30 -p /mnt/shm"
	echo
	echo "Copyright by Siyuan Ma  2011-12."
	echo
}

if [ $# -lt 1 ]
then
	usage
	exit
fi

BLOCKED_FLUSHING='Cached1=`grep MemTotal /proc/meminfo| awk "{print \\$2/1024}"`; 
		while true; do 
			Cached2=`grep -e "^Cached" /proc/meminfo| awk "{print \\$2/1024}"`; 
			DONE=`echo "$Cached1 - $Cached2 < 20"|bc -q`; 
			Cached1=$Cached2; 
			if [ "$DONE" -ne "1" ]; 
				then sleep 1; 
			else 
				break; 
			fi 
		done'

while getopts "fbg:w:" OPTION
do
	case $OPTION in
		w)
			sleep $OPTARG;
			;;
		f)
			echo "Fast Flushing..."
			echo "echo 3 > /proc/sys/vm/drop_caches"|sudo su;
			exit
			;;
		b)
			echo "Blocked Flushing..."
			echo "echo 3 > /proc/sys/vm/drop_caches"|sudo su;
			eval $BLOCKED_FLUSHING;
			exit
			;;
		g)
			HOSTFILE=$OPTARG;
			HOSTS="$(cat $HOSTFILE)"
			for host in $HOSTS; do
				ssh -t -i ~/.ssh/id_rsa $host 'echo "echo 3 > /proc/sys/vm/drop_caches"|sudo su' >/dev/null 2>&1 &
			done
			wait;
			for host in $HOSTS; do
				ssh $host eval "$BLOCKED_FLUSHING" &
			done
			wait;
			exit
			;;
		?)
			echo "unknown arguments"
			usage
			exit
			;;
	esac
done


