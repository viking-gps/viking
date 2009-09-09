#!/bin/sh
#
usage()
{
  echo "Usage: $0 x.y.z master"
}

gen_log()
{
  git log --date=short "--format=* %s" "$@"
}

echo "Viking x.y.z (`date +%Y-%m-%d`)"
echo "New features since x.y.z"
gen_log "$@" | grep -i -v fix
echo
echo "Fixes since x.y.z"
gen_log "$@" | grep -i fix
echo

