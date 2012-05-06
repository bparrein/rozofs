#!/bin/bash

#  Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
#  This file is part of Rozofs.
#
#  Rozofs is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published
#  by the Free Software Foundation; either version 3 of the License,
#  or (at your option) any later version.

#  Rozofs is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.

#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see
#  <http://www.gnu.org/licenses/>.

NAME_LABEL="# $(uname -a)"
DATE_LABEL="# $(date +%d-%m-%Y--%H:%M:%S)"

PASSWORD="helloworld"
MD5_GENERATED=`md5pass ${PASSWORD} rozofs | cut -c 11-`

LOCAL_CONF=$PWD/config_files/

EXPORT_NAME_BASE=localhost
EXPORTS_NAME_PREFIX=export
EXPORTS_ROOT=$PWD/${EXPORTS_NAME_PREFIX}
EXPORT_DAEMON=exportd
EXPORT_CONF_FILE=export.conf

STORAGE_NAME_BASE=localhost
STORAGES_NAME_PREFIX=storage
STORAGES_ROOT=$PWD/${STORAGES_NAME_PREFIX}
STORAGE_DAEMON=storaged
STORAGE_CONF_FILE=storage.conf

LOCAL_MNT_PREFIX=mnt
LOCAL_MNT_ROOT=$PWD/${LOCAL_MNT_PREFIX}
ROZOFS_CLIENT=rozofsmount

SOURCE_DIR=$(dirname $PWD)
BUILD_DIR=$PWD/build
CMAKE_BUILD_TYPE=Debug #Debug or Release
DAEMONS_LOCAL_DIR=${BUILD_DIR}/src/

TESTS_DIR=$PWD/fs_ops

TEST_FS_OP_1_DIR=${TESTS_DIR}/fileop/fileop
TEST_FS_OP_1_LOWER_LMT=1
TEST_FS_OP_1_UPPER_LMT=3
TEST_FS_OP_1_INCREMENT=1
TEST_FS_OP_1_FILE_SIZE=300K

TEST_FS_OP_2_DIR=${TESTS_DIR}/pjd-fstest

build ()
{
    if [ ! -e "${SOURCE_DIR}" ]
    then
        echo "Unable to build RozoFS (${SOURCE_DIR} not exist)"
    fi

    if [ -e "${BUILD_DIR}" ]
    then
        rm -rf ${BUILD_DIR}
    fi

    mkdir ${BUILD_DIR}

    cd ${BUILD_DIR}
    rm -rf ${SOURCE_DIR}/CMakeCache.txt
    cmake -G "Unix Makefiles" -DDAEMON_PID_DIRECTORY=${BUILD_DIR} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ${SOURCE_DIR}
    make
    cd ..
}

# $1 -> LAYOUT
# $2 -> storages by node
gen_storage_conf ()
{
    ROZOFS_LAYOUT=$1
    STORAGES_BY_NODE=$2

    FILE=${LOCAL_CONF}'storage_l'${ROZOFS_LAYOUT}'.conf'

    if [ ! -e "$LOCAL_CONF" ]
    then
        mkdir -p $LOCAL_CONF
    fi

    if [ -e "$FILE" ]
    then
        rm -rf $FILE
    fi

    touch $FILE
    echo ${NAME_LABEL} >> $FILE
    echo ${DATE_LABEL} >> $FILE
    echo "layout = ${ROZOFS_LAYOUT} ;" >> $FILE
    echo 'storages = (' >> $FILE
    for j in $(seq ${STORAGES_BY_NODE}); do
        if [[ ${j} == ${STORAGES_BY_NODE} ]]
        then
            echo "  {sid = $j; root =\"${STORAGES_ROOT}_$j\";}" >> $FILE
        else
            echo "  {sid = $j; root =\"${STORAGES_ROOT}_$j\";}," >> $FILE
        fi
    done;
    echo ');' >> $FILE
}

