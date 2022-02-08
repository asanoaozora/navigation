#!/usr/bin/python3
# -*- coding: latin-1 -*-

"""
**************************************************************************
* @licence app begin@
* SPDX-License-Identifier: MPL-2.0
*
* @copyright Copyright (C) 2014, Alpine Electronics R&D Europe GmbH
* @copyright Copyright (C) 2022-2025, STELLANTIS
*
* @file test-location-input.py
*
* @brief This simple test shows how the location input
*              could be easily tested using a python script
*
* @author Stephan Wiehr <stephan.wiehr@alpine.de>
* @author Philippe Colliot <philippe.colliot@stellantis.com>
*
* @version 1.1
*
* This Source Code Form is subject to the terms of the
* Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with
# this file, You can obtain one at http://mozilla.org/MPL/2.0/.
* List of changes:
* 04-02-2016, Philippe Colliot, Update to the new API ('i' for enumerations and 'yv' for variants), add status handler
*
* @licence end@
**************************************************************************
"""

import dbus
from gi.repository import GLib
import dbus.mainloop.glib
import xml.dom.minidom
import argparse
import sys
import os.path
import genivi
from utilities import (
    checkDltProcess,
    selection_criterion_to_string
)
try:
    from dltTrigger import *
except ImportError:
    g_dltAvailable = False
else:
    g_dltAvailable = True


# constants used into the script
TIME_OUT = 20000

# Default size of the list
WINDOW_SIZE = 20


def vprint(text):
    global g_verbose
    if g_verbose is True:
        print(text)


# Prepare a dictionary array for pretty printing

def dictionary_array_to_string(dict_array, linefeed, offset=0):
    return_value = ''
    i = offset
    for item in dict_array:
        return_value += str(i) + '. ' + str(dictionary_to_string(item))
        if i < offset + len(dict_array) - 1:
            return_value += linefeed
        i += 1

    return return_value


# Prepare a dictionary for pretty printing
# NB: the value is supposed to be [UInt8, Variant], according to the DBus '(yv)', used by CommonAPI
def dictionary_to_string(dictionary):
    return_value = ''
    i = 0
    for key in dictionary.keys():
        value = dictionary[key][1]
        return_value += selection_criterion_to_string(key) + ' = ' + str(value)
        i += 1
        if i < len(dictionary):
            return_value += ', '

    return return_value


# Prepare a selection criteria array for pretty printing

def selection_criteria_array_to_string(selection_criterion_array):
    return_value = ''
    i = 0
    for item in selection_criterion_array:
        return_value += selection_criterion_to_string(item)
        i += 1
        if i < len(selection_criterion_array):
            return_value += ', '

    return return_value


def print_current_context():
    global g_current_selection_criterion
    vprint('\tACTIVE CONTEXT: selection criterion = ' + selection_criterion_to_string(g_current_selection_criterion)
           + ', search string = \'' + g_entered_search_string + '\'')


def change_selection_criterion(selection_criterion):
    global g_current_selection_criterion
    global g_session_handle
    global g_location_input_interface
    g_current_selection_criterion = selection_criterion
    g_location_input_interface.SetSelectionCriterion(dbus.UInt32(g_session_handle), dbus.UInt32(location_input_handle),
                                                   dbus.UInt16(g_current_selection_criterion))


# Full string search
def full_string_search(handle, search_string):
    global g_entered_search_string
    global found_exact_match
    global g_current_selection_criterion
    global g_location_input_interface
    global g_session_handle

    g_entered_search_string = search_string
    found_exact_match = 1  # Force exact match for full string search
    vprint('\nACTION: Full string search, selection criterion = '
           + selection_criterion_to_string(g_current_selection_criterion) + ', trying \'' + search_string + '\'')
    g_location_input_interface.Search(dbus.UInt32(g_session_handle), dbus.UInt32(handle), dbus.String(search_string),
                                    dbus.UInt16(20))


