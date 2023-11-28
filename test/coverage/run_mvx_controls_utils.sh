#!/bin/bash

# Set up environment.
source $(dirname $0)/sourceme

shell severity.sh warning

run_null mvx_encoder_gen -c 10 -w 960 -h 540 -v 24 -r 2000000 -x 1 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -v 24 -r 2000000 /dev/null

run_null mvx_encoder_gen -c 10 -w 960 -h 540 -n 4 -m 44 -x 1 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -n 4 -m 44 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -v 60 -g 1 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -v 60 -g 2 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -v 60 -g 3 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -d 200 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -a 1 /dev/null

run_null mvx_encoder_gen -c 10 -w 960 -h 540 -u 1000 /dev/null

run_null mvx_encoder_gen -c 30 -w 960 -h 540 -p 20 /dev/null

run_null mvx_encoder_gen -c 30 -w 960 -h 540 -b 2 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -o h264 -t 2 -e 1 -f raw /dev/null

run_null mvx_encoder_gen -c 7 -w 960 -h 540 -o h264 -t 0 -e 0 -f raw ./o_4.h264

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -t 4 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -v 24 -l 9 /dev/null

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -j 8 /dev/null

#run_null mvx_encoder_gen -c 10 -o h264 -y 10000000 -v 10 -w 960 -h 540 /dev/null

run_null mvx_encoder_gen -c 30 -o h264 -v 30 -w 960 -h 540 -q 18 -x 1 /dev/null

run_null mvx_encoder_gen -c 6 -w 320 -h 240 -v 10 -t 0 --tmvp 1 -o hevc -f raw /dev/null

run_null mvx_encoder_gen -c 6 -w 320 -h 240 -v 10 -t 0 --es 1 -o hevc -f raw /dev/null

run_null mvx_encoder_gen -c 6 -w 320 -h 240 -v 10 -t 0 --cip 1 -o hevc -f raw /dev/null

run_null mvx_encoder_gen -c 6 -w 320 -h 240 -v 10 --sesc 0 -o h264 -f raw /dev/null

run_null mvx_encoder_gen -c 6 -w 320 -h 240 -v 10 --hmvsr 32 --vmvsr 24 -o h264 -f raw /dev/null

echo "Encoding with 21 settings done."

for i in $(seq 0 15)
do
    run_null mvx_encoder_gen -c 6 -w 320 -h 240 -o h264 -v 10 -t 2 -l $i /dev/null
done

echo "Encoding with all H.264 supported levels, done."

run_null mvx_encoder_gen -c 6 -w 960 -h 540 -v 24 -t 2 -l 10 -o hevc  /dev/null

for i in 0 2 4 6 8 $(seq 10 25)
do
    run_null mvx_encoder_gen -c 6 -w 320 -h 240 -v 10  -t 0 -l $i -o hevc /dev/null
done

echo "Encoding with all HEVC supported levels, done."

run_null mvx_encoder_gen -c 10 -o vp9 -k 2 -w 960 -h 540 /dev/null

echo "Encoding VP9 with tiles, done."

run_null mvx_encoder_gen -c 1 -w 960 -h 540 -o jpeg -f raw --restart_interval 960 --quality 45 /dev/null

echo "Encoding JPEG with non-default settings, done."

run_null mvx_decoder -i vc1 -f raw -o yuv420 --fro 0 $SCRIPTDIR/SA10196_10s_7f.vc1 /dev/null

run_null mvx_decoder -i vc1 -f raw -o yuv420_afbc_8 --tiled $SCRIPTDIR/SA10196_10s_7f.vc1 /dev/null

run_null mvx_decoder -i h264 -f raw -o yuv420 --ish 1 ./o_4.h264 /dev/null

run_null mvx_decoder -i h264 -f raw -o yuv420 --trystop ./o_4.h264 /dev/null

run_null mvx_decoder -y 1000000 -i h264 -f raw -o yuv420 ./o_4.h264 ./o_4.yuv

echo "Decoding with 4 settings, done."

run_null mvx_encoder -i yuv420 -w 960 -h 540 --trystop -f raw -o h264 ./o_4.yuv /dev/null

echo "Encoding with 1 more setting, done."

run_null mvx_decoder -i h264 -n -f raw -o yuv420 $SCRIPTDIR/Parkrun_576i_10fr_30fps_HP_PicAFF_JM18.264 /dev/null

run_null mvx_decoder -i h264 -n -f raw -o yuv420_afbc_8 $SCRIPTDIR/Parkrun_576i_10fr_30fps_HP_PicAFF_JM18.264 /dev/null

echo "Decoding of interlaced file, done."

run_null mvx_decoder -i hevc -f raw -o yuv420 $SCRIPTDIR/sony_480p_0_6_track1.hevc /dev/null

echo "Decoding of HEVC file with HDR, done."

#VC1 Profile Simple
run_null mvx_decoder -i vc1_l -f rcv -o yuv420 $SCRIPTDIR/SSM0012_6f.rcv /dev/null

#VC1 Profile Main
run_null mvx_decoder -i vc1_l -f rcv -o yuv420 $SCRIPTDIR/SMH0002_6f.rcv /dev/null

#VC1 Profile Advanced
run_null mvx_decoder -i vc1 -f raw -o yuv420 $SCRIPTDIR/SA10196_10s_7f.vc1 /dev/null

echo "Decoding of VC1 profiles, done."

# Wait for the firmware binary to be unloaded.
sleep 6
