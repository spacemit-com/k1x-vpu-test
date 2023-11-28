#!/bin/bash
# Set up environment.

# Purpose of this test is to trigger a very specific issue in a
# session scheduler. If a user space process receives interrupt signal
# in a 'right' moment, scheduling sometime stops and
# wait_event_interruptible_timeout() in wait_pending() exits with a
# timeout. As a result, wait_pending() returns with an error leaving
# rest of buffers in non-flushed state.
#
# Currently there is a workaround in place in stop_streaming() which
# works good with buffers allocated by the driver but could cause
# problems for buffers allocated by user space (the latter scenario
# was never observed through).
#
# Having this test both adds coverage for mentioned fall-back work
# around and could help to investigate scheduling issue in future.

source $(dirname $0)/sourceme

TMPP=""

function on_exit()
{
    if [ -e $TMPP ]; then
        rm $TMPP
    fi
}
trap on_exit EXIT

function trykill()
{
    TMPP=$(mktemp -u)
    mknod $TMPP p

    mvx_encoder_gen -c10 /dev/null > $TMPP &
    PP=$!
    while read -r line; do
        if [[ $line = "Stream on 1" ]]; then
            kill $PP
        fi
    done < $TMPP

    rm $TMPP
}

shell severity.sh panic
echo Kill test
for i in `seq 1 20`; do
    echo Try $i
    trykill
    sleep 1
done
