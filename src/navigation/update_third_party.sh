#!/bin/bash

positioning_version='d4c46f13019aefb11aebd0fc1210a29a46f0b521'
navit_version='f93a8df9c0f7967a40b12293873da6d5b53c1133'

echo "version of positioning is: $positioning_version"
echo "version of navit is: $navit_version"

echo "This script deletes and reloads all the third party software"
read -r -p "Are you sure ? [y/N] " input

case "$input" in
	[y/Y])
	./update_navit.sh $navit_version
	./update_positioning.sh $positioning_version
	echo "Please rebuild with at least -c option"
	;;
	*)
	exit 1
	;;
esac





