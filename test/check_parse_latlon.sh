#!/bin/sh
# Copyright: CC0

PROG=./test_parse_latlon

check_success ()
{
    value=$1
    expected=$2
    result=$($PROG "$value")
    if [ "$result" != "$expected" ]; then
      echo "$result != $expected"
      exit 1
    fi
}

check_failure ()
{
    result=$($PROG "$1")
    if [ "$?" = "0" ]; then
      echo "Program unexpectedly succeeded: with $result"
      exit 1
    fi
}

# Various checks to work out what the current code actually does

check_failure "0.34"
# ATM no i18n support for using ',' as decimal separator in input string
#  it effectively splitting the number as if the comma was a space
#  so this returns "0.00000 34.00000"
#check_failure "12,34"
check_success "12, 34" "12.00000 34.00000"
check_failure "S 0.34"
check_failure "-0.02"
# Out of bounds lat/lon numbers not allowed
check_failure "367 56"
check_failure "167 91"
check_failure "-367 56"
check_failure "167 -91"
#
check_success "S34.56 E008.56" "-34.56000 8.56000"
# ATM Can not do this as it returns -8.56 -34.5600!
# Cardinals must be in order NS first then EW after
#check_success "E008.56 S34.56" "-34.56000 8.56000"
# DMM
check_success "N 34° 12.3456 W 123° 45.6789" "34.20576 -123.76131"
check_success "S 34° 12.3456 E 123° 45.6789" "-34.20576 123.76131"
check_success "S034° 12.3456 W003° 45.6789" "-34.20576 -3.76131"
# DMS
check_success "S034° 12 34 W003° 45 54" "-34.20944 -3.76500"
# Not sure if 'invalid' minute/second values should be accepted
# ATM they are converted anyway
#check_failure "S 034° 61 61 W 003° 61 61"
# DDD
check_success "S12.3456 W3.4567" "-12.34560 -3.45670"
check_success "N12.3456 E3.4567" "12.34560 3.45670"
# ATM Cardinal must have space between it and the number
#check_success "12.3456S 3.4567W" "-12.34560 -3.45670"
check_success "12.3456 S 3.4567 W" "-12.34560 -3.45670"

exit 0
