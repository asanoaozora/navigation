/**
* @licence app begin@
* SPDX-License-Identifier: MPL-2.0
*
* \copyright Copyright (C) 2013-2014, PCA Peugeot Citroen
*
* \file genivi_navigationcore_guidance.cxx
*
* \brief This file is part of the Navit POC.
*
* \author Martin Schaller <martin.schaller@it-schaller.de>
* \author Philippe Colliot <philippe.colliot@mpsa.com>
*
* \version 1.0
*
* This Source Code Form is subject to the terms of the
* Mozilla Public License (MPL), v. 2.0.
* If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* For further information see http://www.genivi.org/.
*
* List of changes:
* 
* <date>, <name>, <description of change>
*
* @licence end@
*/
#ifndef DBUS_HAS_RECURSIVE_MUTEX
#define DBUS_HAS_RECURSIVE_MUTEX
#endif
#include <dbus-c++/glib-integration.h>
#include "genivi-navigationcore-constants.h"
#include "genivi-navigationcore-guidance_adaptor.h"

#if (SPEECH_ENABLED)
#include "genivi-speechservice-constants.h"
#include "genivi-speechservice-speechoutput_proxy.h"
#endif

#include <config.h>
#define USE_PLUGINS 1
#include <navit/debug.h>
#include <navit/plugin.h>
#include <navit/item.h>
#include <navit/config_.h>
#include <navit/navit.h>
#include <navit/callback.h>
#include <navit/navigation.h>
#include <navit/map.h>
#include <navit/transform.h>
#include <navit/track.h>
#include <navit/vehicle.h>
#include <navit/route.h>
#include <navit/messages.h>
#include "navigation-common-dbus.h"

#include "log.h"

DLT_DECLARE_CONTEXT(gCtx);

#if (!DEBUG_ENABLED)
#undef dbg
#define dbg(level,...) ;
#endif

static DBus::Glib::BusDispatcher dispatcher;
static DBus::Connection *conn;

#if (SPEECH_ENABLED)
class SpeechOutput
: public org::genivi::hmi::speechservice::SpeechOutput_proxy,
  public DBus::ObjectProxy
{
    public:
    SpeechOutput(DBus::Connection &connection)
        : DBus::ObjectProxy(connection,
                                    "/org/genivi/hmi/speechservice/SpeechOutput",
                                    "org.genivi.hmi.speechservice.SpeechOutput")
    {
    }

    void notifyConnectionStatus(const int32_t& connectionStatus)
    {

    }

    void notifyMarkerReached(const uint32_t& chunkID, const std::string& marker)
    {

    }

    void notifyQueueStatus(const int32_t& queueStatus)
    {

    }

    void notifyTTSStatus(const int32_t& ttsStatus)
    {

    }
};
#endif

class Guidance;

class GuidanceObj
{
	public:
	struct callback *m_guidance_callback;
	struct attr m_route, m_vehicleprofile, m_tracking_callback;
    struct attr m_vehicle_speed;
    uint32_t m_session,m_route_handle;
	Guidance *m_guidance;
#if (SPEECH_ENABLED)
    SpeechOutput *m_speechoutput;
#endif
    bool m_paused;
	bool m_voice_guidance;
    uint16_t m_prompt_mode;
    std::string m_kind_of_voice;
	struct item *get_item(struct map_rect *mr);
	struct map_rect *get_map_rect(void);
    void PauseGuidance(uint32_t sessionHandle);
    void ResumeGuidance(uint32_t sessionHandle);
	void SetVoiceGuidance(const bool& activate, const std::string& voice);
    void SetVoiceGuidanceSettings(const uint16_t& promptMode);
    uint16_t GetVoiceGuidanceSettings();
	void PlayVoiceManeuver();
    void GetGuidanceStatus(int32_t& guidanceStatus, uint32_t& routeHandle);
	void GetDestinationInformation(uint32_t& offset, uint32_t& TravelTime, int32_t& Direction, int16_t& TimeZone);
    void GetManeuver(struct item *item, uint32_t& DistanceToManeuver, int32_t &Maneuver, std::string& RoadAfterManeuver, std::pair<int32_t, DBusCommonAPIVariant> &ManeuverData);
    void GetManeuversList(const uint16_t& requestedNumberOfManeuvers, const uint32_t& maneuverOffset, uint16_t& numberOfManeuvers,std::vector< ::DBus::Struct< std::vector< ::DBus::Struct< std::string, std::vector< ::DBus::Struct< int32_t, std::string > >, std::string > >, std::string, std::string, std::string, std::string, uint16_t, int32_t, uint32_t, std::vector< ::DBus::Struct< uint32_t, uint32_t, int32_t, int32_t, std::map< int32_t, ::DBus::Struct< uint8_t, ::DBus::Variant > > > > > >&  maneuversList);
    void GetGuidanceDetails(bool& voiceGuidance, bool& vehicleOnTheRoad, bool& isDestinationReached, int32_t &maneuver);
	GuidanceObj(class Guidance *guidance, uint32_t SessionHandle, uint32_t RouteHandle);
	~GuidanceObj();
};

