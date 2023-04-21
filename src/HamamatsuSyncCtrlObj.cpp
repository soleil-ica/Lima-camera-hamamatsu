
//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2012
// European Synchrotron Radiation Facility
// BP 220, Grenoble 38043
// FRANCE
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//###########################################################################
#include "HamamatsuSyncCtrlObj.h"

using namespace lima;
using namespace lima::Hamamatsu;
using namespace std;

//-----------------------------------------------------
// @brief Ctor
//-----------------------------------------------------
SyncCtrlObj::SyncCtrlObj(Camera& cam)
    : HwSyncCtrlObj(), m_cam(cam)
{
    DEB_CONSTRUCTOR();
}

//-----------------------------------------------------
// @brief Dtor
//-----------------------------------------------------
SyncCtrlObj::~SyncCtrlObj()
{
    DEB_DESTRUCTOR();
}

//-----------------------------------------------------
// @brief return True if the mode is supported
//-----------------------------------------------------
bool SyncCtrlObj::checkTrigMode(TrigMode trig_mode)
{   
    return m_cam.checkTrigMode(trig_mode);   
 }

//-----------------------------------------------------
// @brief set the trigger mode
//-----------------------------------------------------
void SyncCtrlObj::setTrigMode(TrigMode trig_mode)
{
    DEB_MEMBER_FUNCT();    
    if (!checkTrigMode(trig_mode))
        THROW_HW_ERROR(InvalidValue) << "Invalid " << DEB_VAR1(trig_mode);
    m_cam.setTrigMode(trig_mode);    
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void SyncCtrlObj::getTrigMode(TrigMode& trig_mode)
{
    DEB_MEMBER_FUNCT();    
    m_cam.getTrigMode(trig_mode);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void SyncCtrlObj::setExpTime(double exp_time)
{
    DEB_MEMBER_FUNCT();    
    m_cam.setExpTime(exp_time);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void SyncCtrlObj::getExpTime(double& exp_time)
{
    DEB_MEMBER_FUNCT();    
    m_cam.getExpTime(exp_time);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void SyncCtrlObj::setLatTime(double lat_time)
{
    m_cam.setLatTime(lat_time);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void SyncCtrlObj::getLatTime(double& lat_time)
{
    DEB_MEMBER_FUNCT();    
    m_cam.getLatTime(lat_time);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void SyncCtrlObj::setNbHwFrames(int nb_frames)
{
    DEB_MEMBER_FUNCT();
    m_cam.setNbFrames(nb_frames);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void SyncCtrlObj::getNbHwFrames(int& nb_frames)
{
    DEB_MEMBER_FUNCT();    
    m_cam.getNbFrames(nb_frames);
}

//--- @brief--------------------------------------------------
//
//-----------------------------------------------------
void SyncCtrlObj::getValidRanges(ValidRangesType& valid_ranges)
{
    DEB_MEMBER_FUNCT();
    double min_time;
    double max_time;
    m_cam.getExposureTimeRange(min_time, max_time);
    valid_ranges.min_exp_time = min_time;
    valid_ranges.max_exp_time = max_time;

    m_cam.getLatTimeRange(min_time, max_time);
    valid_ranges.min_lat_time = min_time;
    valid_ranges.max_lat_time = max_time;
}
