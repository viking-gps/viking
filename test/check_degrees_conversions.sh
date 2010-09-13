#!/bin/sh

check_success ()
{
  expected=$1
  shift
  for value in "$@"
  do
    result=`./degrees_converter "$value" | cut -d' ' -f3`
    diff=`echo "$result - $expected" | bc -l`
    if [ $diff != 0 ]
    then
      echo "$value -> $result != $expected"
      exit 1
    fi
  done
}

check_failure ()
{
  expected=$1
  shift
  for value in "$@"
  do
    result=`./degrees_converter "$value" | cut -d' ' -f3`
    diff=`echo "$result - $expected" | bc -l`
    if [ $diff = 0 ]
    then
      echo "$value -> $result = $expected"
      exit 1
    fi
  done
}

check_success 3.5 3.5 3°30 "3°30'0.00"
check_failure 3.5 3.6 3°40 "3°30'1.00"

exit 0