void GuidanceObj_Callback(GuidanceObj *obj);

static struct navit *
get_navit(void)
{
	struct attr navit;
	if (!config_get_attr(config, attr_navit, &navit, NULL))
		return NULL;
	return navit.u.navit;
}

static struct navigation *
get_navigation(void)
{
	struct navit *navit=get_navit();
	struct attr navigation;
	if (!navit)
		return NULL;
	if (!navit_get_attr(navit, attr_navigation, &navigation, NULL))
		return NULL;
	return navigation.u.navigation;
}

static struct tracking *
get_tracking(void)
{
	struct navit *navit=get_navit();
	struct attr tracking;
	if (!navit)
		return NULL;
	if (!navit_get_attr(navit, attr_trackingo, &tracking, NULL))
		return NULL;
	return tracking.u.tracking;
}

static struct vehicle *
get_vehicle(const char *source_prefix)
{
	struct navit *navit=get_navit();
	struct attr vehicle;
	struct vehicle *ret=NULL;
	if (!source_prefix) {
		if (navit_get_attr(navit, attr_vehicle, &vehicle, NULL))
			ret=vehicle.u.vehicle;
	} else {
        void * unused;
        struct attr_iter *iter=navit_attr_iter_new(unused);
		while (navit_get_attr(navit, attr_vehicle, &vehicle, iter)) {
			struct attr source;
			if (vehicle.u.vehicle && vehicle_get_attr(vehicle.u.vehicle, attr_source, &source, NULL) && 
				!strncmp(source.u.str, source_prefix, strlen(source_prefix))) { 
				ret=vehicle.u.vehicle;
				break;
			}
		}
		navit_attr_iter_destroy(iter);
	}	
	return ret;
}

static DBus::Variant
variant_enumeration(DBusCommonAPIEnumeration i)
{
    DBus::Variant variant;
    DBus::MessageIter iter=variant.writer();
    iter << i;
    return variant;
}