# $1 -> LAYOUT
# $2 -> storages by node
# $2 -> Nb. of exports
# $3 -> md5 generated
gen_export_conf ()
{

    ROZOFS_LAYOUT=$1
    STORAGES_BY_NODE=$2

    FILE=${LOCAL_CONF}'export_l'${ROZOFS_LAYOUT}'.conf'

    if [ ! -e "$LOCAL_CONF" ]
    then
        mkdir -p $LOCAL_CONF
    fi

    if [ -e "$FILE" ]
    then
        rm -rf $FILE
    fi

    touch $FILE
    echo ${NAME_LABEL} >> $FILE
    echo ${DATE_LABEL} >> $FILE
    echo "layout = ${ROZOFS_LAYOUT} ;" >> $FILE
    echo 'volume =' >> $FILE
    echo '(' >> $FILE
    echo '   {' >> $FILE
    echo '       cid = 1;' >> $FILE
    echo '       sids =' >> $FILE
    echo '       (' >> $FILE
    for k in $(seq ${STORAGES_BY_NODE}); do
        let idx=${k}-1
        if [[ ${k} == ${STORAGES_BY_NODE} ]]
        then
            echo "           {sid = $k; host = \"${STORAGE_NAME_BASE}\";}" >> $FILE
        else
            echo "           {sid = $k; host = \"${STORAGE_NAME_BASE}\";}," >> $FILE
        fi
    done;
    echo '       );' >> $FILE
    echo '   }' >> $FILE
    echo ');' >> $FILE
    echo 'exports = (' >> $FILE
    for k in $(seq ${NB_EXPORTS}); do
        if [[ ${k} == ${NB_EXPORTS} ]]
        then
            echo "   {eid = $k; root = \"${EXPORTS_ROOT}_$k\"; md5=\"${3}\";}" >> $FILE
        else
            echo "   {eid = $k; root = \"${EXPORTS_ROOT}_$k\"; md5=\"${3}\";}," >> $FILE
        fi
    done;
    echo ');' >> $FILE
}

start_storaged ()
{

    echo "------------------------------------------------------"
    PID=`ps ax | grep ${STORAGE_DAEMON} | grep -v grep | awk '{print $1}'`
    if [ "$PID" == "" ]
    then
        echo "Start ${STORAGE_DAEMON}"
    ${DAEMONS_LOCAL_DIR}${STORAGE_DAEMON} -c ${LOCAL_CONF}${STORAGE_CONF_FILE}
    else
        echo "Unable to start ${STORAGE_DAEMON} (already running as PID: ${PID})"
        exit 0;
    fi

}

stop_storaged ()
{
    echo "------------------------------------------------------"
    PID=`ps ax | grep ${STORAGE_DAEMON} | grep -v grep | awk '{print $1}'`
    if [ "$PID" != "" ]
    then
        echo "Stop ${STORAGE_DAEMON} (PID: ${PID})"
        kill -9 $PID
    else
        echo "Unable to stop ${STORAGE_DAEMON} (not running)"
    fi
}

reload_storaged ()
{
    echo "------------------------------------------------------"
    echo "Reload ${STORAGE_DAEMON}"
    kill -1 `ps ax | grep ${STORAGE_DAEMON} | grep -v grep | awk '{print $1}'`
}

