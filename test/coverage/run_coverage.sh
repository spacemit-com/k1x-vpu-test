#!/bin/bash

# Set up environment.
source $(dirname $0)/sourceme

# Reload amvx twice to also test unloading the module.
shell insmod.sh
shell insmod.sh

# Use MVX utilities to run encode and decode.
shell run_mvx_utils.sh

# Run kill test
shell run_kill.sh

# Run encode->decode test.
shell run_mvx_encode_decode.sh

if is_android
then
    shell run_omx_cts.sh
else
    echo "Not an Android system. Skipping tests."
fi

# Use MVX controls utilities to run encode and decode.
shell run_mvx_controls_utils.sh

# Run FW failure tests.
shell run_fw_fails.sh

# Run MVX suspend resume test.
shell run_mvx_suspend_resume.sh

# Gather up the coverage data.
shell gather_on_test.sh $SCRIPTDIR/coverage.tar.gz

echo "Coverage done"