class  Guidance
: public org::genivi::navigation::navigationcore::Guidance_adaptor,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
	public:
	Guidance(DBus::Connection &connection)
	: DBus::ObjectAdaptor(connection, "/org/genivi/navigationcore")
	{
        m_simulationMode = true; //by default
        m_guidance_active=false;
	}

	void	
    StartGuidance(const uint32_t& SessionHandle, const uint32_t& RouteHandle)
	{
        if (m_guidance_active) {
            LOG_ERROR_MSG(gCtx,"guidance already active");
            throw DBus::ErrorFailed("guidance already active");
        } else {
            mp_guidance=new GuidanceObj(this, SessionHandle, RouteHandle);
            m_guidance_active=true;
            LOG_INFO_MSG(gCtx,"Guidance started");
        }
	}

	void	
    StopGuidance(const uint32_t& SessionHandle)
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
            throw DBus::ErrorFailed("no guidance active");
        } else {
            delete(mp_guidance);
            m_guidance_active=false;
            LOG_INFO_MSG(gCtx,"Guidance stopped");
        }
	}

    void
    GetDestinationInformation(uint32_t& offset, uint32_t& travelTime, int32_t& direction, int32_t& side, int16_t& timeZone, int16_t& daylightSavingTime)
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
            throw DBus::ErrorFailed("no guidance active");
        } else {
            mp_guidance->GetDestinationInformation(offset, travelTime, direction, timeZone);
        }
	}

	::DBus::Struct< uint16_t, uint16_t, uint16_t, std::string >
	GetVersion()
	{
		DBus::Struct<uint16_t, uint16_t, uint16_t, std::string> Version;
		Version._1=3;
		Version._2=0;
		Version._3=0;
		Version._4=std::string("23-10-2013");
		return Version;
	}

	void
    PauseGuidance(const uint32_t& sessionHandle)
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
			throw DBus::ErrorFailed("no guidance active");
        } else {
            mp_guidance->PauseGuidance(sessionHandle);
        }
	}

	void
    ResumeGuidance(const uint32_t& sessionHandle)
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
            throw DBus::ErrorFailed("no guidance active");
        } else {
            mp_guidance->ResumeGuidance(sessionHandle);
        }
	}

	int32_t
	SetVoiceGuidance(const bool& activate, const std::string& voice)
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
            throw DBus::ErrorFailed("no guidance active");
        } else {
            mp_guidance->SetVoiceGuidance(activate,voice);
        }
        return(0); //not implemented yet
	}

	void
    GetGuidanceDetails(bool& voiceGuidance, bool& vehicleOnTheRoad, bool& isDestinationReached, int32_t& maneuver)
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
            throw DBus::ErrorFailed("no guidance active");
        } else {
            mp_guidance->GetGuidanceDetails(voiceGuidance, vehicleOnTheRoad, isDestinationReached, maneuver);
        }
	}

    int32_t
	PlayVoiceManeuver()
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
            throw DBus::ErrorFailed("no guidance active");
        } else {
            mp_guidance->PlayVoiceManeuver();
        }
        return(0); //not implemented yet
	}

    void
    GetWaypointInformation(const uint16_t& requestedNumberOfWaypoints, uint16_t& numberOfWaypoints, std::vector< ::DBus::Struct< uint32_t, uint32_t, int32_t, int32_t, int16_t, int16_t, bool, uint16_t > >& waypointsList)
    {
		throw DBus::ErrorNotSupported("Not yet supported");
	}

    void
    GetManeuversList(const uint16_t& requestedNumberOfManeuvers, const uint32_t& maneuverOffset, int32_t& error, uint16_t& numberOfManeuvers, std::vector< ::DBus::Struct< std::vector< ::DBus::Struct< std::string, std::vector< ::DBus::Struct< int32_t, std::string > >, std::string > >, std::string, std::string, std::string, std::string, uint16_t, int32_t, uint32_t, std::vector< ::DBus::Struct< uint32_t, uint32_t, int32_t, int32_t, std::map< int32_t, ::DBus::Struct< uint8_t, ::DBus::Variant > > > > > >& maneuversList)
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
            throw DBus::ErrorFailed("no guidance active");
        } else {
            mp_guidance->GetManeuversList(requestedNumberOfManeuvers, maneuverOffset, numberOfManeuvers, maneuversList);
        }
        error=0; //not used
    }

	void
    SetRouteCalculationMode(const uint32_t& sessionHandle, const int32_t& routeCalculationMode)
	{
		throw DBus::ErrorNotSupported("Not yet supported");
	}

    int32_t
    SkipNextManeuver(const uint32_t& sessionHandle)
	{
		throw DBus::ErrorNotSupported("Not yet supported");
        return(0); //not implemented yet
	}

	void
    GetGuidanceStatus(int32_t& guidanceStatus, uint32_t& routeHandle)
	{
        if (m_guidance_active==true) {
            mp_guidance->GetGuidanceStatus(guidanceStatus, routeHandle);
		} else {
			guidanceStatus=GENIVI_NAVIGATIONCORE_INACTIVE;
			routeHandle=0;
		}
	}

    int32_t
    SetVoiceGuidanceSettings(const int32_t& promptMode)
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
            throw DBus::ErrorFailed("no guidance active");
        } else {
            mp_guidance->SetVoiceGuidanceSettings(promptMode);
        }
        return(0); //not implemented yet
	}

    int32_t
	GetVoiceGuidanceSettings()
	{
        if (m_guidance_active==false) {
            LOG_ERROR_MSG(gCtx,"no guidance active");
            throw DBus::ErrorFailed("no guidance active");
        } else {
            return mp_guidance->GetVoiceGuidanceSettings();
        }
	}

    void
    GetNearestRouteLocation(double& distance, double& direction)
    {
        throw DBus::ErrorNotSupported("Not yet supported");
    }

    void
    GetNearestRoadLocation(double& distance, int32_t& direction)
    {
        throw DBus::ErrorNotSupported("Not yet supported");
    }

    GuidanceObj *mp_guidance;

    bool m_simulationMode;
    bool m_guidance_active;
};

