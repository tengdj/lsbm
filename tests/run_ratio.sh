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

	#sleep 10; # skip preloading
	for DEV in $DEVICE; do
		blkparse -i $TRACE/$DEV| tail -100 > $RES/${DEV}.out
	done

}

write_test() {
local TAG=$1;
local CONF=$2;

source $CONF;
SROOT=$(dirname $STORE);
rm -rf $STORE 
mkdir -p $STORE;
run $TDIR $RDIR $TAG "$CONF" "$DEVS" 1 &
wait;
# save the data for future reuse
sleep 2;
mv $STORE $STORE/../$BASE;
}

write_test_append() {
local TAG=$1;
local CONF=$2;

source $CONF;
SROOT=$(dirname $STORE);
rm -rf $STORE;
cp -rf $SROOT/$BASE $STORE ;
run $TDIR $RDIR $TAG "$CONF" "$DEVS" 1 &
wait;
sleep 2;
}

read_test() {
local TAG=$1;
local CONF=$2;

source $CONF;
SROOT=$(dirname $STORE);
rm -rf $STORE;
cp -rf $SROOT/$BASE $STORE ;
run $TDIR $RDIR $TAG "$CONF" "$DEVS" 1 &
wait;
}

# global variables
TDIR=$HOME/store/trace/hlsm/ratio;
RDIR=$HOME/store/result/hlsm/ratio;
CDIR=`dirname $0`/conf/ratio;

# block devices used
D1=sde;
DEVS="$D1";

#############
# r = 2
#############
BASE=r2_base;

#write_test r2_w $CDIR/r2_w.conf;
#write_test_append r2_w_a $CDIR/r2_w_a.conf;
#read_test r2_r_p $CDIR/r2_r_preload.conf;
#read_test r2_r_np $CDIR/r2_r_no_preload.conf;


#############
# r = 4
#############
BASE=r4_base;

write_test r4_w_perfect $CDIR/r4_w_perfect.conf;
#write_test r4_w $CDIR/r4_w.conf;
#write_test_append r4_w_a $CDIR/r4_w_a.conf;
#read_test r4_r_p $CDIR/r4_r_preload.conf;
#read_test r4_r_np $CDIR/r4_r_no_preload.conf;


#############
# r = 8
#############
BASE=r8_base;

#write_test r8_w $CDIR/r8_w.conf;
#write_test_append r8_w_a $CDIR/r8_w_a.conf;
#read_test r8_r_p $CDIR/r8_r_preload.conf;
#read_test r8_r_np $CDIR/r8_r_no_preload.conf;


#############
# r = 10
#############
BASE=r10_base;

#write_test r10_w $CDIR/r10_w.conf
#write_test_append r10_w_a $CDIR/r10_w_a.conf
#read_test r10_r_p $CDIR/r10_r_preload.conf;
#read_test r10_r_np $CDIR/r10_r_no_preload.conf;
