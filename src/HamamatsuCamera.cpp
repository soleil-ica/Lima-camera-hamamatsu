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

#include <sstream>
#include <iostream>
#include <string>
#include <math.h>
#include <algorithm>
#include "HamamatsuCamera.h"

using namespace lima;
using namespace lima::Hamamatsu;
using namespace std;

//-----------------------------------------------------------------------------
const double Camera::g_OrcaPixelSize            = 6.5e-6;
const int    Camera::g_DCAMFrameBufferSize      = 10    ;
const int    Camera::g_DCAMStrMsgSize           = 256   ;
const int    Camera::g_GetSubArrayDoNotUseView  = -1    ;

const string Camera::g_TraceLineSeparator       = "--------------------------------------------------------------";
const string Camera::g_TraceLittleLineSeparator = "--------------------------------";

#define TRACE_LINE() DEB_ALWAYS() << __LINE__

#define GET_SUBARRAY_RECT_DO_NOT_USE_VIEW (-1)

//-----------------------------------------------------------------------------
///  Ctor
//-----------------------------------------------------------------------------
#pragma warning( push )
#pragma warning( disable : 4355) // temporary disable the warning cause by the use of this in the initializers

Camera::Camera(const std::string& config_path, int camera_number)
    : m_thread         (this) ,
      m_status         (Ready),
      m_image_number   (0)    ,
	  m_depth          (16)   ,
      m_latency_time   (0.)   ,
      m_bin            (1,1)  ,
      m_camera_handle  (0)    ,
      m_fasttrigger    (0)    ,
      m_exp_time       (1.)   ,
	  m_read_mode      (2)    ,
	  m_lostFramesCount(0)    ,
	  m_fps            (0.0)  ,
	  m_fpsUpdatePeriod(100)  ,
      m_ViewExpTime    (NULL)   // array of exposure value by view

#pragma warning( pop ) 
{
    DEB_CONSTRUCTOR();

    m_config_path   = config_path  ;
    m_camera_number = camera_number;
  
	m_map_triggerMode[IntTrig       ] = "IntTrig"       ;
	m_map_triggerMode[IntTrigMult   ] = "IntTrigMult"   ;
	m_map_triggerMode[ExtGate       ] = "ExtGate"       ;
	m_map_triggerMode[ExtTrigReadout] = "ExtTrigReadout";
	m_map_triggerMode[ExtTrigSingle ] = "ExtTrigSingle" ;
	m_map_triggerMode[ExtTrigMult   ] = "ExtTrigMult"   ;

    DEB_TRACE() << "Starting Hamamatsu camera (DCAMAPI_VER:" << DCAMAPI_VER << ")";

    // --- Get available cameras and select the choosen one.	
	m_camera_handle = dcam_init_open(camera_number);

	if (NULL != m_camera_handle)
	{
        // --- Initialise deeper parameters of the controller                
		initialiseController();

        // retrying the maximum number of views for this camera
        // Will be also used to know if W-View mode is possible
        m_MaxViews = getMaxNumberofViews();
        
        if(m_MaxViews > 1)
        {
            m_ViewExpTime = new double[m_MaxViews];

            for(int ViewIndex = 0 ; ViewIndex < m_MaxViews ; ViewIndex++)
            {
                m_ViewExpTime[ViewIndex] = m_exp_time; // by default
            }
        }
        else
        {
            m_ViewExpTime = NULL;
        }

		// --- BIN already set to 1,1 above.
		// --- Hamamatsu sets the ROI by starting coordinates at 1 and not 0 !!!!
		Size sizeMax;
		getDetectorImageSize(sizeMax);
		Roi aRoi = Roi(0,0, sizeMax.getWidth(), sizeMax.getHeight());

		// Store max image size
		m_maxImageWidth  = sizeMax.getWidth ();
		m_maxImageHeight = sizeMax.getHeight();

		// Display max image size
		DEB_TRACE() << "Detector max width: " << m_maxImageWidth ;
		DEB_TRACE() << "Detector max height:" << m_maxImageHeight;

        // sets no view mode by default
        m_ViewModeEnabled = false; // W-View mode with splitting image
        m_ViewNumber      = 0    ; // number of W-Views

        setViewMode(false, 0);

		// --- setRoi applies both bin and roi
		DEB_TRACE() << "Set the ROI to full frame: "<< aRoi;
		setRoi(aRoi);	    
	    
		// --- Get the maximum exposure time allowed and set default
		setExpTime(m_exp_time);
	    
		// --- Set detector for software single image mode    
		setTrigMode(IntTrig);
	    
		m_nb_frames = 1;

		// --- finally start the acq thread
		m_thread.start();
	}
	else
	{
        manage_error( deb, "Unable to initialize the camera (Check if it is switched on or if an other software is currently using it).");
        THROW_HW_ERROR(Error) << "Unable to initialize the camera (Check if it is switched on or if an other software is currently using it).";
	}
}


//-----------------------------------------------------------------------------
///  Dtor
//-----------------------------------------------------------------------------
Camera::~Camera()
{
    DEB_DESTRUCTOR();

    DCAMERR err;

	stopAcq();
               
    // Close camera
	DEB_TRACE() << "Shutdown camera";

	if (NULL != m_camera_handle)
	{
	    err = dcamdev_close( m_camera_handle );

        if( !failed(err) )
        {
    		DEB_TRACE() << "dcamdev_close() succeeded.";
            m_camera_handle = NULL;
            dcamapi_uninit();
			DEB_TRACE() << "dcamapi_uninit() succeeded.";
        }
        else
        {
            manage_error( deb, "dcam_close() failed !", err);
            THROW_HW_ERROR(Error) << "dcam_close() failed !";
        }
	}    

    if(m_ViewExpTime != NULL)
        delete [] m_ViewExpTime;

	DEB_TRACE() << "Camera destructor done.";
}

//-----------------------------------------------------------------------------
/// return the detector Max image size 
//-----------------------------------------------------------------------------
void Camera::getDetectorMaxImageSize(Size& size) ///< [out] image dimensions
{
	DEB_MEMBER_FUNCT();
	size = Size(m_maxImageWidth, m_maxImageHeight);
}


//-----------------------------------------------------------------------------
/// return the detector image size 
//-----------------------------------------------------------------------------
void Camera::getDetectorImageSize(Size& size) ///< [out] image dimensions
{
    DEB_MEMBER_FUNCT();
    
	int xmax =0;
	int ymax =0;
    
    // --- Get the max image size of the detector
	if (NULL!=m_camera_handle)
	{
		xmax = dcamex_getimagewidth (m_camera_handle);
		ymax = dcamex_getimageheight(m_camera_handle);
	}

	if ((0==xmax) || (0==ymax))
    {
        manage_error( deb, "Cannot get detector size");
        THROW_HW_ERROR(Error) << "Cannot get detector size";
    }     

    size= Size(xmax, ymax);

    DEB_TRACE() << "Size (" << DEB_VAR2(size.getWidth(), size.getHeight()) << ")";
}


//-----------------------------------------------------------------------------
/// return the image type  TODO: this is permanently called by the device -> find a way to avoid DCAM access
//-----------------------------------------------------------------------------
void Camera::getImageType(ImageType& type)
{
    DEB_MEMBER_FUNCT();

	type = Bpp16;

	long bitsType =  dcamex_getbitsperchannel(m_camera_handle);
	
	if (0 != bitsType )
	{
		switch( bitsType )
		{
			case 8 :  type = Bpp8 ; break;
			case 16:  type = Bpp16; break;
			case 32:  type = Bpp32; break;
			default:
			{
                manage_error( deb, "No compatible image type");
                THROW_HW_ERROR(Error) << "No compatible image type";
			}
		}
	}
	else
	{
        manage_error( deb, "Unable to get image type.");
        THROW_HW_ERROR(Error) << "Unable to get image type.";
	}
}


//-----------------------------------------------------
//! Camera::setImageType()
//-----------------------------------------------------
void Camera::setImageType(ImageType type)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setImageType - " << DEB_VAR1(type);
	switch(type)
	{
		case Bpp16:
		{
			m_depth	= 16;
			break;
		}
		default:
            manage_error( deb, "This pixel format of the camera is not managed, only 16 bits cameras are already managed!");
            THROW_HW_ERROR(Error) << "This pixel format of the camera is not managed, only 16 bits cameras are already managed!";
			break;
	}

	DEB_TRACE() << "SetImageType: " << m_depth;
	m_bytesPerPixel = m_depth / 8;
}


//-----------------------------------------------------------------------------
/// return the detector type
//-----------------------------------------------------------------------------
void Camera::getDetectorType(string& type) ///< [out] detector type
{
    DEB_MEMBER_FUNCT();
    
    type = m_detector_type;
}


//-----------------------------------------------------------------------------
/// return the detector model
//-----------------------------------------------------------------------------
void Camera::getDetectorModel(string& type) ///< [out] detector model
{
    DEB_MEMBER_FUNCT();
    type = m_detector_model;
}


//-----------------------------------------------------------------------------
/// return the internal buffer manager
/*!
@ return buffer control object
*/
//-----------------------------------------------------------------------------
HwBufferCtrlObj* Camera::getBufferCtrlObj()
{
    DEB_MEMBER_FUNCT();
    return &m_buffer_ctrl_obj;
}


//-----------------------------------------------------------------------------
/// Checks trigger mode
/*!
@return true if the given trigger mode is supported
*/
//-----------------------------------------------------------------------------
bool Camera::checkTrigMode(TrigMode trig_mode) ///< [in] trigger mode to check
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(trig_mode);

    return Camera::getTriggerMode(trig_mode);
}


