#!/bin/bash

ANALYZER=`dirname $0`/io-analysis.sh;
TESTER=`dirname $0`/test.sh;
CLEANER=`dirname $0`/cache-cleanup.sh;

run() {

	local TDIR=$1;
	local RDIR=$2;
	local TAG=$3;
	local CONF=$4;
	local DEVICE=$5;
	local INUM=$6;

	echo "run $@";
	TRACE=$TDIR/$TAG;
	$CLEANER -b;
	sleep 2;
	RES=$RDIR/$TAG;
	mkdir -p $RES;
        
        echo "start running db_bench"

	$ANALYZER -d "$DEVICE" -o "$TRACE" -p "$TESTER $CONF $RES $INUM";


#	cd $TRACE && cp $TRACE/*.png $RES;
	cd $TRACE && cp $TRACE/*.out $RES;
 	cp -f $STORE/LOG $RES;
        cp /tmp/hlsm_log $RES;

	#sleep 10; # skip preloading
	for DEV in $DEVICE; do
		blkparse -i $TRACE/$DEV| tail -100 > $RES/${DEV}.out
	done

}

mk_store() {
PROOT=$(dirname $STORE); #primary store
rm -rf $STORE;
echo "start copy data.... from /mnt/sde/${BASE} to $STORE"
cp -rf /mnt/sde/${BASE} $STORE &
wait;
echo "successfully copy data from base to target"
}

write_test() {
local TAG=$1;
local CONF=$2;
source $CONF;

PROOT=$(dirname $STORE); #primary store

mkdir -p $STORE;
run $TDIR $RDIR $TAG "$CONF" "$DEVS" 1 &
wait;

# save the data for future reuse
sleep 2;
mv $STORE $PROOT/${BASE};
}

run_test() {
local TAG=$1;
local CONF=$2;

source $CONF;
#mk_store;
run $TDIR $RDIR $TAG "$CONF" "$DEVS" 1&
wait;
}

# global variables
TDIR=$HOME/store/trace/lsm;
RDIR=$HOME/store/result/lsm;
CDIR=`dirname $0`/conf/lsm;

# block devices used
D1="sde";
DEVS="$D1";

#############
# r = 4
#############
BASE=r4_10g_l;

#write_test h_w $CDIR/h_w.conf;
#run_test h_w_a $CDIR/h_w_a.conf
run_test h_max $CDIR/h_max.conf;
#run_test h_90 $CDIR/h_90.conf;
#run_test h_50 $CDIR/h_50.conf;
