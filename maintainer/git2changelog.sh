#:!bin/sh
#
# ChangeLog file generator
#
# This script parse the git history and format it like a ChangeLog file.
# This script is called at distribution tile (see "make dist").
#
git log --date=short "--format=%cd %an <%aE>:%n%x09* %s" "$@" \
| awk '/^[^\t]/{if(previous==$0)next;previous=$0}{print}' \
| sed '/^[^\t]/s/\([^ ]*\) \(.*\)/\n\1\n\2/'
