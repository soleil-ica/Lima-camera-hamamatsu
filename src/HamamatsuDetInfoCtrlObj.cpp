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
#include "HamamatsuDetInfoCtrlObj.h"

using namespace lima;
using namespace lima::Hamamatsu;
using namespace std;


//-----------------------------------------------------
// @brief Ctor
//-----------------------------------------------------
DetInfoCtrlObj::DetInfoCtrlObj(Camera& cam)   :m_cam(cam)
{
    DEB_CONSTRUCTOR();
}

//-----------------------------------------------------
// @brief Dtor
//-----------------------------------------------------
DetInfoCtrlObj::~DetInfoCtrlObj()
{
    DEB_DESTRUCTOR();
}

//-----------------------------------------------------
// @brief return the max image size
//-----------------------------------------------------
void DetInfoCtrlObj::getMaxImageSize(Size& size)
{
    DEB_MEMBER_FUNCT();
    m_cam.getDetectorMaxImageSize(size);
}

//-----------------------------------------------------
// @brief returnt the detector image size
//-----------------------------------------------------
void DetInfoCtrlObj::getDetectorImageSize(Size& size)
{
    DEB_MEMBER_FUNCT();
    m_cam.getDetectorImageSize(size);
}

//-----------------------------------------------------
// @brief return the image type
//-----------------------------------------------------
void DetInfoCtrlObj::getDefImageType(ImageType& image_type)
{
    DEB_MEMBER_FUNCT();
    m_cam.getImageType(image_type);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void DetInfoCtrlObj::getCurrImageType(ImageType& image_type)
{
	DEB_MEMBER_FUNCT();
	m_cam.getImageType(image_type);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void DetInfoCtrlObj::setCurrImageType(ImageType image_type)
{
	DEB_MEMBER_FUNCT();
	m_cam.setImageType(image_type);

}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void DetInfoCtrlObj::getPixelSize(double& xsize, double& ysize)
{
    DEB_MEMBER_FUNCT();
    m_cam.getPixelSize(xsize, ysize);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void DetInfoCtrlObj::getDetectorType(std::string& type)
{
    DEB_MEMBER_FUNCT();
    m_cam.getDetectorType(type);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void DetInfoCtrlObj::getDetectorModel(std::string& model)
{
    DEB_MEMBER_FUNCT();
    m_cam.getDetectorModel(model);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void DetInfoCtrlObj::registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    DEB_MEMBER_FUNCT();
    //m_cam.registerMaxImageSizeCallback(cb);
}

//-----------------------------------------------------
// @brief
//-----------------------------------------------------
void DetInfoCtrlObj::unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb)
{
    DEB_MEMBER_FUNCT();
    //m_cam.unregisterMaxImageSizeCallback(cb);
}
