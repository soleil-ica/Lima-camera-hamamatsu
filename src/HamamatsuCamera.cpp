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
//#define HAMAMATSU_CAMERA_DEBUG_ACQUISITION
//-----------------------------------------------------------------------------
const double Camera::g_orca_pixel_size              = 6.5e-6;
const int    Camera::g_dcam_str_msg_size            = 256   ;
const int    Camera::g_get_sub_array_do_not_use_view= -1    ;

const string Camera::g_trace_line_separator       = "--------------------------------------------------------------";
const string Camera::g_trace_little_line_separator = "--------------------------------";

#define TRACE_LINE() DEB_ALWAYS() << __LINE__

#define GET_SUBARRAY_RECT_DO_NOT_USE_VIEW (-1)

//-----------------------------------------------------------------------------
#define SENSOR_COOLER_NOT_SUPPORTED "NOT_SUPPORTED"
#define SENSOR_COOLER_OFF           "OFF"
#define SENSOR_COOLER_ON            "ON"
#define SENSOR_COOLER_MAX           "MAX"

#define TEMPERATURE_STATUS_NOT_SUPPORTED "NOT_SUPPORTED"
#define TEMPERATURE_STATUS_NORMAL        "NORMAL"
#define TEMPERATURE_STATUS_WARNING       "WARNING"
#define TEMPERATURE_STATUS_PROTECTION    "PROTECTION"

#define COOLER_STATUS_NOT_SUPPORTED "NOT_SUPPORTED"
#define COOLER_STATUS_ERROR4        "ERROR4"
#define COOLER_STATUS_ERROR3        "ERROR3"
#define COOLER_STATUS_ERROR2        "ERROR2"
#define COOLER_STATUS_ERROR1        "ERROR1"
#define COOLER_STATUS_NONE          "NONE"
#define COOLER_STATUS_OFF           "OFF"
#define COOLER_STATUS_READY         "READY"
#define COOLER_STATUS_BUSY          "BUSY"
#define COOLER_STATUS_ALWAYS        "ALWAYS"
#define COOLER_STATUS_WARNING       "WARNING"

#define READOUTSPEED_SLOW_VALUE     1
#define READOUTSPEED_NORMAL_VALUE   2
#define READOUTSPEED_SLOW_NAME      "ULTRA QUIET"
#define READOUTSPEED_NORMAL_NAME    "STANDARD"

#define SENSORMODE_AREA_VALUE           1
#define SENSORMODE_PROGRESSIVE_VALUE    12
#define SENSORMODE_AREA_NAME            "AREA"
#define SENSORMODE_PROGRESSIVE_NAME     "PROGRESSIVE"

//-----------------------------------------------------------------------------
///  Ctor
//-----------------------------------------------------------------------------
#pragma warning( push )
#pragma warning( disable : 4355) // temporary disable the warning caused by the use of this in the initializers

Camera::Camera(const std::string& config_path, int camera_number, int frame_buffer_size)
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
      m_lost_frames_count(0)  ,
      m_fps            (0.0)  ,
      m_hdr_enabled    (false),
      m_view_exp_time  (NULL)   // array of exposure value by view

#pragma warning( pop ) 
{
    DEB_CONSTRUCTOR();

    m_config_path       = config_path  ;
    m_camera_number     = camera_number;
    m_frame_buffer_size = frame_buffer_size;
  
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
        m_max_views = getMaxNumberofViews();
        
        if(m_max_views > 1)
        {
            m_view_exp_time = new double[m_max_views];

            for(int view_index = 0 ; view_index < m_max_views ; view_index++)
            {
                m_view_exp_time[view_index] = m_exp_time; // by default
            }
        }
        else
        {
            m_view_exp_time = NULL;
        }

        // --- BIN already set to 1,1 above.
        // --- Hamamatsu sets the ROI by starting coordinates at 1 and not 0 !!!!
        Size size_max;
        getDetectorImageSize(size_max);
        Roi a_roi = Roi(0,0, size_max.getWidth(), size_max.getHeight());

        // Store max image size
        m_max_image_width  = size_max.getWidth ();
        m_max_image_height = size_max.getHeight();

        // Display max image size
        DEB_TRACE() << "Detector max width: " << m_max_image_width ;
        DEB_TRACE() << "Detector max height:" << m_max_image_height;

        // sets no view mode by default
        m_view_mode_enabled = false; // W-View mode with splitting image
        m_view_number      = 0    ; // number of W-Views

        setViewMode(false, 0);

        // --- setRoi applies both bin and roi
        DEB_TRACE() << "Set the ROI to full frame: "<< a_roi;
        setRoi(a_roi);        
        
        // --- Get the maximum exposure time allowed and set default
        setExpTime(m_exp_time);
        
        // --- Set detector for software single image mode    
        setTrigMode(IntTrig);
        
        m_nb_frames = 1;

        // --- Initialize the map of the camera parameters
        initParametersMap();

        // --- finally start the acq thread
        m_thread.start();
    }
    else
    {
        manage_error( deb, "Unable to initialize the camera (Check if it is already ON or if another software is currently using it).");
        THROW_HW_ERROR(Error) << "Unable to initialize the camera (Check if it is already ON or if another software is currently using it).";
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

    if(m_view_exp_time != NULL)
        delete [] m_view_exp_time;

    DEB_TRACE() << "Camera destructor done.";
}

//-----------------------------------------------------------------------------
/// return the detector Max image size 
//-----------------------------------------------------------------------------
void Camera::getDetectorMaxImageSize(Size& size) ///< [out] image dimensions
{
    DEB_MEMBER_FUNCT();
    size = Size(m_max_image_width, m_max_image_height);
}

//-----------------------------------------------------------------------------
/// return the detector image size 
//-----------------------------------------------------------------------------
void Camera::getDetectorImageSize(Size& size) ///< [out] image dimensions
{
    DEB_MEMBER_FUNCT();
    
    int x_max =0;
    int y_max =0;
    
    // --- Get the max image size of the detector
    if (NULL!=m_camera_handle)
    {
        x_max = dcamex_getimagewidth (m_camera_handle);
        y_max = dcamex_getimageheight(m_camera_handle);
    }

    if ((0==x_max) || (0==y_max))
    {
        manage_error( deb, "Cannot get detector size");
        THROW_HW_ERROR(Error) << "Cannot get detector size";
    }     

    size= Size(x_max, y_max);

    DEB_TRACE() << "Size (" << DEB_VAR2(size.getWidth(), size.getHeight()) << ")";
}

