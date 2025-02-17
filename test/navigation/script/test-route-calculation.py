#!/usr/bin/python3

"""
**************************************************************************
* @licence app begin@
* SPDX-License-Identifier: MPL-2.0
*
* @copyright Copyright (C) 2014, XS Embedded GmbH, PCA Peugeot Citroen
* @copyright Copyright (C) 2022-2025, STELLANTIS
*
* \file test-route-calculation.py
*
* \brief This simple test shows how the route calculation
*              could be easily tested using a python script
*
* \author Marco Residori <marco.residori@xse.de>
* \author Philippe Colliot <philippe.colliot@stellantis.com>
*
* \version 1.1
*
* This Source Code Form is subject to the terms of the
* Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with
# this file, You can obtain one at http://mozilla.org/MPL/2.0/.
* List of changes:
* 04-02-2016, Philippe Colliot, Update to the new API ('i' for enumerations and 'yv' for variants)
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
    checkDltProcess
)
try:
    from dltTrigger import *
except ImportError:
    g_dltAvailable = False
else:
    g_dltAvailable = True


# constants used into the script
TIME_OUT = 15000


# add signal receivers
def catchall_route_calculation_signals_handler(routeHandle, status, percentage):
    global g_session_handle
    print('Route Calculation: ' + str(int(percentage)) + ' %')
    if int(percentage) == 100:
        # get route overview
        overview = g_routing_interface.GetRouteOverview(dbus.UInt32(g_route_handle), dbus.Array([dbus.Int32(genivi.TOTAL_DISTANCE), dbus.Int32(genivi.TOTAL_TIME)]))
        # retrieve distance
        totalDistance = dbus.Struct(overview[dbus.Int32(genivi.TOTAL_DISTANCE)])
        print('Total Distance: ' + str(totalDistance[1] / 1000) + ' km')
        totalTime = dbus.Struct(overview[dbus.Int32(genivi.TOTAL_TIME)])
        m, s = divmod(totalTime[1], 60)
        h, m = divmod(m, 60)
        print("Total Time: %d:%02d:%02d" % (h, m, s))
        # get route segments     GetRouteSegments(const uint32_t& routeHandle, const int16_t& detailLevel, const std::vector< DBusCommonAPIEnumeration >& valuesToReturn, const uint32_t& numberOfSegments, const uint32_t& offset, uint32_t& totalNumberOfSegments, std::vector< std::map< DBusCommonAPIEnumeration, DBusCommonAPIVariant > >& routeSegments)
        valuesToReturn = [dbus.Int32(genivi.ROAD_NAME),
            dbus.Int32(genivi.START_LATITUDE),
            dbus.Int32(genivi.END_LATITUDE),
            dbus.Int32(genivi.START_LONGITUDE),
            dbus.Int32(genivi.END_LONGITUDE),
            dbus.Int32(genivi.DISTANCE),
            dbus.Int32(genivi.TIME),
            dbus.Int32(genivi.SPEED)]
        ret = g_routing_interface.GetRouteSegments(dbus.UInt32(g_route_handle), dbus.Int16(0), dbus.Array(valuesToReturn), dbus.UInt32(500), dbus.UInt32(0))
        print("Total number of segments: " + str(ret[0]))
        # len(ret[1]) is size
        # ret[1][0][genivi.START_LATITUDE] is the start latitude
        route = g_current_route + 1
        if route < routes.length:
            launch_route_calculation(route)
        else:
            for i in range(routes.length):
                g_routing_interface.DeleteRoute(dbus.UInt32(g_session_handle), dbus.UInt32(routes[i].getElementsByTagName("handle")[0].childNodes[0].data))
            g_session_interface.DeleteSession(dbus.UInt32(g_session_handle))


def catchall_session_signals_handler(sessionHandle):
    global g_session_handle
    print('Session handle deleted: ' + str(sessionHandle))
    if sessionHandle == g_session_handle:
        exit(0)
    else:
        exit(1)


def catchall_route_deleted_signals_handler(routeHandle):
    print('Route handle deleted: ' + str(routeHandle))


# timeout
def timeout():
    print('Timeout Expired\n')
    exit(1)


def exit(value):
    global g_exit
    g_exit = value
    if g_dltAvailable is True:
        stopTrigger(test_name)
    loop.quit()


def launch_route_calculation(route):
    global g_current_route
    global g_route_handle
    global g_routing_interface
    global g_session_handle
    g_current_route = route
    print('Route name: ' + routes[g_current_route].getElementsByTagName("name")[0].childNodes[0].data)
    # get route handle
    ret = g_routing_interface.CreateRoute(dbus.UInt32(g_session_handle))
    g_route_handle = ret[1]
    routes[g_current_route].getElementsByTagName("handle")[0].childNodes[0].data = g_route_handle
    print('Route handle: ' + str(g_route_handle))
    start = routes[g_current_route].getElementsByTagName("start")[0].childNodes[0].data
    dest = routes[g_current_route].getElementsByTagName("destination")[0].childNodes[0].data
    print('Calculating route from '
        + start
        + '(' + str(locations[routes[g_current_route].getElementsByTagName("start")[0].childNodes[0].data][0]) + ', ' + str(locations[routes[g_current_route].getElementsByTagName("start")[0].childNodes[0].data][1]) + ') to '
        + dest
        + '(' + str(locations[routes[g_current_route].getElementsByTagName("destination")[0].childNodes[0].data][0]) + ', ' + str(locations[routes[g_current_route].getElementsByTagName("destination")[0].childNodes[0].data][1]) + ')')
    # set waypoints
    g_routing_interface.SetWaypoints(dbus.UInt32(g_session_handle),
                                    dbus.UInt32(g_route_handle),
                                    dbus.Boolean(0),
                                    dbus.Array([
                                        dbus.Dictionary({dbus.UInt16(genivi.LATITUDE): dbus.Struct([0, dbus.Double(locations[routes[g_current_route].getElementsByTagName("start")[0].childNodes[0].data][0])]),
                                        dbus.UInt16(genivi.LONGITUDE): dbus.Struct([0, dbus.Double(locations[routes[g_current_route].getElementsByTagName("start")[0].childNodes[0].data][1])])}),
                                        dbus.Dictionary({dbus.UInt16(genivi.LATITUDE): dbus.Struct([0, dbus.Double(locations[routes[g_current_route].getElementsByTagName("destination")[0].childNodes[0].data][0])]),
                                        dbus.UInt16(genivi.LONGITUDE):dbus.Struct([0, dbus.Double(locations[routes[g_current_route].getElementsByTagName("destination")[0].childNodes[0].data][1])])})
                                    ]))
    # calculate route
    g_routing_interface.CalculateRoute(dbus.UInt32(g_session_handle), dbus.UInt32(g_route_handle))


if __name__ == '__main__':
    global g_verbose
    global g_session_handle

    # name of the test
    test_name = "route calculation"

    parser = argparse.ArgumentParser(description='Route Calculation Test for navigation PoC and FSA.')
    parser.add_argument('-r', '--rou', action='store', dest='routes', help='List of routes in xml format')
    parser.add_argument("-v", "--verbose", action='store_true', help='Print the whole log messages')
    parser.add_argument('-a', '--address', action='store', dest='host', help='Set remote host address')
    parser.add_argument('-p', '--prt', action='store', dest='port', help='Set remote port number')
    args = parser.parse_args()

    if args.routes is None:
        print('route file is missing')
        print('Test ' + test_name + ' FAILED')
        sys.exit(1)
    else:
        if not os.path.isfile(args.routes):
            print('file not exists')
            print('Test ' + test_name + ' FAILED')
            sys.exit(1)
        try:
            DOMTree = xml.dom.minidom.parse(args.routes)
        except OSError as e:
            print('Test ' + test_name + ' FAILED')
            sys.exit(1)
        route_set = DOMTree.documentElement

    print('Route Calculation Test')

    g_dltAvailable = checkDltProcess()

    # exit value on loop quit
    g_exit = 0

    g_verbose = args.verbose

    print("Country : %s" % route_set.getAttribute("country"))

    routes = route_set.getElementsByTagName("route")

    # create dictionary with the locations
    locations = {}
    for location in route_set.getElementsByTagName("location"):
        lat_long = [location.getElementsByTagName("latitude")[0].childNodes[0].data, location.getElementsByTagName("longitude")[0].childNodes[0].data]
        locations[location.getAttribute("name")] = lat_long

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    # connect to session bus (remote or local)
    if args.host is not None:
        bus = dbus.bus.BusConnection("tcp:host=" + args.host + ",port=" + args.port)
    else:
        bus = dbus.SessionBus()

    bus.add_signal_receiver(catchall_route_calculation_signals_handler,
                            dbus_interface="org.genivi.navigation.navigationcore.Routing",
                            signal_name="RouteCalculationProgressUpdate")

    bus.add_signal_receiver(catchall_route_deleted_signals_handler,
                            dbus_interface="org.genivi.navigation.navigationcore.Routing",
                            signal_name="RouteDeleted")

    bus.add_signal_receiver(catchall_session_signals_handler,
                            dbus_interface="org.genivi.navigation.navigationcore.Session",
                            signal_name="SessionDeleted")

    if g_dltAvailable is True:
        startTrigger(test_name)

    session = bus.get_object('org.genivi.navigation.navigationcore.Session', '/org/genivi/navigationcore')
    g_session_interface = dbus.Interface(session, dbus_interface='org.genivi.navigation.navigationcore.Session')

    # get session handle
    ret = g_session_interface.CreateSession(dbus.String("test route calculation"))
    g_session_handle = ret[1]
    print('Session handle: ' + str(g_session_handle))

    routing_obj = bus.get_object('org.genivi.navigation.navigationcore.Routing', '/org/genivi/navigationcore')
    g_routing_interface = dbus.Interface(routing_obj, dbus_interface='org.genivi.navigation.navigationcore.Routing')

    g_current_route = 0
    launch_route_calculation(0)

    # main loop
    GLib.timeout_add(TIME_OUT, timeout)
    loop = GLib.MainLoop()
    loop.run()
    if g_exit == 1:
        print('Test ' + test_name + ' FAILED')
    else:
        print('Test ' + test_name + ' PASSED')
    sys.exit(g_exit)