def evaluate_address(address, guidable):
    test_passed = 0
    print('\nAddress complete!\nEvaluating...')
    if COUNTRY_STRING[current_address_index] == '':
        test_passed = 1
    elif address[genivi.COUNTRY][1] == COUNTRY_STRING[current_address_index]:
        print('Country\t\t\t-> ok (' + address[genivi.COUNTRY][1] + ')')
        if CITY_STRING[current_address_index] == '':
            test_passed = 1
        elif address[genivi.CITY][1] == CITY_STRING[current_address_index]:
            print('City\t\t\t-> ok (' + address[genivi.CITY][1] + ')')
            if STREET_STRING[current_address_index] == '':
                test_passed = 1
            elif address[genivi.STREET][1] == STREET_STRING[current_address_index]:
                print('Street\t\t\t-> ok (' + address[genivi.STREET][1] + ')')
                if HOUSE_NUMBER_STRING[current_address_index] == '':
                    test_passed = 1
                elif address[genivi.HOUSE_NUMBER][1] == HOUSE_NUMBER_STRING[current_address_index]:
                    print('House number\t-> ok (' + address[genivi.HOUSE_NUMBER][1] + ')')
                    test_passed = 1

    if guidable == 1:
        if test_passed == 1:
            print('TEST PASSED')
        else:
            print('wrong address')
            exit(1)
    else:
        print('non-guidable address')
        exit(1)
    address_index = current_address_index + 1
    if address_index < len(COUNTRY_STRING):
        startSearch(address_index)
    else:
        print('END OF THE TEST')
        exit(0)


# Signal receiver

# Handler for ContentUpdated callback

def search_status_handler(handle, status):
    global g_session_handle
    global g_location_input_interface

    vprint('\n::Search status ' + str(int(status)))
    if status == genivi.FINISHED:
        g_location_input_interface.RequestListUpdate(dbus.UInt32(g_session_handle), dbus.UInt32(handle),
                                                   dbus.UInt16(0),
                                                   dbus.UInt16(WINDOW_SIZE))


# Spell search
def spell_search(handle, entered_string, search_string, valid_characters, first=0):
    global g_entered_search_string
    global g_current_selection_criterion
    global g_location_input_interface
    global g_session_handle

    vprint('-> SpellSearch - entered \'' + entered_string + '\' target \'' + search_string + '\'')

    if str(search_string) != str(entered_string):
        found = str(search_string).lower().find(str(entered_string).lower())
        if found == 0:
            is_valid = -1
            if first == 0:
                spell_character = search_string[len(entered_string)]
                is_valid = valid_characters.find(spell_character)
            else:
                spell_character = ''
                is_valid = 0
            if is_valid != -1:
                g_entered_search_string = entered_string + spell_character
                vprint('\nACTION: Spell search, selection criterion = '
                       + selection_criterion_to_string(g_current_selection_criterion) + ', trying \'' + spell_character
                       + '\'')
                g_location_input_interface.Spell(dbus.UInt32(g_session_handle), dbus.UInt32(handle),
                                               dbus.String(spell_character), dbus.UInt16(20))
            else:
                print('Target character can not be entered')
                exit(1)
        else:
            print('Unexpected completion')
            exit(1)
    else:
        print('Full spell match')


def content_updated_handler(handle, guidable, available_selection_criteria, address):
    global target_search_string
    global g_entered_search_string
    global g_current_selection_criterion

    vprint('\n::ContentUpdated for LocationInputHandle ' + str(int(handle)))
    print_current_context()
    vprint('\tGuidable = ' + str(guidable))
    vprint('\tAvailable selection criteria = ' + selection_criteria_array_to_string(available_selection_criteria))
    vprint('\tADDRESS: ' + dictionary_to_string(address))

    search_mode = -1

    if g_current_selection_criterion == genivi.COUNTRY:
        change_selection_criterion(genivi.CITY)
        target_search_string = CITY_STRING[current_address_index]
        search_mode = city_search_mode
    elif g_current_selection_criterion == genivi.CITY:
        change_selection_criterion(genivi.STREET)
        target_search_string = STREET_STRING[current_address_index]
        search_mode = street_search_mode
    elif g_current_selection_criterion == genivi.STREET:
        change_selection_criterion(genivi.HOUSE_NUMBER)
        target_search_string = HOUSE_NUMBER_STRING[current_address_index]
        search_mode = house_number_search_mode
    elif g_current_selection_criterion == genivi.HOUSE_NUMBER:
        target_search_string = ''

    g_entered_search_string = ''

    if target_search_string == '':
        evaluate_address(address, guidable)
    elif search_mode == 0:
        spell_search(handle, g_entered_search_string, target_search_string, '', 1)
    elif search_mode == 1:
        full_string_search(handle, target_search_string)
    else:
        print('Invalid search mode')
        exit(1)