void
GuidanceObj::GetDestinationInformation(uint32_t& Distance, uint32_t& TravelTime, int32_t& Direction, int16_t &TimeZone)
{
	struct coord c[2];
	int idx=0;
	struct attr destination_time, destination_length;
	struct map_rect *mr=get_map_rect();
	struct item *item;
    if (!mr) {
        LOG_ERROR_MSG(gCtx,"GetDestinationInformation: failed to get map rect");
		throw DBus::ErrorFailed("internal error:failed to get map rect");
    }
	while (item=map_rect_get_item(mr)) {
		if (item_coord_get(item, &c[idx], 1)) {
			if (!idx) {
				if (!item_attr_get(item, attr_destination_time, &destination_time))
					destination_time.u.num=-1;
				if (!item_attr_get(item, attr_destination_length, &destination_length))
					destination_length.u.num=-1;
				idx=1;
			}
		}
	}
	if (!idx)
		throw DBus::ErrorFailed("internal error:navigation has only one coordinate");
	if (destination_time.u.num == -1 || destination_length.u.num == -1) {
        LOG_ERROR(gCtx,"time %d length %d",(int) destination_time.u.num, (int) destination_length.u.num);
		throw DBus::ErrorFailed("internal error:failed to get time or length");
	}
	Distance=destination_length.u.num;
	TravelTime=destination_time.u.num/10;
	Direction=transform_get_angle_delta(&c[0], &c[1], 0);
	TimeZone=0;
}

