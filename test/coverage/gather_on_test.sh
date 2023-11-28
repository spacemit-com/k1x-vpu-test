#!/bin/bash

function cleanup()
{
  if [ -e "$TEMPDIR" ] ; then
    rm -rf $TEMPDIR
  fi
}

function error()
{
  echo "Error: Failed to create coverage archive."
  exit 1
}

trap cleanup EXIT
trap error ERR

DEST=$1
if [ -z "$DEST" ] ; then
  echo "Usage: $0 <output.tar.gz>" >&2
  exit 1
fi

# Create temporary directory that the gcda files will be copied to. It is
# necessary to copy the files with 'cat', or else they will be stored empty
# in the archive.
TEMPDIR=$(mktemp -d)

# Try to find the driver subdirectory in the coverage file tree.
# Execute one command per line to make sure to trap errors.
DRIVER_DIR=$(find /sys/kernel/debug/gcov -type d | grep 'driver/if$')
DRIVER_DIR=$(dirname $DRIVER_DIR)
DRIVER_DIR=$(dirname $DRIVER_DIR)
cd $DRIVER_DIR

echo "Creating directories"
find -type d -exec sh -c 'mkdir -p $0/$1' $TEMPDIR {} \;

echo "Coping gcda files"
find -name '*.gcda' -exec sh -c 'cat $1 > $0/$1' $TEMPDIR {} \;

# Change back to original directory to make sure that $DEST is written
# to the correct directory.
cd - > /dev/null

echo "Creating archive"
tar czf $DEST -C $TEMPDIR .

echo "$DEST successfully created"