//-----------------------------------------------------------------------------
/// return the image type  TODO: this is permanently called by the device -> find a way to avoid DCAM access
//-----------------------------------------------------------------------------
void Camera::getImageType(ImageType& type)
{
    DEB_MEMBER_FUNCT();

    type = Bpp16;

    long bits_type =  dcamex_getbitsperchannel(m_camera_handle);
    
    if (0 != bits_type )
    {
        switch( bits_type )
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
            m_depth    = 16;
            break;
        }
        default:
            manage_error( deb, "This pixel format of the camera is not managed, only 16 bits cameras are already managed!");
            THROW_HW_ERROR(Error) << "This pixel format of the camera is not managed, only 16 bits cameras are already managed!";
            break;
    }

    DEB_TRACE() << "SetImageType: " << m_depth;
    m_bytes_per_pixel = m_depth / 8;
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
        DCAMERR err;
        int trigger_source = -1;
        int trigger_active = -1;
        int trigger_mode   = -1;

        if(mode == IntTrig)
        {
            trigger_source = DCAMPROP_TRIGGERSOURCE__INTERNAL;
            trigger_active = DCAMPROP_TRIGGERACTIVE__EDGE    ;
            trigger_mode   = DCAMPROP_TRIGGER_MODE__NORMAL   ;
        }
        else
        if(mode == IntTrigMult)
        {
            trigger_source = DCAMPROP_TRIGGERSOURCE__INTERNAL;
            trigger_active = DCAMPROP_TRIGGERACTIVE__EDGE    ;
            trigger_mode   = DCAMPROP_TRIGGER_MODE__NORMAL   ;
        }
        else
        if(mode == ExtTrigReadout)
        {
            trigger_source = DCAMPROP_TRIGGERSOURCE__EXTERNAL   ;
            trigger_active = DCAMPROP_TRIGGERACTIVE__SYNCREADOUT;
            trigger_mode   = DCAMPROP_TRIGGER_MODE__NORMAL      ;
        }
        else
        if(mode == ExtTrigSingle)
        {
            trigger_source = DCAMPROP_TRIGGERSOURCE__EXTERNAL;
            trigger_active = DCAMPROP_TRIGGERACTIVE__EDGE    ;
            trigger_mode   = DCAMPROP_TRIGGER_MODE__START    ;
        }
        else
        if(mode == ExtTrigMult)
        {
            trigger_source = DCAMPROP_TRIGGERSOURCE__EXTERNAL;
            trigger_active = DCAMPROP_TRIGGERACTIVE__EDGE    ;
            trigger_mode   = DCAMPROP_TRIGGER_MODE__NORMAL   ;
        }
        else
        if(mode == ExtGate)
        {
            trigger_source = DCAMPROP_TRIGGERSOURCE__EXTERNAL;
            trigger_active = DCAMPROP_TRIGGERACTIVE__LEVEL   ;
            trigger_mode   = DCAMPROP_TRIGGER_MODE__NORMAL   ;
        }
        else
        {
            manage_error( deb, "Failed to set trigger mode", DCAMERR_NONE, "setTrigMode", "VALUE=%d", mode);
            THROW_HW_ERROR(Error) << "Failed to set trigger mode";
        }

        // set the trigger source
        if(trigger_source != -1)
        {
            err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_TRIGGERSOURCE, static_cast<double>(trigger_source));
            if( failed(err) )
            {
                manage_error( deb, "Cannot set trigger option", err, 
                              "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_TRIGGERSOURCE, VALUE=%d", trigger_source);
                THROW_HW_ERROR(Error) << "Cannot set trigger option";
            }
        }

        // set the trigger active
        if(trigger_active != -1)
        {
            err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_TRIGGERACTIVE, static_cast<double>(trigger_active));
            if( failed(err) )
            {
                manage_error( deb, "Cannot set trigger option", err, 
                              "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_TRIGGERACTIVE, VALUE=%d", trigger_active);
                THROW_HW_ERROR(Error) << "Cannot set trigger option";
            }
        }

        // set the trigger mode
        if(trigger_mode != -1)
        {
            err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_TRIGGER_MODE, static_cast<double>(trigger_mode));
            if( failed(err) )
            {
                manage_error( deb, "Cannot set trigger option", err, 
                              "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_TRIGGER_MODE, VALUE=%d", trigger_mode);
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

    if(!m_view_mode_enabled)
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

        double temp_exp_time;
        getExpTime(temp_exp_time);
        manage_trace( deb, "Changed Exposure time", DCAMERR_NONE, NULL, "exp:%lf >> real:%lf", m_exp_time, temp_exp_time);
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
    if(!m_view_mode_enabled)
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
void Camera::getExposureTimeRange(double& min_expo,    ///< [out] minimum exposure time
                                  double& max_expo) ///< [out] maximum exposure time
                                  const
{
    DEB_MEMBER_FUNCT();

    FeatureInfos feature_obj  ;

    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_EXPOSURETIME", DCAM_IDPROP_EXPOSURETIME, feature_obj ) )
    {
        manage_error( deb, "Failed to get exposure time");
        THROW_HW_ERROR(Error) << "Failed to get exposure time";
    }

    min_expo = feature_obj.m_min;
    max_expo = feature_obj.m_max;

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
        //case CameraThread::Stopped  :
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

    Point top_left = set_roi.getTopLeft();
    Size  size     = set_roi.getSize   ();
    int   x        = top_left.x        * m_bin.getX();
    int   y        = top_left.y        * m_bin.getY();
    int   width    = size.getWidth () * m_bin.getX();
    int   height   = size.getHeight() * m_bin.getY();

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
        m_feature_pos_x.RoundValue (x     );
        m_feature_pos_y.RoundValue (y     );
        m_feature_size_x.RoundValue(width );
        m_feature_size_y.RoundValue(height);

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
        set_roi_size = Size(m_max_image_width, m_max_image_height);
    }

    Roi new_roi(set_roi_topleft, set_roi_size);

    DEB_TRACE() << "setRoi(): " << set_roi_topleft.x        << ", " 
                                << set_roi_topleft.y        << ", " 
                                << set_roi_size.getWidth () << ", " 
                                << set_roi_size.getHeight();

    // Changing the ROI is not allowed in W-VIEW mode except for full frame
    if(m_view_mode_enabled)
    {
        Roi FullFrameRoi(Point(0, 0), Size(m_max_image_width, m_max_image_height));
        if(new_roi != FullFrameRoi)
        {
            manage_error( deb, "Cannot change ROI in W-VIEW mode! Only full frame is supported.", DCAMERR_NONE, "setRoi");
            THROW_HW_ERROR(Error) << "Cannot change ROI in W-VIEW mode! Only full frame is supported.";
        }
    }

    // view mode activated and two views 
    if((m_view_mode_enabled) && (m_view_number == 2))
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
                                    g_get_sub_array_do_not_use_view))
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

    if (!dcamex_getsubarrayrect( m_camera_handle, left, top, width,    height, GET_SUBARRAY_RECT_DO_NOT_USE_VIEW ) )
    {
        manage_error( deb, "Cannot get detector ROI");
        THROW_HW_ERROR(Error) << "Cannot get detector ROI";
    }    

    // view mode activated and two views 
    if((m_view_mode_enabled) && (m_view_number == 2))
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

    if(!m_view_mode_enabled)
    {
        if (!dcamex_getsubarrayrect( m_camera_handle, left, top, width,    height, g_get_sub_array_do_not_use_view ) )
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
        for(int view_index = 0 ; view_index < m_max_views ; view_index++)
        {
            if (!dcamex_getsubarrayrect( m_camera_handle, left, top, width,    height, view_index ) )
            {
                manage_error( deb, "Cannot get detector View ROI");
            }    
            else
            {
                DEB_TRACE() << "View Roi (" << (view_index+1) << "): (" << left << ", " << top << ", " <<  width << ", " <<  height << ")";
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
bool Camera::isBinningSupported(const int bin_value)    ///< [in] binning value to chck for
{
    DEB_MEMBER_FUNCT();

    bool found = false;

    for (unsigned int i=0; i<m_vectBinnings.size(); i++)
    {
        if (bin_value == m_vectBinnings.at(i))
        {
            found = true;
            break;
        }
    }

    return found;
}

//-----------------------------------------------------------------------------
/// Get the corresponding binning mode
/*
@return the corresponding mode
*/
//-----------------------------------------------------------------------------
int32 Camera::GetBinningMode(const int bin_value)    ///< [in] binning value to chck for
{
    DEB_MEMBER_FUNCT();
    
    if(bin_value == 1 ) return DCAMPROP_BINNING__1 ;
    if(bin_value == 2 ) return DCAMPROP_BINNING__2 ;
    if(bin_value == 4 ) return DCAMPROP_BINNING__4 ;
    if(bin_value == 8 ) return DCAMPROP_BINNING__8 ;
    if(bin_value == 16) return DCAMPROP_BINNING__16;

    manage_error( deb, "Incoherent binning value - no mode found.", DCAMERR_NONE, "GetBinningMode", "binning value = %d", bin_value);
    THROW_HW_ERROR(Error) << "Incoherent binning value - no mode found.";
}

//-----------------------------------------------------------------------------
/// Get the corresponding binning from binning mode
/*
@return the corresponding mode
*/
//-----------------------------------------------------------------------------
int Camera::GetBinningFromMode(const int32 bin_mode)    ///< [in] binning mode to chck for
{
    DEB_MEMBER_FUNCT();
    
    if(bin_mode == DCAMPROP_BINNING__1 ) return 1 ;
    if(bin_mode == DCAMPROP_BINNING__2 ) return 2 ;
    if(bin_mode == DCAMPROP_BINNING__4 ) return 4 ;
    if(bin_mode == DCAMPROP_BINNING__8 ) return 8 ;
    if(bin_mode == DCAMPROP_BINNING__16) return 16;

    manage_error( deb, "Incoherent binning mode.", DCAMERR_NONE, "GetBinningFromMode", "binning mode = %d", bin_mode);
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
void Camera::getPixelSize(double& sizex,    ///< [out] horizontal pixel size
                          double& sizey)    ///< [out] vertical   pixel size
{
    DEB_MEMBER_FUNCT();
    
    sizex = Camera::g_orca_pixel_size;
    sizey = Camera::g_orca_pixel_size;
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

    DEB_TRACE() << g_trace_line_separator.c_str();

    // Create the list of available binning modes from camera capabilities
    DCAMERR             err   ;
    DCAMDEV_CAPABILITY    devcap;

    memset( &devcap, 0, sizeof(DCAMDEV_CAPABILITY) );
    devcap.size    = sizeof(DCAMDEV_CAPABILITY);

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
        FeatureInfos feature_obj;

        if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_BINNING", DCAM_IDPROP_BINNING, feature_obj ) )
        {
            manage_error( deb, "Failed to get binning modes");
            THROW_HW_ERROR(Error) << "Failed to get binning modes";
        }

        DEB_TRACE() << g_trace_line_separator.c_str();
        feature_obj.traceModePossibleValues();

        if(feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__1 ))) m_vectBinnings.push_back(1 );
        if(feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__2 ))) m_vectBinnings.push_back(2 );
        if(feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__4 ))) m_vectBinnings.push_back(4 );
        if(feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__8 ))) m_vectBinnings.push_back(8 );
        if(feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_BINNING__16))) m_vectBinnings.push_back(16);

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
    FeatureInfos trigger_source_feature_obj;
    FeatureInfos trigger_active_feature_obj;
    FeatureInfos trigger_mode_feature_obj  ;

    // trigger source
    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_TRIGGERSOURCE", DCAM_IDPROP_TRIGGERSOURCE, trigger_source_feature_obj ) )
    {
        manage_error( deb, "Failed to get trigger source modes");
        THROW_HW_ERROR(Error) << "Failed to get trigger source modes";
    }

    DEB_TRACE() << g_trace_line_separator.c_str();
    trigger_source_feature_obj.traceModePossibleValues();

    // trigger active
    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_TRIGGERACTIVE", DCAM_IDPROP_TRIGGERACTIVE, trigger_active_feature_obj ) )
    {
        manage_error( deb, "Failed to get trigger active modes");
        THROW_HW_ERROR(Error) << "Failed to get trigger active modes";
    }

    DEB_TRACE() << g_trace_line_separator.c_str();
    trigger_active_feature_obj.traceModePossibleValues();

    // trigger mode
    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_TRIGGER_MODE", DCAM_IDPROP_TRIGGER_MODE, trigger_mode_feature_obj ) )
    {
        manage_error( deb, "Failed to get trigger mode modes");
        THROW_HW_ERROR(Error) << "Failed to get trigger mode modes";
    }

    DEB_TRACE() << g_trace_line_separator.c_str();
    trigger_mode_feature_obj.traceModePossibleValues();

    // IntTrig
    if((trigger_source_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__INTERNAL ))) &&
       (trigger_active_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__EDGE     ))) &&
       (trigger_mode_feature_obj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL    ))))
        m_map_trig_modes[IntTrig] = true;

    // IntTrigMult
    if((trigger_source_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__INTERNAL ))) &&
       (trigger_active_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__EDGE     ))) &&
       (trigger_mode_feature_obj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL    ))))
        m_map_trig_modes[IntTrigMult] = true;

    // ExtTrigReadout
    if((trigger_source_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__EXTERNAL   ))) &&
       (trigger_active_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__SYNCREADOUT))) &&
       (trigger_mode_feature_obj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL      ))))
        m_map_trig_modes[ExtTrigReadout] = true;

    // ExtTrigSingle
    if((trigger_source_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__EXTERNAL))) &&
       (trigger_active_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__EDGE    ))) &&
       (trigger_mode_feature_obj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__START    ))))
        m_map_trig_modes[ExtTrigSingle] = true;

    // ExtTrigMult
    if((trigger_source_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__EXTERNAL))) &&
       (trigger_active_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__EDGE    ))) &&
       (trigger_mode_feature_obj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL   ))))
        m_map_trig_modes[ExtTrigMult] = true;

    // ExtGate
    if((trigger_source_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERSOURCE__EXTERNAL))) &&
       (trigger_active_feature_obj.checkifValueExists(static_cast<double>(DCAMPROP_TRIGGERACTIVE__LEVEL   ))) &&
       (trigger_mode_feature_obj.checkifValueExists  (static_cast<double>(DCAMPROP_TRIGGER_MODE__NORMAL   ))))
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
        FeatureInfos feature_obj;

        if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_EXPOSURETIME", DCAM_IDPROP_EXPOSURETIME, feature_obj ) )
        {
            manage_error( deb, "Failed to get exposure time");
            THROW_HW_ERROR(Error) << "Failed to get exposure time";
        }

        m_exp_time_max = feature_obj.m_max;

        // Display obtained values - Exposure time
        DEB_TRACE() << "Min exposure time: " << feature_obj.m_min;
        DEB_TRACE() << "Max exposure time: " << feature_obj.m_max;
    }

    //---------------------------------------------------------------------
    // Checking ROI properties
    {
        DEB_TRACE() << g_trace_line_separator.c_str();
        traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHPOS" , DCAM_IDPROP_SUBARRAYHPOS , &m_feature_pos_x );
        DEB_TRACE() << g_trace_line_separator.c_str();
        traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVPOS" , DCAM_IDPROP_SUBARRAYVPOS , &m_feature_pos_y );
        DEB_TRACE() << g_trace_line_separator.c_str();
        traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHSIZE", DCAM_IDPROP_SUBARRAYHSIZE, &m_feature_size_x);
        DEB_TRACE() << g_trace_line_separator.c_str();
        traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVSIZE", DCAM_IDPROP_SUBARRAYVSIZE, &m_feature_size_y);
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

//=============================================================================
// READOUT SPEED
//=============================================================================
//-----------------------------------------------------------------------------
/// Return the readout speed mode support by the current detector
//-----------------------------------------------------------------------------
bool Camera::isReadoutSpeedSupported(void)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;
    bool    supported;
    double  temp;
    
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_READOUTSPEED, &temp );
    
    if( failed(err) )
    {
        if((err == DCAMERR_INVALIDPROPERTYID)||(err == DCAMERR_NOTSUPPORT))
        {
            supported = false;
        }
        else
        {
            manage_trace( deb, "Unable to retrieve the readout speed mode", err, "dcamprop_getvalue - DCAM_IDPROP_READOUTSPEED");
            THROW_HW_ERROR(Error) << "Unable to retrieve the readout speed mode";
        }
    }    
    else
    {
        supported = true;
    }

    return supported;
}

