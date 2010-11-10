#!/bin/sh
#
# Convert "git show" output as po/ChangeLog fragment
#
# Usage:
# git show | sh maintainer/git-diff2po_ChangeLog.sh
#

#
# viking -- GPS Data and Topo Analyzer, Explorer, and Manager
#
# Copyright (C) 2009-2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

awk '/^\+\+\+/{filename=substr($2,6);date=""} /^\+"PO-Revision-Date: /{date=$2} /"Last-Translator: /{translator=substr($0, 20, length($0)-22); if (date!="")printf "%s %s %s\t* %s: updated\n", date, translator, email, filename} ' "$@" \
| sort -r \
| sed -e 's/$/\n/' -e 's/\t/\n\t/'
