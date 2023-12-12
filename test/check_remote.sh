#!/bin/sh

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi

# Use full Viking program
# Same for normal and via distcheck
VIKING=../src/viking

CONFDIR=$XDG_RUNTIME_DIR/vikingtestconf
pid=`pgrep viking`
if [ "$pid" != "" ]; then
    echo "An instance of Viking is already running, thus skipping remote check tests"
    exit 0
fi

# This should fail returning an exit code of 1
$VIKING -c $CONFDIR -r $srcdir/Stonehenge.gpx
if [ $? != 1 ]; then
   exit 3
fi

$VIKING -c $CONFDIR &
pid=`pgrep viking`
# Give a little time for viking to startup fully before running test
sleep 1
result=0
# Check socket exists
if [ $(ss -lx | grep viking.command-socket | wc -l) != 1 ]; then
    result=2
else
    # Try sending the remote load command
    $VIKING -c $CONFDIR -r $srcdir/Stonehenge.gpx
    result=$?
fi
sleep 1
# Remove running instance & cleanup
kill $pid
rm $CONFDIR/* 2> /dev/null
rmdir $CONFDIR
exit $result