//-----------------------------------------------------------------------------
/// Set the readout speed value
/*!
@remark possible values are 1 or 2
*/
//-----------------------------------------------------------------------------
void Camera::setReadoutSpeed(const short int readout_speed) ///< [in] new readout speed
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(readout_speed);

    DCAMERR err;

    // set the readout speed
    err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_READOUTSPEED, static_cast<double>(readout_speed) );
    if( failed(err) )
    {
        manage_error( deb, "Failed to set readout speed", err, 
                      "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_SUBARRAYVPOS, VALUE=%d",static_cast<int>(readout_speed));
        THROW_HW_ERROR(Error) << "Failed to set readout speed";
    }

    m_read_mode = readout_speed;
}

//-----------------------------------------------------------------------------
/// Get the readout speed value
//-----------------------------------------------------------------------------
short int Camera::getReadoutSpeed(void) const
{
    DEB_MEMBER_FUNCT();

    DCAMERR err   ;
    int32   read_mode = 0  ;
    double  v  =   0.0;

    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_READOUTSPEED, &v );
    
    if( failed(err) )
    {
        manage_trace( deb, "Unable to retrieve the readout speed value", err, "dcamprop_getvalue - DCAM_IDPROP_READOUTSPEED");
    }
    else    
    {
        read_mode = static_cast<int32>(v);
    }

    DEB_TRACE() << DEB_VAR1(read_mode);

    return  read_mode;
}

//-----------------------------------------------------------------------------
// Get the label of a readout speed.
//-----------------------------------------------------------------------------
std::string Camera::getReadoutSpeedLabelFromValue(const short int in_readout_speed) const
{
    std::string label = "";

    switch (in_readout_speed)
    {
        case READOUTSPEED_SLOW_VALUE  : label = READOUTSPEED_SLOW_NAME  ; break;
        case READOUTSPEED_NORMAL_VALUE: label = READOUTSPEED_NORMAL_NAME; break;
        default: label = "ERROR"; break;
    }

    return label;
}

//-----------------------------------------------------------------------------
// Get the readout speed from a label.
//-----------------------------------------------------------------------------
short int Camera::getReadoutSpeedFromLabel(const std::string & in_readout_speed_label) const
{
    DEB_MEMBER_FUNCT();

    short int   readout_speed = READOUTSPEED_NORMAL_VALUE;
    std::string label         = in_readout_speed_label;

	transform(label.begin(), label.end(), label.begin(), ::toupper);

    if (label == READOUTSPEED_NORMAL_NAME)
    {
        readout_speed = READOUTSPEED_NORMAL_VALUE;
    }
    else
    if (label == READOUTSPEED_SLOW_NAME)
    {
        readout_speed = READOUTSPEED_SLOW_VALUE;
    }
    else
	{			
		string user_msg;
        user_msg = string("Available Readout speeds are:\n- ") + string(READOUTSPEED_NORMAL_NAME) + string("\n- ") + string(READOUTSPEED_SLOW_NAME);
        THROW_HW_ERROR(Error) << user_msg.c_str();
	}

    return readout_speed;
}

//-----------------------------------------------------------------------------
// Get the readout speed label.
//-----------------------------------------------------------------------------
std::string Camera::getReadoutSpeedLabel(void)
{
    return getReadoutSpeedLabelFromValue(getReadoutSpeed());
}

//-----------------------------------------------------------------------------
// Set the readout speed label.
//-----------------------------------------------------------------------------
void Camera::setReadoutSpeedLabel(const std::string & in_readout_speed_label)
{
    setReadoutSpeed(getReadoutSpeedFromLabel(in_readout_speed_label));
}

//=============================================================================
// SENSOR MODE
//=============================================================================
//-----------------------------------------------------------------------------
/// Return the sensor mode support by the current detector
//-----------------------------------------------------------------------------
bool Camera::isSensorModeSupported(void)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;
    bool    supported;
    double  temp;
    
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORMODE, &temp );
    
    if( failed(err) )
    {
        if((err == DCAMERR_INVALIDPROPERTYID)||(err == DCAMERR_NOTSUPPORT))
        {
            supported = false;
        }
        else
        {
            manage_trace( deb, "Unable to retrieve the sensor mode", err, "dcamprop_getvalue - DCAM_IDPROP_SENSORMODE");
            THROW_HW_ERROR(Error) << "Unable to retrieve the sensor mode";
        }
    }    
    else
    {
        supported = true;
    }

    return supported;
}

//-----------------------------------------------------------------------------
/// Set the sensor mode value
/*!
@remark possible values are 1 or 2
*/
//-----------------------------------------------------------------------------
void Camera::setSensorMode(const short int sensor_mode) ///< [in] new sensor mode
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(sensor_mode);

    DCAMERR err;

    // set the sensor mode
    err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_SENSORMODE, static_cast<double>(sensor_mode) );
    if( failed(err) )
    {
        manage_error( deb, "Failed to set sensor mode", err, 
                      "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_SUBARRAYVPOS, VALUE=%d",static_cast<int>(sensor_mode));
        THROW_HW_ERROR(Error) << "Failed to set sensor mode";
    }

    m_read_mode = sensor_mode;
}

//-----------------------------------------------------------------------------
/// Get the sensor mode value
//-----------------------------------------------------------------------------
short int Camera::getSensorMode(void) const
{
    DEB_MEMBER_FUNCT();

    DCAMERR err   ;
    int32   read_mode = 0  ;
    double  v  =   0.0;

    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORMODE, &v );
    
    if( failed(err) )
    {
        manage_trace( deb, "Unable to retrieve the sensor mode value", err, "dcamprop_getvalue - DCAM_IDPROP_SENSORMODE");
    }
    else    
    {
        read_mode = static_cast<int32>(v);
    }

    DEB_TRACE() << DEB_VAR1(read_mode);

    return  read_mode;
}

//-----------------------------------------------------------------------------
// Get the label of a sensor mode.
//-----------------------------------------------------------------------------
std::string Camera::getSensorModeLabelFromValue(const short int in_sensor_mode) const
{
    std::string label = "";

    switch (in_sensor_mode)
    {
        case SENSORMODE_AREA_VALUE  : label = SENSORMODE_AREA_NAME  ; break;
        case SENSORMODE_PROGRESSIVE_VALUE: label = SENSORMODE_PROGRESSIVE_NAME; break;
        default: label = "ERROR"; break;
    }

    return label;
}

//-----------------------------------------------------------------------------
// Get the sensor mode from a label.
//-----------------------------------------------------------------------------
short int Camera::getSensorModeFromLabel(const std::string & in_sensor_mode_label) const
{
    DEB_MEMBER_FUNCT();

    short int   sensor_mode = SENSORMODE_AREA_VALUE;
    std::string label         = in_sensor_mode_label;

	transform(label.begin(), label.end(), label.begin(), ::toupper);

    if (label == SENSORMODE_AREA_NAME)
    {
        sensor_mode = SENSORMODE_AREA_VALUE;
    }
    else
    if (label == SENSORMODE_PROGRESSIVE_NAME)
    {
        sensor_mode = SENSORMODE_PROGRESSIVE_VALUE;
    }
    else
	{			
		string user_msg;
        user_msg = string("Available sensor modes are:\n- ") + string(SENSORMODE_AREA_NAME) + string("\n- ") + string(SENSORMODE_PROGRESSIVE_NAME);
        THROW_HW_ERROR(Error) << user_msg.c_str();
	}

    return sensor_mode;
}

//-----------------------------------------------------------------------------
// Get the sensor mode label.
//-----------------------------------------------------------------------------
std::string Camera::getSensorModeLabel(void)
{
    return getSensorModeLabelFromValue(getSensorMode());
}

//-----------------------------------------------------------------------------
// Set the sensor mode label.
//-----------------------------------------------------------------------------
void Camera::setSensorModeLabel(const std::string & in_sensor_mode_label)
{
    setSensorMode(getSensorModeFromLabel(in_sensor_mode_label));
}

//=============================================================================
// LOST FRAMES
//=============================================================================
//-----------------------------------------------------------------------------
/// Get the lost frames value
//-----------------------------------------------------------------------------
void Camera::getLostFrames(unsigned long int& lost_frames) ///< [out] current lost frames
{
    DEB_MEMBER_FUNCT();

    lost_frames = m_lost_frames_count;
}

//=============================================================================
// LOST FRAMES
//=============================================================================
//-----------------------------------------------------------------------------
/// Get the frame rate
//-----------------------------------------------------------------------------
void Camera::getFPS(double& fps) ///< [out] last computed fps
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
    DEB_TRACE() << g_trace_line_separator.c_str();

    traceAllRoi();

    m_image_number = 0;
    m_fps          = 0;

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
    DEB_TRACE() << g_trace_line_separator.c_str();

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
    m_wait_handle = NULL ;
    DEB_TRACE() << "DONE";
}

/************************************************************************
 * \brief destructor
 ************************************************************************/
