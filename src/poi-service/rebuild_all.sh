#!/bin/bash

debug="OFF"
franca="OFF"
dbus="ON"
commonapi_tools_option=""

while getopts df opt
do
	case $opt in
	d)
		debug="ON"
		;;
	f)
		franca="ON"
		dbus="OFF"
		;;
	\?)
		echo "Usage:"
		echo "$0 [-df]"
		echo "-d: Enable the debug messages"
		echo "-f: Build using the Franca interfaces"
		exit 1
	esac
done
set -e

if [ $franca="ON" ]
then
	if [ ! $COMMONAPI_TOOL_DIR ]
	then 
		echo 'Set the dir of the common api tools'
		echo 'export COMMONAPI_TOOL_DIR=<path>'
		exit 1
	fi

	if [ ! $COMMONAPI_DBUS_TOOL_DIR ]
	then 
		echo 'Set the dir of the common api dbus tools'
		echo 'export COMMONAPI_DBUS_TOOL_DIR=<path>'
		exit 1
	fi
	commonapi_tools_option="-DCOMMONAPI_DBUS_TOOL_DIR="$COMMONAPI_DBUS_TOOL_DIR" -DCOMMONAPI_TOOL_DIR="$COMMONAPI_TOOL_DIR
fi

echo 'delete the build folder'
rm -rf build

mkdir build
cd build

echo 'build poi-server'
cmake -DWITH_FRANCA_DBUS_INTERFACE=$franca -DWITH_DBUS_INTERFACE=$dbus $commonapi_tools_option -DWITH_DEBUG=$debug ../

make

