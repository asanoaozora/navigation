#!/bin/sh

# @licence app begin@
# SPDX-License-Identifier: MPL-2.0
#
# \copyright Copyright (C) 2013-2014, PCA Peugeot Citroen
#
# \file run
#
# \brief This file is part of the Build System.
#
# \author Philippe Colliot <philippe.colliot@mpsa.com>
#
# \version 1.0
#
# This Source Code Form is subject to the terms of the
# Mozilla Public License (MPL), v. 2.0.
# If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#
# For further information see http://www.genivi.org/.
#
# List of changes:
# 
# <date>, <name>, <description of change>
#
# @licence end@

export LD_LIBRARY_PATH=$DBUS_LIB_PATH/lib

CURDIR=$PWD

POI_MANAGER_CLIENT_DIR=$CURDIR
POI_MANAGER_CLIENT_BIN_DIR=$POI_MANAGER_CLIENT_DIR/bin
POI_MANAGER_SERVER_DIR=$CURDIR/../../../src/poi-service/poi-manager-server
POI_MANAGER_SERVER_BIN_DIR=$POI_MANAGER_SERVER_DIR/bin
RESOURCE=$POI_MANAGER_SERVER_DIR/../resource
echo '------------------------start the proof of concept------------------------'
cp $RESOURCE/poi-database-managed.db ./bin
COMMONAPI_DEFAULT_CONFIG=$RESOURCE/commonapi4dbus.ini \
COMMONAPI_DBUS_DEFAULT_CONFIG=$RESOURCE/commonapi-dbus.ini \
$POI_MANAGER_SERVER_BIN_DIR/poi-manager-server -f $POI_MANAGER_CLIENT_BIN_DIR/poi-database-managed.db &
COMMONAPI_DEFAULT_CONFIG=$RESOURCE/commonapi4dbus.ini \
COMMONAPI_DBUS_DEFAULT_CONFIG=$RESOURCE/commonapi-dbus.ini \
$POI_MANAGER_CLIENT_BIN_DIR/poi-manager-client -t 
kill -9 `ps -ef | egrep poi-manager-server | grep -v grep | awk '{print $2}'`



