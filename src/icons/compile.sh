#!/bin/bash
for i in *.png; do gdk-pixbuf-csource --name=${i%.png} --struct $i > $i.h; done
ls *.h|sed 's/\(.*\)/#include "icons\/\1"/g' > ../icons.h
