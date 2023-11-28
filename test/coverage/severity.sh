#!/usr/bin/env bash

function to_mvx_severity()
{
    case $1 in
        'panic')
            echo -n '0'
            ;;
        'error')
            echo -n '1'
            ;;
        'warning')
            echo -n '2'
            ;;
        'info')
            echo -n '3'
            ;;
        'debug')
            echo -n '4'
            ;;
        'verbose')
            echo -n '5'
            ;;
        [0-5])
            echo -n $1
            ;;
        *)
            echo "Invalid severity '$1'." >&2
            exit 1
    esac
}

function to_videobuf2_severity()
{
    case $1 in
        'panic'|'error'|'warning'|'info')
            echo -n '0'
            ;;
        'debug')
            echo -n '4'
            ;;
        'verbose')
            echo -n '5'
            ;;
        [0-3])
            echo -n 0
            ;;
        [4-5])
            echo -n $1
            ;;
        *)
            echo "Invalid severity '$1'." >&2
            exit 1
    esac
}

function to_video4linux_severity()
{
    IOCTL=0x01
    IOCTL_ARG=0x02
    FOP=0x04
    STREAMING=0x08
    POLL=0x10

    case $1 in
        'panic'|'error'|'warning'|'info')
            echo -n '0'
            ;;
        'debug')
            echo -n $((IOCTL + IOCTL_ARG + FOP + STREAMING))
            ;;
        'verbose')
            echo -n $((IOCTL + IOCTL_ARG + FOP + STREAMING + POLL))
            ;;
        [0-9]*)
            if [ $1 -lt 0 ] || [ $1 -ge 32 ]
            then
                echo "Invalid severity '$1'." >&2
                exit 1
            fi
            echo -n $1
            ;;
        *)
            echo "Invalid severity '$1'." >&2
            exit 1
    esac
}

function to_severity()
{
    file=$1
    severity=$2

    case $file in
        '/sys/kernel/debug/'*)
            to_mvx_severity $severity
            ;;
        '/sys/module/video'*)
            to_videobuf2_severity $severity
            ;;
        '/sys/class/video4linux/'*)
            to_video4linux_severity $severity
            ;;
        *)
            echo "Invalid severity file '$file'." >&2
            exit 1
    esac
}

function get_severity()
{
    for i in $(find /sys/kernel/debug/amvx -name severity | grep -v 'firmware_interface/severity$') \
             $(find /sys/module/video* -name debug -type f 2> /dev/null) \
             $(find /sys/class/video4linux/video*/ -name dev_debug -type f 2> /dev/null)
    do
        echo -n "$i:$(cat $i) "
    done
}

function set_severity()
{
    for i in $@
    do
        # <file>=<severity> or <severity>
        t=(${i/:/ })
        if [ ${#t[@]} -le 1 ]
        then
            for file in $(find /sys/kernel/debug/amvx -name severity | grep -v 'firmware_interface/severity$') \
                        $(find /sys/module/video* -name debug -type f 2> /dev/null) \
                        $(find /sys/class/video4linux/video*/ -name dev_debug -type f 2> /dev/null)
            do
                severity=$(to_severity $file ${t[0]})
                echo $severity > $file
            done
        else
            file=${t[0]}
            severity=$(to_severity $file ${t[1]})
            echo $severity > $file
        fi
    done
}

if [ $# -ge 1 ]
then
    set_severity $@
else
    get_severity
fi
