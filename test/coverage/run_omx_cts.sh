#!/system/bin/sh

# Set up environment.
source $(dirname $0)/sourceme

shell_null cts.sh media:DecoderTest:testCodecBasicH263
shell_null cts.sh media:DecoderTest:testCodecBasicH264
shell_null cts.sh media:DecoderTest:testCodecBasicHEVC
shell_null cts.sh media:DecoderTest:testCodecBasicMpeg4
shell_null cts.sh media:DecoderTest:testCodecBasicVP8
shell_null cts.sh media:DecoderTest:testCodecBasicVP9
shell_null cts.sh media:EncodeDecodeTest:testEncodeDecodeVideoFromBufferToBuffer720p
