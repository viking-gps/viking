#!/bin/sh

# Usage: extract-authors.sh x.y.z master

git shortlog --summary "$@" | awk '{$1="";print}'