Camera::CameraThread::~CameraThread()
{
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "CameraThread::~CameraThread";
    abort();
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
    DEB_TRACE() << "CameraThread::init DONE";
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::abort()
//---------------------------------------------------------------------------------------
void Camera::CameraThread::abort()
{
    DEB_MEMBER_FUNCT();
    CmdThread::abort();
    DEB_TRACE() << "CameraThread::abort DONE";
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

    long    status;
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
void Camera::CameraThread::createWaitHandle(HDCAMWAIT & wait_handle) const
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;

    wait_handle = NULL;

    // open wait handle
    DCAMWAIT_OPEN waitOpenHandle;

    memset( &waitOpenHandle, 0, sizeof(DCAMWAIT_OPEN) );
    waitOpenHandle.size     = sizeof(DCAMWAIT_OPEN) ;
    waitOpenHandle.hdcam = m_cam->m_camera_handle;

    err = dcamwait_open( &waitOpenHandle );

    if( failed(err) )
    {
        static_manage_error( m_cam, deb, "Cannot create the wait handle", err, "dcamwait_open");
        THROW_HW_ERROR(Error) << "Cannot create the wait handle";
    }
    else
    {
        wait_handle = waitOpenHandle.hwait; // after this, no need to keep the waitopen structure, freed by the stack
    }
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::releaseWaitHandle()
// does not throw exception in case of problem but trace an error.
//---------------------------------------------------------------------------------------
void Camera::CameraThread::releaseWaitHandle(HDCAMWAIT & wait_handle) const
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;

    err = dcamwait_close( wait_handle );

    if( failed(err) )
    {
        static_manage_error( m_cam, deb, "Cannot release the wait handle", err, "dcamwait_close");
    }
    
    wait_handle = NULL;
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::abortCapture()
// Stop the capture, releasing the Wait handle and setting the boolean stop flag.
//---------------------------------------------------------------------------------------
void Camera::CameraThread::abortCapture(void)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err = DCAMERR_NONE;

    m_cam->m_mutex_force_stop.lock();

    if(m_wait_handle != NULL)
    {
        err = dcamwait_abort( m_wait_handle );
    }

    if( failed(err) )
    {
        static_manage_error( m_cam, deb, "Cannot abort wait handle.", err, "dcamwait_abort");
    }

    m_force_stop = true;
    m_cam->m_mutex_force_stop.unlock();
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::getTransfertInfo()
// throws an exception in case of problem
//---------------------------------------------------------------------------------------
void Camera::CameraThread::getTransfertInfo(int32 & frame_index,
                                            int32 & frame_count)
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
        frame_index = transferinfo.nNewestFrameIndex;
        frame_count = transferinfo.nFrameCount      ;
    }
}

