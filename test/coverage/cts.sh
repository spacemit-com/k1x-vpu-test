#!/system/bin/sh

function on_exit()
{
    pkill -P $$
    exit 1
}

# Split a <suite>:<class>:<test> into an array.
# Android takes a very long time spawning processes, so this function has been
# implemented without any
function split()
{
  local tmp="$1:"
  local start=0
  local current=0
  local m=1

  while [ $current -lt ${#tmp} ]
  do
    if [ ${tmp:$current:1} == ':' ]
    then
      if [ $m -eq $2 ]
      then
        echo -n ${tmp:$start:((current-start))}
        return
      fi

      ((m++))
      ((start=current+1))
    fi

    ((current++))
  done
}

# Get the 'suite' from the tupel.
function get_suite()
{
    split $1 1
}

# Get the 'class' from the tupel.
function get_class()
{
    split $1 2
}

# Get the 'test' from the tupel.
function get_test()
{
    split $1 3
}

# Detect if a filter matches suite:class:test.
# $1: Match filter.
# $2: Test tuple.
# Return: 0 if filter match. Else 1.
function match()
{
    # Filter is a colon delimited string.
    # An empty filter is treated as matching any string.
    # <suite>:<class>:<test>
    for i in 1 2 3
    do
        local m=$(split $1 $i)
        local t=$(split $2 $i)

        # Check if string does not match filter.
        if [ ! -z ${m} ] && [ "${t}"  == "${t/$m/}" ]
        then
            return 1
        fi
    done

    return 0
}

# Detect if test shall be filtered or not.
# $1: include filters
# $2: exclude filters
# $3: test tuple
# Return: 1 if test should be excluded. Else 0.
function filter()
{
    local include=$1
    local exclude=$2
    local tuple=$3

    # Exclude test if it matches any of the exclude filters.
    for e in ${exclude}
    do
        match ${e} ${tuple}

        if [ $? -eq 0 ]
        then
            return 1
        fi
    done

    # No include filter matches all tuples.
    if [ -z ${include} ]
    then
        return 0
    fi

    # Include test if it matches any include filter.
    for i in ${include}
    do
        match ${i} ${tuple}
        if [ $? -eq 0 ]
        then
            return 0
        fi
    done

    return 1
}

# Clear log.
function clear_log()
{
    dmesg -c > /dev/null
    logcat -c
    mvx_logd -c /sys/kernel/debug/amvx_if/log/drain/ram0/msg
}

# Start logging.
# $1: Log directory
function start_log()
{
    trap 'pkill -P $BASHPID' INT

    local logdir=$1
    mkdir -m 755 ${logdir}

    dmesg --follow > ${logdir}/dmesg.log &
    logcat > ${logdir}/logcat.log &
    mvx_logd -f text --follow > ${logdir}/fw.log &

    wait

    cat ${logdir}/logcat.log | grep ' E MVXOMX' > ${logdir}/logcat.err
    if [ ! -s ${logdir}/logcat.err ]
    then
        rm ${logdir}/logcat.err
    fi

    cat ${logdir}/dmesg.log | grep -E 'BUG:|KASAN:|UBSAN:' > ${logdir}/dmesg.err
    if [ ! -s ${logdir}/dmesg.err ]
    then
        rm ${logdir}/dmesg.err
    fi
}

function list_classes()
{
    local suite=$1
    am instrument -w -e log true android.${suite}.cts/android.support.test.runner.AndroidJUnitRunner | sed -n -r "s/android\.${suite}\.cts\.([A-Za-z0-9_]+):.*/\1/p"
}

function list_tests()
{
    local suite=$1
    local class=$2

    logcat -c
    am instrument -w -e log true -e class android.${suite}.cts.${class} android.${suite}.cts/android.test.InstrumentationTestRunner &> /dev/null
    logcat -d | sed -n -r 's/.* TestRunner: started: ([A-Za-z0-9_]+).*/\1/p'
}

# Start logging.
# $1: Log directory
# $2: Suite
# $3: Class
# $4: Test
function run_test()
{
    local logdir=$1
    local suite=$2
    local class=$3
    local test=$4

    clear_log
    start_log ${logdir}/${suite}_${class}_${test} &
    lpid=$!

    am instrument -w -e class android.${suite}.cts.${class}\#${test} android.${suite}.cts/android.test.InstrumentationTestRunner

    kill -2 $lpid
    wait
}

function help()
{
    echo "Usage: $0 [OPTIONS] [TESTS]"
    echo
    echo "A test is selected with <suite>:<class>:<test>."
    echo "To select a class only the test argument is omitted like <suite>:<class>."
    echo "For filters the suite and class might not be relevant and can be excluded like ::<test>."
    echo
    echo "Positional arguments:"
    echo "    TESTS   Select which suite, class or tests to run."
    echo
    echo "Optional arguments:"
    echo "    -i <DESC>   Select which suite/class/test to include."
    echo "                '::CodecBasic' will only filter tests that match 'CodecBasic'."
    echo "                ':Encode' or ':Encode:' will only filter classes that match 'Encode'"
    echo "    -e <DESC>   Select which suite/class/test to exclude."
    echo "                Exclusion takes precidence over inclusion."
    echo "    -o OFFSET   Skip first OFFSET tests."
    echo "                Default 0"
    echo "    -m MAX      Run MAX test starting from OFFSET."
    echo "                Default -1"
    echo
    echo "Examples:"
    echo "    # Run testCodecBasicH264."
    echo "    $0 media:DecoderTest:testCodecBasicH264"
    echo
    echo "    # Run all tests in DecoderTest that match 'CodecBasic'."
    echo "    $0 -i ::CodecBasic media:DecoderTest"
    echo
    echo "    # Run all tests in VideoEncoderTest but exclude tests that match 'Goog'."
    echo "    $0 -e ::Goog media:VideoEncoderTest"
    echo

    exit 1
}

trap on_exit INT
umask 022
logcat -G 1m

# Parse command line arguments.
args=()
include=()
exclude=()
offset=1
max=1000000

while [ $# -gt 0 ]
do
    case $1 in
    -i)
        include=(${include[@]} "$2")
        shift
        ;;
    -e)
        exclude=(${exclude[@]} "$2")
        shift
        ;;
    -o)
        offset=($2)
        shift
        ;;
    -m)
        max=($2)
        shift
        ;;
    -h)
        help
        ;;
    -*)
        echo "Error: Unsupported argument '$1'."
        help
        ;;
    *)
        args=($args "$1")
        ;;
    esac

    shift
