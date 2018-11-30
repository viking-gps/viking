#!/bin/sh
# Copyright: CC0

PROG=./test_decimal_output

check_success ()
{
    value=$1
    expected=$2
    result=`$PROG "$value"`
    if [ "$result" != "$expected" ]; then
      echo "$result != $expected"
      exit 1
    fi
}

# Output on PC AMD64, but probably the same/similar for other systems
# Viking < 1.7 gave: 0.34000000000000002
check_success "0.34" "0.34"
# Viking < 1.7 gave: 6.9999999999999994e-05
check_success "0.00007" "0.00007"
# Unreleased Viking < 1.7 including the SF Bug#22 fix gave: 6e-7
check_success "0.0000006" "0.0000006"

# Check various pathological cases...
check_success "100000000" "100000000"
check_success "0.00000000000000000000000000000000123" "0"
check_success "0.000000000000000000000123" "0"
check_success "-1.2345678901234567890234567890" "-1.2345678901234567"
check_success "1.2345678901234567890234567890" "1.2345678901234567"
check_success "-0.000000000000000000012345678" "-0.00000000000000000001"
check_success "0.00000000000012345678" "0.00000000000012345678"
check_success "123456789.01234567" "123456789.01234567"
check_success "000012345" "12345"
# Badly truncates, but doesn't crash - not a sensible number anyway!
check_success "12340000000000000000000000000000000" "12340000000000000000000"

exit 0
