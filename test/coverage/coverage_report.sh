#!/bin/bash

GIT_TOP=$(dirname $(dirname $(dirname $0)))
DRIVER_DIR="$GIT_TOP/driver"
COVERAGE_DIR="$GIT_TOP/coverage"
SRC=$1

if [ -z "$SRC" ] ; then
  echo "Usage: $0 <coverage.tar.gz>" >&2
  exit 1
fi

# Create soft links from '<src>.gcno' to '._tmp_<src>.gcno'.
find $DRIVER_DIR -type l -name "*.gcno" -delete
find $DRIVER_DIR -name ".tmp_*.gcno" -exec bash -c 'ln -s ${0##*/} ${0/.tmp_/}' {} \;

# Unpack coverage data to driver directory.
tar -C $GIT_TOP -zxvf $SRC

# Run lcov
rm -rf $COVERAGE_DIR
mkdir $COVERAGE_DIR
lcov -c -o $COVERAGE_DIR/coverage.info --rc lcov_branch_coverage=1 --directory $DRIVER_DIR
lcov --rc lcov_branch_coverage=1 --remove $COVERAGE_DIR/coverage.info \
    '*/driver/arch/*' '*/driver/include/*' \
    '*/driver/dev/arch/*' '*/driver/dev/include/*' \
    '*/driver/if/arch/*' '*/driver/if/include/*' \
    '*/driver/if/v4l2/arch/*' '*/driver/if/v4l2/include/*' > $COVERAGE_DIR/coverage.tmp
genhtml -o $COVERAGE_DIR $COVERAGE_DIR/coverage.tmp --branch-coverage