# Handler for SpellResult callback
def spell_result_handler(handle, unique_string, valid_characters, full_match):
    global g_entered_search_string
    global spell_next_character
    global found_exact_match
    global available_characters

    vprint('\n::SpellResult for LocationInputHandle ' + str(int(handle)))
    if unique_string != g_entered_search_string:
        vprint('\tAUTOCOMPLETE: \'' + g_entered_search_string + '\' -> \'' + unique_string + '\'')
    g_entered_search_string = unique_string
    available_characters = valid_characters
    print_current_context()
    vprint('\tUnique string = \'' + unique_string + '\'')
    vprint('\tValid Characters = \'' + valid_characters + '\'')
    vprint('\tFull Match = ' + str(full_match))

    if len(valid_characters) == 1:
        if str(valid_characters[0]) == u'\x08':
            print('Dead end spelling')
            exit(1)

    if str(g_entered_search_string) == str(target_search_string):
        found_exact_match = 1

    spell_next_character = 1


# Handler for SearchResultList callback

def search_result_list_handler(handle, total_size, window_offset, window_size, result_list_window):
    global spell_next_character
    global found_exact_match
    global g_current_selection_criterion
    global g_location_input_interface
    global g_session_handle

    vprint('\n::SearchResultList for LocationInputHandle ' + str(int(handle)))
    print_current_context()
    vprint('\tTotal size = ' + str(int(total_size)) + ', Window offset = ' + str(int(window_offset))
           + ', Window size = ' + str(int(window_size)))
    vprint('\t' + dictionary_array_to_string(result_list_window, '\n\t', window_offset))

    if found_exact_match == 1:
        found_exact_match = 0
        i = 0
        for address in result_list_window:
            if str(address[g_current_selection_criterion][1]) == target_search_string:
                vprint('\nACTION: Found exact match, selecting \'' + str(address[g_current_selection_criterion][1])
                       + '\' (Session ' + str(int(g_session_handle)) + ' LocationInputHandle ' + str(int(handle)) + ')')
                g_location_input_interface.SelectEntry(dbus.UInt32(g_session_handle), dbus.UInt32(handle), dbus.UInt16(i))
                break
            i += 1
        if i == window_size:
            vprint('\nACTION: Found exact match, searching in next page (Session ' + str(int(g_session_handle))
                   + ' LocationInputHandle ' + str(int(handle)) + ')')
            g_location_input_interface.RequestListUpdate(dbus.UInt32(g_session_handle), dbus.UInt32(handle),
                                                       dbus.UInt16(window_offset + window_size),
                                                       dbus.UInt16(window_size))
    elif total_size == 1:
        selection_name = result_list_window[0][g_current_selection_criterion]
        if selection_name[1] == target_search_string:
            vprint('\nACTION: Single entry list, selecting \'' + selection_name[1]
                   + '\' (Session ' + str(int(g_session_handle)) + ' LocationInputHandle ' + str(int(handle)) + ')')
            g_location_input_interface.SelectEntry(dbus.UInt32(g_session_handle), dbus.UInt32(handle), dbus.UInt16(0))
        else:
            print('Unexpected single result list')
            exit(1)
    elif spell_next_character == 1:
        spell_next_character = 0
        spell_search(handle, g_entered_search_string, target_search_string, available_characters)


# Timeout
def timeout():
    print('Timeout Expired\n')
    exit(1)


def exit(value):
    global g_exit
    global g_session_handle
    global g_location_input_interface

    g_exit = value
    error = g_location_input_interface.DeleteLocationInput(dbus.UInt32(g_session_handle), dbus.UInt32(location_input_handle))
    print('Delete location input: ' + str(int(error)))
    error = session_interface.DeleteSession(dbus.UInt32(g_session_handle))
    print('Delete session: ' + str(int(error)))
    if g_dltAvailable is True:
        stopTrigger(test_name)
    loop.quit()


def startSearch(address_index):
    global g_entered_search_string
    global spell_next_character
    global found_exact_match
    global available_characters
    global target_search_string
    global country_search_mode
    global current_address_index
    current_address_index = address_index
    g_entered_search_string = ''
    spell_next_character = 0
    found_exact_match = 0
    available_characters = ''
    target_search_string = COUNTRY_STRING[current_address_index]

    change_selection_criterion(genivi.COUNTRY)
    if country_search_mode == 0:
        spell_search(location_input_handle, g_entered_search_string, target_search_string, available_characters, 1)
    elif country_search_mode == 1:
        full_string_search(location_input_handle, target_search_string)