//-----------------------------------------------------------------------------
/// Set the new trigger mode
//-----------------------------------------------------------------------------
void Camera::setTrigMode(TrigMode mode) ///< [in] trigger mode to set
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(mode);

	// Get the dcam_sdk mode associated to the given LiMA TrigMode
    if(getTriggerMode(mode))
	{
        DCAMERR err           ;
        int TriggerSource = -1;
        int TriggerActive = -1;
        int TriggerMode   = -1;

        if(mode == IntTrig)
        {
            TriggerSource = DCAMPROP_TRIGGERSOURCE__INTERNAL;
            TriggerActive = DCAMPROP_TRIGGERACTIVE__EDGE    ;
            TriggerMode   = DCAMPROP_TRIGGER_MODE__NORMAL   ;
        }
        else
        if(mode == IntTrigMult)
        {
            TriggerSource = DCAMPROP_TRIGGERSOURCE__INTERNAL;
            TriggerActive = DCAMPROP_TRIGGERACTIVE__EDGE    ;
            TriggerMode   = DCAMPROP_TRIGGER_MODE__NORMAL   ;
        }
        else
        if(mode == ExtTrigReadout)
        {
            TriggerSource = DCAMPROP_TRIGGERSOURCE__EXTERNAL   ;
            TriggerActive = DCAMPROP_TRIGGERACTIVE__SYNCREADOUT;
            TriggerMode   = DCAMPROP_TRIGGER_MODE__NORMAL      ;
        }
        else
        if(mode == ExtTrigSingle)
        {
            TriggerSource = DCAMPROP_TRIGGERSOURCE__EXTERNAL;
            TriggerActive = DCAMPROP_TRIGGERACTIVE__EDGE    ;
            TriggerMode   = DCAMPROP_TRIGGER_MODE__START    ;
        }
        else
        if(mode == ExtTrigMult)
        {
            TriggerSource = DCAMPROP_TRIGGERSOURCE__EXTERNAL;
            TriggerActive = DCAMPROP_TRIGGERACTIVE__EDGE    ;
            TriggerMode   = DCAMPROP_TRIGGER_MODE__NORMAL   ;
        }
        else
        if(mode == ExtGate)
        {
            TriggerSource = DCAMPROP_TRIGGERSOURCE__EXTERNAL;
            TriggerActive = DCAMPROP_TRIGGERACTIVE__LEVEL   ;
            TriggerMode   = DCAMPROP_TRIGGER_MODE__NORMAL   ;
        }
        else
        {
            manage_error( deb, "Failed to set trigger mode", DCAMERR_NONE, "setTrigMode", "VALUE=%d", mode);
            THROW_HW_ERROR(Error) << "Failed to set trigger mode";
        }

        // set the trigger source
        if(TriggerSource != -1)
        {
            err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_TRIGGERSOURCE, static_cast<double>(TriggerSource));
	        if( failed(err) )
	        {
                manage_error( deb, "Cannot set trigger option", err, 
                              "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_TRIGGERSOURCE, VALUE=%d", TriggerSource);
                THROW_HW_ERROR(Error) << "Cannot set trigger option";
	        }
        }

        // set the trigger active
        if(TriggerActive != -1)
        {
	        err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_TRIGGERACTIVE, static_cast<double>(TriggerActive));
	        if( failed(err) )
	        {
                manage_error( deb, "Cannot set trigger option", err, 
                              "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_TRIGGERACTIVE, VALUE=%d", TriggerActive);
                THROW_HW_ERROR(Error) << "Cannot set trigger option";
	        }
        }

        // set the trigger mode
        if(TriggerMode != -1)
        {
	        err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_TRIGGER_MODE, static_cast<double>(TriggerMode));
	        if( failed(err) )
	        {
                manage_error( deb, "Cannot set trigger option", err, 
                              "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_TRIGGER_MODE, VALUE=%d", TriggerMode);
                THROW_HW_ERROR(Error) << "Cannot set trigger option";
	        }
        }

		m_trig_mode = mode;    

        TraceTriggerData();
	}
}


//-----------------------------------------------------------------------------
/// Get the current trigger mode
//-----------------------------------------------------------------------------
void Camera::getTrigMode(TrigMode& mode) ///< [out] current trigger mode
{
    DEB_MEMBER_FUNCT();
    mode = m_trig_mode;
    
    DEB_RETURN() << DEB_VAR1(mode);
}


//-----------------------------------------------------------------------------
/// Set the new exposure time
//-----------------------------------------------------------------------------
void Camera::setExpTime(double exp_time) ///< [in] exposure time to set
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(exp_time);

    if(!m_ViewModeEnabled)
    {
        DCAMERR err;

        // set the exposure time
	    err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_EXPOSURETIME, exp_time);

        if( failed(err) )
	    {
            manage_error( deb, "Cannot set exposure time", err, 
                          "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_EXPOSURETIME, VALUE=%lf", exp_time);
            THROW_HW_ERROR(Error) << "Cannot set exposure time";
	    }

        m_exp_time = exp_time;

        double tempexp_time;
        getExpTime(tempexp_time);
        manage_trace( deb, "Changed Exposure time", DCAMERR_NONE, NULL, "exp:%lf >> real:%lf", m_exp_time, tempexp_time);
    }
}


//-----------------------------------------------------------------------------
/// Get the current exposure time
//-----------------------------------------------------------------------------
void Camera::getExpTime(double& exp_time) ///< [out] current exposure time
{
    DEB_MEMBER_FUNCT();

    DCAMERR err     ;
	double  exposure;

    // classic mode
    if(!m_ViewModeEnabled)
    {
        // get the binding
        err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_EXPOSURETIME, &exposure );
        
	    if(failed(err) )
	    {
            manage_error( deb, "Cannot get exposure time", err, 
                          "dcamprop_getvalue", "DCAM_IDPROP_EXPOSURETIME");
            THROW_HW_ERROR(Error) << "Cannot get exposure time";
        }
    }
    else
    // W-View mode - the exposure is the last exposure value
    {
        exposure = m_exp_time;
    }

    exp_time = exposure;

    DEB_RETURN() << DEB_VAR1(exp_time);
}


//-----------------------------------------------------------------------------
/// Set the new latency time between images
//-----------------------------------------------------------------------------
void Camera::setLatTime(double lat_time) ///< [in] latency time
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(lat_time);

	if (lat_time != 0.0)
	{
        manage_error( deb, "Latency is not supported");
        THROW_HW_ERROR(Error) << "Latency is not supported";
	}
}


//-----------------------------------------------------------------------------
/// Get the current latency time
//-----------------------------------------------------------------------------
void Camera::getLatTime(double& lat_time) ///< [out] current latency time
{
    DEB_MEMBER_FUNCT();
  
    lat_time = 0.0;
    
    DEB_RETURN() << DEB_VAR1(lat_time);
}


//-----------------------------------------------------------------------------
/// Get the exposure time range
//-----------------------------------------------------------------------------
void Camera::getExposureTimeRange(double& min_expo,	///< [out] minimum exposure time
								  double& max_expo) ///< [out] maximum exposure time
								  const
{
    DEB_MEMBER_FUNCT();

    FeatureInfos FeatureObj  ;

	if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_EXPOSURETIME", DCAM_IDPROP_EXPOSURETIME, FeatureObj ) )
	{
        manage_error( deb, "Failed to get exposure time");
        THROW_HW_ERROR(Error) << "Failed to get exposure time";
	}

    min_expo = FeatureObj.m_min;
    max_expo = FeatureObj.m_max;

    DEB_RETURN() << DEB_VAR2(min_expo, max_expo);
}


//-----------------------------------------------------------------------------
///  Get the latency time range
//-----------------------------------------------------------------------------
void Camera::getLatTimeRange(double& min_lat, ///< [out] minimum latency
							 double& max_lat) ///< [out] maximum latency
							 const
{   
    DEB_MEMBER_FUNCT();

    // --- no info on min latency
    min_lat = 0.;       
    
    // --- do not know how to get the max_lat, fix it as the max exposure time
    max_lat = m_exp_time_max;

    DEB_RETURN() << DEB_VAR2(min_lat, max_lat);
}


//-----------------------------------------------------------------------------
/// Set the number of frames to be taken
//-----------------------------------------------------------------------------
void Camera::setNbFrames(int nb_frames) ///< [in] number of frames to take
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(nb_frames);
    
    m_nb_frames = nb_frames;
}


//-----------------------------------------------------------------------------
/// Get the number of frames to be taken
//-----------------------------------------------------------------------------
void Camera::getNbFrames(int& nb_frames) ///< [out] current number of frames to take
{
    DEB_MEMBER_FUNCT();
    nb_frames = m_nb_frames;
    DEB_RETURN() << DEB_VAR1(nb_frames);
}


//-----------------------------------------------------------------------------
/// Get the current acquired frames
//-----------------------------------------------------------------------------
void Camera::getNbHwAcquiredFrames(int &nb_acq_frames)
{ 
    DEB_MEMBER_FUNCT();    
    nb_acq_frames = m_image_number;
}


//-----------------------------------------------------------------------------
/// Get the camera status
//-----------------------------------------------------------------------------
Camera::Status Camera::getStatus() ///< [out] current camera status
{
	DEB_MEMBER_FUNCT();

	int thread_status = m_thread.getStatus();

	DEB_RETURN() << DEB_VAR1(thread_status);

	switch (thread_status)
	{
		case CameraThread::Ready   :
			return Camera::Ready   ;

		case CameraThread::Exposure:
			return Camera::Exposure;

		case CameraThread::Readout :
			return Camera::Readout ;

		case CameraThread::Latency :
			return Camera::Latency ;

        case CameraThread::Fault   :
			return Camera::Fault   ;

        case CameraThread::InInit   :
        case CameraThread::Stopped  :
        case CameraThread::Finished :
            manage_error( deb, "CameraThread is on an invalid state.");
			return Camera::Fault   ;

		default:
			throw LIMA_HW_EXC(Error, "Invalid thread status");
	}
}


