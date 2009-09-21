#!/bin/sh
#
# Convert "git show" output as po/ChangeLog fragment
#
# Usage:
# git show | sh maintainer/git-diff2po_ChangeLog.sh
#
awk '/^\+\+\+/{filename=substr($2,6)} /^\+"PO-Revision-Date: /{date=$2} /^\+"Last-Translator: /{translator=substr($0, 20, length($0)-22); printf "%s %s %s\t* %s: updated\n", date, translator, email, filename} ' "$@" \
| sort -r \
| sed -e 's/$/\n/' -e 's/\t/\n\t/'