if __name__ == '__main__':
    global g_verbose
    global g_current_selection_criterion
    global g_session_handle
    global g_location_input_interface

    #  name of the test
    test_name = "location input"

    parser = argparse.ArgumentParser(description='Location input Test for navigation PoC and FSA.')
    parser.add_argument('-l', '--loc', action='store', dest='locations', help='List of locations in xml format')
    parser.add_argument("-v", "--verbose", action='store_true', help='Print the whole log messages')
    parser.add_argument('-a', '--address', action='store', dest='host', help='Set remote host address')
    parser.add_argument('-p', '--prt', action='store', dest='port', help='Set remote port number')
    args = parser.parse_args()
    if args.locations is None:
        print('location file is missing')
        print('Test ' + test_name + ' FAILED')
        sys.exit(1)
    else:
        if not os.path.isfile(args.locations):
            print('file not exists')
            print('Test ' + test_name + ' FAILED')
            sys.exit(1)
        try:
            DOMTree = xml.dom.minidom.parse(args.locations)
        except OSError as e:
            print('Test ' + test_name + ' FAILED')
            sys.exit(1)
        location_set = DOMTree.documentElement

    print('LocationInput Test')

    g_dltAvailable = checkDltProcess()

    # exit value on loop quit
    g_exit = 0

    g_verbose = args.verbose

    # Search mode (0 = Spell, 1 = Full string search)
    country_search_mode = 1
    city_search_mode = 0
    street_search_mode = 1  # set to full because of a bug to be fixed in the plug-in
    house_number_search_mode = 1

    print("Area : %s" % location_set.getAttribute("area"))

    locations = location_set.getElementsByTagName("location")

    # List of addresses
    COUNTRY_STRING = list()
    CITY_STRING = list()
    STREET_STRING = list()
    HOUSE_NUMBER_STRING = list()
    for location in location_set.getElementsByTagName("location"):
        COUNTRY_STRING.append(location.getElementsByTagName("country")[0].childNodes[0].data)
        CITY_STRING.append(location.getElementsByTagName("city")[0].childNodes[0].data)
        STREET_STRING.append(location.getElementsByTagName("street")[0].childNodes[0].data)
        # HOUSE_NUMBER_STRING.append(location.getElementsByTagName("number")[0].childNodes[0].data)
        HOUSE_NUMBER_STRING.append('')  # there's a bug in the navigation core when the house number doesn't exist, so deactivated

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    # connect to session bus (remote or local)
    if args.host is not None:
        bus = dbus.bus.BusConnection("tcp:host=" + args.host + ",port=" + args.port)
    else:
        bus = dbus.SessionBus()

    # add signal receiver
    bus.add_signal_receiver(search_status_handler,
                            dbus_interface='org.genivi.navigation.navigationcore.LocationInput',
                            signal_name='SearchStatus')

    bus.add_signal_receiver(search_result_list_handler,
                            dbus_interface='org.genivi.navigation.navigationcore.LocationInput',
                            signal_name='SearchResultList')

    bus.add_signal_receiver(spell_result_handler,
                            dbus_interface='org.genivi.navigation.navigationcore.LocationInput',
                            signal_name='SpellResult')

    bus.add_signal_receiver(content_updated_handler,
                            dbus_interface='org.genivi.navigation.navigationcore.LocationInput',
                            signal_name='ContentUpdated')

    if g_dltAvailable is True:
        startTrigger(test_name)

    session = bus.get_object('org.genivi.navigation.navigationcore.Session', '/org/genivi/navigationcore')
    session_interface = dbus.Interface(session, dbus_interface='org.genivi.navigation.navigationcore.Session')

    # Get SessionHandle
    ret = session_interface.CreateSession(dbus.String('test location input'))
    g_session_handle = ret[1]
    print('Session handle = ' + str(g_session_handle))

    location_input_obj = bus.get_object('org.genivi.navigation.navigationcore.LocationInput', '/org/genivi/navigationcore')
    g_location_input_interface = dbus.Interface(location_input_obj, dbus_interface='org.genivi.navigation.navigationcore.LocationInput')

    # Get LocationInputHandle
    ret = g_location_input_interface.CreateLocationInput(dbus.UInt32(g_session_handle))
    location_input_handle = ret[1]
    print('LocationInput handle = ' + str(location_input_handle))

    attributes = g_location_input_interface.GetSupportedAddressAttributes()
    print('Initially supported address attributes = ' + selection_criteria_array_to_string(attributes))

    # Configuration
    current_address_index = 0
    g_entered_search_string = ''
    spell_next_character = 0
    found_exact_match = 0
    available_characters = ''
    target_search_string = ''

    g_current_selection_criterion = genivi.COUNTRY

    startSearch(0)

    # Main loop
    GLib.timeout_add(TIME_OUT, timeout)
    loop = GLib.MainLoop()
    loop.run()
    if g_exit == 1:
        print('Test ' + test_name + ' FAILED')
    else:
        print('Test ' + test_name + ' PASSED')
    sys.exit(g_exit)
