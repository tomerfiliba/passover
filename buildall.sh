#!/bin/bash

cd module
rm -rf build
python setup.py build

if [ $? -ne 0 ]; then
    echo "-- FAILED --"
    exit 1
fi

cd ..
cp module/build/lib*/_passover.so .
echo "-- SUCCESS --"