//-----------------------------------------------------------------------------
/// checkRoi
//-----------------------------------------------------------------------------
void Camera::checkRoi(const Roi & set_roi, ///< [in]  Roi values to set
						    Roi & hw_roi ) ///< [out] Updated Roi values
{
    DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(set_roi);

    Point topleft = set_roi.getTopLeft();
    Size  size    = set_roi.getSize   ();
    int   x       = topleft.x        * m_bin.getX();
    int   y       = topleft.y        * m_bin.getY();
    int   width   = size.getWidth () * m_bin.getX();
    int   height  = size.getHeight() * m_bin.getY();

	if ((width == 0) && (height == 0))
	{
		DEB_TRACE() << "Ignore 0x0 roi";
        hw_roi = set_roi;
	}
    else
    {
        DEB_TRACE() << "checkRoi() - before rounding :" << x << ", " << y << ", " << width << ", " << height;

        // rounding the values
        // this code can be improved by checking the right-bottom corner but special cases will be rejected during dcamex_setsubarrayrect call
        m_FeaturePosx.RoundValue (x     );
        m_FeaturePosy.RoundValue (y     );
        m_FeatureSizex.RoundValue(width );
        m_FeatureSizey.RoundValue(height);

        DEB_TRACE() << "checkRoi() - after rounding :" << x << ", " << y << ", " << width << ", " << height;

        hw_roi.setTopLeft(Point(x     / m_bin.getX(), y      / m_bin.getY()));
        hw_roi.setSize   (Size (width / m_bin.getX(), height / m_bin.getY()));

        if(set_roi != hw_roi)
        {
            manage_error( deb, "This ROI is not a valid one.", DCAMERR_NONE, "checkRoi");
            THROW_HW_ERROR(Error) << "This ROI is not a valid one. Please try (" 
                                    << x      / m_bin.getX() << ", " 
                                    << y      / m_bin.getY() << ", " 
                                    << width  / m_bin.getX() << ", " 
                                    << height / m_bin.getY() << ")";
        }
    }    

    DEB_RETURN() << DEB_VAR1(hw_roi);
}


//-----------------------------------------------------------------------------
/// Set the new roi
// The ROI given by LIMA has a size which depends on the binning.
// SDK Sub array are binning independants.
//-----------------------------------------------------------------------------
void Camera::setRoi(const Roi & set_roi) ///< [in] New Roi values
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(set_roi);

	DEB_TRACE() << "setRoi() - new values : " 
                << set_roi.getTopLeft().x           << ", " 
                << set_roi.getTopLeft().y           << ", " 
                << set_roi.getSize   ().getWidth () << ", " 
                << set_roi.getSize   ().getHeight();

    Point set_roi_topleft(set_roi.getTopLeft().x       * m_bin.getX(), set_roi.getTopLeft().y        * m_bin.getY());
    Size  set_roi_size   (set_roi.getSize().getWidth() * m_bin.getX(), set_roi.getSize().getHeight() * m_bin.getY());

    // correction of a 0x0 ROI sent by the generic part
    if ((set_roi_size.getWidth() == 0) && (set_roi_size.getHeight() == 0))
    {
	    DEB_TRACE() << "Correcting 0x0 roi...";
        set_roi_size = Size(m_maxImageWidth, m_maxImageHeight);
    }

    Roi new_roi(set_roi_topleft, set_roi_size);

	DEB_TRACE() << "setRoi(): " << set_roi_topleft.x        << ", " 
                                << set_roi_topleft.y        << ", " 
                                << set_roi_size.getWidth () << ", " 
                                << set_roi_size.getHeight();

    // Changing the ROI is not allowed in W-VIEW mode except for full frame
    if(m_ViewModeEnabled)
    {
        Roi FullFrameRoi(Point(0, 0), Size(m_maxImageWidth, m_maxImageHeight));
        if(new_roi != FullFrameRoi)
        {
            manage_error( deb, "Cannot change ROI in W-VIEW mode! Only full frame is supported.", DCAMERR_NONE, "setRoi");
            THROW_HW_ERROR(Error) << "Cannot change ROI in W-VIEW mode! Only full frame is supported.";
        }
    }

    // view mode activated and two views 
    if((m_ViewModeEnabled) && (m_ViewNumber == 2))
    {
        if (!dcamex_setsubarrayrect(m_camera_handle, 
                                    set_roi_topleft.x      , set_roi_topleft.y         ,
                                    set_roi_size.getWidth(), set_roi_size.getHeight()/2,
                                    0))
        {
            manage_error( deb, "Cannot set detector ROI for View1 !");
            THROW_HW_ERROR(Error) << "Cannot set detector ROI for View1!";
        }

        if (!dcamex_setsubarrayrect(m_camera_handle, 
                                    set_roi_topleft.x      , set_roi_topleft.y         ,
                                    set_roi_size.getWidth(), set_roi_size.getHeight()/2,
                                    1))
        {
            manage_error( deb, "Cannot set detector ROI for View2 !");
            THROW_HW_ERROR(Error) << "Cannot set detector ROI for View2!";
        }
    }
    else
    {
        if (!dcamex_setsubarrayrect(m_camera_handle, 
                                    set_roi_topleft.x      , set_roi_topleft.y       ,
                                    set_roi_size.getWidth(), set_roi_size.getHeight(),
                                    g_GetSubArrayDoNotUseView))
        {
            manage_error( deb, "Cannot set detector ROI!");
            THROW_HW_ERROR(Error) << "Cannot set detector ROI!";
        }
    }

    m_roi = new_roi;
}


//-----------------------------------------------------------------------------
/// Get the current roi values
//-----------------------------------------------------------------------------
void Camera::getRoi(Roi & hw_roi) ///< [out] Roi values
{
    DEB_MEMBER_FUNCT();

	int32 left, top, width, height;

    if (!dcamex_getsubarrayrect( m_camera_handle, left, top, width,	height, GET_SUBARRAY_RECT_DO_NOT_USE_VIEW ) )
    {
        manage_error( deb, "Cannot get detector ROI");
        THROW_HW_ERROR(Error) << "Cannot get detector ROI";
    }    

    // view mode activated and two views 
    if((m_ViewModeEnabled) && (m_ViewNumber == 2))
    {
        height *= 2; // height correction to get the global ROI (two views height)
    }

    hw_roi = Roi(left  / m_bin.getX(), top    / m_bin.getY(), 
                 width / m_bin.getX(), height / m_bin.getY());

    DEB_TRACE() << "getRoi(): " 
                << left   / m_bin.getX() << ", " 
                << top    / m_bin.getY() << ", "
                << width  / m_bin.getX() << ", " 
                << height / m_bin.getY();

    DEB_RETURN() << DEB_VAR1(hw_roi);
}

//-----------------------------------------------------------------------------
/// Trace all the ROI configuration (General, View1, View2, ...)
//-----------------------------------------------------------------------------
void Camera::traceAllRoi(void)
{
    DEB_MEMBER_FUNCT();

	int32 left, top, width, height;

    if(!m_ViewModeEnabled)
    {
        if (!dcamex_getsubarrayrect( m_camera_handle, left, top, width,	height, g_GetSubArrayDoNotUseView ) )
        {
            manage_error( deb, "Cannot get detector ROI");
        }    
        else
        {
            DEB_TRACE() << "General Roi: (" << left << ", " << top << ", " <<  width << ", " <<  height << ")";
        }
    }
    else
    {
        for(int ViewIndex = 0 ; ViewIndex < m_MaxViews ; ViewIndex++)
        {
            if (!dcamex_getsubarrayrect( m_camera_handle, left, top, width,	height, ViewIndex ) )
            {
                manage_error( deb, "Cannot get detector View ROI");
            }    
            else
            {
                DEB_TRACE() << "View Roi (" << (ViewIndex+1) << "): (" << left << ", " << top << ", " <<  width << ", " <<  height << ")";
            }
        }
    }
}

//-----------------------------------------------------------------------------
/// Check is the given binning values are supported, rise an exception otherwise
//-----------------------------------------------------------------------------
void Camera::checkBin(Bin & hw_bin) ///< [out] binning values to update
{
    DEB_MEMBER_FUNCT();

	if ( (hw_bin.getX() != hw_bin.getY()) || (!isBinningSupported(hw_bin.getX())) )
	{
		DEB_ERROR() << "Binning values not supported";
		THROW_HW_ERROR(Error) << "Binning values not supported";
	}

    DEB_RETURN() << DEB_VAR1(hw_bin);
}


//-----------------------------------------------------------------------------
/// set the new binning mode
//-----------------------------------------------------------------------------
void Camera::setBin(const Bin & set_bin) ///< [in] binning values objects
{
    DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(set_bin);

	// Binning values have been checked by checkBin()
    DCAMERR err;

    // set the binning
	err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_BINNING, static_cast<double>(GetBinningMode(set_bin.getX())));

    if( !failed(err) )
	{
		DEB_TRACE() << "dcam_setbinning() ok: " << set_bin.getX() << "x" << set_bin.getY();
        m_bin = set_bin; // update current binning values		
    }
    else
    {
        manage_error( deb, "Cannot set detector BIN", err, 
                      "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_BINNING, VALUE=%d", GetBinningMode(set_bin.getX()));
        THROW_HW_ERROR(Error) << "Cannot set detector BIN";
	}
    
    DEB_RETURN() << DEB_VAR1(set_bin);
}


//-----------------------------------------------------------------------------
/// Get the current binning mode
//-----------------------------------------------------------------------------
void Camera::getBin(Bin & hw_bin) ///< [out] binning values object
{
    DEB_MEMBER_FUNCT();

    DCAMERR err ;
    double  temp;

    // get the binding
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_BINNING, &temp );
    
	if(!failed(err) )
	{
    	int32 nBinningMode = static_cast<int32>(temp);
        int   nBinning     = GetBinningFromMode(nBinningMode);

        DEB_TRACE() << "dcamprop_getvalue(): Mode:" << nBinningMode << ", Binning:" << nBinning;
		hw_bin = Bin(nBinning, nBinning);
	}
    else
    {
        manage_error( deb, "Cannot get detector BIN", err, 
                      "dcamprop_getvalue", "DCAM_IDPROP_BINNING");
        THROW_HW_ERROR(Error) << "Cannot get detector BIN";
    }

    DEB_RETURN() << DEB_VAR1(hw_bin);
}


//-----------------------------------------------------------------------------
/// Check if a binning value is supported
/*
@return true if the given binning value exists
*/
//-----------------------------------------------------------------------------
bool Camera::isBinningSupported(const int binValue)	///< [in] binning value to chck for
{
	DEB_MEMBER_FUNCT();

	bool bFound = false;
	for (unsigned int i=0; i<m_vectBinnings.size(); i++)
	{
		if (binValue == m_vectBinnings.at(i))
		{
			bFound = true;
			break;
		}
	}

	return bFound;
}

