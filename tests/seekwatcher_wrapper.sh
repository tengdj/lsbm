#!/bin/bash

usage()
{
        echo "Usage: `echo $0| awk -F/ '{print $NF}'`  [-option]"
        echo "[option]:"
        echo "  -d  devices"
        echo "          devices under /dev(sda sdb)"
        echo "  -o  path"
        echo "          where the traces resides"
        echo "  -r  resolution"
        echo "          dpi"
        echo
        echo
}

if [ $# -lt 4 ]
then
        usage
        exit
fi

DPI=1600;

while getopts "d:p:o:r:" OPTION
do
        case $OPTION in
                d)
                        DEVICES="$OPTARG";
                        ;;
                o)
                        OPATH=$OPTARG;
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

for dev in $DEVICES; do
	cd $OPATH && seekwatcher -t $dev -o ${dev}.png --dpi=$DPI;
	cd $OPATH && seekwatcher -R -t $dev -o ${dev}_R.png --dpi=$DPI;
	cd $OPATH && seekwatcher -W -t $dev -o ${dev}_W.png --dpi=$DPI;
done
