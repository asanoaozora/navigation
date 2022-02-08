#!/usr/bin/python3
# -*- coding: latin-1 -*-

"""
**************************************************************************
* @licence app begin@
* SPDX-License-Identifier: MPL-2.0
*
* @copyright Copyright (C) 2022-2025, STELLANTIS
*
* @file utilities.py
*
* @brief Utilities used by other test scripts
*
* @author Philippe Colliot <philippe.colliot@stellantis.com>
*
* @version 1.1
*
* This Source Code Form is subject to the terms of the
* Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with
# this file, You can obtain one at http://mozilla.org/MPL/2.0/.
* List of changes:
*
* @licence end@
**************************************************************************
"""
from readline import get_current_history_length
import subprocess
import genivi


# Turn selection criteria values to their corresponding string description
def selection_criterion_to_string(selection_criterion):
    return_value = ''
    if selection_criterion == genivi.LATITUDE:
        return_value += 'Latitude'
    elif selection_criterion == genivi.LONGITUDE:
        return_value += 'Longitude'
    elif selection_criterion == genivi.COUNTRY:
        return_value += 'Country'
    elif selection_criterion == genivi.STATE:
        return_value += 'State'
    elif selection_criterion == genivi.CITY:
        return_value += 'City'
    elif selection_criterion == genivi.TOWN_CENTER:
        return_value += 'City center'
    elif selection_criterion == genivi.ZIPCODE:
        return_value += 'ZipCode'
    elif selection_criterion == genivi.STREET:
        return_value += 'Street'
    elif selection_criterion == genivi.HOUSE_NUMBER:
        return_value += 'House number'
    elif selection_criterion == genivi.CROSSING:
        return_value += 'Crossing'
    elif selection_criterion == genivi.FULL_ADDRESS:
        return_value += 'Full address'
    else:
        return_value += str(selection_criterion)

    return return_value


def checkDltProcess():
    output = subprocess.check_output(['ps', '-A'])
    if 'dlt' not in str(output):
        return False
    return True
