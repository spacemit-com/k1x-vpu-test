#!/bin/bash

# Set up environment
source $(dirname $0)/sourceme

if [ -z ${AMVX+x} ]
then
    AMVX='/modules/amvx.ko'
fi

if [ $# -gt 0 ]
then
    AMVX=$1
fi

# Remove amvx module
if [ "$(lsmod | grep amvx)" != "" ]
then
    run rmmod amvx
fi

# Insert amvx.ko
if [ ! -e /dev/video0 ]
then
    run mknod -m 0666 /dev/video0 c 81 0
fi

run insmod $AMVX
