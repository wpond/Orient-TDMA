#! /bin/sh

gcc demo.c server.c rs232.c -o bs.exe -Wall -LC:\Windows\System32 -lwsock32
rc=$?
if [[ $rc != 0 ]] ; then
	echo "unable to build"
    exit $rc
fi

echo "build complete"
bs
