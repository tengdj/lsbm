#!/bin/bash

ANALYZER=`dirname $0`/io-analysis.sh;
TESTER=`dirname $0`/test.sh;
CLEANER=`dirname $0`/cache-cleanup.sh;

mk_fm_store() {
SROOT=$(dirname $STORE); #primary store
rm -rf $STORE $SEC_STORAGE;

cp -rf $SROOT/$BASE $STORE &
cp -rf $SROOT/$BASE $SEC_STORAGE &
wait;
$CLEANER -b;
}

mk_hlsm_store() {
PROOT=$(dirname $STORE); #primary store
SROOT=$(dirname $SEC_STORAGE); #secondary store
rm -rf $STORE $SEC_STORAGE;

cp -rf $PROOT/${BASE}_p $STORE &
cp -rf $SROOT/${BASE}_s $SEC_STORAGE &
wait;
}

mk_store() {

SROOT=$(dirname $STORE); #primary store
rm -rf $STORE; 
cp -rf $SROOT/$BASE $STORE;

wait;
$CLEANER -b;
}

# global variables
TDIR=$HOME/store/trace/hlsm/ycsb;
RDIR=$HOME/store/result/hlsm/ycsb;
CDIR=`dirname $0`/conf/ycsb;

# block devices used
D1=sde;
D2=md0;
DEVS="$D1 $D2";

# default r=4
mk_r4() {
BASE=r4_base
STORE=/mnt/s3700/sec
mk_store
}

# default r=10
mk_r10(){
BASE=r10_base
STORE=/mnt/s3700/sec
mk_store
}

# fm r=4
mk_fm_r4(){
BASE=r4_base
STORE=/mnt/s3700/sec
SEC_STORAGE=/mnt/md0/pri
mk_fm_store
}

# 2-phase r=4
mk_2p_r4(){
BASE=r4_base
SEC_STORAGE=/mnt/s3700/sec
STORE=/mnt/md0/pri
mk_hlsm_store
}

$@
