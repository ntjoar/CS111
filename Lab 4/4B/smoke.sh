#!/bin/bash

echo | ./lab4b --bogus &> /dev/null
if [[ $? -ne 1 ]]
then
    echo "--bogus failed"
    exit 1
fi

./lab4b --period=3 --scale=F --log=LOG <<-EOF
SCALE=C
PERIOD=5
STOP
START
LOG 
OFF
EOF

if [[ $? -ne 0 ]]
then
	echo "exit code for correct input incorrect"
    exit 1
fi

if [ ! -s LOG ]
then
	echo "log file cannot be found"
    exit 1
fi

grep "PERIOD=5" LOG &> /dev/null; \
if [[ $? -ne 0 ]]
then
    echo "PERIOD was not logged"
    exit 1
fi

grep "SCALE=C" LOG &> /dev/null; \
if [[ $? -ne 0 ]]
then
	echo "SCALE was not logged"
    exit 1
fi

grep "LOG" LOG &> /dev/null; \
if [[ $? -ne 0 ]]
then
    echo "LOG was not logged"
    exit 1
fi

grep "SHUTDOWN" LOG &> /dev/null; \
if [[ $? -ne 0 ]]
then
    echo "SHUTDOWN was not logged"
    exit 1
fi

rm -f LOG

echo "Smoke test passed"
exit 0