#!/bin/bash

FRAME_COUNT="1"
WIDTH="320"
HEIGHT="240"

temp=$(mktemp -d)
echo cd $temp
cd $temp

function get_mve()
{
  hardware_id=$(cat /sys/kernel/debug/amvx_dev0/regs | sed -n 's/HARDWARE_ID = 0x\(.*\)/\1/p')

  case $hardware_id in
    '5650'*)
      echo 'gorm'
      ;;
    '5655'*)
      echo 'auda'
      ;;
    '5660'*|'5661'*)
      echo "egil"
      ;;
    '5662'*)
      echo 'atle'
      ;;
    *)
      echo "Error: Unknown hardware '$hardware_id'."
      exit 1
      ;;
  esac
}

function get_encoder_formats()
{
  mve=$(get_mve)

  case $mve in
    'gorm')
      echo 'yuv420 yuv420_nv12'
      ;;
    'auda')
      echo 'yuv420 yuv420_nv12 yuv420_nv21 yuv422_yuy2 yuv422_uyvy'
      ;;
    'egil')
      echo 'yuv420 yuv420_nv12 yuv420_nv21 yuv420_p010 yuv420_y0l2 yuv422_yuy2 yuv422_uyvy yuv422_y210 rgba bgra argb abgr'
      ;;
    'atle')
      echo 'yuv420 yuv420_nv12 yuv420_nv21 yuv420_p010 yuv420_y0l2 yuv422_yuy2 yuv422_uyvy yuv422_y210 rgba bgra argb abgr'
      ;;
    *)
      exit 1
      ;;
  esac
}

function get_encoder_codecs()
{
  mve=$(get_mve)

  case $mve in
    'gorm')
      echo 'h264 vp8 jpeg'
      ;;
    'auda')
      echo 'h264 hevc vp8 jpeg'
      ;;
    'egil')
      echo 'h264 hevc vp8 vp9 jpeg'
      ;;
    'atle')
      echo 'h264 hevc vp8 vp9 jpeg'
      ;;
    *)
      exit 1
      ;;
  esac
}

function get_decoder_formats()
{
  mve=$(get_mve)

  case $mve in
    'gorm')
      echo 'yuv420 yuv420_nv12 yuv420_afbc_8'
      ;;
    'auda')
      echo 'yuv420 yuv420_nv12 yuv420_nv21 yuv420_p010 yuv420_y0l2 yuv422_yuy2 yuv422_uyvy yuv422_y210 yuv420_afbc_8 yuv420_afbc_10 yuv422_afbc_8 yuv422_afbc_10'
      ;;
    'egil')
      echo 'yuv420 yuv420_nv12 yuv420_nv21 yuv420_p010 yuv420_y0l2 yuv422_yuy2 yuv422_uyvy yuv422_y210 yuv420_afbc_8 yuv420_afbc_10 yuv422_afbc_8 yuv422_afbc_10'
      ;;
    'atle')
      echo 'yuv420 yuv420_nv12 yuv420_nv21 yuv420_p010 yuv420_y0l2 yuv422_yuy2 yuv422_uyvy yuv422_y210 yuv420_afbc_8 yuv420_afbc_10 yuv422_afbc_8 yuv422_afbc_10'
      ;;
    *)
      exit 1
      ;;
  esac
}

function get_subsampling()
{
  case $1 in
    'yuv420'|'yuv420_nv12'|'yuv420_nv21'|'yuv420_p010'|'yuv420_y0l2'|'yuv420_afbc_8'|'yuv420_afbc_10')
      echo '420'
      ;;
    'yuv422_yuy2'|'yuv422_uyvy'|'yuv422_y210'|'yuv422_afbc_8'|'yuv422_afbc_10')
      echo '422'
      ;;
    'rgba'|'bgra'|'argb'|'abgr')
      echo '422'
      ;;
    *)
      echo "Error: Can't determine subsampling for unknown format '$1'."
      exit 1
      ;;
  esac
}

function get_bitdepth()
{
  case $1 in
    'yuv420'|'yuv420_nv12'|'yuv420_nv21'|'yuv422_yuy2'|'yuv422_uyvy'|'yuv420_afbc_8'|'yuv422_afbc_8')
      echo '8'
      ;;
    'yuv420_p010'|'yuv420_y0l2'|'yuv422_y210'|'yuv420_afbc_10'|'yuv422_afbc_10')
      echo '10'
      ;;
    'rgba'|'bgra'|'argb'|'abgr')
      echo '8'
      ;;
    *)
      echo "Error: Can't determine number of bits for unknown format '$1'."
      exit 1
      ;;
  esac
}

function run()
{
  local fname=$1
  local ret=0
  shift

  # Clear logs
  dmesg -C
  mvx_logd -C

  # Run test
  $@ &> $fname.log
  ret=$?

  # Dump logs to file
  dmesg -c > $fname.dmesg
  mvx_logd -f text $fname.fw

  # Check that output file exists with non zero size
  if [ ! -s $fname ]
  then
    return 1
  fi

  # Search for errors in fw log
  if [ -n "$(grep -E 'ERROR|SET_OPTION_FAIL' $fname.fw)" ]
  then
    return 1
  fi

  return $ret
}

for iff in $(get_encoder_formats)
do
  echo "$iff"

  for bf in $(get_encoder_codecs)
  do
    # BUG
    if [[ "$iff" == "yuv422_y210" ]] && [[ "$bf" == "jpeg" ]]
    then
      continue
    fi

    echo "    $bf"

    enc="$iff.$bf"
    cmd="mvx_encoder_gen -c $FRAME_COUNT -w $WIDTH -h $HEIGHT -i $iff -o $bf $enc"
    run $enc $cmd
    if [ $? -ne 0 ]
    then
      echo $cmd >> failed
      continue
    fi

    for off in $(get_decoder_formats)
    do
      ibd=$(get_bitdepth $iff)
      iss=$(get_subsampling $iff)

      # H264, VP8 and JPEG downsample 10 bit to 8 bit
      if [[ "$ibd" == "10" ]] && ( [[ "$bf" == "h264" ]] || [[ "$bf" == "vp8" ]] || [[ "$bf" == "jpeg" ]] )
      then
        ibd="8"
      fi

      # All codecs except for jpeg downsample 422 to 420
      if [[ "$bf" != "jpeg" ]]
      then
        iss="420"
      fi

      # Decoding 420 to 422 is not supported
      if [[ "$iss" == "420" ]] && [[ "$(get_subsampling $off)" == "422" ]]
      then
        continue
      fi

      # AFBC requires subsampling and bitdepth to match
      if [[ "$off" == *"afbc"* ]] && ( [[ "$iss" != "$(get_subsampling $off)" ]] || [[ "$ibd" != "$(get_bitdepth $off)" ]] )
      then
        continue
      fi

      echo "        $off"

      dec="$enc.$off"
      cmd="mvx_decoder -i $bf -o $off $enc $dec"
      run $dec $cmd
      if [ $? -ne 0 ]
      then
        echo $cmd >> failed
      fi
    done

  done
done

if [ -s failed ]
then
  echo "Failure. Check $temp/failed for further information."
else
  echo "Success. Removing temp directory."
  rm -rf $temp
fi
