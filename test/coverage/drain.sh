#!/usr/bin/env bash

function set_drain()
{
    for i in $@
    do
        # <file>=<drain> or <drain>
        t=(${i/:/ })
        if [ ${#t[@]} -le 1 ]
        then
            for drain in $(find /sys/kernel/debug/amvx -path '*/group/*/drain' | grep -v 'firmware_interface/drain$')
            do
                echo $i > $drain
            done
        else
            echo ${t[1]} > ${t[0]}
        fi
    done
}

function get_drain()
{
    for drain in $(find /sys/kernel/debug/amvx -path '*/group/*/drain' | grep -v 'firmware_interface/drain$')
    do
        echo -n "$drain:$(cat $drain) "
    done
}

if [ $# -ge 1 ]
then
    set_drain $@
else
    get_drain
fi
