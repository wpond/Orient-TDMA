#! /bin/sh

gcc main.c rs232.c -o bs.exe -Wall
rc=$?
if [[ $rc != 0 ]] ; then
	echo "unable to build"
    exit $rc
fi

echo "build complete"
bs
