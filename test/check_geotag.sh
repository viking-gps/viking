#!/bin/sh

check_success_read_lat ()
{
  expected=$1
  shift
  result=$(./geotag_read "$1" | cut -d' ' -f1)
  diff=$(echo "$result - $expected" | bc -l)
  if [ $diff != 0 ]; then
    echo "Expected=$expected but result is=$result"
    exit 1
  fi
}
 
check_success_read_lon ()
{
  expected=$1
  shift
  result=$(./geotag_read "$1" | cut -d' ' -f2)
  diff=$(echo "$result - $expected" | bc -l)
  if [ $diff != 0 ]; then
    echo "Expected=$expected but result is=$result"
    exit 1
  fi
}

# Read test

check_success_read_lat 51.881861 ViewFromCribyn-Wales-GPS.jpg
check_success_read_lon -3.419592 ViewFromCribyn-Wales-GPS.jpg

# Write and then re-read test
cp Stonehenge.jpg tmp.jpg
result=$(./geotag_write tmp.jpg)
if [ $result != 0 ]; then
  echo "geotag_write failure"
  exit 1
fi
check_success_read_lat 51.179489 tmp.jpg
check_success_read_lon -1.826217 tmp.jpg
rm tmp.jpg

exit 0