//---------------------------------------------------------------------------------------
//! Camera::CameraThread::execStartAcq()
//---------------------------------------------------------------------------------------
void Camera::CameraThread::execStartAcq()
{
    DEB_MEMBER_FUNCT();

    DCAMERR   err          = DCAMERR_NONE;
    bool      continue_acq = true        ;
    Timestamp T0    = Timestamp::now();
    Timestamp T1    = Timestamp::now();
    Timestamp DeltaT;
    long      status;

    DEB_TRACE() << m_cam->g_trace_line_separator.c_str();
    DEB_TRACE() << "CameraThread::execStartAcq - BEGIN";
    setStatus(CameraThread::Exposure);

    // Allocate frames to capture
    err = dcambuf_alloc( m_cam->m_camera_handle, m_cam->m_frame_buffer_size );

    if( failed(err) )
    {
        std::string errorText = static_manage_error( m_cam, deb, "Failed to allocate frames for the capture", err, 
                                                     "dcambuf_alloc", "number_of_buffer=%d",m_cam->m_frame_buffer_size);
        REPORT_EVENT(errorText);
        THROW_HW_ERROR(Error) << "Cannot allocate frame for capturing (dcam_allocframe()).";
    }
    else
    {
        DEB_ALWAYS() << "Allocated frames: " << m_cam->m_frame_buffer_size;
    }

    // --- check first the acquisition is idle
    err = dcamcap_status( m_cam->m_camera_handle, &status );
    if( failed(err) )
    {
        std::string errorText = static_manage_error( m_cam, deb, "Cannot get camera status", err, "dcamcap_status");
        REPORT_EVENT(errorText);
        THROW_HW_ERROR(Error) << "Cannot get camera status!";
    }

    if (DCAMCAP_STATUS_READY != status)
    {
        DEB_ERROR() << "Cannot start acquisition, camera is not ready";
        THROW_HW_ERROR(Error) << "Cannot start acquisition, camera is not ready";
    }

    StdBufferCbMgr& buffer_mgr = m_cam->m_buffer_ctrl_obj.getBuffer();
    buffer_mgr.setStartTimestamp(Timestamp::now());

    DEB_TRACE() << "Run";

    // Write some informations about the camera before the acquisition
    bool ViewModeEnabled;
    m_cam->getViewMode(ViewModeEnabled);

    if(ViewModeEnabled)
    {
        DEB_TRACE() << "View mode activated";

        int    view_index   = 0;
        double view_exposure;

        while(view_index < m_cam->m_max_views)
        {
            m_cam->getViewExpTime(view_index, view_exposure);
            DEB_TRACE() << "View " << (view_index+1) << " exposure : " << view_exposure;
            view_index++;
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
    createWaitHandle(m_wait_handle);

    // Start the real capture (this function returns immediately)
    err = dcamcap_start( m_cam->m_camera_handle, DCAMCAP_START_SEQUENCE );

    if( failed(err) )
    {
        dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
        releaseWaitHandle( m_wait_handle           );        
        dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
        setStatus        ( CameraThread::Fault    );

        std::string errorText = static_manage_error( m_cam, deb, "Cannot start the capture", err, "dcamcap_start");
        REPORT_EVENT(errorText);
        THROW_HW_ERROR(Error) << "Frame capture failed";
    }

    //-----------------------------------------------------------------------------
    // Transfer the images as they are beeing captured from dcam_sdk buffer to Lima
    //-----------------------------------------------------------------------------
    // Timestamp before DCAM "snapshot"
    T0 = Timestamp::now();

    m_cam->m_lost_frames_count = 0;

    int32 lastFrameCount = 0 ;
    int32 frame_count     = 0 ;
    int32 lastFrameIndex = -1;
    int32 frame_index     = 0 ;
    
    // Main acquisition loop
    while (    ( continue_acq )    &&
            ( (0==m_cam->m_nb_frames) || (m_cam->m_image_number < m_cam->m_nb_frames) ) )
    {
        setStatus(CameraThread::Exposure);

        // Check first if acq. has been stopped
        if (m_force_stop)
        {
            //abort the current acquisition
            continue_acq  = false;
            m_force_stop = false;
            continue;
        }

        // Wait for the next image to become available or the end of the capture by user
        // set wait param
        DCAMWAIT_START waitstart;
        memset( &waitstart, 0, sizeof(DCAMWAIT_START) );
        waitstart.size        = sizeof(DCAMWAIT_START);
        waitstart.eventmask    = DCAMWAIT_CAPEVENT_FRAMEREADY | DCAMWAIT_CAPEVENT_STOPPED;
        waitstart.timeout    = DCAMWAIT_TIMEOUT_INFINITE;

        // wait image
        err = dcamwait_start( m_wait_handle, &waitstart );

        if( failed(err) )
        {
            // If capture was aborted (by stopAcq() -> dcam_idle())
            if (DCAMERR_ABORT == err)
            {
                DEB_TRACE() << "DCAMERR_ABORT";
                continue_acq = false;
                continue;
            }
            else 
            if (DCAMERR_TIMEOUT == err)
            {
                dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
                releaseWaitHandle( m_wait_handle           );        
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
                ++m_cam->m_lost_frames_count;
                continue;
            }
            else
            {                    
                dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
                releaseWaitHandle( m_wait_handle           );        
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
                continue_acq = false;
                continue;
            }
        }

        if (m_force_stop)
        {
            //abort the current acqusition
            continue_acq  = false;
            m_force_stop = false;
            break;
        }

        // Transfert the new images
        setStatus(CameraThread::Readout);
        
        int32 deltaFrames = 0;

        getTransfertInfo(frame_index, frame_count);

        // manage the frame info
        {
            deltaFrames = frame_count-lastFrameCount;
            DEB_TRACE() << g_trace_little_line_separator.c_str();
            DEB_TRACE() << "(m_image_number:"  << m_cam->m_image_number << ")"
                        << " (lastFrameIndex:" << lastFrameIndex        << ")" 
                        << " (frame_index:"    << frame_index           << ")" 
                        << " (frame_count:"    << frame_count           << ")"
                        << " (deltaFrames:"    << deltaFrames           << ")";

            if (0 == frame_count)
            {
                dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
                releaseWaitHandle( m_wait_handle           );        
                dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
                setStatus        ( CameraThread::Fault    );

                std::string errorText = "No image captured.";
                DEB_ERROR() << errorText;
                REPORT_EVENT(errorText);
                THROW_HW_ERROR(Error) << "No image captured.";
            }
            if (deltaFrames > m_cam->m_frame_buffer_size)
            {
                m_cam->m_lost_frames_count += deltaFrames;
                DEB_TRACE() << "deltaFrames > m_frame_buffer_size (" << deltaFrames << ")";
            }
            lastFrameCount = frame_count;
        }

        // Check if acquisition was stopped & abort the current acqusition
        if (m_force_stop)
        {
            continue_acq  = false;
            m_force_stop = false;
            break;
        }

        int nbFrameToCopy = 0;

        try
        {
            m_cam->m_mutex_force_stop.lock();

            // Copy frames from DCAM_SDK to LiMa
            nbFrameToCopy  = (deltaFrames < m_cam->m_frame_buffer_size) ? deltaFrames : m_cam->m_frame_buffer_size; // if more than m_frame_buffer_size have arrived

            continue_acq    = copyFrames( (lastFrameIndex+1)% m_cam->m_frame_buffer_size, // index of the first image to copy from the ring buffer
                                          nbFrameToCopy,
                                          buffer_mgr);
            lastFrameIndex = frame_index;
        }
        catch (...)
        {
            // be sure to unlock the mutex before throwing the exception!
            m_cam->m_mutex_force_stop.unlock();

            dcamcap_stop     ( m_cam->m_camera_handle ); // Stop the acquisition
            releaseWaitHandle( m_wait_handle           );        
            dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
            setStatus        ( CameraThread::Fault    );

            throw;
        }

        m_cam->m_mutex_force_stop.unlock();
        
        // Update fps 
        T1     = Timestamp::now();
        DeltaT = T1 - T0;

        if (DeltaT > 0.0)
            m_cam->m_fps = m_cam->m_image_number / DeltaT;

    } // end of acquisition loop

    // Stop the acquisition
    err = dcamcap_stop( m_cam->m_camera_handle);

    if( failed(err) )
    {
        releaseWaitHandle( m_wait_handle           );        
        dcambuf_release  ( m_cam->m_camera_handle ); // Release the capture frame
        setStatus        ( CameraThread::Fault    );

        std::string errorText = static_manage_error( m_cam, deb, "Cannot stop acquisition.", err, "dcamcap_stop");
        REPORT_EVENT(errorText);
        THROW_HW_ERROR(Error) << "Cannot stop acquisition.";
    }

    // release the wait handle
    releaseWaitHandle( m_wait_handle );        

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

    DEB_ALWAYS() << g_trace_line_separator.c_str();
    DEB_ALWAYS() << "Total time (s): " << (T1 - T0);
    DEB_ALWAYS() << "FPS           : " << int(m_cam->m_image_number / (T1 - T0) );
    DEB_ALWAYS() << "Lost frames   : " << m_cam->m_lost_frames_count; 
    DEB_ALWAYS() << g_trace_line_separator.c_str();

    setStatus(CameraThread::Ready);
    DEB_TRACE() << "CameraThread::execStartAcq - END";
}

//-----------------------------------------------------------------------------
// Copy the given frames to the buffer manager
//-----------------------------------------------------------------------------
bool Camera::CameraThread::copyFrames(const int        index_frame_begin, ///< [in] index of the frame where to begin copy
                                      const int        nb_frames_count, ///< [in] number of frames to copy
                                      StdBufferCbMgr & buffer_mgr ) ///< [in] buffer manager object
{
    DEB_MEMBER_FUNCT();
    
    DEB_TRACE() << "copyFrames(" << index_frame_begin << ", nb:" << nb_frames_count << ")";

    DCAMERR  err         ;
    FrameDim frame_dim   = buffer_mgr.getFrameDim();
    Size     frame_size  = frame_dim.getSize     ();
    int      height      = frame_size.getHeight  ();
    int      memSize     = frame_dim.getMemSize  ();
    bool     CopySuccess = false                   ;
    int      iFrameIndex = index_frame_begin             ; // Index of frame in the DCAM cycling buffer

    for  (int cptFrame = 1 ; cptFrame <= nb_frames_count ; cptFrame++)
    {
        void     * dst         = buffer_mgr.getFrameBufferPtr(m_cam->m_image_number);
        void     * src         ;
        long int   sRowbytes   ;
        bool       bImageCopied;

        // prepare frame stucture
        DCAMBUF_FRAME bufframe;
        memset( &bufframe, 0, sizeof(bufframe) );
        bufframe.size    = sizeof(bufframe);
        bufframe.iFrame    = iFrameIndex     ;

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

            #ifdef HAMAMATSU_CAMERA_DEBUG_ACQUISITION
                DEB_TRACE() << "Acquired (m_image_number:" << m_cam->m_image_number << ")"
                            << " (frame_index:"            << iFrameIndex           << ")" 
                            << " (rowbytes:"               << sRowbytes             << ")"
                            << " (height:"                 << height                << ")";
            #endif
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

        iFrameIndex = (iFrameIndex+1) % m_cam->m_frame_buffer_size;
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

    FeatureInfos feature_obj;
    int32        nView     = 0;

    if( !dcamex_getfeatureinq( m_camera_handle, "DCAM_IDPROP_NUMBEROF_VIEW", DCAM_IDPROP_NUMBEROF_VIEW, feature_obj ) )
    {
        manage_trace( deb, "Failed to get number of view");
    }
    else
    {
        DEB_TRACE() << g_trace_line_separator.c_str();
        feature_obj.traceGeneralInformations();

        nView = static_cast<int32>(feature_obj.m_max);
    }

    DEB_TRACE() << DEB_VAR1(nView);

    return  nView;
}

//-----------------------------------------------------------------------------
/// Set the W-VIEW mode
//-----------------------------------------------------------------------------
void Camera::setViewMode(bool in_view_mode_activated, ///< [in] view mode activation or not
                         int  in_views_number      ) ///< [in] number of views if view mode activated
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR2(in_view_mode_activated, in_views_number);

    DCAMERR  err;

    if(in_view_mode_activated)
    {
        if(m_max_views > 1)
        {
            // checking if the W-View mode is possible for this camera
            if( m_max_views < in_views_number )
            {
                manage_error( deb, "Unable to activate W-VIEW mode", DCAMERR_NONE, NULL, "max views number %d, needed %d", m_max_views, in_views_number);
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

            m_view_mode_enabled = true          ; // W-View mode with splitting image
            m_view_number      = in_views_number; // number of W-Views

            manage_trace( deb, "W-VIEW mode activated", DCAMERR_NONE, NULL, "views number %d", in_views_number);
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
        if (m_view_mode_enabled)
        {
            setExpTime(m_exp_time); // rewrite the value in the camera
        }

        m_view_mode_enabled = false; // W-View mode with splitting image
        m_view_number      = 0    ; // number of W-Views

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

    if(getStatus() == CameraThread::Ready)
    {
        double  sensor_mode;
        DCAMERR err        ;

        err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORMODE, &sensor_mode);

        if( failed(err) )
        {
            manage_error( deb, "Cannot get sensor mode", err, 
                          "dcamprop_getvalue", "IDPROP=DCAM_IDPROP_SENSORMODE");
            THROW_HW_ERROR(Error) << "Cannot get sensor mode";
        }

        flag = (static_cast<int>(sensor_mode) == DCAMPROP_SENSORMODE__SPLITVIEW);
    }
    else
    // do not call the sdk functions during acquisition
    {
        flag = m_view_mode_enabled;
    }
}

//-----------------------------------------------------------------------------
/// Set the new exposure time for a view
//-----------------------------------------------------------------------------
void Camera::setViewExpTime(int    view_index, ///< [in] view index [0...m_max_views[
                            double exp_time  ) ///< [in] exposure time to set
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR2(view_index, exp_time);

    DCAMERR err;

    // W-View is not supported for this camera
    if(m_max_views < 2)
    {
        manage_error( deb, "Cannot set view exposure time - This camera does not support the W-View mode.", DCAMERR_NONE);
        THROW_HW_ERROR(Error) << "Cannot set view exposure time - This camera does not support the W-View mode.";
    }
    else
    if(view_index < m_max_views)
    {
        // Changing a View exposure time is not allowed if W-VIEW mode is not activated but we keep the value
        if(!m_view_mode_enabled)
        {
            manage_error( deb, "Cannot change W-View exposure time when W-VIEW mode is unactivated!", DCAMERR_NONE, "setViewExpTime");
        }
        else
        {
            // set the exposure time
            err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_VIEW_((view_index + 1), DCAM_IDPROP_EXPOSURETIME), exp_time);

            if( failed(err) )
            {
                manage_error( deb, "Cannot set view exposure time", err, 
                              "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_EXPOSURETIME, VIEW INDEX=%d, VALUE=%lf", view_index, exp_time);
                THROW_HW_ERROR(Error) << "Cannot set view exposure time";
            }

            double temp_exp_time;
            getViewExpTime(view_index, temp_exp_time);
            manage_trace( deb, "Changed View Exposure time", DCAMERR_NONE, NULL, "views index %d, exp:%lf >> real:%lf", view_index, exp_time, temp_exp_time);
        }

        m_view_exp_time[view_index] = exp_time;
    }
    else
    {
        manage_error( deb, "Cannot set view exposure time", DCAMERR_NONE, 
                      "", "VIEW INDEX=%d, MAX VIEWS=%d", view_index, m_max_views);
        THROW_HW_ERROR(Error) << "Cannot set view exposure time";
    }
}

//-----------------------------------------------------------------------------
/// Get the current exposure time for a view
//-----------------------------------------------------------------------------
void Camera::getViewExpTime(int      view_index, ///< [in] view index [0...m_max_views[
                            double & exp_time  ) ///< [out] current exposure time
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(view_index);

    DCAMERR err;

    // W-View is not supported for this camera
    if(m_max_views < 2)
    {
        exp_time = m_exp_time;
    }
    else
    if(view_index < m_max_views)
    {
        double exposure;

        // if W-VIEW mode is not activated we cannot retrieve the current value 
        if(!m_view_mode_enabled)
        {
            exposure = m_view_exp_time[view_index];
        }
        else
        if(getStatus() == CameraThread::Ready)
        {
            err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_VIEW_((view_index + 1), DCAM_IDPROP_EXPOSURETIME), &exposure);

            if( failed(err) )
            {
                manage_error( deb, "Cannot get view exposure time", err, 
                              "dcamprop_getvalue", "IDPROP=DCAM_IDPROP_EXPOSURETIME, VIEW INDEX=%d", view_index);
                THROW_HW_ERROR(Error) << "Cannot get view exposure time";
            }
        }
        else
        // do not call the sdk functions during acquisition
        {
            exposure = m_view_exp_time[view_index];
        }

        exp_time = exposure;
    }
    else
    {
        manage_error( deb, "Cannot get view exposure time", DCAMERR_NONE, 
                      "", "VIEW INDEX=%d, MAX VIEWS=%d", view_index, m_max_views);
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
    int    view_index   = 0   ;
    double exposure     = -1.0; // we search the minimum value - this is the not set value
    double view_exposure; 

    if(m_max_views > 1)
    {
        while(view_index < m_max_views)
        {
            getViewExpTime(view_index, view_exposure);
            
            if((exposure == -1.0) || (view_exposure < exposure))
                exposure = view_exposure; // sets the new minimum

            view_index++;
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
void Camera::setSyncReadoutBlankMode(enum SyncReadOut_BlankMode in_sync_read_out_mode) ///< [in] type of sync-readout trigger's blank
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(in_sync_read_out_mode);

    DCAMERR  err ;
    int      mode;

    if(in_sync_read_out_mode == SyncReadOut_BlankMode_Standard)
    {
        mode = DCAMPROP_SYNCREADOUT_SYSTEMBLANK__STANDARD;
    }
    else
    if(in_sync_read_out_mode == SyncReadOut_BlankMode_Minimum)
    {
        mode = DCAMPROP_SYNCREADOUT_SYSTEMBLANK__MINIMUM;
    }
    else
    {
        manage_error( deb, "Unable to set the SyncReadout blank mode", DCAMERR_NONE, 
                           "", "in_sync_read_out_mode is unknown %d", static_cast<int>(in_sync_read_out_mode));
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
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHPOS" , DCAM_IDPROP_SUBARRAYHPOS , NULL );
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVPOS" , DCAM_IDPROP_SUBARRAYVPOS , NULL );
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHSIZE", DCAM_IDPROP_SUBARRAYHSIZE, NULL );
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVSIZE", DCAM_IDPROP_SUBARRAYVSIZE, NULL );

    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHPOS VIEW1" , DCAM_IDPROP_VIEW_(1, DCAM_IDPROP_SUBARRAYHPOS ), NULL );
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVPOS VIEW1" , DCAM_IDPROP_VIEW_(1, DCAM_IDPROP_SUBARRAYVPOS ), NULL );
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHSIZE VIEW1", DCAM_IDPROP_VIEW_(1, DCAM_IDPROP_SUBARRAYHSIZE), NULL );
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVSIZE VIEW1", DCAM_IDPROP_VIEW_(1, DCAM_IDPROP_SUBARRAYVSIZE), NULL );

    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHPOS VIEW2" , DCAM_IDPROP_VIEW_(2, DCAM_IDPROP_SUBARRAYHPOS ), NULL );
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVPOS VIEW2" , DCAM_IDPROP_VIEW_(2, DCAM_IDPROP_SUBARRAYVPOS ), NULL );
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYHSIZE VIEW2", DCAM_IDPROP_VIEW_(2, DCAM_IDPROP_SUBARRAYHSIZE), NULL );
    DEB_TRACE() << g_trace_line_separator.c_str();
    traceFeatureGeneralInformations(m_camera_handle, "DCAM_IDPROP_SUBARRAYVSIZE VIEW2", DCAM_IDPROP_VIEW_(2, DCAM_IDPROP_SUBARRAYVSIZE), NULL );
}

//-----------------------------------------------------------------------------
/// Return the sensor temperature support by the current detector
//-----------------------------------------------------------------------------
bool Camera::isSensorTemperatureSupported(void)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;
    bool    supported;
    double  temperature = 0.0;
    
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORTEMPERATURE, &temperature );
    
    if( failed(err) )
    {
        if((err == DCAMERR_INVALIDPROPERTYID)||(err == DCAMERR_NOTSUPPORT))
        {
            supported = false;
        }
        else
        {
            manage_trace( deb, "Unable to retrieve the sensor temperature", err, "dcamprop_getvalue - DCAM_IDPROP_SENSORTEMPERATURE");
            THROW_HW_ERROR(Error) << "Unable to retrieve the sensor temperature";
        }
    }    
    else
    {
        supported = true;
    }

    return supported;
}

//-----------------------------------------------------------------------------
/// Return the current sensor temperature
//-----------------------------------------------------------------------------
double Camera::getSensorTemperature(void)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;
    double  temperature = 0.0;
    
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORTEMPERATURE, &temperature );
    
    if( failed(err) )
    {
        manage_trace( deb, "Unable to retrieve the sensor temperature", err, "dcamprop_getvalue - DCAM_IDPROP_SENSORTEMPERATURE");

        if((err != DCAMERR_INVALIDPROPERTYID)&&(err != DCAMERR_NOTSUPPORT))
        {
            THROW_HW_ERROR(Error) << "Unable to retrieve the sensor temperature";
        }
    }    
    else
    {
        DEB_TRACE() << DEB_VAR1(temperature);
    }
    
    return temperature;
}

