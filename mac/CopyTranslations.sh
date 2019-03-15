#!/usr/bin/env bash

cd $1

mkdir -p mac
for file in *.gmo; do
	ext=${file%.*}
	mkdir -p "mac/${ext}.lproj"
	cp -p "$file" "mac/${ext}.lproj/hugin.mo"
done
mkdir -p "mac/en_US.lproj"
