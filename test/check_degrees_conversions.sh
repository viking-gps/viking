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

check_format ()
{
  result=$(LC_ALL=C ./degrees_converter "$1") || exit 1
  result=$(printf '%s\n' "$result" | cut -d' ' -f6-)
  if [ "$result" != "$2" ]
  then
    echo "$1 -> $result != $2"
    exit 1
  fi
}

degree=$(printf '\302\260')
check_format 10.1 "N10${degree}6.000000' E10${degree}6.000000' N10${degree}6'0.0000\" E10${degree}6'0.0000\""
check_format 51.3 "N51${degree}18.000000' E51${degree}18.000000' N51${degree}18'0.0000\" E51${degree}18'0.0000\""
check_format -10.1 "S10${degree}6.000000' W10${degree}6.000000' S10${degree}6'0.0000\" W10${degree}6'0.0000\""
check_format 12.999999999 "N13${degree}0.000000' E13${degree}0.000000' N13${degree}0'0.0000\" E13${degree}0'0.0000\""
check_format 12.34567 "N12${degree}20.740200' E12${degree}20.740200' N12${degree}20'44.4120\" E12${degree}20'44.4120\""

exit 0