//-----------------------------------------------------------------------------
/// Get the corresponding binning mode
/*
@return the corresponding mode
*/
//-----------------------------------------------------------------------------
int32 Camera::GetBinningMode(const int binValue)	///< [in] binning value to chck for
{
	DEB_MEMBER_FUNCT();
    
    if(binValue == 1 ) return DCAMPROP_BINNING__1 ;
    if(binValue == 2 ) return DCAMPROP_BINNING__2 ;
    if(binValue == 4 ) return DCAMPROP_BINNING__4 ;
    if(binValue == 8 ) return DCAMPROP_BINNING__8 ;
    if(binValue == 16) return DCAMPROP_BINNING__16;

    manage_error( deb, "Incoherent binning value - no mode found.", DCAMERR_NONE, "GetBinningMode", "binning value = %d", binValue);
    THROW_HW_ERROR(Error) << "Incoherent binning value - no mode found.";
}

//-----------------------------------------------------------------------------
/// Get the corresponding binning from binning mode
/*
@return the corresponding mode
*/
//-----------------------------------------------------------------------------
int Camera::GetBinningFromMode(const int32 binMode)	///< [in] binning mode to chck for
{
	DEB_MEMBER_FUNCT();
    
    if(binMode == DCAMPROP_BINNING__1 ) return 1 ;
    if(binMode == DCAMPROP_BINNING__2 ) return 2 ;
    if(binMode == DCAMPROP_BINNING__4 ) return 4 ;
    if(binMode == DCAMPROP_BINNING__8 ) return 8 ;
    if(binMode == DCAMPROP_BINNING__16) return 16;

    manage_error( deb, "Incoherent binning mode.", DCAMERR_NONE, "GetBinningFromMode", "binning mode = %d", binMode);
    THROW_HW_ERROR(Error) << "Incoherent binning mode.";
}

//-----------------------------------------------------------------------------
/// Tells if binning is available
/*!
@return always true, hw binning mode is supported
*/
//-----------------------------------------------------------------------------
bool Camera::isBinningAvailable()
{
    DEB_MEMBER_FUNCT();
    return true;
}


//-----------------------------------------------------------------------------
/// return the detector pixel size in meter
//-----------------------------------------------------------------------------
void Camera::getPixelSize(double& sizex,	///< [out] horizontal pixel size
						  double& sizey)	///< [out] vertical   pixel size
{
    DEB_MEMBER_FUNCT();
    
    sizex = Camera::g_OrcaPixelSize;
    sizey = Camera::g_OrcaPixelSize;
    DEB_RETURN() << DEB_VAR2(sizex, sizey); 
}


//-----------------------------------------------------------------------------
/// reset the camera, no hw reset available on Hamamatsu camera
//-----------------------------------------------------------------------------
void Camera::reset()
{
    DEB_MEMBER_FUNCT();
    return;
}


//-----------------------------------------------------------------------------
///    initialise controller with speeds and preamp gain
//-----------------------------------------------------------------------------
void Camera::initialiseController()
{
    DEB_MEMBER_FUNCT();

	DEB_TRACE() << g_TraceLineSeparator.c_str();

	// Create the list of available binning modes from camera capabilities
    DCAMERR             err   ;
    DCAMDEV_CAPABILITY	devcap;

	memset( &devcap, 0, sizeof(DCAMDEV_CAPABILITY) );
	devcap.size	= sizeof(DCAMDEV_CAPABILITY);

	err = dcamdev_getcapability( m_camera_handle, &devcap );
	if( failed(err) )
	{
        manage_error( deb, "Failed to get capabilities", err, "dcamdev_getcapability");
        THROW_HW_ERROR(Error) << "Failed to get capabilities";
	}

	BOOL bTimestamp  = (devcap.capflag & DCAMDEV_CAPFLAG_TIMESTAMP ) ? TRUE : FALSE;
	BOOL bFramestamp = (devcap.capflag & DCAMDEV_CAPFLAG_FRAMESTAMP) ? TRUE : FALSE;

    //---------------------------------------------------------------------
	// Create the list of available binning modes from camera capabilities
    {
        FeatureInfos FeatureObj;

	    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_BINNING", DCAM_IDPROP_BINNING, FeatureObj ) )
	    {
            manage_error( deb, "Failed to get binning modes");
            THROW_HW_ERROR(Error) << "Failed to get binning modes";
	    }

    	DEB_TRACE() << g_TraceLineSeparator.c_str();
        FeatureObj.traceModePossibleValues();

        if(FeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__1 ))) m_vectBinnings.push_back(1 );
        if(FeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__2 ))) m_vectBinnings.push_back(2 );
        if(FeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__4 ))) m_vectBinnings.push_back(4 );
        if(FeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__8 ))) m_vectBinnings.push_back(8 );
        if(FeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__16))) m_vectBinnings.push_back(16);

        if(m_vectBinnings.size() == 0)
        {
            manage_error( deb, "Failed to get binning modes - none found");
            THROW_HW_ERROR(Error) << "Failed to get binning modes - none found";
        }

		// Create the binning object with the maximum possible value
		int max = *std::max_element(m_vectBinnings.begin(), m_vectBinnings.end());
		m_bin_max = Bin(max, max);
    }

	// Display obtained values - Binning modes
    DEB_TRACE() << "Selected binning mode:";

    vector<int>::const_iterator iterBinningMode = m_vectBinnings.begin();
    while (m_vectBinnings.end()!=iterBinningMode)
    {
        DEB_TRACE() << *iterBinningMode;
        ++iterBinningMode;
    }

    //---------------------------------------------------------------------
	// Create the list of available trigger modes from camera capabilities
    FeatureInfos TriggerSourceFeatureObj;
    FeatureInfos TriggerActiveFeatureObj;
    FeatureInfos TriggerModeFeatureObj  ;

    // trigger source
    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_TRIGGERSOURCE", DCAM_IDPROP_TRIGGERSOURCE, TriggerSourceFeatureObj ) )
    {
        manage_error( deb, "Failed to get trigger source modes");
        THROW_HW_ERROR(Error) << "Failed to get trigger source modes";
    }

	DEB_TRACE() << g_TraceLineSeparator.c_str();
    TriggerSourceFeatureObj.traceModePossibleValues();

    // trigger active
    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_TRIGGERACTIVE", DCAM_IDPROP_TRIGGERACTIVE, TriggerActiveFeatureObj ) )
    {
        manage_error( deb, "Failed to get trigger active modes");
        THROW_HW_ERROR(Error) << "Failed to get trigger active modes";
    }

	DEB_TRACE() << g_TraceLineSeparator.c_str();
    TriggerActiveFeatureObj.traceModePossibleValues();

    // trigger mode
    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_TRIGGER_MODE", DCAM_IDPROP_TRIGGER_MODE, TriggerModeFeatureObj ) )
    {
        manage_error( deb, "Failed to get trigger mode modes");
        THROW_HW_ERROR(Error) << "Failed to get trigger mode modes";
    }

	DEB_TRACE() << g_TraceLineSeparator.c_str();
    TriggerModeFeatureObj.traceModePossibleValues();

    // IntTrig
    if((TriggerSourceFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__INTERNAL ))) &&
       (TriggerActiveFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__EDGE     ))) &&
       (TriggerModeFeatureObj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL    ))))
        m_map_trig_modes[IntTrig] = true;

    // IntTrigMult
    if((TriggerSourceFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__INTERNAL ))) &&
       (TriggerActiveFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__EDGE     ))) &&
       (TriggerModeFeatureObj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL    ))))
        m_map_trig_modes[IntTrigMult] = true;

    // ExtTrigReadout
    if((TriggerSourceFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__EXTERNAL   ))) &&
       (TriggerActiveFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__SYNCREADOUT))) &&
       (TriggerModeFeatureObj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL      ))))
        m_map_trig_modes[ExtTrigReadout] = true;

    // ExtTrigSingle
    if((TriggerSourceFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__EXTERNAL))) &&
       (TriggerActiveFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__EDGE    ))) &&
       (TriggerModeFeatureObj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__START    ))))
        m_map_trig_modes[ExtTrigSingle] = true;

    // ExtTrigMult
    if((TriggerSourceFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__EXTERNAL))) &&
       (TriggerActiveFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__EDGE    ))) &&
       (TriggerModeFeatureObj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL   ))))
        m_map_trig_modes[ExtTrigMult] = true;

    // ExtGate
    if((TriggerSourceFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__EXTERNAL))) &&
       (TriggerActiveFeatureObj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__LEVEL   ))) &&
       (TriggerModeFeatureObj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL   ))))
        m_map_trig_modes[ExtGate] = true;

	// Display obtained values - Trigger modes
	trigOptionsMap::const_iterator iter = m_map_trig_modes.begin();
	DEB_TRACE() << "Trigger modes:";

    while (m_map_trig_modes.end()!=iter)
	{
		DEB_TRACE() << ">" << m_map_triggerMode[iter->first];
		++iter;
	}

    //---------------------------------------------------------------------
	// Forcing the trigger polarity to positive value
    setTriggerPolarity(Trigger_Polarity_Positive);

    //---------------------------------------------------------------------
	// Retrieve exposure time
    {
        FeatureInfos FeatureObj;

	    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_EXPOSURETIME", DCAM_IDPROP_EXPOSURETIME, FeatureObj ) )
	    {
            manage_error( deb, "Failed to get exposure time");
            THROW_HW_ERROR(Error) << "Failed to get exposure time";
	    }

        m_exp_time_max = FeatureObj.m_max;

	    // Display obtained values - Exposure time
	    DEB_TRACE() << "Min exposure time: " << FeatureObj.m_min;
	    DEB_TRACE() << "Max exposure time: " << FeatureObj.m_max;
    }

    //---------------------------------------------------------------------
	// Checking ROI properties
    {
        DEB_TRACE() << g_TraceLineSeparator.c_str();
        traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHPOS" , DCAM_IDPROP_SUBARRAYHPOS , &m_FeaturePosx );
    	DEB_TRACE() << g_TraceLineSeparator.c_str();
        traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVPOS" , DCAM_IDPROP_SUBARRAYVPOS , &m_FeaturePosy );
    	DEB_TRACE() << g_TraceLineSeparator.c_str();
        traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHSIZE", DCAM_IDPROP_SUBARRAYHSIZE, &m_FeatureSizex);
    	DEB_TRACE() << g_TraceLineSeparator.c_str();
        traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVSIZE", DCAM_IDPROP_SUBARRAYVSIZE, &m_FeatureSizey);
    }
}