//=============================================================================
// COOLER MODE
//=============================================================================
//-----------------------------------------------------------------------------
/// Return the cooler mode support by the current detector
//-----------------------------------------------------------------------------
bool Camera::isCoolerModeSupported(void)
{
    return !(getCoolerMode() == Cooler_Mode::Cooler_Mode_Not_Supported);
}

//-----------------------------------------------------------------------------
/// Return the current cooler mode
//-----------------------------------------------------------------------------
enum Camera::Cooler_Mode Camera::getCoolerMode(void)
{
    DEB_MEMBER_FUNCT();

    enum Cooler_Mode result = Cooler_Mode::Cooler_Mode_Not_Supported;

    DCAMERR err ;
    double  temp;
    
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORCOOLER, &temp );
    
    if( failed(err) )
    {
        manage_trace( deb, "Unable to retrieve the sensor cooler", err, "dcamprop_getvalue - DCAM_IDPROP_SENSORCOOLER");

        if((err != DCAMERR_INVALIDPROPERTYID)&&(err != DCAMERR_NOTSUPPORT))
        {
            THROW_HW_ERROR(Error) << "Unable to retrieve the sensor cooler";
        }
    }    
    else
    {
        int32 nMode = static_cast<int32>(temp);

        DEB_TRACE() << DEB_VAR1(nMode);

        switch (nMode)
        {
            case DCAMPROP_SENSORCOOLER__OFF: result = Cooler_Mode::Cooler_Mode_Off; break;
            case DCAMPROP_SENSORCOOLER__ON : result = Cooler_Mode::Cooler_Mode_On ; break;
            case DCAMPROP_SENSORCOOLER__MAX: result = Cooler_Mode::Cooler_Mode_Max; break;
            default: break; // result will be Sensor_Cooler_Not_Supported
        }
    }
    
    return result;
}

//-----------------------------------------------------------------------------
// Get the cooler mode label.
//-----------------------------------------------------------------------------
std::string Camera::getCoolerModeLabel(void)
{
    return getCoolerModeLabelFromMode(getCoolerMode());
}

//-----------------------------------------------------------------------------
// Get a cooler mode label from a mode.
//-----------------------------------------------------------------------------
std::string Camera::getCoolerModeLabelFromMode(enum Camera::Cooler_Mode in_cooler_mode)
{
    std::string label = "";

    switch (in_cooler_mode)
    {
        case Camera::Cooler_Mode_Off          : label = SENSOR_COOLER_OFF          ; break;
        case Camera::Cooler_Mode_On           : label = SENSOR_COOLER_ON           ; break;
        case Camera::Cooler_Mode_Max          : label = SENSOR_COOLER_MAX          ; break;
        case Camera::Cooler_Mode_Not_Supported: label = SENSOR_COOLER_NOT_SUPPORTED; break;
        default: break;
    }

    return label;
}

//=============================================================================
// TEMPERATURE STATUS
//=============================================================================
//-----------------------------------------------------------------------------
/// Return the temperature status support by the current detector
//-----------------------------------------------------------------------------
bool Camera::isTemperatureStatusSupported(void)
{
    return !(getTemperatureStatus() == Temperature_Status::Temperature_Status_Not_Supported);
}

//-----------------------------------------------------------------------------
/// Return the current temperature status
//-----------------------------------------------------------------------------
enum Camera::Temperature_Status Camera::getTemperatureStatus(void)
{
    DEB_MEMBER_FUNCT();

    enum Temperature_Status result = Temperature_Status::Temperature_Status_Not_Supported;

    DCAMERR err ;
    double  temp;
    
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORTEMPERATURE_STATUS, &temp );
    
    if( failed(err) )
    {
        manage_trace( deb, "Unable to retrieve the temperature status", err, "dcamprop_getvalue - DCAM_IDPROP_SENSORTEMPERATURE_STATUS");

        if((err != DCAMERR_INVALIDPROPERTYID)&&(err != DCAMERR_NOTSUPPORT))
        {
            THROW_HW_ERROR(Error) << "Unable to retrieve the temperature status";
        }
    }    
    else
    {
        int32 nMode = static_cast<int32>(temp);

        DEB_TRACE() << DEB_VAR1(nMode);

        switch (nMode)
        {
            case DCAMPROP_SENSORTEMPERATURE_STATUS__NORMAL    : result = Temperature_Status::Temperature_Status_Normal    ; break;
            case DCAMPROP_SENSORTEMPERATURE_STATUS__WARNING   : result = Temperature_Status::Temperature_Status_Warning   ; break;
            case DCAMPROP_SENSORTEMPERATURE_STATUS__PROTECTION: result = Temperature_Status::Temperature_Status_Protection; break;
            default: break; // result will be Temperature_Status_Not_Supported
        }
    }
    
    return result;
}

//-----------------------------------------------------------------------------
// Get a temperature status label from the status.
//-----------------------------------------------------------------------------
std::string Camera::getTemperatureStatusLabelFromStatus(enum Camera::Temperature_Status in_temperature_status)
{
    std::string label = "";

    switch (in_temperature_status)
    {
        case Camera::Temperature_Status_Not_Supported : label = TEMPERATURE_STATUS_NOT_SUPPORTED; break;
        case Camera::Temperature_Status_Normal        : label = TEMPERATURE_STATUS_NORMAL       ; break;
        case Camera::Temperature_Status_Warning       : label = TEMPERATURE_STATUS_WARNING      ; break;
        case Camera::Temperature_Status_Protection    : label = TEMPERATURE_STATUS_PROTECTION   ; break;
        default: break;
    }

    return label;
}

//-----------------------------------------------------------------------------
// Get the temperature status label.
//-----------------------------------------------------------------------------
std::string Camera::getTemperatureStatusLabel(void)
{
    return getTemperatureStatusLabelFromStatus(getTemperatureStatus());
}

//=============================================================================
// COOLER STATUS
//=============================================================================
//-----------------------------------------------------------------------------
// Get a cooler status label from a status.
//-----------------------------------------------------------------------------
std::string Camera::getCoolerStatusLabelFromStatus(enum Camera::Cooler_Status in_cooler_status)
{
    std::string label = "";

    switch (in_cooler_status)
    {
        case Camera::Cooler_Status_Not_Supported : label = COOLER_STATUS_NOT_SUPPORTED; break;
        case Camera::Cooler_Status_Error4        : label = COOLER_STATUS_ERROR4       ; break;
        case Camera::Cooler_Status_Error3        : label = COOLER_STATUS_ERROR3       ; break;
        case Camera::Cooler_Status_Error2        : label = COOLER_STATUS_ERROR2       ; break;
        case Camera::Cooler_Status_Error1        : label = COOLER_STATUS_ERROR1       ; break;
        case Camera::Cooler_Status_None          : label = COOLER_STATUS_NONE         ; break;
        case Camera::Cooler_Status_Off           : label = COOLER_STATUS_OFF          ; break;
        case Camera::Cooler_Status_Ready         : label = COOLER_STATUS_READY        ; break;
        case Camera::Cooler_Status_Busy          : label = COOLER_STATUS_BUSY         ; break;
        case Camera::Cooler_Status_Always        : label = COOLER_STATUS_ALWAYS       ; break;
        case Camera::Cooler_Status_Warning       : label = COOLER_STATUS_WARNING      ; break;

        default: break;
    }

    return label;
}

//-----------------------------------------------------------------------------
/// Return the cooler status support by the current detector
//-----------------------------------------------------------------------------
bool Camera::isCoolerStatusSupported(void)
{
    return !(getCoolerStatus() == Cooler_Status::Cooler_Status_Not_Supported);
}

