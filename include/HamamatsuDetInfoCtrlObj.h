//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2014
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
#ifndef HAMAMATSUDETINFOCTRLOBJ_H
#define HAMAMATSUDETINFOCTRLOBJ_H

#include "lima/HwDetInfoCtrlObj.h"
#include "HamamatsuCamera.h"

namespace lima
{
    namespace Hamamatsu
    {

/*******************************************************************
 * \class DetInfoCtrlObj
 * \brief Control object providing Hamamatsu detector info interface
 *******************************************************************/

	class LIBHAMAMATSU_API DetInfoCtrlObj : public HwDetInfoCtrlObj
	{
	    DEB_CLASS_NAMESPC(DebModCamera, "DetInfoCtrlObj", "Hamamatsu");

	public:
	    DetInfoCtrlObj(Camera& cam);
	    virtual ~DetInfoCtrlObj();

	    virtual void getMaxImageSize(Size& max_image_size);
	    virtual void getDetectorImageSize(Size& det_image_size);

	    virtual void getDefImageType(ImageType& def_image_type);
	    virtual void getCurrImageType(ImageType& curr_image_type);
	    virtual void setCurrImageType(ImageType  curr_image_type);

	    virtual void getPixelSize(double& xsize, double& ysize);
	    virtual void getDetectorType(std::string& det_type);
	    virtual void getDetectorModel(std::string& det_model);

	    virtual void registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb);
	    virtual void unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb);

	private:
	    Camera& m_cam;
	};

    } // namespace Hamamatsu
} // namespace lima

#endif // HamamatsuDETINFOCTRLOBJ_H