//-----------------------------------------------------------------------------
/// Get the dcamdsk trigger mode value associated to the given Lima TrigMode 
/*!
sets dcamdsk trigger option with its property.
@return true if an associated value was found
*/
//-----------------------------------------------------------------------------
bool Camera::getTriggerMode(const TrigMode trig_mode) const ///< [in]  lima trigger mode value
{
    bool result = false;
    trigOptionsMap::const_iterator iterFind = m_map_trig_modes.find(trig_mode);

	if (m_map_trig_modes.end()!=iterFind)
	{
        result = true;
	}

    return result;
}


//-----------------------------------------------------------------------------
/// Set the readout speed value
/*!
@remark possible values are 1 or 2
*/
//-----------------------------------------------------------------------------
void Camera::setReadoutSpeed(const short int readoutSpeed) ///< [in] new readout speed
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(readoutSpeed);

    DCAMERR err;

    // set the readout speed
	err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_READOUTSPEED, static_cast<double>(readoutSpeed) );
	if( failed(err) )
	{
        manage_error( deb, "Failed to set readout speed", err, 
                      "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_SUBARRAYVPOS, VALUE=%d",static_cast<int>(readoutSpeed));
        THROW_HW_ERROR(Error) << "Failed to set readout speed";
	}

	m_read_mode = readoutSpeed;
}


//-----------------------------------------------------------------------------
/// Get the readout speed value
//-----------------------------------------------------------------------------
void Camera::getReadoutSpeed(short int& readoutSpeed)		///< [out] current readout speed
{
	DEB_MEMBER_FUNCT();
	
	readoutSpeed = m_read_mode;
}


//-----------------------------------------------------------------------------
/// Get the lost frames value
//-----------------------------------------------------------------------------
void Camera::getLostFrames(unsigned long int& lostFrames)	///< [out] current lost frames
{
	DEB_MEMBER_FUNCT();

	lostFrames = m_lostFramesCount;
}


//-----------------------------------------------------------------------------
/// Get the lost frames value
//-----------------------------------------------------------------------------
void Camera::getFPS(double& fps)							///< [out] last computed fps
{
	DEB_MEMBER_FUNCT();

	fps = m_fps;
}

//-----------------------------------------------------------------------------
/// CAPTURE
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/// Set detector for single image acquisition
//-----------------------------------------------------------------------------
void Camera::prepareAcq()
{
	DEB_MEMBER_FUNCT();
}


//-----------------------------------------------------------------------------
///  start the acquistion
//-----------------------------------------------------------------------------
void Camera::startAcq()
{
    DEB_MEMBER_FUNCT();
	DEB_TRACE() << g_TraceLineSeparator.c_str();

    DCAMERR err              = DCAMERR_NONE;
    int32   number_of_buffer = Camera::g_DCAMFrameBufferSize;
	long    status;

    traceAllRoi();

	// Allocate frames to capture
	err = dcambuf_alloc( m_camera_handle, number_of_buffer );

	if( failed(err) )
	{
        manage_error( deb, "Failed to allocate frames for the capture", err, 
                      "dcambuf_alloc", "number_of_buffer=%d",number_of_buffer);
        THROW_HW_ERROR(Error) << "Cannot allocate frame for capturing (dcam_allocframe()).";
	}
    else
    {
		DEB_TRACE() << "Allocated frames: " << number_of_buffer;
    }

    m_image_number = 0;
	m_fps          = 0;

    // --- check first the acquisition is idle
	err = dcamcap_status( m_camera_handle, &status );
	if( failed(err) )
	{
        manage_error( deb, "Cannot get camera status", err, "dcamcap_status");
        THROW_HW_ERROR(Error) << "Cannot get camera status";
	}

	if (DCAMCAP_STATUS_READY != status)
	{
		DEB_ERROR() << "Cannot start acquisition, camera is not ready";
        THROW_HW_ERROR(Error) << "Cannot start acquisition, camera is not ready";
	}

	// init force stop flag before starting acq thread
	m_thread.m_force_stop = false;

	m_thread.sendCmd      (CameraThread::StartAcq);
	m_thread.waitNotStatus(CameraThread::Ready   );
}


//-----------------------------------------------------------------------------
/// stop the acquisition
//-----------------------------------------------------------------------------
void Camera::stopAcq()
{
    DEB_MEMBER_FUNCT();
	DEB_TRACE() << g_TraceLineSeparator.c_str();

    execStopAcq();

    if(m_thread.getStatus() != CameraThread::Fault)
    {
	    // Wait for thread to finish
	    m_thread.waitStatus(CameraThread::Ready);
    }
    else
    {
        // aborting the thread
        m_thread.abort();
    }
}

