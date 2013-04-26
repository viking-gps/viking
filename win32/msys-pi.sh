# License: CC0
# Fix up MSYS install
# Don't ask for questions
# fix fstab to the values I need

. /etc/profile

echo
echo "Creating /etc/fstab with mingw mount bindings."
cat <<EOF>/etc/fstab
C:\MinGW /mingw
C:\Perl /opt/perl
EOF

# Stuff from original post MSYS install script
echo
echo    "        Normalizing your MSYS environment."
echo

for I in awk cmd echo egrep ex fgrep printf pwd rvi rview rvim vi view
do
  if [ -f /bin/$I. ]
  then
    echo You have script /bin/$I
    if [ -f /bin/$I.exe ]
    then
      echo Removing /bin/$I.exe
      rm -f /bin/$I.exe
    fi
  fi
done

for I in ftp ln make
do
  if [ -f /bin/$I.exe ] && [ -f /bin/$I. ]
  then
    echo You have both /bin/$I.exe and /bin/$I.
    echo Removing /bin/$I.
    rm -f /bin/$I.
  fi
done
