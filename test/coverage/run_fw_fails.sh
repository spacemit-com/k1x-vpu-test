#!/bin/bash

# Set up environment.
source $(dirname $0)/sourceme

COV_DIR=$(dirname $0)
FW_DIR=${FW_DIR:-"/lib/firmware/linlon-v5-0-2"}
FW_ORIG="$FW_DIR/h264enc.fwb"
FW_TMP="$FW_DIR/h264enc.fwb.tmp"

function run_fw()
{
    FW_TEST="$COV_DIR"/$1

    cp $FW_TEST $FW_ORIG
    run_null "mvx_encoder_gen -c 1 /dev/null"
    echo 1 > /sys/kernel/amvx0/fw_cache/flush
    # Let the Linux Kernel flush it's internal firmware cache.
    sleep 3
}

function on_exit()
{
    if [ -e "$FW_TMP" ];
    then
        mv $FW_TMP $FW_ORIG
    fi

    echo 1 > /sys/kernel/amvx0/fw_cache/flush
}
trap on_exit EXIT

# Backup original fw
mv "$FW_ORIG" "$FW_TMP"

# Invalid protocol version
run_fw "h264enc.prot-ver.fwb"
