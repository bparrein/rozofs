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

if [[ $# -ne 1 ]]; then
    echo "rpcgen.sh file"
    exit
fi

XFILE=$1

if [[ ! -f $XFILE ]]; then
    echo "file $XFILE not found."
    exit
fi

XDIRNAME=`dirname $XFILE`
XBASENAME=`basename $XFILE .x`
HEADER=${XDIRNAME}/${XBASENAME}.h
XDR=${XDIRNAME}/${XBASENAME}_xdr.c
CLT=${XDIRNAME}/${XBASENAME}_clt.c
SVC=${XDIRNAME}/${XBASENAME}_svc.c

rm -f $HEADER
rm -f $XDR
rm -f $CLT
rm -f $SVC

rpcgen -C -h -o $HEADER $XFILE
rpcgen -C -c -o $XDR $XFILE
rpcgen -C -l -o $CLT $XFILE
rpcgen -C -m -o $SVC $XFILE

exit