//-----------------------------------------------------------------------------
/// Return the current cooler status
//-----------------------------------------------------------------------------
enum Camera::Cooler_Status Camera::getCoolerStatus(void)
{
    DEB_MEMBER_FUNCT();

    enum Cooler_Status result = Cooler_Status::Cooler_Status_Not_Supported;

    DCAMERR err ;
    double  temp;
    
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_SENSORCOOLERSTATUS, &temp );
    
    if( failed(err) )
    {
        manage_trace( deb, "Unable to retrieve the cooler status", err, "dcamprop_getvalue - DCAM_IDPROP_SENSORCOOLERSTATUS");

        if((err != DCAMERR_INVALIDPROPERTYID)&&(err != DCAMERR_NOTSUPPORT))
        {
            THROW_HW_ERROR(Error) << "Unable to retrieve the cooler status";
        }
    }    
    else
    {
        int32 nMode = static_cast<int32>(temp);

        DEB_TRACE() << DEB_VAR1(nMode);

        switch (nMode)
        {
            case DCAMPROP_SENSORCOOLERSTATUS__ERROR4 : result = Cooler_Status::Cooler_Status_Error4 ; break;
            case DCAMPROP_SENSORCOOLERSTATUS__ERROR3 : result = Cooler_Status::Cooler_Status_Error3 ; break;
            case DCAMPROP_SENSORCOOLERSTATUS__ERROR2 : result = Cooler_Status::Cooler_Status_Error2 ; break;
            case DCAMPROP_SENSORCOOLERSTATUS__ERROR1 : result = Cooler_Status::Cooler_Status_Error1 ; break;
            case DCAMPROP_SENSORCOOLERSTATUS__NONE   : result = Cooler_Status::Cooler_Status_None   ; break;
            case DCAMPROP_SENSORCOOLERSTATUS__OFF    : result = Cooler_Status::Cooler_Status_Off    ; break;
            case DCAMPROP_SENSORCOOLERSTATUS__READY  : result = Cooler_Status::Cooler_Status_Ready  ; break;
            case DCAMPROP_SENSORCOOLERSTATUS__BUSY   : result = Cooler_Status::Cooler_Status_Busy   ; break;
            case DCAMPROP_SENSORCOOLERSTATUS__ALWAYS : result = Cooler_Status::Cooler_Status_Always ; break;
            case DCAMPROP_SENSORCOOLERSTATUS__WARNING: result = Cooler_Status::Cooler_Status_Warning; break;
            default: break; // result will be Cooler_Status_Not_Supported
        }
    }
    
    return result;
}

//-----------------------------------------------------------------------------
// Get the cooler status label.
//-----------------------------------------------------------------------------
std::string Camera::getCoolerStatusLabel(void)
{
    return getCoolerStatusLabelFromStatus(getCoolerStatus());
}

//=============================================================================
// HIGH DYNAMIC RANGE
//=============================================================================
//-----------------------------------------------------------------------------
/// Return the high dynamic range support by the current detector
//-----------------------------------------------------------------------------
bool Camera::isHighDynamicRangeSupported(void)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;
    bool    supported;
    double  temp;
    
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_HIGHDYNAMICRANGE_MODE, &temp );
    
    if( failed(err) )
    {
        if((err == DCAMERR_INVALIDPROPERTYID)||(err == DCAMERR_NOTSUPPORT))
        {
            supported = false;
        }
        else
        {
            manage_trace( deb, "Unable to retrieve the high dynamic range mode", err, "dcamprop_getvalue - DCAM_IDPROP_HIGHDYNAMICRANGE_MODE");
            THROW_HW_ERROR(Error) << "Unable to retrieve the high dynamic range mode";
        }
    }    
    else
    {
        supported = true;
    }

    return supported;
}

//-----------------------------------------------------------------------------
/// get the current high dynamic range activation state
//-----------------------------------------------------------------------------
bool Camera::getHighDynamicRangeEnabled(void)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;
    double  temp;
    bool    high_dynamic_range_mode = false;
    
    if(getStatus() == CameraThread::Ready)
    {
        err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_HIGHDYNAMICRANGE_MODE, &temp );
        
        if( failed(err) )
        {
            manage_trace( deb, "Unable to retrieve the high dynamic range mode", err, "dcamprop_getvalue - DCAM_IDPROP_HIGHDYNAMICRANGE_MODE");

            if((err != DCAMERR_INVALIDPROPERTYID)&&(err != DCAMERR_NOTSUPPORT))
            {
                THROW_HW_ERROR(Error) << "Unable to retrieve the high dynamic range mode";
            }
        }    
        else
        {
            int high_dynamic_range = static_cast<int>(temp);

            DEB_TRACE() << DEB_VAR1(high_dynamic_range);

            if(high_dynamic_range == DCAMPROP_MODE__OFF) high_dynamic_range_mode = false;
            else
            if(high_dynamic_range == DCAMPROP_MODE__ON ) high_dynamic_range_mode = true ;
            else
            {
                manage_trace( deb, "The read high dynamic range mode is incoherent!", err, "dcamprop_getvalue - DCAM_IDPROP_HIGHDYNAMICRANGE_MODE");
            }
        }
    }
    else
    // do not call the sdk functions during acquisition
    {
        high_dynamic_range_mode = m_hdr_enabled;
    }
    
    return high_dynamic_range_mode;
}

//-----------------------------------------------------------------------------
/// set the current high dynamic range activation state
//-----------------------------------------------------------------------------
void Camera::setHighDynamicRangeEnabled(const bool & in_enabled)
{
    DEB_MEMBER_FUNCT();

    DCAMERR err;

    double temp = (in_enabled) ? static_cast<double>(DCAMPROP_MODE__ON) : static_cast<double>(DCAMPROP_MODE__OFF);

    // set the value
    err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_HIGHDYNAMICRANGE_MODE, temp);

    if( failed(err) )
    {
        manage_error( deb, "Cannot set high dynamic range mode", err, 
                      "dcamprop_setvalue", "IDPROP=DCAM_IDPROP_HIGHDYNAMICRANGE_MODE, VALUE=%d", static_cast<int>(temp));
        THROW_HW_ERROR(Error) << "Cannot set high dynamic range mode";
    }

    manage_trace( deb, "Changed high dynamic range mode", DCAMERR_NONE, NULL, "%s", ((in_enabled) ? "DCAMPROP_MODE__ON" : "DCAMPROP_MODE__OFF"));

    // forcing the image pixel type to 16 bits
    dcamex_setimagepixeltype( m_camera_handle, DCAM_PIXELTYPE_MONO16);

    // keep the latest value
    m_hdr_enabled = in_enabled;
}

//=============================================================================
//-----------------------------------------------------
// Return the event control object
//-----------------------------------------------------
HwEventCtrlObj* Camera::getEventCtrlObj()
{
    return &m_event_ctrl_obj;
}

//=============================================================================
// OUTPUT TRIGGER KIND
//=============================================================================
//-----------------------------------------------------------------------------
/// Return the output trigger kind of given channel by the current detector
//-----------------------------------------------------------------------------
enum Camera::Output_Trigger_Kind Camera::getOutputTriggerKind(int channel)
{
    DEB_MEMBER_FUNCT();

    DEB_TRACE() << " Camera::Output_Trigger_Kind Camera::getOutputTriggerKind(int channel) : ..."; 

    DCAMERR err;
    double kindArraySize = 0;
    enum Output_Trigger_Kind kind = Camera::Output_Trigger_Kind_Not_Supported;

    // get property attribute that contains the Kind (that may be an array)
    DCAMPROP_ATTR    basepropattr;
    memset(&basepropattr, 0, sizeof(basepropattr));
    basepropattr.cbSize = sizeof(basepropattr);
    basepropattr.iProp = DCAM_IDPROP_OUTPUTTRIGGER_KIND;
    err = dcamprop_getattr(m_camera_handle, &basepropattr);

    if( failed(err) )
    {
        manage_trace( deb, "Unable to retrieve the output trigger kind attribute", err, "dcamprop_getattr - DCAM_IDPROP_OUTPUTTRIGGER_KIND");

        if((err != DCAMERR_INVALIDPROPERTYID) && (err != DCAMERR_NOTSUPPORT))
        {
            THROW_HW_ERROR(Error) << "Unable to retrieve the output trigger kind attribute";
        }
    }    
    else
    {
        //Get the ARRAYELEMENT size to ensure that the given channel is reachable
        err = dcamprop_getvalue(m_camera_handle, basepropattr.iProp_NumberOfElement, &kindArraySize);
        if (!failed(err) && channel < kindArraySize)
        {
            //Get the channel kind value
            double tmp = 99;
            err = dcamprop_getvalue(m_camera_handle, basepropattr.iProp + channel * basepropattr.iPropStep_Element, &tmp);

            if(!failed(err))
            {                
                int32 value = static_cast<int32>(tmp);

                switch(value) 
                {
                    case DCAMPROP_OUTPUTTRIGGER_KIND__LOW              : kind = Camera::Output_Trigger_Kind_Low             ; break;
                    case DCAMPROP_OUTPUTTRIGGER_KIND__EXPOSURE         : kind = Camera::Output_Trigger_Kind_Global_Exposure ; break;
                    case DCAMPROP_OUTPUTTRIGGER_KIND__PROGRAMABLE      : kind = Camera::Output_Trigger_Kind_Programmable    ; break;
                    case DCAMPROP_OUTPUTTRIGGER_KIND__TRIGGERREADY     : kind = Camera::Output_Trigger_Kind_TriggerReady    ; break;
                    case DCAMPROP_OUTPUTTRIGGER_KIND__HIGH             : kind = Camera::Output_Trigger_Kind_High            ; break;
                    default: break; // result will be Output_Trigger_Kind_Not_Supported

                }
            } //else TODO
        }
    }
    
    return kind;
}

//=============================================================================
// OUTPUT TRIGGER POLARITY
//=============================================================================
//-----------------------------------------------------------------------------
/// Return the output trigger polarity of given channel by the current detector
//-----------------------------------------------------------------------------
enum Camera::Output_Trigger_Polarity Camera::getOutputTriggerPolarity(int channel)
{
    DEB_MEMBER_FUNCT();

    DEB_TRACE() << "Camera::getOutputTriggerPolarity(int channel) : channel = " << channel;

    DCAMERR err;
    double polarityArraySize = 0;
    enum Output_Trigger_Polarity polarity = Camera::Output_Trigger_Polarity_Not_Supported;

    // get property attribute that contains the Polarity (that may be an array)
    DCAMPROP_ATTR basepropattr;
    memset(&basepropattr, 0, sizeof(basepropattr));
    basepropattr.cbSize = sizeof(basepropattr);
    basepropattr.iProp = DCAM_IDPROP_OUTPUTTRIGGER_POLARITY;
    err = dcamprop_getattr(m_camera_handle, &basepropattr);

