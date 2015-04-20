#!/bin/bash

CLEANER=`dirname $0`/cache-cleanup.sh;
usage()
{
        echo "Usage: `echo $0| awk -F/ '{print $NF}'`  [-option]"
        echo "[option]:"
        echo "  -d  devices"
        echo "          devices under /dev(sda sdb)"
        echo "  -p  executable"
        echo "          program to run"
        echo "  -o  path"
        echo "          where to dump the result"
        echo "  -r  resolution"
        echo "          dpi"
        echo
        echo
}

if [ $# -lt 6 ]
then
        usage
        exit
fi

DPI=800;

while getopts "d:p:o:r:" OPTION
do
        case $OPTION in
                d)
                        DEVICES="$OPTARG";
			for dev in $DEVICES; do
				FDEVS="/dev/$dev $FDEVS";
			done
			echo $FDEVS
                        ;;
                o)
                        OPATH=$OPTARG;
			mkdir -p $OPATH;
                        ;;
                p)
                        EXEC="$OPTARG";
                        ;;
                r)
                        DPI="$OPTARG";
                        ;;
                ?)
                        echo "unknown arguments"
                        usage
                        exit
                        ;;
        esac
done

cd $OPATH && sudo blktrace -d $FDEVS &
$CLEANER -w 3 -b &
eval "$EXEC" 2>&1 > $OPATH/p.out

sudo kill -15 `pgrep blktrace`;
sleep 1;

#for dev in $DEVICES; do
#	cd $OPATH && seekwatcher -t $dev -o ${dev}.png --dpi=$DPI;
#	cd $OPATH && seekwatcher -R -t $dev -o ${dev}_R.png --dpi=$DPI;
#	cd $OPATH && seekwatcher -W -t $dev -o ${dev}_W.png --dpi=$DPI;
#done
