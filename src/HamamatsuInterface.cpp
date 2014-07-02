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
#include "HamamatsuInterface.h"
#include <algorithm>

using namespace lima;
using namespace lima::Hamamatsu;
using namespace std;


//-----------------------------------------------------
// @brief Ctor
//-----------------------------------------------------
Interface::Interface(Camera& cam)
    : m_cam(cam),m_det_info(cam),
      m_sync(cam),m_bin(cam),m_roi(cam)
{
    DEB_CONSTRUCTOR();
    
    HwDetInfoCtrlObj *det_info = &m_det_info;
    m_cap_list.push_back(HwCap(det_info));
    
    m_cap_list.push_back(HwCap(cam.getBufferCtrlObj()));
    
    HwSyncCtrlObj *sync = &m_sync;
    m_cap_list.push_back(HwCap(sync));
    
    HwRoiCtrlObj *roi = &m_roi;
    m_cap_list.push_back(HwCap(roi));
    
    if(m_cam.isBinningAvailable())
    {
	   HwBinCtrlObj *bin = &m_bin;
	   m_cap_list.push_back(HwCap(bin));
    }
}

//-----------------------------------------------------
// @brief Dtor
//-----------------------------------------------------
Interface::~Interface()
{
    DEB_DESTRUCTOR();
}

//-----------------------------------------------------
// @brief return the capability list
//-----------------------------------------------------
void Interface::getCapList(HwInterface::CapList &cap_list) const
{
    DEB_MEMBER_FUNCT();
    cap_list = m_cap_list;
}

//-----------------------------------------------------
// @brief reset the interface, stop the acqisition
//-----------------------------------------------------
void Interface::reset(ResetLevel reset_level)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(reset_level);

    stopAcq();
}

//-----------------------------------------------------
// @brief do nothing
//-----------------------------------------------------
void Interface::prepareAcq()
{
    DEB_MEMBER_FUNCT();
	m_cam.prepareAcq();
}

//-----------------------------------------------------
// @brief start the camera acquisition
//-----------------------------------------------------
void Interface::startAcq()
{
    DEB_MEMBER_FUNCT();
    m_cam.startAcq();
}

//-----------------------------------------------------
// @brief stop the camera acquisition
//-----------------------------------------------------
void Interface::stopAcq()
{
  DEB_MEMBER_FUNCT();
  m_cam.stopAcq();
}

//-----------------------------------------------------
// @brief return the status of detector/acquisition
//-----------------------------------------------------
void Interface::getStatus(StatusType& status)
{
    DEB_MEMBER_FUNCT();
    
    Camera::Status Hamamatsu_status = Camera::Ready;
    Hamamatsu_status = m_cam.getStatus();
    switch (Hamamatsu_status)
    {
    case Camera::Ready:
      status.set(HwInterface::StatusType::Ready);
      break;
    case Camera::Exposure:
      status.set(HwInterface::StatusType::Exposure);
      break;
    case Camera::Readout:
      status.set(HwInterface::StatusType::Readout);
      break;
    case Camera::Latency:
      status.set(HwInterface::StatusType::Latency);
      break;
    case Camera::Fault:
      status.set(HwInterface::StatusType::Fault);
    }
    
    DEB_RETURN() << DEB_VAR1(status);
}


//-----------------------------------------------------
// @brief return the hw number of acquired frames
//-----------------------------------------------------
int Interface::getNbHwAcquiredFrames()
{
     DEB_MEMBER_FUNCT();
     int acq_frames;
     m_cam.getNbHwAcquiredFrames(acq_frames);
     return acq_frames;
}

/////////////////////////////////////////////////////////////
// HERE we just map setter/getter methods of the HamamatsuCamera 
// class for a public access, stupid but useful !!
// one could merge HamamatsuCamera and HamamatsuInterface, ok next 
// release.
/////////////////////////////////////////////////////////////