    DEB_TRACE() << " Camera::getOutputTriggerPolarity(int channel) : get property attribute done";

    if (failed(err))
    {
        manage_trace(deb, "Unable to retrieve the output trigger kind attribute", err, "dcamprop_getattr - DCAM_IDPROP_OUTPUTTRIGGER_KIND");

        if ((err != DCAMERR_INVALIDPROPERTYID) && (err != DCAMERR_NOTSUPPORT))
        {
            THROW_HW_ERROR(Error) << "Unable to retrieve the output trigger kind attribute";
        }
    }
    else
    {
        //Get the ARRAYELEMENT size to ensure that the given channel is reachable
        err = dcamprop_getvalue(m_camera_handle, basepropattr.iProp_NumberOfElement, &polarityArraySize);
        if (!failed(err) && channel < polarityArraySize)
        {
            //Get the channel polarity value
            double tmp = 99;
            err = dcamprop_getvalue(m_camera_handle, basepropattr.iProp + channel * basepropattr.iPropStep_Element, &tmp);

            if (!failed(err))
            {
                int32 value = static_cast<int32>(tmp);

                switch (value)
                {
                case DCAMPROP_OUTPUTTRIGGER_POLARITY__NEGATIVE:
                    polarity = Camera::Output_Trigger_Polarity_Negative;
                    break;
                case DCAMPROP_OUTPUTTRIGGER_POLARITY__POSITIVE:
                    polarity = Camera::Output_Trigger_Polarity_Positive;
                    break;
                default:
                    break; // result will be Output_Trigger_Polarity_Not_Supported
                }
            }
        }
    }

    return polarity;
}

//=============================================================================
// OUTPUT TRIGGER KIND
//=============================================================================
//-----------------------------------------------------------------------------
/// Set the output trigger kind of given channel by the current detector
//-----------------------------------------------------------------------------
void Camera::setOutputTriggerKind(int channel, enum Output_Trigger_Kind in_output_trig_kind)
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(in_output_trig_kind);

    DEB_TRACE() << "Camera::setOutputTriggerKind(int channel, enum Output_Trigger_Kind in_output_trig_kind)";

    DCAMERR  err ;
    int      kind;

    if(in_output_trig_kind == Output_Trigger_Kind_Low)
    {
        kind = DCAMPROP_OUTPUTTRIGGER_KIND__LOW;
    }
    else
    if(in_output_trig_kind == Output_Trigger_Kind_Global_Exposure)
    {
        kind = DCAMPROP_OUTPUTTRIGGER_KIND__EXPOSURE;
    }
    else
    if(in_output_trig_kind == Output_Trigger_Kind_Programmable)
    {
        kind = DCAMPROP_OUTPUTTRIGGER_KIND__PROGRAMABLE;
    }
    else
    if(in_output_trig_kind == Output_Trigger_Kind_TriggerReady)
    {
        kind = DCAMPROP_OUTPUTTRIGGER_KIND__TRIGGERREADY;
    }
    else
    if(in_output_trig_kind == Output_Trigger_Kind_High)
    {
        kind = DCAMPROP_OUTPUTTRIGGER_KIND__HIGH;
    }
    else
    {
        manage_error( deb,  "Unable to set the Output trigger Kind",
                            DCAMERR_NONE, 
                            "",
                            "in_output_trig_kind is unknown %d",
                            static_cast<int>(in_output_trig_kind));

        THROW_HW_ERROR(Error) << "Unable to set the Output trigger Kind";
    }

    //Compute property ID for given channel
    int32 property_id = 0;
    int32 array_base = 0;
    int32 step_element = 0;

    getPropertyData(DCAM_IDPROP_OUTPUTTRIGGER_KIND, array_base, step_element);

    property_id = array_base + step_element * channel;

    // set the kind
    err = dcamprop_setvalue( m_camera_handle,property_id , static_cast<double>(kind) );

    if( failed(err) )
    {
        if((err == DCAMERR_INVALIDPROPERTYID) || (err == DCAMERR_NOTSUPPORT))
        {
            manage_trace( deb, "Unable to set the Output trigger Kind",
                                err, 
                               "dcamprop_setvalue",
                               "DCAM_IDPROP_OUTPUTTRIGGER_KIND[%d] %d",
                               channel,
                               kind);

            THROW_HW_ERROR(Error) << "Unable to set the Output trigger Kind";
        }
    }
}

//=============================================================================
// OUTPUT TRIGGER POLARITY
//=============================================================================
//-----------------------------------------------------------------------------
/// Set the output trigger polarity of given channel by the current detector
//-----------------------------------------------------------------------------
void Camera::setOutputTriggerPolarity(int in_channel, enum Camera::Output_Trigger_Polarity in_output_trig_polarity)
{
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::setOutputTriggerPolarity(int in_channel, enum Camera::Output_Trigger_Polarity in_output_trig_polarity) : ...";
    DEB_PARAM() << DEB_VAR1(in_output_trig_polarity);

    DCAMERR err;
    int     polarity;

    if(in_output_trig_polarity == Output_Trigger_Polarity_Negative)
    {
        polarity = DCAMPROP_OUTPUTTRIGGER_POLARITY__NEGATIVE;
    }
    else if(in_output_trig_polarity ==  Output_Trigger_Polarity_Positive)
    {
        polarity = DCAMPROP_OUTPUTTRIGGER_POLARITY__POSITIVE;
    }
    else
    {
        manage_error( deb,  "Unable to set the Output trigger Polarity", 
                            DCAMERR_NONE, 
                            "",
                            "in_output_trig_polarity is unknown %d",
                            static_cast<int>(in_output_trig_polarity));
        
        THROW_HW_ERROR(Error) << "Unable to set the Output trigger Polarity";
    }

    //Compute property ID for given channel
    int32 property_id = 0;
    int32 array_base = 0;
    int32 step_element = 0;

    getPropertyData(DCAM_IDPROP_OUTPUTTRIGGER_POLARITY, array_base, step_element);

    property_id = array_base + step_element * in_channel;

    //set the polarity
    err = dcamprop_setvalue(m_camera_handle, property_id, static_cast<double>(polarity) );

     if( failed(err) )
    {
        if((err == DCAMERR_INVALIDPROPERTYID) || (err == DCAMERR_NOTSUPPORT))
        {
            manage_trace( deb, "Unable to set the Output trigger Polarity", 
                                err, 
                                "dcamprop_setvalue",
                                "DCAM_IDPROP_OUTPUTTRIGGER_POLARITY[%d] %d", 
                                in_channel,
                                polarity);

            THROW_HW_ERROR(Error) << "Unable to set the Output trigger Polarity";
        }
    }
}

//-----------------------------------------------------------------------------
/// Utility function
//-----------------------------------------------------------------------------
void Camera::getPropertyData(int32 property, int32 & array_base, int32 & step_element)
{
    DCAMERR err;
    // get property attribute
    DCAMPROP_ATTR basepropattr;
    memset(&basepropattr, 0, sizeof(basepropattr));
    basepropattr.cbSize = sizeof(basepropattr);
    basepropattr.iProp = property;
    err = dcamprop_getattr(m_camera_handle, &basepropattr);
    if (!failed(err))
    {
        array_base = basepropattr.iProp_ArrayBase;
        step_element = basepropattr.iPropStep_Element;
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
std::string Camera::getAllParameters()
{
    DEB_MEMBER_FUNCT();

    std::stringstream res;

    int32 parameter_id = 0; /* parameter ID */
    int32 last_id = 0;
	
    char name[ 64 ];
	DCAMERR err;
	
	do
	{
        err = dcamprop_getnextid(m_camera_handle, &parameter_id, DCAMPROP_OPTION_SUPPORT);
        if(failed(err) || last_id == parameter_id)
        {
            break;
        }
        last_id = parameter_id;
        /* Getting parameter name. */
        err = dcamprop_getname(m_camera_handle, parameter_id, name, sizeof(name));
        if(failed(err))
        {
            break;
        }
        std::string param = getParameter(name); 

        res << name << " = "<< param;
        
	} while(!failed(err) && parameter_id != 0);

    return res.str();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
std::string Camera::getParameter(std::string parameter_name)
{
    DEB_MEMBER_FUNCT();

    std::stringstream res;
	DCAMERR err;

    double value;

    int parameter_id = m_map_parameters[parameter_name];
    err = dcamprop_getvalue(m_camera_handle, parameter_id, &value);
    if(failed(err))
    {
        manage_error( deb, "Unable to get the value of the parameter", err, 
                               "dcamprop_getvalue");
        THROW_HW_ERROR(Error) << "Unable to get the value of the parameter";
    }
    
    res << value << std::endl;
    return res.str();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Camera::setParameter(std::string parameter_name, double value)
{
    DEB_MEMBER_FUNCT();

	DCAMERR err;
    int parameter_id = m_map_parameters[parameter_name];
    err = dcamprop_setvalue(m_camera_handle, parameter_id, value);
    if(failed(err))
    {
        if(err == DCAMERR_NOTSUPPORT)
        {
            manage_error( deb, "Parameter is not supported", err, "dcamprop_setvalue");
            THROW_HW_ERROR(Error) << "Parameter is not supported";
        }
        else if (err == DCAMERR_INVALIDPARAM)
        {
            manage_error( deb, "Invalid parameter", err, "dcamprop_setvalue");
            THROW_HW_ERROR(Error) << "Invalid parameter";
        }
        else
        {
            manage_error( deb, "Unable to set the parameter", err, "dcamprop_setvalue");
            THROW_HW_ERROR(Error) << "Unable to set the parameter";
        }
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Camera::initParametersMap()
{
    DEB_MEMBER_FUNCT();

    std::stringstream res;

    int32 parameter_id; /* parameter ID */
	
	parameter_id = 0;
	DCAMERR err;
	
	do
	{
        err = dcamprop_getnextid(m_camera_handle, &parameter_id, DCAMPROP_OPTION_SUPPORT);

        if(failed(err))
        {
            break;
        }

        mapIdParameter(parameter_id);
        
	} while(!failed(err) && parameter_id != 0);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Camera::mapIdParameter(int32 parameter_id)
{
    DEB_MEMBER_FUNCT();

    std::stringstream res;
	DCAMERR err;

    char name[ 64 ];
 
    /* Getting parameter name. */
    err = dcamprop_getname(m_camera_handle, parameter_id, name, sizeof(name));
    if(failed(err))
    {
        manage_error( deb, "Unable to get the name of the parameter", err, 
                               "dcamprop_getname");
        THROW_HW_ERROR(Error) << "Unable to get the name of the parameter";
    }
    m_map_parameters.insert({name, parameter_id});
}