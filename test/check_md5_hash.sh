#!/bin/sh

test_string="Test hash of this string"

expected_result=$(echo -n "$test_string" | md5sum | awk '{print $1}')
my_result=$(./test_md5_hash "$test_string")

if [ "$my_result" = "$expected_result" ]; then
	# Success
	exit 0
else
	exit 1
fi
