#!/bin/bash

#************************************************#
#          test_create_and_write_file.sh         #
#            write by Sylvain DAVID              #
#                30 August 2010                  #
#                                                #
#                                                #
#************************************************#

usage()
{
        echo "Usage: $0 -h <@IP of MDS> -i <input file> -b <size of buffer> -n <nb. of files to create and write> -f <logfile>";
        exit 1;
}

if [ $# -lt 5 ] ; then
        usage;
fi

while getopts h:i:b:n:f: OPTION
do
    case "$OPTION" in
      	h)
        	host="$OPTARG"
      	;;
      	i)
        	filename="$OPTARG"
        	if [ ! -e "$filename" ]; then
				echo "PROBLEM: $filename doesn't exist"
		        exit 1;
		    elif [ ! -f "$filename" ]; then
				echo "PROBLEM: $filename isn't a regular file"
				exit 1;
			fi
      	;;
		b)
        	buff_begin="$OPTARG"
      	;;
        n)
        	nb_files="$OPTARG"
        	if [ ! "$nb_files" -gt 0 ]; then
				echo "PROBLEM: the nb. of files must be > 0"
				exit 1;
			fi
      	;;
        f)
        	log_file="$OPTARG"
        	if [ -e "$log_file" ]; then
				echo "PROBLEM: $log_file exists"
		        exit 1;
			fi
      	;;
    esac
done
shift $(($OPTIND - 1))

echo "TEST DATE: `date`" >> ${log_file}
echo "TESTER: `whoami`" >> ${log_file}
echo "IP address of MDS: "${host} >> ${log_file}
echo "Size of buffer: ${buff_begin} bytes" >> ${log_file}
echo "Nb. of files to create and write: ${nb_files}" >> ${log_file}
echo "Nb. of files to create and write: ${nb_files}"
size=$(stat -c %s ${filename})
echo "Input file size: ${size} bytes">> ${log_file}

var=1

while [ "$var" -le "$nb_files" ]

do
	echo >> ${log_file}
	
	echo Num. test ${var}: BSIZE=${buff_begin} bytes, output file: ${filename}_${var} >> ${log_file}
	
	sudo time --output=${log_file} -a -p ./sendfile ${host} ${filename} ${filename}_${var} ${buff_begin}
	
	let "var += 1"
                          
done

exit 0