//-----------------------------------------------------------------------------
/// utility thread
//-----------------------------------------------------------------------------
//---------------------------------------------------------------------------------------
//! Camera::CameraThread::CameraThread()
//---------------------------------------------------------------------------------------
Camera::CameraThread::CameraThread(Camera * cam)
: m_cam(cam)
{
	DEB_MEMBER_FUNCT();
	m_force_stop = false;
    m_waitHandle = NULL ;
	DEB_TRACE() << "DONE";
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::start()
//---------------------------------------------------------------------------------------
void Camera::CameraThread::start()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "BEGIN";
	CmdThread::start();
	waitStatus(Ready);
	DEB_TRACE() << "END";
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::init()
//---------------------------------------------------------------------------------------
void Camera::CameraThread::init()
{
	DEB_MEMBER_FUNCT();
    setStatus(CameraThread::Ready);
	DEB_TRACE() << "DONE";
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::abort()
//---------------------------------------------------------------------------------------
void Camera::CameraThread::abort()
{
	DEB_MEMBER_FUNCT();
    CmdThread::abort();
	DEB_TRACE() << "DONE";
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::execCmd()
//---------------------------------------------------------------------------------------
void Camera::CameraThread::execCmd(int cmd)
{
	DEB_MEMBER_FUNCT();

    int status = getStatus();

    try
    {
        switch (cmd)
	    {
		    case StartAcq:
			    if (status != Ready)
				    throw LIMA_HW_EXC(InvalidValue, "Not Ready to StartAcq");
			    execStartAcq();
			    break;
	    }
    }
    catch (...)
    {
    }
}

//---------------------------------------------------------------------------------------
//! Camera::execStopAcq()
//---------------------------------------------------------------------------------------
void Camera::execStopAcq()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "executing StopAcq command...";

    if((getStatus() != Camera::Exposure) && (getStatus() != Camera::Readout))
    	DEB_WARNING() << "Execute a stop acq command but not in [exposure,Readout] status. ThreadStatus=" << m_thread.getStatus();

    m_thread.abortCapture();
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::checkStatusBeforeCapturing()
// throws an exception if the status is incorrect
//---------------------------------------------------------------------------------------
void Camera::CameraThread::checkStatusBeforeCapturing() const
{
	DEB_MEMBER_FUNCT();

    long	status;
    DCAMERR err   ;

    // Check the status and stop capturing if capturing is already started.
	err = dcamcap_status( m_cam->m_camera_handle, &status );

    if( failed(err) )
	{
        static_manage_error( m_cam, deb, "Cannot get status", err, "dcamcap_status");
        THROW_HW_ERROR(Error) << "Cannot get status";
	}

	if (DCAMCAP_STATUS_READY != status) // After allocframe, the camera status should be READY
	{
		DEB_ERROR() << "Camera could not be set in the proper state for image capture";
		THROW_HW_ERROR(Error) << "Camera could not be set in the proper state for image capture";
	}
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::createWaitHandle()
// throws an exception in case of problem
//---------------------------------------------------------------------------------------
void Camera::CameraThread::createWaitHandle(HDCAMWAIT & waitHandle) const
{
	DEB_MEMBER_FUNCT();

    DCAMERR err;

    waitHandle = NULL;

	// open wait handle
	DCAMWAIT_OPEN waitOpenHandle;

    memset( &waitOpenHandle, 0, sizeof(DCAMWAIT_OPEN) );
	waitOpenHandle.size	 = sizeof(DCAMWAIT_OPEN) ;
	waitOpenHandle.hdcam = m_cam->m_camera_handle;

	err = dcamwait_open( &waitOpenHandle );

	if( failed(err) )
	{
        static_manage_error( m_cam, deb, "Cannot create the wait handle", err, "dcamwait_open");
        THROW_HW_ERROR(Error) << "Cannot create the wait handle";
	}
	else
	{
		waitHandle = waitOpenHandle.hwait; // after this, no need to keep the waitopen structure, freed by the stack
    }
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::releaseWaitHandle()
// does not throw exception in case of problem but trace an error.
//---------------------------------------------------------------------------------------
void Camera::CameraThread::releaseWaitHandle(HDCAMWAIT & waitHandle) const
{
	DEB_MEMBER_FUNCT();

    DCAMERR err;

    err = dcamwait_close( waitHandle );

	if( failed(err) )
	{
        static_manage_error( m_cam, deb, "Cannot release the wait handle", err, "dcamwait_close");
	}
    
    waitHandle = NULL;
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::abortCapture()
// Stop the capture, releasing the Wait handle and setting the boolean stop flag.
//---------------------------------------------------------------------------------------
void Camera::CameraThread::abortCapture(void)
{
	DEB_MEMBER_FUNCT();

    DCAMERR err;

	m_cam->m_mutexForceStop.lock();

    if(m_waitHandle != NULL)
    {
        err = dcamwait_abort( m_waitHandle );
    }

    if( failed(err) )
    {
        static_manage_error( m_cam, deb, "Cannot abort wait handle.", err, "dcamwait_abort");
    }

	m_force_stop = true;
    m_cam->m_mutexForceStop.unlock();
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::getTransfertInfo()
// throws an exception in case of problem
//---------------------------------------------------------------------------------------
void Camera::CameraThread::getTransfertInfo(int32 & frameIndex,
                                            int32 & frameCount)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;

    // transferinfo param
    DCAMCAP_TRANSFERINFO transferinfo;
    memset( &transferinfo, 0, sizeof(DCAMCAP_TRANSFERINFO) );
    transferinfo.size = sizeof(DCAMCAP_TRANSFERINFO);

    // get number of captured images
    err = dcamcap_transferinfo( m_cam->m_camera_handle, &transferinfo );

    if( failed(err) )
    {
        setStatus(CameraThread::Fault);

        static_manage_error( m_cam, deb, "Cannot get transfer info.", err, "dcamcap_transferinfo");
        THROW_HW_ERROR(Error) << "Cannot get transfer info.";
    }
    else
    {
        frameIndex = transferinfo.nNewestFrameIndex;
        frameCount = transferinfo.nFrameCount      ;
    }
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::execStartAcq()
//---------------------------------------------------------------------------------------
void Camera::CameraThread::execStartAcq()
{
	DEB_MEMBER_FUNCT();

    DCAMERR   err         = DCAMERR_NONE;
    bool      continueAcq = true        ;
    Timestamp T0          ;
    Timestamp T1          ;
    Timestamp DeltaT      ;

	DEB_TRACE() << m_cam->g_TraceLineSeparator.c_str();
	DEB_TRACE() << "CameraThread::execStartAcq - BEGIN";
    setStatus(CameraThread::Exposure);

	StdBufferCbMgr& buffer_mgr = m_cam->m_buffer_ctrl_obj.getBuffer();
	buffer_mgr.setStartTimestamp(Timestamp::now());

    DEB_TRACE() << "Run";

    // Write some informations about the camera before the acquisition
    bool ViewModeEnabled;
    m_cam->getViewMode(ViewModeEnabled);

    if(ViewModeEnabled)
    {
    	DEB_TRACE() << "View mode activated";

        int    ViewIndex   = 0;
	    double Viewexposure;

        while(ViewIndex < m_cam->m_MaxViews)
        {
            m_cam->getViewExpTime(ViewIndex, Viewexposure);
            DEB_TRACE() << "View " << (ViewIndex+1) << " exposure : " << Viewexposure;
            ViewIndex++;
        }
    }
    else
    {
    	DEB_TRACE() << "View mode unactivated";
	    double exposure;

        m_cam->getExpTime(exposure);
        DEB_TRACE() << "exposure : " << exposure;
    }

    // Check the status and stop capturing if capturing is already started.
    checkStatusBeforeCapturing();

    // Create the wait handle
    createWaitHandle(m_waitHandle);

	// Start the real capture (this function returns immediately)
	err = dcamcap_start( m_cam->m_camera_handle, DCAMCAP_START_SEQUENCE );

    if( failed(err) )
	{
	    dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
        releaseWaitHandle( m_waitHandle           );        
    	dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
        setStatus        ( CameraThread::Fault    );

        std::string errorText = static_manage_error( m_cam, deb, "Cannot start the capture", err, "dcamcap_start");
        REPORT_EVENT(errorText);
        THROW_HW_ERROR(Error) << "Frame capture failed";
	}

	// Transfer the images as they are beeing captured from dcam_sdk buffer to Lima
	m_cam->m_lostFramesCount = 0;

    int32 lastFrameCount = 0 ;
    int32 frameCount     = 0 ;
    int32 lastFrameIndex = -1;
    int32 frameIndex     = 0 ;
	
	// Main acquisition loop
    while (	( continueAcq )	&&
            ( (0==m_cam->m_nb_frames) || (m_cam->m_image_number < m_cam->m_nb_frames) ) )
    {
        setStatus(CameraThread::Exposure);

        // Timestamp before DCAM "snapshot"
		T0 = Timestamp::now();

		// Check first if acq. has been stopped
		if (m_force_stop)
        {
			//abort the current acquisition
			continueAcq  = false;
			m_force_stop = false;
			continue;
        }

		// Wait for the next image to become available or the end of the capture by user
		// set wait param
		DCAMWAIT_START waitstart;
		memset( &waitstart, 0, sizeof(DCAMWAIT_START) );
		waitstart.size		= sizeof(DCAMWAIT_START);
		waitstart.eventmask	= DCAMWAIT_CAPEVENT_FRAMEREADY | DCAMWAIT_CAPEVENT_STOPPED;
		waitstart.timeout	= DCAMWAIT_TIMEOUT_INFINITE;

		// wait image
		err = dcamwait_start( m_waitHandle, &waitstart );

        if( failed(err) )
		{
			// If capture was aborted (by stopAcq() -> dcam_idle())
			if (DCAMERR_ABORT == err)
			{
				DEB_TRACE() << "DCAMERR_ABORT";
				continueAcq = false;
				continue;
			}
			else 
			if (DCAMERR_TIMEOUT == err)
			{
	            dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
                releaseWaitHandle( m_waitHandle           );        
    	        dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
                setStatus        ( CameraThread::Fault    );

                std::string errorText = static_manage_error( m_cam, deb, "Error during the frame capture wait", err, "dcamwait_start");
                REPORT_EVENT(errorText);

				THROW_HW_ERROR(Error) << "DCAMERR_TIMEOUT";
			}
            else
			if(( DCAMERR_LOSTFRAME == err) || (DCAMERR_MISSINGFRAME_TROUBLE == err) )
			{
                static_manage_error( m_cam, deb, "Error during the frame capture wait", err, "dcamwait_start");
				++m_cam->m_lostFramesCount;
				continue;
            }
            else
			{					
	            dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
                releaseWaitHandle( m_waitHandle           );        
    	        dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
                setStatus        ( CameraThread::Fault    );

                std::string errorText = static_manage_error( m_cam, deb, "Error during the frame capture wait", err, "dcamwait_start");
                REPORT_EVENT(errorText);
                THROW_HW_ERROR(Error) << "Error during the frame capture wait";
			}
		}
        else
        // wait succeeded
        {
			if (waitstart.eventhappened & DCAMWAIT_CAPEVENT_STOPPED)
			{
				DEB_TRACE() << "DCAM_EVENT_CAPTUREEND";
				continueAcq = false;
				continue;
			}
		}

		if (m_force_stop)
		{
			//abort the current acqusition
			continueAcq  = false;
			m_force_stop = false;
			break;
		}

		// Transfert the new images
        setStatus(CameraThread::Readout);
		
		int32 deltaFrames = 0;

        getTransfertInfo(frameIndex, frameCount);

        // manage the frame info
        {
			deltaFrames = frameCount-lastFrameCount;
        	DEB_TRACE() << g_TraceLittleLineSeparator.c_str();
			DEB_TRACE() << "m_image_number > "  << m_cam->m_image_number 
			            << " lastFrameIndex > " << lastFrameIndex
			            << " frameIndex     > " << frameIndex
			            << " frameCount     > " << frameCount 
                        << " (delta: " << deltaFrames << ")";

			if (0 == frameCount)
			{
	            dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
                releaseWaitHandle( m_waitHandle           );        
    	        dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
                setStatus        ( CameraThread::Fault    );

                std::string errorText = "No image captured.";
				DEB_ERROR() << errorText;
                REPORT_EVENT(errorText);
				THROW_HW_ERROR(Error) << "No image captured.";
			}
			if (deltaFrames > Camera::g_DCAMFrameBufferSize)
			{
				m_cam->m_lostFramesCount += deltaFrames;
				DEB_TRACE() << "deltaFrames > Camera::g_DCAMFrameBufferSize (" << deltaFrames << ")";
			}
			lastFrameCount = frameCount;
		}

		// Check if acquisition was stopped & abort the current acqusition
		if (m_force_stop)
		{
			continueAcq  = false;
			m_force_stop = false;
			break;
		}

        int nbFrameToCopy = 0;

        try
        {
            m_cam->m_mutexForceStop.lock();

            // Copy frames from DCAM_SDK to LiMa
            nbFrameToCopy  = (deltaFrames < Camera::g_DCAMFrameBufferSize) ? deltaFrames : Camera::g_DCAMFrameBufferSize; // if more than Camera::g_DCAMFrameBufferSize have arrived

            continueAcq    = copyFrames( (lastFrameIndex+1)% Camera::g_DCAMFrameBufferSize, // index of the first image to copy from the ring buffer
                                          nbFrameToCopy                                   ,
                                          buffer_mgr                                      );
            lastFrameIndex = frameIndex;
        }
        catch (...)
        {
		    // be sure to unlock the mutex before throwing the exception!
		    m_cam->m_mutexForceStop.unlock();

            dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
            releaseWaitHandle( m_waitHandle           );        
	        dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
            setStatus        ( CameraThread::Fault    );

            throw;
        }

	    m_cam->m_mutexForceStop.unlock();
		
		// Update fps
		{
			T1 = Timestamp::now();

			DeltaT = T1 - T0;
			if (DeltaT > 0.0) m_cam->m_fps = 1.0 * nbFrameToCopy / DeltaT;
		}

    } // end of acquisition loop

	// Stop the acquisition
	err = dcamcap_stop( m_cam->m_camera_handle);

	if( failed(err) )
	{
        releaseWaitHandle( m_waitHandle           );        
        dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
        setStatus        ( CameraThread::Fault    );

        std::string errorText = static_manage_error( m_cam, deb, "Cannot stop acquisition.", err, "dcamcap_stop");
        REPORT_EVENT(errorText);
        THROW_HW_ERROR(Error) << "Cannot stop acquisition.";
	}

    // release the wait handle
    releaseWaitHandle( m_waitHandle );        

	// Release the capture frame
	err = dcambuf_release( m_cam->m_camera_handle );

	if( failed(err) )
	{
        setStatus(CameraThread::Fault);
        std::string errorText = static_manage_error( m_cam, deb, "Unable to free capture frame", err, "dcambuf_release");
        REPORT_EVENT(errorText);
        THROW_HW_ERROR(Error) << "Unable to free capture frame";
	}
    else
    {
		DEB_TRACE() << "dcambuf_release success.";
    }

	DEB_TRACE() << g_TraceLineSeparator.c_str();
	DEB_TRACE() << "Total time (s): " << (Timestamp::now() - T0);
	DEB_TRACE() << "FPS           : " << int(m_cam->m_image_number / (Timestamp::now() - T0) );
	DEB_TRACE() << "Lost frames   : " << m_cam->m_lostFramesCount; 

    setStatus(CameraThread::Ready);
	DEB_TRACE() << "CameraThread::execStartAcq - END";
}


//-----------------------------------------------------------------------------
// Copy the given frames to the buffer manager
//-----------------------------------------------------------------------------
bool Camera::CameraThread::copyFrames(const int        iFrameBegin, ///< [in] index of the frame where to begin copy
							          const int        iFrameCount, ///< [in] number of frames to copy
								      StdBufferCbMgr & buffer_mgr ) ///< [in] buffer manager object
{
	DEB_MEMBER_FUNCT();
	
	DEB_TRACE() << "copyFrames(" << iFrameBegin << ", nb:" << iFrameCount << ")";

    DCAMERR  err         ;
    FrameDim frame_dim   = buffer_mgr.getFrameDim();
    Size     frame_size  = frame_dim.getSize     ();
    int      height      = frame_size.getHeight  ();
    int      memSize     = frame_dim.getMemSize  ();
    bool     CopySuccess = false                   ;
    int      iFrameIndex = iFrameBegin             ; // Index of frame in the DCAM cycling buffer

	for  (int cptFrame = 1 ; cptFrame <= iFrameCount ; cptFrame++)
	{
        void     * dst         = buffer_mgr.getFrameBufferPtr(m_cam->m_image_number);
		void     * src         ;
		long int   sRowbytes   ;
		bool       bImageCopied;

	    // prepare frame stucture
	    DCAMBUF_FRAME bufframe;
	    memset( &bufframe, 0, sizeof(bufframe) );
	    bufframe.size	= sizeof(bufframe);
	    bufframe.iFrame	= iFrameIndex     ;

	    // access image
	    err = dcambuf_lockframe( m_cam->m_camera_handle, &bufframe );

	    if( failed(err) )
	    {
			bImageCopied = false;
            setStatus(CameraThread::Fault);

            std::string errorText = static_manage_error( m_cam, deb, "Unable to lock frame data", err, "dcambuf_lockframe");
            REPORT_EVENT(errorText);
            THROW_HW_ERROR(Error) << "Unable to lock frame data";
	    }
        else
        {
            sRowbytes = bufframe.rowbytes;
            src       = bufframe.buf     ;

    		if(sRowbytes * height != memSize)
            {
    			bImageCopied = false;
                static_manage_trace( m_cam, deb, "Incoherent sizes during frame copy process", DCAMERR_NONE,
                                     "copyFrames", "source size %d, dest size %d", memSize, sRowbytes * height);
            }
            else
            {
                memcpy( dst, src, sRowbytes * height );
			    bImageCopied = true;

                DEB_TRACE() << "Aquired m_image_number > " << m_cam->m_image_number
                            << " (frameIndex:"             << iFrameIndex           << ")" 
                            << " (rowbytes:"               << sRowbytes             << ")"
                            << " (height:"                 << height                << ")";
            }
        }

		if (!bImageCopied)
		{
            setStatus(CameraThread::Fault);
			CopySuccess = false;

            std::string errorText = static_manage_error( m_cam, deb, "Cannot get image.", DCAMERR_NONE, "copyFrames");
            REPORT_EVENT(errorText);
            THROW_HW_ERROR(Error) << "Cannot get image.";
			break;
		}
		else
		{
			HwFrameInfoType frame_info;
			frame_info.acq_frame_nb = m_cam->m_image_number;		

			// Make the new frame available
			if ( (0==m_cam->m_nb_frames) || (m_cam->m_image_number < m_cam->m_nb_frames) )
			{
				CopySuccess = buffer_mgr.newFrameReady(frame_info);
				++m_cam->m_image_number;
			}

			// Done capturing (SNAP)
			if ( (m_cam->m_image_number == m_cam->m_nb_frames) && (0!=m_cam->m_nb_frames))
			{
				DEB_TRACE() << "All images captured.";
				break;
			}
		}

		iFrameIndex = (iFrameIndex+1) % Camera::g_DCAMFrameBufferSize;
	}

	DEB_TRACE() << DEB_VAR1(CopySuccess);
	return CopySuccess;
}

//-----------------------------------------------------------------------------
/// Return the current number of views for this camera
//-----------------------------------------------------------------------------
int Camera::getNumberofViews(void)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err   ;
    int32   nView = 0  ;
    double  v     = 0.0;

	err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_NUMBEROF_VIEW, &v );
    if( failed(err) )
    {
        manage_trace( deb, "Unable to retrieve the number of possible W-VIEW", err, "dcamprop_getvalue - DCAM_IDPROP_NUMBEROF_VIEW");
    }
    else    
    {
    	nView = static_cast<int32>(v);
    }

    DEB_TRACE() << DEB_VAR1(nView);

    return  nView;
}


//-----------------------------------------------------------------------------
/// Return the maximum number of views for this camera
//-----------------------------------------------------------------------------
int Camera::getMaxNumberofViews(void)
{
    DEB_MEMBER_FUNCT();

    FeatureInfos FeatureObj;
    int32        nView     = 0;

    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_NUMBEROF_VIEW", DCAM_IDPROP_NUMBEROF_VIEW, FeatureObj ) )
    {
        manage_trace( deb, "Failed to get number of view");
    }
    else
    {
	    DEB_TRACE() << g_TraceLineSeparator.c_str();
        FeatureObj.traceGeneralInformations();

        nView = static_cast<int32>(FeatureObj.m_max);
    }

    DEB_TRACE() << DEB_VAR1(nView);

    return  nView;
}

//-----------------------------------------------------------------------------
/// Set the W-VIEW mode
//-----------------------------------------------------------------------------
void Camera::setViewMode(bool in_ViewModeActivated, ///< [in] view mode activation or not
                         int  in_ViewsNumber      ) ///< [in] number of views if view mode activated
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR2(in_ViewModeActivated, in_ViewsNumber);

    DCAMERR  err;

    if(in_ViewModeActivated)
    {
        if(m_MaxViews > 1)
        {
            // checking if the W-View mode is possible for this camera
	        if( m_MaxViews < in_ViewsNumber )
            {
                manage_error( deb, "Unable to activate W-VIEW mode", DCAMERR_NONE, NULL, "max views number %d, needed %d", m_MaxViews, in_ViewsNumber);
                THROW_HW_ERROR(Error) << "Unable to activate W-VIEW mode";
            }

		    // set split view mode
		    err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_SENSORMODE, static_cast<double>(DCAMPROP_SENSORMODE__SPLITVIEW) );

            if( failed(err) )
		    {
                manage_error( deb, "Unable to activate W-VIEW mode", err, 
                                   "dcamprop_setvalue", "DCAM_IDPROP_SENSORMODE - DCAMPROP_SENSORMODE__SPLITVIEW");
                THROW_HW_ERROR(Error) << "Unable to activate W-VIEW mode";
		    }

            m_ViewModeEnabled = true          ; // W-View mode with splitting image
            m_ViewNumber      = in_ViewsNumber; // number of W-Views

            manage_trace( deb, "W-VIEW mode activated", DCAMERR_NONE, NULL, "views number %d", in_ViewsNumber);
        }
        else
        // W-View is not supported for this camera
        {
            manage_error( deb, "Cannot set the W-View mode - This camera does not support the W-View mode.", DCAMERR_NONE);
            THROW_HW_ERROR(Error) << "Cannot set the W-View mode - This camera does not support the W-View mode.";
        }
    }
    else
    // unactivated
    {
		// set area mode
		err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_SENSORMODE, static_cast<double>(DCAMPROP_SENSORMODE__AREA) );

        if( failed(err) )
		{
            manage_error( deb, "Unable to activate AREA mode", err, 
                               "dcamprop_setvalue", "DCAM_IDPROP_SENSORMODE - DCAMPROP_SENSORMODE__AREA");
            THROW_HW_ERROR(Error) << "Unable to activate AREA mode";
		}

        // if view mode was activated, we rewrite the exposure time
        if (m_ViewModeEnabled)
        {
            setExpTime(m_exp_time); // rewrite the value in the camera
        }

        m_ViewModeEnabled = false; // W-View mode with splitting image
        m_ViewNumber      = 0    ; // number of W-Views

        manage_trace( deb, "W-VIEW mode unactivated");
    }
}

//-----------------------------------------------------------------------------
/// Set the W-VIEW mode
//-----------------------------------------------------------------------------
void Camera::setViewMode(bool flag)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(flag);

    setViewMode(flag, 2); // 2 views max for ORCA
}

//-----------------------------------------------------------------------------
/// Get the W-VIEW mode
//-----------------------------------------------------------------------------
void Camera::getViewMode(bool& flag)
{
    DEB_MEMBER_FUNCT();
    double  sensormode;
    DCAMERR err       ;

    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORMODE, &sensormode);

    if( failed(err) )
    {
        manage_error( deb, "Cannot get sensor mode", err, 
                      "dcamprop_getvalue", "IDPROP=DCAM_IDPROP_SENSORMODE");
        THROW_HW_ERROR(Error) << "Cannot get sensor mode";
    }

    flag = (static_cast<int>(sensormode) == DCAMPROP_SENSORMODE__SPLITVIEW);
}

