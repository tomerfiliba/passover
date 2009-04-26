#!/bin/bash

cd module
python setup.py build

if [ $? -ne 0 ]; then
    echo "-- failed --"
    exit 1
fi

cp build/lib*/_passover.so ..

