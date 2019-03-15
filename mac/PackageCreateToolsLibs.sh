#!/usr/bin/env bash

rm -r $1 2> /dev/null
mkdir -p $1 || exit 1
cd $1		|| exit 1

#ignore first arg
shift

# iterate
while test ${#} -gt 0
do
	if [[ ${1} == :* ]]; then
		ln -sf "../PTBatcherGUI.app/Contents/MacOS/${1:1}" "."
	else
		ln -sf "../Hugin.app/Contents/MacOS/${1}" "."
	fi
	shift
done

