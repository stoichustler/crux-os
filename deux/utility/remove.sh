#!/usr/bin/env bash

files=`find . -name $1`

for file in $files; do
	echo "-- removing $file"
	rm -rf $file
done