void
GuidanceObj::GetManeuver(struct item *item, uint32_t& DistanceToManeuver, int32_t& Maneuver, std::string& RoadAfterManeuver, std::pair< int32_t, DBusCommonAPIVariant >& ManeuverData)
{
	struct attr length, street_name;
    int32_t index;
    DBusCommonAPIVariant data;

    if (item_attr_get(item, attr_length, &length)) {
		DistanceToManeuver=length.u.num;
	}
	if (item_attr_get(item, attr_street_name, &street_name)) {
		RoadAfterManeuver=std::string(street_name.u.str);
	}

    index = GENIVI_NAVIGATIONCORE_DIRECTION;
	switch (item->type) {
	case type_nav_straight:
        Maneuver=GENIVI_NAVIGATIONCORE_CROSSROAD;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_STRAIGHT);
		break;
	case type_nav_turnaround:
        Maneuver=GENIVI_NAVIGATIONCORE_CROSSROAD;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_UTURN_LEFT);
		break;
	case type_nav_right_1:
        Maneuver=GENIVI_NAVIGATIONCORE_CROSSROAD;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_SLIGHT_RIGHT);
		break;
	case type_nav_right_2:
        Maneuver=GENIVI_NAVIGATIONCORE_CROSSROAD;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_RIGHT);
		break;
	case type_nav_right_3:
        Maneuver=GENIVI_NAVIGATIONCORE_CROSSROAD;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_HARD_RIGHT);
		break;
	case type_nav_left_1:
        Maneuver=GENIVI_NAVIGATIONCORE_CROSSROAD;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_SLIGHT_LEFT);
		break;
	case type_nav_left_2:
        Maneuver=GENIVI_NAVIGATIONCORE_CROSSROAD;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_LEFT);
		break;
	case type_nav_left_3:
        Maneuver=GENIVI_NAVIGATIONCORE_CROSSROAD;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_HARD_LEFT);
		break;
	// FIXME: Are ManeuverDirection values right here?
	case type_nav_roundabout_r1:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_HARD_RIGHT);
		break;
	case type_nav_roundabout_r2:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_RIGHT);
		break;
	case type_nav_roundabout_r3:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_SLIGHT_RIGHT);
		break;
	case type_nav_roundabout_r4:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_STRAIGHT);
		break;
	case type_nav_roundabout_r5:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_SLIGHT_LEFT);
		break;
	case type_nav_roundabout_r6:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_LEFT);
		break;
	case type_nav_roundabout_r7:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_HARD_LEFT);
		break;
	case type_nav_roundabout_r8:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_UTURN_LEFT);
		break;
	// FIXME: Distinguish between clockwise and counterclockwise roundabout?
	case type_nav_roundabout_l1:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_HARD_LEFT);
		break;
	case type_nav_roundabout_l2:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_LEFT);
		break;
	case type_nav_roundabout_l3:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_SLIGHT_LEFT);
		break;
	case type_nav_roundabout_l4:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_STRAIGHT);
		break;
	case type_nav_roundabout_l5:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_SLIGHT_RIGHT);
		break;
	case type_nav_roundabout_l6:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_RIGHT);
		break;
	case type_nav_roundabout_l7:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_HARD_RIGHT);
		break;
	case type_nav_roundabout_l8:
        Maneuver=GENIVI_NAVIGATIONCORE_ROUNDABOUT;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_UTURN_RIGHT);
		break;
	case type_nav_destination:
        Maneuver=GENIVI_NAVIGATIONCORE_DESTINATION;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_STRAIGHT);
		break;
    case type_nav_merge_left:
        Maneuver=GENIVI_NAVIGATIONCORE_DESTINATION;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_LEFT);
        break;
    case type_nav_merge_right:
        Maneuver=GENIVI_NAVIGATIONCORE_DESTINATION;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_RIGHT);
        break;
    case type_nav_turnaround_left:
        Maneuver=GENIVI_NAVIGATIONCORE_DESTINATION;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_UTURN_LEFT);
        break;
    case type_nav_turnaround_right:
        Maneuver=GENIVI_NAVIGATIONCORE_DESTINATION;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_UTURN_RIGHT);
        break;
    case type_nav_exit_left:
        Maneuver=GENIVI_NAVIGATIONCORE_DESTINATION;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_LEFT);
        break;
    case type_nav_exit_right:
        Maneuver=GENIVI_NAVIGATIONCORE_DESTINATION;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_RIGHT);
        break;
    case type_nav_keep_left:
        Maneuver=GENIVI_NAVIGATIONCORE_DESTINATION;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_LEFT);
        break;
    case type_nav_keep_right:
        Maneuver=GENIVI_NAVIGATIONCORE_DESTINATION;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_RIGHT);
        break;
    default:
        LOG_ERROR(gCtx,"Unable to convert type %s",item_to_name(item->type));
        Maneuver=GENIVI_NAVIGATIONCORE_INVALID;
        index = GENIVI_NAVIGATIONCORE_INVALID;
        data._2=variant_enumeration(GENIVI_NAVIGATIONCORE_INVALID);
	}
    std::pair< int32_t, DBusCommonAPIVariant > ret(index,data);
    ManeuverData=ret;
}


void
GuidanceObj::GetGuidanceDetails(bool& voiceGuidance, bool& vehicleOnTheRoad, bool& isDestinationReached, int32_t& maneuver)
{
    struct map_rect *mr=get_map_rect();
    if (!mr) {
        LOG_ERROR_MSG(gCtx,"Failed to get map rect");
        return;
    }
    struct item *item;
    item=get_item(mr);
    std::string road_name_after_maneuver;
    uint32_t offset_maneuver;
    std::pair< int32_t, DBusCommonAPIVariant > maneuver_data;

    voiceGuidance = m_voice_guidance;
    vehicleOnTheRoad = true; //by default, no off-road managed
    isDestinationReached = false; //to be done

    GetManeuver(item, offset_maneuver, maneuver, road_name_after_maneuver, maneuver_data);

}

