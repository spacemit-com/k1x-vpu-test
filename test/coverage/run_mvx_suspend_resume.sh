#!/bin/bash

# Set up environment
source $(dirname $0)/sourceme

if [ ! -e /sys/power/state ]
then
    exit
fi

shell severity.sh verbose
echo devices > /sys/power/pm_test
echo mem > /sys/power/state

sleep 6

shell severity.sh warning
echo mem > /sys/power/state

echo "Testing suspend - resume done."

sleep 6