//-----------------------------------------------------------------------------
/// Set the new exposure time for a view
//-----------------------------------------------------------------------------
void Camera::setViewExpTime(int    ViewIndex, ///< [in] view index [0...m_MaxViews[
                            double exp_time ) ///< [in] exposure time to set
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR2(ViewIndex, exp_time);

    DCAMERR err;

    // W-View is not supported for this camera
    if(m_MaxViews < 2)
    {
        manage_error( deb, "Cannot set view exposure time - This camera does not support the W-View mode.", DCAMERR_NONE);
        THROW_HW_ERROR(Error) << "Cannot set view exposure time - This camera does not support the W-View mode.";
    }
    else
    if(ViewIndex < m_MaxViews)
    {
        // Changing a View exposure time is not allowed if W-VIEW mode is not activated but we keep the value
        if(!m_ViewModeEnabled)
        {
            manage_error( deb, "Cannot change W-View exposure time when W-VIEW mode is unactivated!", DCAMERR_NONE, "setViewExpTime");
        }
        else
        {
            // set the exposure time
            err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_VIEW_((ViewIndex + 1), DCAM_IDPROP_EXPOSURETIME), exp_time);

            if( failed(err) )
            {
                manage_error( deb, "Cannot set view exposure time", err, 
                              "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_EXPOSURETIME, VIEW INDEX=%d, VALUE=%lf", ViewIndex, exp_time);
                THROW_HW_ERROR(Error) << "Cannot set view exposure time";
            }

            double tempexp_time;
            getViewExpTime(ViewIndex, tempexp_time);
            manage_trace( deb, "Changed View Exposure time", DCAMERR_NONE, NULL, "views index %d, exp:%lf >> real:%lf", ViewIndex, exp_time, tempexp_time);
        }

        m_ViewExpTime[ViewIndex] = exp_time;
    }
    else
    {
        manage_error( deb, "Cannot set view exposure time", DCAMERR_NONE, 
                      "", "VIEW INDEX=%d, MAX VIEWS=%d", ViewIndex, m_MaxViews);
        THROW_HW_ERROR(Error) << "Cannot set view exposure time";
    }
}