void
GuidanceObj::GetManeuversList(const uint16_t& requestedNumberOfManeuvers, const uint32_t& maneuverOffset, uint16_t& numberOfManeuvers,std::vector< ::DBus::Struct< std::vector< ::DBus::Struct< std::string, std::vector< ::DBus::Struct< int32_t, std::string > >, std::string > >, std::string, std::string, std::string, std::string, uint16_t, int32_t, uint32_t, std::vector< ::DBus::Struct< uint32_t, uint32_t, int32_t, int32_t, std::map< int32_t, ::DBus::Struct< uint8_t, ::DBus::Variant > > > > > >&  maneuversList)
{
	struct map_rect *mr=get_map_rect();
    if (!mr) {
        LOG_ERROR_MSG(gCtx,"Failed to get map rect");
        return;
    }
    struct item *item;
    uint16_t maneuverIndex;
    std::map< int32_t, DBusCommonAPIVariant >::iterator it;

    numberOfManeuvers = 0;
    maneuverIndex = 0;
    while (item=get_item(mr)) { //scan the list of maneuvers of the route
        if (maneuverIndex >= maneuverOffset && maneuverIndex < maneuverOffset+requestedNumberOfManeuvers) {
            ::DBus::Struct< std::vector< ::DBus::Struct< std::string, std::vector< ::DBus::Struct< int32_t, std::string > >, std::string > >, std::string, std::string, std::string, std::string, uint16_t, int32_t, uint32_t, std::vector< ::DBus::Struct< uint32_t, uint32_t, int32_t, int32_t, std::map< int32_t, DBusCommonAPIVariant > > > > maneuver;
            ::DBus::Struct< uint32_t, uint32_t, int32_t, int32_t, std::map< int32_t, DBusCommonAPIVariant > > sub_maneuver;
            std::pair< int32_t, DBusCommonAPIVariant > maneuver_data;
            maneuver._4 = ""; //roadNumberAfterManeuver
            maneuver._6 = GENIVI_NAVIGATIONCORE_DEFAULT; //roadPropertyAfterManeuver
            maneuver._7 = GENIVI_NAVIGATIONCORE_RIGHT; //drivingSide
            maneuver._8 = 0; //offsetOfNextManeuver
            //get infos about maneuver: sub_maneuver._1: DistanceToManeuver, sub_maneuver._4: Maneuver, maneuver._5: RoadAfterManeuver
            //maneuver_data is a map of attribute and value (e.g. DIRECTION STRAIGHT_ON )
            GetManeuver(item, sub_maneuver._1, sub_maneuver._4, maneuver._5, maneuver_data);
            sub_maneuver._5.insert(maneuver_data);
            maneuver._9.push_back(sub_maneuver);
            if (maneuversList.size())
                maneuversList.back()._8 = sub_maneuver._1; //offsetOfNextManeuver of the last record is the offsetOfManeuver  of this one
			maneuversList.push_back(maneuver);
            numberOfManeuvers++;
		}
        maneuverIndex++;
	}
	map_rect_destroy(mr);
}

void
GuidanceObj::PauseGuidance(uint32_t sessionHandle)
{
	struct vehicle *vehicle=get_vehicle("demo:");
	if (vehicle) {
		struct attr vehicle_speed0={attr_speed,(char *)0};
		vehicle_set_attr(vehicle, &vehicle_speed0);
	}
	m_paused=true;
}

void
GuidanceObj::ResumeGuidance(uint32_t sessionHandle)
{
	struct vehicle *vehicle=get_vehicle("demo:");
	GuidanceObj_Callback(this);
	if (vehicle) 
    {
        vehicle_set_attr(vehicle, &m_vehicle_speed);
    }
    m_paused=false;
}

