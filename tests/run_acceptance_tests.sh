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


set -o nounset
#set -o errexit # warning $? unusable

# ROZO ENV
BIN_DIR="../src"
HOSTNAME="localhost"
EXPORTD=${BIN_DIR}/exportd
STORAGED=${BIN_DIR}/storaged
ROZOFS=${BIN_DIR}/rozofs
EXPORTD_CONF_FILE="./export.conf"
EXPORTD_ROOT="./export"
STORAGED_CONF_FILE="./storage.conf"
STORAGED_ROOT="./storage"
MOUNT_DIR="./mnt/"

# TESTS FILES AND ARGS
# File for test File Sytem Operations
TEST_FS_FILE="test_fs_operations"
TEST_FS_ARG_FILE_SIZE=100
TEST_FS_ARG_NB_LEVEL=2

usage() {
    echo "usage : $0 install|uninstall|start|stop|mount|umount|test|all"
    exit 0
}

install() {
    echo "Installing acceptance tests environment"
    mkdir ${EXPORTD_ROOT}
    for suffix in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
    do
        mkdir ${STORAGED_ROOT}${suffix}
    done

    mkdir ${MOUNT_DIR}
    ${EXPORTD} --create ${EXPORTD_ROOT}
    
    # create export conf file
    echo "volume = (" > ${EXPORTD_CONF_FILE}
    for suffix in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15
    do
        echo "{uuid = \"00000000-0000-0000-0000-0000000000${suffix}\"; host = \"${HOSTNAME}\";}," >> ${EXPORTD_CONF_FILE}
    done
    echo "{uuid = \"00000000-0000-0000-0000-000000000016\"; host = \"${HOSTNAME}\";}" >> ${EXPORTD_CONF_FILE}
    echo ");" >> ${EXPORTD_CONF_FILE}
    echo "exports = ({root = \"`pwd`/export\";});" >> ${EXPORTD_CONF_FILE}
    
    # create storage conf file
    echo "storages = (" > ${STORAGED_CONF_FILE}
    for suffix in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15
    do
        echo "{uuid = \"00000000-0000-0000-0000-0000000000${suffix}\"; root = \"`pwd`/storage${suffix}\";}," >> ${STORAGED_CONF_FILE}
    done
    echo "{uuid = \"00000000-0000-0000-0000-000000000016\"; root = \"`pwd`/storage16\";}" >> ${STORAGED_CONF_FILE}
    echo ");" >> ${STORAGED_CONF_FILE}
}

uninstall() {
    echo "Uninstalling acceptance tests environment"
    rm -f ${EXPORTD_CONF_FILE}
    rm -f ${STORAGED_CONF_FILE}
    rm -rf ${EXPORTD_ROOT}
    for suffix in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
    do
        rm -rf ${STORAGED_ROOT}${suffix}
    done
    rm -rf ${MOUNT_DIR}
}

start_exportd() {
    echo -e "\tStart Meta Filesystem Daemon"
    sudo ${EXPORTD} -c ${EXPORTD_CONF_FILE} --start 
}

start_storaged() {
    echo -e "\tStart Projections Storage Daemon"
    sudo ${STORAGED} -c ${STORAGED_CONF_FILE} --start
}

start() {
    echo "Start Daemons"
    start_storaged
    start_exportd
}

stop_exportd() {
    echo -e "\tStop Export Daemon"
    sudo ${EXPORTD} --stop
}

stop_storaged() {
    echo -e "\tStop Storage Daemon"
    sudo ${STORAGED} --stop
}

mount_fs(){
    echo -e "\tMount file system in " ${MOUNT_DIR}
    sudo ${ROZOFS} ${HOSTNAME} "`pwd`/export" ${MOUNT_DIR}
}

umount_fs(){
    echo -e "Umount file system"
    sudo umount ${MOUNT_DIR}
}

stop() {
    echo "Stop Daemons"
    stop_storaged
    stop_exportd
}

run_test() {
    echo "Run acceptance tests: "
    set -o errexit # warning $? unusable
    echo "Run test 1: "${TEST_FS_FILE}
    ./${TEST_FS_FILE} -d ${MOUNT_DIR} -f ${TEST_FS_ARG_NB_LEVEL} -s ${TEST_FS_ARG_FILE_SIZE}
}

[ $# -ne 1 ] && usage

COMMAND=$1

case $COMMAND in
    help)
        usage
        ;;
    install)
        install
        ;;
    uninstall)
        uninstall
        ;;
    start)
        start
        ;;
    stop)
        stop
        ;;
    mount)
        mount_fs
        ;;
    umount)
        umount_fs
        ;;
    test)
        run_test
        ;;
    all)
        uninstall
        install
        start
        mount_fs
        run_test
        umount_fs
        stop
        ;;
    *)
        echo "$COMMAND : unknow command."
        usage
        ;;
esac

exit 0