done

# Create log directory.
logdir="/log/$(date +%Y%m%d_%H%M%S)"
mkdir -m 755 -p ${logdir}

count=0
for arg in ${args[*]}
do
    # Extract suite from argument.
    suite=$(get_suite ${arg} 0)
    if [ -z ${suite} ]
    then
        echo "Error: Test suite must be defined in '${arg}'."
        continue
    fi

    # Extract class from argument. If no class was found, then auto detect all classes in suite.
    classes=$(get_class ${arg})
    if [ -z ${classes} ]
    then
        classes=$(list_classes ${suite})
    fi

    for class in ${classes}
    do
        # Extract test from argument. If no test was found, then auto detect all tests in suite.
        tests=$(get_test ${arg})
        if [ -z ${tests} ]
        then
            tests=$(list_tests ${suite} ${class})
        fi

        for test in ${tests}
        do
            count=$(($count + 1))

            if [ ${count} -lt ${offset} ] || [ ${count} -ge $((${offset} + ${max})) ]
            then
                echo "SKIPPING ${count} ${suite}:${class}:${test}"
                continue
            fi

            # Should test be excluded?
            filter "${include[*]}" "${exclude[*]}" ${suite}:${class}:${test}
            if [ $? -ne 0 ]
            then
                    echo "SKIPPING ${count} ${suite}:${class}:${test}"
                    continue
            fi

            echo "RUNNING ${count} ${suite}:${class}:${test}"
            run_test ${logdir} ${suite} ${class} ${test}
        done
    done
done