void GuidanceObj::SetVoiceGuidance(const bool& activate, const std::string& voice)
{
    m_voice_guidance = activate;
    m_kind_of_voice.clear();
    m_kind_of_voice.append(voice);
}

void GuidanceObj::PlayVoiceManeuver()
{
#if (SPEECH_ENABLED)
    message *messages;
    messages = navit_get_messages(get_navit());
    if(messages){
        m_speechoutput->openPrompter(GENIVI_SPEECHSERVICE_CT_NAVIGATION,GENIVI_SPEECHSERVICE_PPT_NAVIGATION);
        while(messages){
            m_speechoutput->addTextChunk(std::string(messages->text));
            messages=messages->next;
        }
        m_speechoutput->closePrompter();
    }
#endif
}

void GuidanceObj::SetVoiceGuidanceSettings(const uint16_t& promptMode)
{
    m_prompt_mode = promptMode;
}

uint16_t GuidanceObj::GetVoiceGuidanceSettings()
{
    return m_prompt_mode;
}

void
GuidanceObj::GetGuidanceStatus(int32_t &guidanceStatus, uint32_t& routeHandle)
{
	if (m_paused)
        guidanceStatus = GENIVI_NAVIGATIONCORE_INACTIVE;
    else
        guidanceStatus = GENIVI_NAVIGATIONCORE_ACTIVE;
	routeHandle=m_route_handle;
}

void
GuidanceObj_Callback(GuidanceObj *obj)
{
	struct attr level;
	struct map_rect *mr;
	struct item *item;
	if (obj->m_paused)
		return;
    mr=obj->get_map_rect();
	if (!mr) {
        LOG_ERROR_MSG(gCtx,"Failed to get map rect");
		return;
	}
	item=obj->get_item(mr);
	if (item && item_attr_get(item, attr_level, &level)) {
		int maneuver;
        LOG_INFO(gCtx,"Level: %d",(int) level.u.num);
		switch(level.u.num) {
		case 3:
            maneuver=GENIVI_NAVIGATIONCORE_PASSED;
			break;
		case 2:
			maneuver=GENIVI_NAVIGATIONCORE_MANEUVER_APPEARED;
			break;
		case 1:
			maneuver=GENIVI_NAVIGATIONCORE_PRE_ADVICE;
			break;
		case 0:
			maneuver=GENIVI_NAVIGATIONCORE_ADVICE;
			break;
		default:
			maneuver=GENIVI_NAVIGATIONCORE_INVALID;
		}
		obj->m_guidance->ManeuverChanged(maneuver);
        LOG_INFO(gCtx,"Maneuver: %d",maneuver);
        obj->PlayVoiceManeuver();
    } else {
        LOG_ERROR(gCtx,"Maneuver item not found: %p",item);
	}
}

static DBus::Variant
variant_double(double d)
{
	DBus::Variant variant;
	DBus::MessageIter iter=variant.writer();
	iter << d;
	return variant;
}

void
GuidanceObj_TrackingCallback(GuidanceObj *obj)
{
	struct attr attr;
    route_set_position_from_tracking(obj->m_route.u.route, get_tracking(), projection_mg);
	if (!obj->m_paused)
        obj->m_guidance->PositionOnRouteChanged(0); //to do return the current offset on the route in meters from the beginning of the route

	int destreached=route_destination_reached(obj->m_route.u.route);
	if (destreached)
		obj->m_guidance->WaypointReached(destreached == 2);
	if (destreached == 2) 
		route_set_destination(obj->m_route.u.route, NULL, 0);
}

struct item *
GuidanceObj::get_item(struct map_rect *mr)
{
	struct item *ret;
	while (mr && (ret = map_rect_get_item(mr))) {
		if (ret->type != type_nav_position && ret->type != type_nav_none)
			break;
	}
	return ret;
}

struct map_rect *
GuidanceObj::get_map_rect(void)
{
	struct map *map=navigation_get_map(get_navigation());
    if (!map) {
        LOG_ERROR_MSG(gCtx,"map_rect null");
        return NULL;
    }
	return map_rect_new(map, NULL);
}

