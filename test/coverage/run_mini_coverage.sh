#!/bin/bash

# Set up environment.
source $(dirname $0)/sourceme

run_null "mvx_encoder_gen -c1 /dev/null"

# Gather up the coverage data.
shell gather_on_test.sh $SCRIPTDIR/coverage_mini.tar.gz

echo "Coverage mini done"

