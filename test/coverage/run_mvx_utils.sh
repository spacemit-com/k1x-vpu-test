#!/bin/bash

# Set up environment.
source $(dirname $0)/sourceme

WIDTH='320'
HEIGHT='240'

function run_cmd()
{
    run_null $@ &
    local pid=$!

    # Loop over files in debugfs and cat them.
    while pid_running $pid
    do
        local files=$(find /sys/kernel/amvx0 /sys/kernel/debug/amvx_* -type f -not -name 'msg')

        # Always succeed catting the file.
        # Some files are removed when the session is destroyed.
        cat $files &> /dev/null || true
    done
}

# Fetch supported formats
run "mvx_info -f"

# Run Codec in blocking mode to make use of wait_prepare() and
# wait_finish() Vb2 callbacks
run_null "mvx_encoder_gen -c10 -z /dev/null"

# Run tests with different severity.
for severity in debug panic
do
    shell severity.sh $severity
    run_cmd "mvx_encoder_gen -c 50 -w $WIDTH -h $HEIGHT /dev/null"
    run_cmd "mvx_decoder -f raw $SCRIPTDIR/input.h264 /dev/null"
done

# Encode different frame formats.
for fmt in yuv420 yuv420_nv12 yuv420_nv21 yuv420_p010 yuv420_y0l2 yuv422_yuy2 yuv422_uyvy yuv422_y210 rgba bgra argb abgr
do
    run_null "mvx_encoder_gen -c 1 -w $WIDTH -h $HEIGHT -i $fmt /dev/null"
done

# Test decode of different bitstream formats.
run_null "dd if=/dev/zero of=$SCRIPTDIR/input.zero bs=4096 count=1"
for fmt in h263 h264 hevc mpeg2 mpeg4 vc1 vp8 vp9 # TODO enable jpeg
do
    run_null "mvx_decoder -f raw -i $fmt $SCRIPTDIR/input.zero /dev/null"
done

# Multi session decode.
run_null "mvx_decoder_multi -n 6 $SCRIPTDIR/input.h264 $SCRIPTDIR/output.yuv"

# Run log daemon
mvx_logd > /dev/null
mvx_logd -C

shell severity.sh debug

echo null > /sys/kernel/debug/amvx/log/group/firmware_interface/drain || true

echo ram0 > /sys/kernel/debug/amvx/log/group/generic/drain

# Test redirecting firmware log to dmesg.
echo dmesg > /sys/kernel/debug/amvx/log/group/firmware_interface/drain
run_cmd "mvx_encoder_gen -c 6 -w $WIDTH -h $HEIGHT /dev/null"
echo ram0 > /sys/kernel/debug/amvx/log/group/firmware_interface/drain

echo dmesg > /sys/kernel/debug/amvx/log/group/generic/drain

shell severity.sh warning

# Wait for the firmware binary to be unloaded.
sleep 6