GuidanceObj::GuidanceObj(Guidance *guidance, uint32_t SessionHandle, uint32_t RouteHandle)
{
	m_guidance=guidance;
	m_session=SessionHandle;
	m_route_handle=RouteHandle;
    m_guidance_callback=callback_new_1(reinterpret_cast<void (*)(void)>(GuidanceObj_Callback), this);
    m_paused=false;
	m_voice_guidance=false;
    m_kind_of_voice="DEFAULT";
    m_prompt_mode=GENIVI_NAVIGATIONCORE_MANUAL_PROMPT;
	m_tracking_callback.type=attr_callback;
	m_tracking_callback.u.callback=NULL;
	struct attr id={attr_id};
    m_vehicle_speed={attr_speed,(char *)40};
	id.u.num=RouteHandle;
	struct attr *in[]={&id, NULL};
	struct attr **ret=NULL;
	struct attr callback_list;
	struct navit *navit=get_navit();

#if (SPEECH_ENABLED)
    m_speechoutput=new SpeechOutput(*conn);
#endif
	if (navit_get_attr(navit, attr_callback_list, &callback_list, NULL)) {
		callback_list_call_attr_4(callback_list.u.callback_list, attr_command, "navit_genivi_get_route", in, &ret, NULL);
        if (ret && ret[0] && ret[1] && ret[0]->type == attr_route && ret[1]->type == attr_vehicleprofile) {
			struct tracking *tracking=get_tracking();
            m_route=*ret[0];
            m_vehicleprofile=*ret[1];
			m_tracking_callback.u.callback=callback_new_attr_1(reinterpret_cast<void (*)(void)>(GuidanceObj_TrackingCallback), attr_position_coord_geo, this);
            tracking_add_attr(tracking, &m_tracking_callback);
            struct vehicle *vehicle=get_vehicle("demo:");
            if (vehicle) {
                vehicle_set_attr(vehicle, &m_route);
                vehicle_set_attr(vehicle, &m_vehicle_speed);
			}
            tracking_set_route(get_tracking(), m_route.u.route);
            navigation_set_route(get_navigation(), m_route.u.route);
            navigation_register_callback(get_navigation(), attr_navigation_speech, m_guidance_callback);
        }
		g_free(ret);
	}
	m_guidance->GuidanceStatusChanged(GENIVI_NAVIGATIONCORE_ACTIVE, RouteHandle);
    LOG_INFO_MSG(gCtx,"Guidance status changed to active");
}

GuidanceObj::~GuidanceObj()
{
    navigation_unregister_callback(get_navigation(), attr_navigation_speech, m_guidance_callback);

    struct vehicle *vehicle=get_vehicle("demo:");
    if (vehicle) {
        vehicle_remove_attr(vehicle,&m_route);
        vehicle_remove_attr(vehicle,&m_vehicle_speed);
    }

    if (m_tracking_callback.u.callback) {
		struct tracking *tracking=get_tracking();
		if (tracking)
			tracking_remove_attr(tracking, &m_tracking_callback);
		callback_destroy(m_tracking_callback.u.callback);
	}

    callback_destroy(m_guidance_callback);

    m_guidance->GuidanceStatusChanged(GENIVI_NAVIGATIONCORE_INACTIVE, m_route_handle);
#if (SPEECH_ENABLED)
    delete(m_speechoutput);
#endif
    LOG_INFO_MSG(gCtx,"Guidance status changed to inactive");

}

static class Guidance *s_server;

void
plugin_init(void)
{
    DLT_REGISTER_APP("GUIDS","GUIDANCE SERVER");
    DLT_REGISTER_CONTEXT(gCtx,"GUIDS","Global Context");

    dispatcher.attach(NULL);
	DBus::default_dispatcher = &dispatcher;
	// FIXME: What dbus address to use? 
	conn = new DBus::Connection(DBus::Connection::SessionBus());
	conn->setup(&dispatcher);
	conn->request_name("org.genivi.navigation.navigationcore.Guidance");
    s_server=new Guidance(*conn);
}
