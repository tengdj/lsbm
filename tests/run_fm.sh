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

	$ANALYZER -d "$DEVICE" -o "$TRACE" -p "$TESTER $CONF $RES $INUM";


	cd $TRACE && cp $TRACE/*.png $RES;
	cd $TRACE && cp $TRACE/*.out $RES;
	cp -f $STORE/LOG $RES;
	cp -f $STORE/1/LOG $RES; 
	cp /tmp/hlsm_log $RES;

	#sleep 10; # skip preloading
	for DEV in $DEVICE; do
		blkparse -i $TRACE/$DEV| tail -100 > $RES/${DEV}.out
	done

}

mk_store() {
SROOT=$(dirname $STORE); #primary store
rm -rf $STORE $SEC_STORAGE;

cp -rf $SROOT/$BASE $STORE &
cp -rf $SROOT/$BASE $SEC_STORAGE &
wait;
}


run_test() {
local TAG=$1;
local CONF=$2;

source $CONF;
mk_store;
run $TDIR $RDIR $TAG "$CONF" "$DEVS" 1 &
wait;
}

run_orig_test() {
local TAG=$1;
local CONF=$2;

source $CONF;
SROOT=$(dirname $STORE); #primary store
rm -rf $STORE; 
cp -rf $SROOT/$BASE $STORE;

run $TDIR $RDIR $TAG "$CONF" "$DEVS" 1 &
wait;
}

# global variables
TDIR=$HOME/store/trace/hlsm/fm;
RDIR=$HOME/store/result/hlsm/fm;
CDIR=`dirname $0`/conf/fm;

# block devices used
D1=sde;
D2=md0;
DEVS="$D1 $D2";

#############
# r = 4
#############
BASE=r4_base;

#run_test fm_w_a $CDIR/fm_w_a.conf;
#run_orig_test df_w_a $CDIR/df_w_a.conf;
run_orig_test df_r $CDIR/df_r.conf;
#run_orig_test dfh_w_a $CDIR/dfh_w_a.conf;

#run_orig_test df_max $CDIR/df_max.conf;
#run_test fm_max $CDIR/fm_max.conf;

#run_orig_test df_50 $CDIR/df_50.conf;
#run_orig_test df_90 $CDIR/df_90.conf;

#run_orig_test df_r $CDIR/df_r.conf;
#run_test fm_r $CDIR/fm_r.conf;
