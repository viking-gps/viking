#!/bin/sh
# Copyright: CC0
test_string="Test hash of this string"

expected_result=$(echo "$test_string" | tr -d '\n' | md5sum | awk '{print $1}')
my_result=$(./test_md5_hash "$test_string")

if [ "$my_result" = "$expected_result" ]; then
	# Success
	exit 0
else
	exit 1
fi