# $1 -> storages by node
create_storages ()
{

    if [ ! -e "${LOCAL_CONF}${STORAGE_CONF_FILE}" ]
    then
        echo "Unable to remove storage directories (configuration file doesn't exist)"
    else
        STORAGES_BY_NODE=`grep sid ${LOCAL_CONF}${STORAGE_CONF_FILE} | wc -l`

        for j in $(seq ${STORAGES_BY_NODE}); do

            if [ -e "${STORAGES_ROOT}_${j}" ]
            then
                rm -rf ${STORAGES_ROOT}_${j}/*.bins
            else
                mkdir -p ${STORAGES_ROOT}_${j}
            fi

        done;
    fi
}

# $1 -> storages by node
remove_storages ()
{
    if [ ! -e "${LOCAL_CONF}${STORAGE_CONF_FILE}" ]
    then
        echo "Unable to remove storage directories (configuration file doesn't exist)"
    else
        STORAGES_BY_NODE=`grep sid ${LOCAL_CONF}${STORAGE_CONF_FILE} | wc -l`

        for j in $(seq ${STORAGES_BY_NODE}); do

            if [ -e "${STORAGES_ROOT}_${j}" ]
            then
                rm -rf ${STORAGES_ROOT}_${j}
            fi

        done;
    fi
}

# $1 -> LAYOUT
go_layout ()
{
    ROZOFS_LAYOUT=$1

    if [ ! -e "${LOCAL_CONF}export_l${ROZOFS_LAYOUT}.conf" ] || [ ! -e "${LOCAL_CONF}export_l${ROZOFS_LAYOUT}.conf" ]
    then
        echo "Unable to change configuration files to layout ${ROZOFS_LAYOUT}"
        exit 0
    else
        ln -s -f ${LOCAL_CONF}'export_l'${ROZOFS_LAYOUT}'.conf' ${LOCAL_CONF}${EXPORT_CONF_FILE}
        ln -s -f ${LOCAL_CONF}'storage_l'${ROZOFS_LAYOUT}'.conf' ${LOCAL_CONF}${STORAGE_CONF_FILE}
    fi
}

deploy_clients_local ()
{
    echo "------------------------------------------------------"
    if [ ! -e "${LOCAL_CONF}${EXPORT_CONF_FILE}" ]
        then
        echo "Unable to mount RozoFS (configuration file doesn't exist)"
    else
        NB_EXPORTS=`grep eid ${LOCAL_CONF}${EXPORT_CONF_FILE} | wc -l`

        for j in $(seq ${NB_EXPORTS}); do
            mountpoint -q ${LOCAL_MNT_ROOT}${j}
            if [ "$?" -ne 0 ]
            then
                echo "Mount RozoFS (export: ${EXPORTS_NAME_PREFIX}_${j}) on ${LOCAL_MNT_PREFIX}${j}"

                if [ ! -e "${LOCAL_MNT_ROOT}${j}" ]
                then
                    mkdir -p ${LOCAL_MNT_ROOT}${j}
                fi

                ${DAEMONS_LOCAL_DIR}${ROZOFS_CLIENT} -H ${EXPORT_NAME_BASE} -E ${EXPORTS_ROOT}_${j} -o exportpasswd=${PASSWORD} ${LOCAL_MNT_ROOT}${j}
            else
                echo "Unable to mount RozoFS (${LOCAL_MNT_PREFIX}_${j} already mounted)"
            fi
        done;
    fi
}

undeploy_clients_local ()
{
    echo "------------------------------------------------------"
    if [ ! -e "${LOCAL_CONF}${EXPORT_CONF_FILE}" ]
        then
        echo "Unable to umount RozoFS (configuration file doesn't exist)"
    else
        NB_EXPORTS=`grep eid ${LOCAL_CONF}${EXPORT_CONF_FILE} | wc -l`

        for j in $(seq ${NB_EXPORTS}); do
            echo "Umount RozoFS mnt: ${LOCAL_MNT_PREFIX}${j}"
            umount ${LOCAL_MNT_ROOT}${j}
            test -d ${LOCAL_MNT_ROOT}${j} && rm -rf ${LOCAL_MNT_ROOT}${j}
        done;
    fi
}

start_exportd ()
{
    echo "------------------------------------------------------"
    PID=`ps ax | grep ${EXPORT_DAEMON} | grep -v grep | awk '{print $1}'`
    if [ "$PID" == "" ]
    then
        echo "Start ${EXPORT_DAEMON}"
        ${DAEMONS_LOCAL_DIR}${EXPORT_DAEMON} -c ${LOCAL_CONF}${EXPORT_CONF_FILE}
    else
        echo "Unable to start ${EXPORT_DAEMON} (already running as PID: ${PID})"
        exit 0;
    fi
}

stop_exportd ()
{
    echo "------------------------------------------------------"
    PID=`ps ax | grep ${EXPORT_DAEMON} | grep -v grep | awk '{print $1}'`
    if [ "$PID" != "" ]
    then
        echo "Stop ${EXPORT_DAEMON} (PID: ${PID})"
        kill -9 $PID
    else
        echo "Unable to stop ${EXPORT_DAEMON} (not running)"
    fi
}

reload_exportd ()
{
    echo "------------------------------------------------------"
    PID=`ps ax | grep ${EXPORT_DAEMON} | grep -v grep | awk '{print $1}'`
    if [ "$PID" != "" ]
    then
        echo "Reload ${EXPORT_DAEMON} (PID: ${PID})"
        kill -1 $PID
    else
        echo "Unable to reload ${EXPORT_DAEMON} (not running)"
    fi
}

# $1 -> Nb. of exports
create_exports ()
{
    if [ ! -e "${LOCAL_CONF}${EXPORT_CONF_FILE}" ]
    then
        echo "Unable to create export directories (configuration file doesn't exist)"
    else
        NB_EXPORTS=`grep eid ${LOCAL_CONF}${EXPORT_CONF_FILE} | wc -l`

        for k in $(seq ${NB_EXPORTS}); do
            if [ -e "${EXPORTS_ROOT}_${k}" ]
            then
                rm -rf ${EXPORTS_ROOT}_${k}/*
            else
                mkdir -p ${EXPORTS_ROOT}_${k}
            fi
        done;
    fi
}

# $1 -> Nb. of exports
remove_exports ()
{
    if [ ! -e "${LOCAL_CONF}${EXPORT_CONF_FILE}" ]
    then
        echo "Unable to remove export directories (configuration file doesn't exist)"
    else
        NB_EXPORTS=`grep eid ${LOCAL_CONF}${EXPORT_CONF_FILE} | wc -l`

        for j in $(seq ${NB_EXPORTS}); do

            if [ -e "${EXPORTS_ROOT}_${j}" ]
            then
                rm -rf ${EXPORTS_ROOT}_${j}
            fi
        done;
    fi
}

remove_config_files ()
{
    echo "------------------------------------------------------"
    echo "Remove configuration files"
    rm -rf $LOCAL_CONF
}

remove_all ()
{
    echo "------------------------------------------------------"
    echo "Remove configuration files, storage and exports directories"
    rm -rf $LOCAL_CONF
    rm -rf $STORAGES_ROOT*
    rm -rf $EXPORTS_ROOT*
}

remove_build ()
{
    echo "------------------------------------------------------"
    echo "Remove build directory"
    rm -rf $BUILD_DIR
}

clean_all ()
{
    undeploy_clients_local
    stop_storaged
    stop_exportd
    remove_build
    remove_all
}

fs_test_1(){

    NB_EXPORTS=`grep eid ${LOCAL_CONF}${EXPORT_CONF_FILE} | wc -l`
    EXPORT_LAYOUT=`grep layout ${LOCAL_CONF}${EXPORT_CONF_FILE} | grep -v grep | cut -c 10`

    for j in $(seq ${NB_EXPORTS}); do
        echo "------------------------------------------------------"
        mountpoint -q ${LOCAL_MNT_ROOT}${j}
        if [ "$?" -eq 0 ]
        then
            echo "Run FS operations test 1 on ${LOCAL_MNT_PREFIX}${j} with layout $EXPORT_LAYOUT"
            echo "------------------------------------------------------"
            ${TEST_FS_OP_1_DIR} -l ${TEST_FS_OP_1_LOWER_LMT} -u ${TEST_FS_OP_1_UPPER_LMT} -i ${TEST_FS_OP_1_INCREMENT} -e -s ${TEST_FS_OP_1_FILE_SIZE} -d ${LOCAL_MNT_ROOT}${j}
        else
            echo "Unable to run FS operations test 1 (${LOCAL_MNT_PREFIX}${j} is not mounted)"
        fi
    done;

}

fs_test_2(){

    NB_EXPORTS=`grep eid ${LOCAL_CONF}${EXPORT_CONF_FILE} | wc -l`
    EXPORT_LAYOUT=`grep layout ${LOCAL_CONF}${EXPORT_CONF_FILE} | grep -v grep | cut -c 10`

    for j in $(seq ${NB_EXPORTS}); do
        echo "------------------------------------------------------"
        mountpoint -q ${LOCAL_MNT_ROOT}${j}
        if [ "$?" -eq 0 ]
        then
            echo "Run FS operations test 2 on ${LOCAL_MNT_PREFIX}${j} with layout $EXPORT_LAYOUT"
            echo "------------------------------------------------------"

            cd ${LOCAL_MNT_ROOT}${j}
            prove -r ${TEST_FS_OP_2_DIR}
            cd ..

        else
            echo "Unable to run FS operations test 2 (${LOCAL_MNT_PREFIX}${j} is not mounted)"
        fi
    done;

}

check_no_run ()
{

    PID_EXPORTD=`ps ax | grep ${EXPORT_DAEMON} | grep -v grep | awk '{print $1}'`
    PID_STORAGED=`ps ax | grep ${STORAGE_DAEMON} | grep -v grep | awk '{print $1}'`

    if [ "$PID_STORAGED" != "" ] || [ "$PID_EXPORTD" != "" ]
    then
        echo "${EXPORT_DAEMON} or/and ${STORAGE_DAEMON} already running"
        exit 0;
    fi

}

check_build ()
{

    if [ ! -e "${DAEMONS_LOCAL_DIR}${EXPORT_DAEMON}" ]
    then
        echo "Daemons are not build !!! use $0 build"
        exit 0;
    fi

}


usage ()
{
    echo >&2 "Usage:"
    echo >&2 "$0 start <Layout>"
    echo >&2 "$0 stop"
    echo >&2 "$0 test"
    echo >&2 "$0 build"
    exit 0;
}

run_fs_test () 
{
        NB_EXPORTS=2
        gen_storage_conf 0 4
        gen_export_conf 0 4 ${MD5_GENERATED}
        gen_storage_conf 1 8
        gen_export_conf 1 8 ${MD5_GENERATED}
        gen_storage_conf 2 16
        gen_export_conf 2 16 ${MD5_GENERATED}

        go_layout 0
        create_storages
        create_exports
        start_storaged
        start_exportd
        deploy_clients_local
        fs_test_1
        fs_test_2
        undeploy_clients_local
        stop_storaged
        stop_exportd
        remove_exports
        remove_storages

        go_layout 1
        create_storages
        create_exports
        start_storaged
        start_exportd
        deploy_clients_local
        fs_test_1
        fs_test_2
        undeploy_clients_local
        stop_storaged
        stop_exportd
        remove_exports
        remove_storages

        go_layout 2
        create_storages
        create_exports
        start_storaged
        start_exportd
        deploy_clients_local
        fs_test_1
        fs_test_2
        undeploy_clients_local
        stop_storaged
        stop_exportd
        remove_exports
        remove_storages

        remove_all
}

main ()
{
    [ $# -lt 1 ] && usage

    if [ "$1" == "start" ]
    then

        [ $# -lt 2 ] && usage

        if [ "$2" -eq 0 ]
        then
            ROZOFS_LAYOUT=$2
            STORAGES_BY_NODE=4
        elif [ "$2" -eq 1 ]
        then
            ROZOFS_LAYOUT=$2
            STORAGES_BY_NODE=8
        elif [ "$2" -eq 2 ]
        then
            ROZOFS_LAYOUT=$2
            STORAGES_BY_NODE=16
        else
	        echo >&2 "Rozofs layout must be equal to 0,1 or 2."
	        exit 1
        fi

        check_build
        check_no_run

        NB_EXPORTS=2

        gen_storage_conf ${ROZOFS_LAYOUT} ${STORAGES_BY_NODE}
        gen_export_conf ${ROZOFS_LAYOUT} ${STORAGES_BY_NODE}

        go_layout ${ROZOFS_LAYOUT}

        create_storages
        create_exports

        start_storaged
        start_exportd

        deploy_clients_local

    elif [ "$1" == "stop" ]
    then

        undeploy_clients_local

        stop_storaged
        stop_exportd

        remove_all

    elif [ "$1" == "test" ]
    then

        check_build
        check_no_run
        run_fs_test

    elif [ "$1" == "mount" ]
    then
        check_build
        deploy_clients_local

    elif [ "$1" == "umount" ]
    then
        check_build
        undeploy_clients_local

    elif [ "$1" == "build" ]
    then
        build
    elif [ "$1" == "clean" ]
    then
        clean_all
    else
        usage
    fi
    exit 0;
}

main $@