//-----------------------------------------------------------------------------
/// Get the current exposure time for a view
//-----------------------------------------------------------------------------
void Camera::getViewExpTime(int      ViewIndex, ///< [in] view index [0...m_MaxViews[
                            double & exp_time ) ///< [out] current exposure time
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(ViewIndex);

    DCAMERR err;

    // W-View is not supported for this camera
    if(m_MaxViews < 2)
    {
        exp_time = m_exp_time;
    }
    else
    if(ViewIndex < m_MaxViews)
    {
        double exposure;

        // if W-VIEW mode is not activated we cannot retrieve the current value 
        if(!m_ViewModeEnabled)
        {
            exposure = m_ViewExpTime[ViewIndex];
        }
        else
        {
            err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_VIEW_((ViewIndex + 1), DCAM_IDPROP_EXPOSURETIME), &exposure);

	        if( failed(err) )
	        {
                manage_error( deb, "Cannot get view exposure time", err, 
                              "dcamprop_getvalue", "IDPROP=DCAM_IDPROP_EXPOSURETIME, VIEW INDEX=%d", ViewIndex);
                THROW_HW_ERROR(Error) << "Cannot get view exposure time";
	        }
        }

	    exp_time = exposure;
    }
    else
    {
        manage_error( deb, "Cannot get view exposure time", DCAMERR_NONE, 
                      "", "VIEW INDEX=%d, MAX VIEWS=%d", ViewIndex, m_MaxViews);
        THROW_HW_ERROR(Error) << "Cannot get view exposure time";
    }
}

//-----------------------------------------------------------------------------
/// Get the minimum exposure time of all views
//-----------------------------------------------------------------------------
void Camera::getMinViewExpTime(double& exp_time) ///< [out] current exposure time
{
    DEB_MEMBER_FUNCT();

    // the exposure is the minimum of the views'exposure values
    int    ViewIndex   = 0   ;
	double exposure    = -1.0; // we search the minimum value - this is the not set value
    double Viewexposure; 

    if(m_MaxViews > 1)
    {
        while(ViewIndex < m_MaxViews)
        {
            getViewExpTime(ViewIndex, Viewexposure);
            
            if((exposure == -1.0) || (Viewexposure < exposure))
                exposure = Viewexposure; // sets the new minimum

            ViewIndex++;
        }
    }

    exp_time = exposure;
}

//-----------------------------------------------------------------------------
/// Set the new exposure time for the first view
//-----------------------------------------------------------------------------
void Camera::setViewExpTime1(double exp_time ) ///< [in] exposure time to set
{
    setViewExpTime(0, exp_time );
}

//-----------------------------------------------------------------------------
/// Set the new exposure time for the second view
//-----------------------------------------------------------------------------
void Camera::setViewExpTime2(double exp_time ) ///< [in] exposure time to set
{
    setViewExpTime(1, exp_time );
}

//-----------------------------------------------------------------------------
/// Get the current exposure time for the first view
//-----------------------------------------------------------------------------
void Camera::getViewExpTime1(double & exp_time ) ///< [out] current exposure time
{
    getViewExpTime(0, exp_time );
}

//-----------------------------------------------------------------------------
/// Get the current exposure time for the second view
//-----------------------------------------------------------------------------
void Camera::getViewExpTime2(double & exp_time ) ///< [out] current exposure time
{
    getViewExpTime(1, exp_time );
}

//-----------------------------------------------------------------------------
/// Set the blank mode of the Sync-readout trigger
//-----------------------------------------------------------------------------
void Camera::setSyncReadoutBlankMode(enum SyncReadOut_BlankMode in_SyncReadOutMode) ///< [in] type of sync-readout trigger's blank
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(in_SyncReadOutMode);

    DCAMERR  err ;
    int      mode;

    if(in_SyncReadOutMode == SyncReadOut_BlankMode_Standard)
    {
        mode = DCAMPROP_SYNCREADOUT_SYSTEMBLANK__STANDARD;
    }
    else
    if(in_SyncReadOutMode == SyncReadOut_BlankMode_Minimum)
    {
        mode = DCAMPROP_SYNCREADOUT_SYSTEMBLANK__MINIMUM;
    }
    else
	{
        manage_error( deb, "Unable to set the SyncReadout blank mode", DCAMERR_NONE, 
                           "", "in_SyncReadOutMode is unknown %d", static_cast<int>(in_SyncReadOutMode));
        THROW_HW_ERROR(Error) << "Unable to set the SyncReadout blank mode";
	}

    // set the mode
	err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_SYNCREADOUT_SYSTEMBLANK, static_cast<double>(mode) );

    if( failed(err) )
	{
        if((err == DCAMERR_INVALIDPROPERTYID)||(err == DCAMERR_NOTSUPPORT))
        {
            manage_trace( deb, "Unable to set the SyncReadout blank mode", err, 
                               "dcamprop_setvalue", "DCAM_IDPROP_SYNCREADOUT_SYSTEMBLANK %d", mode);
        }
        else
        {
            manage_error( deb, "Unable to set the SyncReadout blank mode", err, 
                               "dcamprop_setvalue", "DCAM_IDPROP_SYNCREADOUT_SYSTEMBLANK %d", mode);
            THROW_HW_ERROR(Error) << "Unable to set the SyncReadout blank mode";
        }
	}
}

//-----------------------------------------------------------------------------
/// Checking the ROI properties (min, max, step...)
//-----------------------------------------------------------------------------
void Camera::checkingROIproperties(void)
{
    DEB_MEMBER_FUNCT();

	// Checking ROI properties
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHPOS" , DCAM_IDPROP_SUBARRAYHPOS , NULL );
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVPOS" , DCAM_IDPROP_SUBARRAYVPOS , NULL );
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHSIZE", DCAM_IDPROP_SUBARRAYHSIZE, NULL );
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVSIZE", DCAM_IDPROP_SUBARRAYVSIZE, NULL );

	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHPOS VIEW1" , DCAM_IDPROP_VIEW_(1, DCAM_IDPROP_SUBARRAYHPOS ), NULL );
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVPOS VIEW1" , DCAM_IDPROP_VIEW_(1, DCAM_IDPROP_SUBARRAYVPOS ), NULL );
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHSIZE VIEW1", DCAM_IDPROP_VIEW_(1, DCAM_IDPROP_SUBARRAYHSIZE), NULL );
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVSIZE VIEW1", DCAM_IDPROP_VIEW_(1, DCAM_IDPROP_SUBARRAYVSIZE), NULL );

	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHPOS VIEW2" , DCAM_IDPROP_VIEW_(2, DCAM_IDPROP_SUBARRAYHPOS ), NULL );
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVPOS VIEW2" , DCAM_IDPROP_VIEW_(2, DCAM_IDPROP_SUBARRAYVPOS ), NULL );
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHSIZE VIEW2", DCAM_IDPROP_VIEW_(2, DCAM_IDPROP_SUBARRAYHSIZE), NULL );
	DEB_TRACE() << g_TraceLineSeparator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVSIZE VIEW2", DCAM_IDPROP_VIEW_(2, DCAM_IDPROP_SUBARRAYVSIZE), NULL );
}

//-----------------------------------------------------------------------------
/// Return the current sensor temperature
//-----------------------------------------------------------------------------
double Camera::getSensorTemperature(bool & out_NotSupported)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;
    double  temperature = 0.0;
    
	err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORTEMPERATURE, &temperature );
    
    if( failed(err) )
	{
        if((err == DCAMERR_INVALIDPROPERTYID)||(err == DCAMERR_NOTSUPPORT))
        {
            manage_trace( deb, "Unable to retrieve the sensor temperature", err, "dcamprop_getvalue - DCAM_IDPROP_SENSORTEMPERATURE");
            out_NotSupported = true;
        }
        else
        {
            manage_trace( deb, "Unable to retrieve the sensor temperature", err, "dcamprop_getvalue - DCAM_IDPROP_SENSORTEMPERATURE");
            THROW_HW_ERROR(Error) << "Unable to retrieve the sensor temperature";
        }
    }    
    else
    {
        out_NotSupported = false;
        DEB_TRACE() << DEB_VAR1(temperature);
    }
    
    return temperature;
}

//-----------------------------------------------------
// Return the event control object
//-----------------------------------------------------
HwEventCtrlObj* Camera::getEventCtrlObj()
{
	return &m_event_ctrl_obj;
}