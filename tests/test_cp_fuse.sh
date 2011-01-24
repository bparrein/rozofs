#!/bin/bash
# Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
# This file is part of Rozo.
#
# Rozo is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 3 of the License,
# or (at your option) any later version.
#
# Rozo is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see
# <http://www.gnu.org/licenses/>.

#************************************************#
#               test_cp_fuse.sh                  #
#            write by Sylvain DAVID              #
#                30 August 2010                  #
# Copy a same file many times to mount point     #
#                                                #
#************************************************#

usage()
{
    echo "Usage: $0 -m <mount point directory> -i <input file> -o <output file> -n <nb. of files to copy> -f <logfile>";
    exit 1;
}

if [ $# -lt 5 ] ; then
    usage;
fi

while getopts m:i:o:n:f: OPTION
do
    case "$OPTION" in
      	m)
            mount_point="$OPTARG"
      	;;
      	i)
            input_file="$OPTARG"
            if [ ! -e "$input_file" ]; then
                echo "PROBLEM: $input_file doesn't exist"
                exit 1;
            elif [ ! -f "$input_file" ]; then
                echo "PROBLEM: $input_file isn't a regular file"
                exit 1;
            fi
      	;;
        o)
            output_file="$OPTARG"
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
echo "Nb. of files to copy: ${nb_files}" >> ${log_file}
echo "Nb. of files to copy: ${nb_files}"
echo "Input file: ${input_file}" >> ${log_file}
echo "Output file: ${output_file}" >> ${log_file}
size=$(stat -c %s ${input_file})
echo "Input file size: ${size} bytes">> ${log_file}

var=1

while [ "$var" -le "$nb_files" ]

do
    echo >> ${log_file}

    echo Num. test ${var} output file: ${output_file}_${var} >> ${log_file}
    echo Num. test ${var} output file: ${output_file}_${var}

    sudo time --output=${log_file} -a -p cp ${input_file} ${mount_point}/${output_file}_${var}

    let "var += 1"
                          
done

exit 0
