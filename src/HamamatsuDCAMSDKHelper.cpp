///###########################################################################
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
//############################################################################

#include <string>
#include "HamamatsuCamera.h"

using namespace lima;
using namespace lima::Hamamatsu;
using namespace std;

//-----------------------------------------------------------------------------
/// initialize DCAM-API and get HDCAM camera handle.
/*!
@return a valid camera handle or NULL if failed
*/
//-----------------------------------------------------------------------------
HDCAM Camera::dcam_init_open(long camera_number)	///< [in] id of the camera to open
{
	DEB_MEMBER_FUNCT();

	char	buf[DCAM_STRMSG_SIZE];
	int32	nDevice;
	int32	iDevice;
	bool    bUninit = false;

	// initialize DCAM-API
	DEB_TRACE() << "calling dcam_init()";

	if( dcam_init( NULL, &nDevice, NULL ) )
	{
		DEB_TRACE() << "dcam_init ok";
		DEB_TRACE() << "nDevice " << nDevice; 
		if( nDevice >= 1 )
		{
			// At the moment we choose the first camera of the list if there is more thant one camera
			iDevice = camera_number;

			// get camera information
			{
				showCameraInfo(iDevice);

				if (dcam_getmodelinfo( iDevice, DCAM_IDSTR_VENDOR,		buf, sizeof( buf ) ))
				{					
					m_detector_type = buf;
				}

				if (dcam_getmodelinfo( iDevice, DCAM_IDSTR_MODEL,		buf, sizeof( buf ) ))
				{
					m_detector_model = buf;
				}
			}

			// open specified camera
			HDCAM	hdcam;
			if( dcam_open( &hdcam, iDevice, NULL ) )
			{
				// success
				return hdcam;
			}
			else
			{
				DEB_TRACE() << "dcam_open failed";
				bUninit = true;
			}
		}

		// uninitialize DCAM-API
		dcam_uninit( NULL, NULL );
	}

	DEB_TRACE() << "dcam_init() failed";

	// failure
	return NULL;
}


//-----------------------------------------------------------------------------
/// Initialize the subarray mode (define ROI -rectangle-)
/*!
@return true if successfull
*/
//-----------------------------------------------------------------------------
bool Camera::dcamex_setsubarrayrect( HDCAM hdcam,  ///< [in] camera handle
									 long left,	   ///< [in] left  (x)
									 long top,     ///< [in] top   (y)
									 long width,   ///< [in] horizontal size
									 long height ) ///< [in] vertical size
{
	DCAM_PARAM_SUBARRAY		param;
	memset( &param, 0, sizeof( param ) );
	param.hdr.cbSize	= sizeof( param );
	param.hdr.id		= DCAM_IDPARAM_SUBARRAY;
	param.hdr.iFlag		= dcamparam_subarray_hpos
						| dcamparam_subarray_vpos	
						| dcamparam_subarray_hsize
						| dcamparam_subarray_vsize
						;

	param.hpos	= left;
	param.vpos	= top;
	param.hsize	= width;
	param.vsize	= height;

	return dcam_extended( hdcam, DCAM_IDMSG_SETPARAM, &param, sizeof( param ) )?true:false;
}


//-----------------------------------------------------------------------------
/// Get the current subarray parameters (get ROI settings)
/*!
@return true if successfull
*/
//-----------------------------------------------------------------------------
bool Camera::dcamex_getsubarrayrect( HDCAM hdcam,    ///< [in] camera handle
									 int32& left,	 ///< [in] left  (x)
									 int32& top,	 ///< [in] top   (y)
									 int32& width,	 ///< [in] horizontal size
									 int32& height ) ///< [in] vertical size
{
	bool bReturn = false;

	DCAM_PARAM_SUBARRAY		param;
	memset( &param, 0, sizeof( param ) );
	param.hdr.cbSize	= sizeof( param );
	param.hdr.id		= DCAM_IDPARAM_SUBARRAY;
	param.hdr.iFlag		= dcamparam_subarray_hpos
						| dcamparam_subarray_vpos	
						| dcamparam_subarray_hsize
						| dcamparam_subarray_vsize
						;

	if (dcam_extended( hdcam, DCAM_IDMSG_GETPARAM, &param, sizeof( param ) ) )
	{
		left	= param.hpos;
		top		= param.vpos;
		width	= param.hsize;
		height	= param.vsize;
		
		bReturn = true;
	}

	return bReturn;
}


