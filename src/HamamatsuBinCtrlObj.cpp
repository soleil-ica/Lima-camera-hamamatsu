
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
#include "HamamatsuBinCtrlObj.h"

using namespace lima;
using namespace lima::Hamamatsu;
using namespace std;


//-----------------------------------------------------
// @brief
//-----------------------------------------------------
BinCtrlObj::BinCtrlObj(Camera &cam) : m_cam(cam) 
{
    DEB_CONSTRUCTOR();
}

void BinCtrlObj::setBin(const Bin& aBin)
{
    DEB_MEMBER_FUNCT();    
    m_cam.setBin(aBin);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void BinCtrlObj::getBin(Bin &aBin)
{
    DEB_MEMBER_FUNCT();    
    m_cam.getBin(aBin);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void BinCtrlObj::checkBin(Bin &aBin)
{
    DEB_MEMBER_FUNCT();    
    m_cam.checkBin(aBin);
}