//-----------------------------------------------------------------------------
/// Get the width of the image
/*!
@return width of the image or 0 if failed
*/
//-----------------------------------------------------------------------------
long Camera::dcamex_getimagewidth(const HDCAM hdcam ) ///< [in] camera handle
{
	SIZE	sz;
	if( ! dcam_getdatasize( hdcam, &sz ) )
		return 0;

	return sz.cx;
}


//-----------------------------------------------------------------------------
/// Get the height of the image
/*!
@return width of the image or 0 if failed
*/
//-----------------------------------------------------------------------------
long Camera::dcamex_getimageheight(const HDCAM hdcam ) ///< [in] camera handle
{
	SIZE	sz;
	if( ! dcam_getdatasize( hdcam, &sz ) )
		return 0;

	return sz.cy;
}


//-----------------------------------------------------------------------------
/// Get the settings of a feature
/*!
@return true if the feature settings could be obtained
*/
//-----------------------------------------------------------------------------
bool Camera::dcamex_getfeatureinq( HDCAM hdcam,				///< [in]  camera handle
								   long idfeature,			///< [in]  feature id
								   long& capflags,			///< [out] ?
								   double& min,				///< [out] min value of the feature	
								   double& max,				///< [out] max value of the feature
								   double& step,			///< [out] ?
								   double& defaultvalue )	///< [out] default value of the feature
								   const
{
	DCAM_PARAM_FEATURE_INQ	inq;
	memset( &inq, 0, sizeof( inq ) );
	inq.hdr.cbSize	= sizeof( inq );
	inq.hdr.id		= DCAM_IDPARAM_FEATURE_INQ;
	inq.hdr.iFlag	= dcamparam_featureinq_featureid
					| dcamparam_featureinq_capflags
					| dcamparam_featureinq_min
					| dcamparam_featureinq_max
					| dcamparam_featureinq_step
					| dcamparam_featureinq_defaultvalue
					| dcamparam_featureinq_units
					;
	inq.featureid	= idfeature;

	if( !dcam_extended( hdcam, DCAM_IDMSG_GETPARAM, &inq, sizeof( inq ) ) )
	{
		return false;
	}

	if( inq.hdr.oFlag & dcamparam_featureinq_capflags )		capflags	 = inq.capflags;
	if( inq.hdr.oFlag & dcamparam_featureinq_min )			min			 = inq.min;
	if( inq.hdr.oFlag & dcamparam_featureinq_max )			max			 = inq.max;
	if( inq.hdr.oFlag & dcamparam_featureinq_step )			step		 = inq.step;
	if( inq.hdr.oFlag & dcamparam_featureinq_defaultvalue )	defaultvalue = inq.defaultvalue;

	return true;
}


//-----------------------------------------------------------------------------
/// Get the number of bits per channel
/*!
@return the number of bits bet pixel of 0 if failed
*/
//-----------------------------------------------------------------------------
long Camera::dcamex_getbitsperchannel( HDCAM hdcam )    ///< [in] camera handle
{
	int32	vmax, vmin;
	if( ! dcam_getdatarange( hdcam, &vmax, &vmin ) )
		return 0;

	int	i;
	for( i = 0; vmax > 0; i++ )
		vmax >>= 1;

	return i;
}


//-----------------------------------------------------------------------------
/// Show camera model info
//-----------------------------------------------------------------------------
void Camera::showCameraInfo(const int iDevice) ///< [in] camera device id
{
	DEB_MEMBER_FUNCT();

	list<int> listProps;
	listProps.push_back(DCAM_IDSTR_VENDOR);
	listProps.push_back(DCAM_IDSTR_MODEL);
	listProps.push_back(DCAM_IDSTR_BUS);
	listProps.push_back(DCAM_IDSTR_CAMERAID);
	listProps.push_back(DCAM_IDSTR_CAMERAVERSION);
	listProps.push_back(DCAM_IDSTR_DRIVERVERSION);

	list<int>::const_iterator iterProp = listProps.begin();
	while (listProps.end()!=iterProp)
	{
		char buf[DCAM_STRMSG_SIZE];
		if (dcam_getmodelinfo( iDevice, *iterProp, buf, sizeof( buf ) ))
		{
			DEB_TRACE() << ">" << buf;
		}
		++iterProp;
	}
}
