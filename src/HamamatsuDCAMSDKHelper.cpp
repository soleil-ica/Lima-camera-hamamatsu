///###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2017
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

/* Visual Studios 2012 and earlier don't have va_copy() */
#if defined(_MSC_VER) && _MSC_VER <= 1700
    #define va_copy(dest, src) ((dest) = (src))
#endif

//-----------------------------------------------------------------------------
/// fill a standard string with printf format (va_list way)
/*!
@return string
*/
//-----------------------------------------------------------------------------
std::string Camera::string_format_arg(const char* format, va_list args)
{
    va_list     args_copy;
    int         size     ;

    va_copy( args_copy, args ); // we make a copy to avoid memory problems

    size = _vscprintf(format, args_copy);
    std::string result(++size, 0);
    vsnprintf_s((char*)result.data(), size, _TRUNCATE, format, args_copy);

    va_end(args_copy);

    return result;
}

//-----------------------------------------------------------------------------
/// fill a standard string with printf format
/*!
@return string
*/
//-----------------------------------------------------------------------------
std::string Camera::string_format(const char* format, ...)
{
    va_list     args  ;
    std::string result;

    va_start(args, format);
    result = string_format_arg(format, args);
    va_end(args);

    return result;
}

//-----------------------------------------------------------------------------
/// find the corresponding string using DCAM-API.
/*!
@return the error string
*/
//-----------------------------------------------------------------------------
string Camera::dcam_get_string( HDCAM hd_cam ,       ///< [in] camera handle 
                                int32 id_str ) const ///< [in] string identifier
{
	DEB_MEMBER_FUNCT();

    std::string      text     ;
    DCAMDEV_STRING	 param    ;
    DCAMERR          err      ;
    char           * chartext = new char[Camera::g_dcam_str_msg_size];

    memset( chartext, 0, sizeof(chartext) );
    memset( &param  , 0, sizeof(param   ) );
    
    param.size		= sizeof(param);
    param.text		= chartext     ;
    param.iString	= id_str        ;
    param.textbytes	= sizeof(char) * Camera::g_dcam_str_msg_size;
	
    err  = dcamdev_getstring( hd_cam, &param );

    if(failed( err ))
    {
        DEB_TRACE() << "dcamdev_getstring failed - ErrorId:" << DEB_HEX(err) << " StringId:" << DEB_HEX(id_str);
        text = "Could not found the corresponding string!";
    }
    else
    {
        text = chartext;
    }

    delete [] chartext;

    return( text );
}

//-----------------------------------------------------------------------------
//  trace method with optional parameters (printf style).
/// find the corresponding string error using DCAM-API and trace this
//  information as an error or an info.
/*!
@return none
*/
//-----------------------------------------------------------------------------
void Camera::manage_trace( DebObj     & deb       ,       ///< [in] trace object
                           const char * opt_desc   ,       ///< [in] optional description (NULL if not used)
                           int32        id_str     ,       ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                           const char * fct       ,       ///< [in] function name which returned the error (NULL if not used)
                           const char * opt  , ...) const ///< [in] optional string to concat to the error string (NULL if not used)
{
    va_list args(NULL);

    va_start(args, opt);
    static_trace_string_va_list( this, deb, opt_desc, id_str, fct, opt, args, false);
    va_end(args);
}

//-----------------------------------------------------------------------------
//  static trace method with optional parameters (printf style).
/// find the corresponding string error using DCAM-API and trace this
//  information as an error or an info.
/*!
@return none
*/
//-----------------------------------------------------------------------------
void Camera::static_manage_trace( const Camera     * const cam,      ///< [in] camera object
                                  DebObj           & deb      ,      ///< [in] trace object
                                  const char       * opt_desc  ,      ///< [in] optional description (NULL if not used)
                                  int32              id_str    ,      ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                                  const char       * fct      ,      ///< [in] function name which returned the error (NULL if not used)
                                  const char       * opt      , ...) ///< [in] optional string to concat to the error string (NULL if not used)
{
    va_list args(NULL);

    va_start(args, opt);
    static_trace_string_va_list( cam, deb, opt_desc, id_str, fct, opt, args, false);
    va_end(args);
}

//-----------------------------------------------------------------------------
//  trace method with optional parameters (printf style).
/// find the corresponding string error using DCAM-API and trace this
//  information as an error or an info.
/*!
@return none
*/
//-----------------------------------------------------------------------------
void Camera::manage_error( DebObj     & deb       ,       ///< [in] trace object
                           const char * opt_desc   ,       ///< [in] optional description (NULL if not used)
                           int32        id_str     ,       ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                           const char * fct       ,       ///< [in] function name which returned the error (NULL if not used)
                           const char * opt  , ...) const ///< [in] optional string to concat to the error string (NULL if not used)
{
    va_list args(NULL);

    va_start(args, opt);
    static_trace_string_va_list( this, deb, opt_desc, id_str, fct, opt, args, true);
    va_end(args);
}

//-----------------------------------------------------------------------------
//  static trace method with optional parameters (printf style).
/// find the corresponding string error using DCAM-API and trace this
//  information as an error or an info.
/*!
@return the trace text
*/
//-----------------------------------------------------------------------------
std::string Camera::static_manage_error( const Camera     * const cam,      ///< [in] camera object
                                         DebObj           & deb      ,      ///< [in] trace object
                                         const char       * opt_desc  ,      ///< [in] optional description (NULL if not used)
                                         int32              id_str    ,      ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                                         const char       * fct      ,      ///< [in] function name which returned the error (NULL if not used)
                                         const char       * opt      , ...) ///< [in] optional string to concat to the error string (NULL if not used)
{
    va_list     args(NULL);
    std::string final_text ;

    va_start(args, opt);
    final_text = static_trace_string_va_list( cam, deb, opt_desc, id_str, fct, opt, args, true);
    va_end(args);
    
    return final_text;
}

//-----------------------------------------------------------------------------
//  static trace method with va_list optional parameters.
/// find the corresponding string error using DCAM-API and trace this
//  information as an error or an info.
/*!
@return the trace text
*/
//-----------------------------------------------------------------------------
std::string Camera::static_trace_string_va_list( const Camera     * const cam, ///< [in] camera object
                                          DebObj           & deb      , ///< [in] trace object
                                          const char       * opt_desc  , ///< [in] optional description (NULL if not used)
                                          int32              id_str    , ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                                          const char       * fct      , ///< [in] function name which returned the error (NULL if not used)
                                          const char       * opt      , ///< [in] optional string to concat to the error string (NULL if not used)
                                          va_list            args     , ///< [in] optional args (printf style) to merge with the opt string (NULL if not used)
                                          bool               is_error  ) ///< [in] true if traced like an error, false for a classic info trace
{
    std::string final_text("");
    std::string separator(" - "); 

    // optional description
    if(opt_desc != NULL)
    {
        final_text += opt_desc;
    }

    // optional function name
    if(fct != NULL)
    {
        if(!final_text.empty())
            final_text += separator;
        
        final_text += Camera::string_format("%s FAILED", fct);
    }

    // dcam error
    if(id_str != DCAMERR_NONE)
    {
        std::string ErrorString = cam->dcam_get_string( cam->m_camera_handle, id_str ); // we get the d-cam error string

        if(!final_text.empty())
            final_text += separator;

        final_text += Camera::string_format("(DCAMERR 0x%08X %s)", id_str, ErrorString.c_str());
    }

    // optional string
	if( opt != NULL ) 
	{
        if (args != NULL)
        {
            va_list args_copy(NULL);

            va_copy( args_copy, args ); // we make a copy to avoid memory problems

            if(!final_text.empty())
                final_text += separator;

            final_text += string_format_arg(opt, args_copy);

            // free the arg copy
            va_end(args_copy);
        }
        else
        {
            final_text += opt;
        }
	}

    if(is_error)
    {
        DEB_ERROR() << final_text;
    }
    else
    {
        DEB_TRACE() << final_text;
    }
    
    return final_text;
}

//-----------------------------------------------------------------------------
/// Show camera model info
//-----------------------------------------------------------------------------
void Camera::showCameraInfo(HDCAM hd_cam) ///< [in] camera device id
{
    DEB_MEMBER_FUNCT();

    DEB_TRACE() << "Retrieving camera information...";
    DEB_TRACE() << "VENDOR         > " << dcam_get_string( hd_cam, DCAM_IDSTR_VENDOR       ).c_str();
    DEB_TRACE() << "MODEL          > " << dcam_get_string( hd_cam, DCAM_IDSTR_MODEL        ).c_str();
    DEB_TRACE() << "BUS            > " << dcam_get_string( hd_cam, DCAM_IDSTR_BUS          ).c_str();
    DEB_TRACE() << "CAMERA_ID      > " << dcam_get_string( hd_cam, DCAM_IDSTR_CAMERAID     ).c_str();
    DEB_TRACE() << "CAMERA_VERSION > " << dcam_get_string( hd_cam, DCAM_IDSTR_CAMERAVERSION).c_str();
    DEB_TRACE() << "DRIVER_VERSION > " << dcam_get_string( hd_cam, DCAM_IDSTR_DRIVERVERSION).c_str();
}

//-----------------------------------------------------------------------------
/// Show camera model info in detail
//-----------------------------------------------------------------------------
void Camera::showCameraInfoDetail(HDCAM hd_cam) ///< [in] camera device id
{
    DEB_MEMBER_FUNCT();

    DEB_TRACE() << "Retrieving detailed camera information...";
    DEB_TRACE() << "VENDOR           > " << dcam_get_string( hd_cam, DCAM_IDSTR_VENDOR        ).c_str();
    DEB_TRACE() << "MODEL            > " << dcam_get_string( hd_cam, DCAM_IDSTR_MODEL         ).c_str();
    DEB_TRACE() << "BUS              > " << dcam_get_string( hd_cam, DCAM_IDSTR_BUS           ).c_str();
    DEB_TRACE() << "CAMERA_ID        > " << dcam_get_string( hd_cam, DCAM_IDSTR_CAMERAID      ).c_str();
    DEB_TRACE() << "CAMERA_VERSION   > " << dcam_get_string( hd_cam, DCAM_IDSTR_CAMERAVERSION ).c_str();
    DEB_TRACE() << "DRIVER_VERSION   > " << dcam_get_string( hd_cam, DCAM_IDSTR_DRIVERVERSION ).c_str();
    DEB_TRACE() << "MODULE_VERSION   > " << dcam_get_string( hd_cam, DCAM_IDSTR_MODULEVERSION ).c_str();
    DEB_TRACE() << "DCAM_API_VERSION > " << dcam_get_string( hd_cam, DCAM_IDSTR_DCAMAPIVERSION).c_str();
}

//-----------------------------------------------------------------------------
/// initialize DCAM-API and get HDCAM camera handle.
/*!
@return a valid camera handle or NULL if failed
*/
//-----------------------------------------------------------------------------
HDCAM Camera::dcam_init_open(long camera_number) ///< [in] id of the camera to open
{
	DEB_MEMBER_FUNCT();

    DCAMAPI_INIT param_init    ;
    DCAMERR      err           ;
    int32        init_option[] = { DCAMAPI_INITOPTION_APIVER__LATEST,
                                   DCAMAPI_INITOPTION_ENDMARK       }; // it is necessary to set as the last value.

    // initialize DCAM-API
	DEB_TRACE() << g_trace_line_separator.c_str();
	DEB_TRACE() << "calling dcam_init..."      ;

    memset( &param_init, 0, sizeof(param_init) );
    param_init.size            = sizeof(param_init) ;
    param_init.initoptionbytes = sizeof(init_option);
    param_init.initoption      = init_option        ;

	err = dcamapi_init( &param_init );
	if( !failed(err) )
    {
        int32 nDevice = param_init.iDeviceCount; // number of devices

        DEB_TRACE() << "dcamapi_init ok"    ;
        DEB_TRACE() << "Number of Devices : " << nDevice; 

		if( nDevice >= 1 )
		{
    		if(camera_number >= nDevice)
            {
                DEB_ERROR() << ">Incoherent camera number:" << camera_number;
            }
            else
            {
                HDCAM iDevice = reinterpret_cast<HDCAM>(camera_number);

			    // get camera information
			    showCameraInfo(iDevice);

                m_detector_model = dcam_get_string( iDevice, DCAM_IDSTR_MODEL );
                m_detector_type  = dcam_get_string( iDevice, DCAM_IDSTR_VENDOR);

    			// open specified camera
            	DEB_TRACE() << "Opening the camera ...";

                {
                    DCAMDEV_OPEN paramopen;
	            
                    memset( &paramopen, 0, sizeof(paramopen) );
	                paramopen.size	= sizeof(paramopen);
	                paramopen.index	= camera_number    ;

	                err = dcamdev_open( &paramopen );
	                
    				// success
                    if( ! failed(err) )
	                {
                    	DEB_TRACE() << "Camera opening success.";
                		showCameraInfoDetail(paramopen.hdcam);

		                return ( paramopen.hdcam );
                    }
                    else
                    {
        				DEB_ERROR() << "dcamdev_open failed";
                    }
                }
            }
        }

		// uninitialize DCAM-API
        dcamapi_uninit();	// recommended call dcamapi_uninit() when dcamapi_init() is called even if it failed.
    }

	DEB_TRACE() << "dcamapi_init() failed"; // we need a hd_cam to manage the error string...

	// failure
	return NULL;
}

//-----------------------------------------------------------------------------
/// Initialize the subarray mode (define ROI -rectangle-)
/*!
@return true if successfull
*/
//-----------------------------------------------------------------------------
bool Camera::dcamex_setsubarrayrect( HDCAM hd_cam    , ///< [in] camera handle
									 long  left     , ///< [in] left  (x)
									 long  top      , ///< [in] top   (y)
									 long  width    , ///< [in] horizontal size
									 long  height   , ///< [in] vertical size
                                     int   ViewIndex) ///< [in] View index [0...max view[. Use g_get_sub_array_do_not_use_view for general subarray 
{
	DEB_MEMBER_FUNCT();

    DCAMERR err;

    // temporary disable subarraymode
	err = dcamprop_setvalue( hd_cam, DCAM_IDPROP_SUBARRAYMODE, static_cast<double>(DCAMPROP_MODE__OFF) );
    if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_setsubarrayrect", err, "dcamprop_setvalue()", "IDPROP=SUBARRAYMODE, VALUE=OFF");
		return false;
	}

    // set the sub array width
	err = dcamprop_setvalue( hd_cam, DCAM_IDPROP_SUBARRAYHSIZE, static_cast<double>(width) );
	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_setsubarrayrect", err, "dcamprop_setvalue()", "IDPROP=SUBARRAYHSIZE, VALUE=%d", width);
		return false;
	}

    // set the sub array x value
    if(ViewIndex == g_get_sub_array_do_not_use_view)
    {
        err = dcamprop_setvalue( hd_cam, DCAM_IDPROP_SUBARRAYHPOS, static_cast<double>(left));
    }
    else
    {
        err = dcamprop_setvalue( hd_cam, DCAM_IDPROP_VIEW_((ViewIndex + 1), DCAM_IDPROP_SUBARRAYHPOS), static_cast<double>(left));
    }

	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_setsubarrayrect", err, "dcamprop_setvalue()", "IDPROP=SUBARRAYHPOS, VALUE=%d", left );
		return false;
	}

    // set the sub array height
	err = dcamprop_setvalue( hd_cam, DCAM_IDPROP_SUBARRAYVSIZE, static_cast<double>(height) );
	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_setsubarrayrect", err, "dcamprop_setvalue()", "IDPROP=SUBARRAYVSIZE, VALUE=%d",height );
		return false;
	}

    // set the sub array y value
    if(ViewIndex == g_get_sub_array_do_not_use_view)
    {
        err = dcamprop_setvalue( hd_cam, DCAM_IDPROP_SUBARRAYVPOS, static_cast<double>(top) );
    }
    else
    {
        err = dcamprop_setvalue( hd_cam, DCAM_IDPROP_VIEW_((ViewIndex + 1), DCAM_IDPROP_SUBARRAYVPOS), static_cast<double>(top) );
    }

	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_setsubarrayrect", err, "dcamprop_setvalue()", "IDPROP=SUBARRAYVPOS, VALUE=%d",top );
		return false;
	}
    
    // enable subarraymode
	err = dcamprop_setvalue( hd_cam, DCAM_IDPROP_SUBARRAYMODE, static_cast<double>(DCAMPROP_MODE__ON) );
	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_setsubarrayrect", err, "dcamprop_setvalue()", "IDPROP=SUBARRAYMODE, VALUE=ON" );
		return false;
	}
    
    return true;
}

//-----------------------------------------------------------------------------
/// Get the current subarray parameters (get ROI settings)
/*!
@return true if successfull
*/
//-----------------------------------------------------------------------------
bool Camera::dcamex_getsubarrayrect( HDCAM   hd_cam    , ///< [in] camera handle
									 int32 & left     , ///< [in] left  (x)
									 int32 & top      , ///< [in] top   (y)
									 int32 & width    , ///< [in] horizontal size
									 int32 & height   , ///< [in] vertical size
                                     int     ViewIndex) ///< [in] View index [0...max view[. Use g_get_sub_array_do_not_use_view for general subarray 
{
	DEB_MEMBER_FUNCT();

    DCAMERR err         ;
	double  generic_value;

    // get the sub array width
	err = dcamprop_getvalue( hd_cam, DCAM_IDPROP_SUBARRAYHSIZE, &generic_value );
	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_getsubarrayrect", err, "dcamprop_getvalue()", "IDPROP=SUBARRAYHSIZE" );
		return false;
	}
    else
    {
    	width = static_cast<int32>(generic_value);
    }

    // get the sub array x value
    if(ViewIndex == g_get_sub_array_do_not_use_view)
    {
        err = dcamprop_getvalue( hd_cam, DCAM_IDPROP_SUBARRAYHPOS, &generic_value);
    }
    else
    {
    	err = dcamprop_getvalue( hd_cam, DCAM_IDPROP_VIEW_((ViewIndex + 1), DCAM_IDPROP_SUBARRAYHPOS), &generic_value );
    }

	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_getsubarrayrect", err, "dcamprop_getvalue()", "IDPROP=SUBARRAYHPOS");
		return false;
	}
    else
    {
    	left = static_cast<int32>(generic_value);
    }

    // get the sub array height
	err = dcamprop_getvalue( hd_cam, DCAM_IDPROP_SUBARRAYVSIZE, &generic_value );
	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_getsubarrayrect", err, "dcamprop_getvalue()", "IDPROP=SUBARRAYVSIZE");
		return false;
	}
    else
    {
    	height = static_cast<int32>(generic_value);
    }

    // get the sub array y value
    if(ViewIndex == g_get_sub_array_do_not_use_view)
    {
        err = dcamprop_getvalue( hd_cam, DCAM_IDPROP_SUBARRAYVPOS, &generic_value );
    }
    else
    {
    	err = dcamprop_getvalue( hd_cam, DCAM_IDPROP_VIEW_((ViewIndex + 1), DCAM_IDPROP_SUBARRAYVPOS), &generic_value );
    }

	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_getsubarrayrect", err, "dcamprop_getvalue()", "IDPROP=SUBARRAYVPOS");
		return false;
	}
    else
    {
    	top = static_cast<int32>(generic_value);
    }

    return true;
}

//-----------------------------------------------------------------------------
/// Get the width of the image
/*!
@return width of the image or 0 if failed
*/
//-----------------------------------------------------------------------------
long Camera::dcamex_getimagewidth(const HDCAM hd_cam ) ///< [in] camera handle
{
	DEB_MEMBER_FUNCT();

    DCAMERR err         ;
	double  generic_value;

    // image width
	err = dcamprop_getvalue( hd_cam, DCAM_IDPROP_IMAGE_WIDTH, &generic_value );
    if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_getimagewidth", err, "dcamprop_getvalue()", "IDPROP=DCAM_IDPROP_IMAGE_WIDTH");
		return 0;
	}
	else
    {
		return static_cast<long>(generic_value);
    }
}

//-----------------------------------------------------------------------------
/// Get the height of the image
/*!
@return width of the image or 0 if failed
*/
//-----------------------------------------------------------------------------
long Camera::dcamex_getimageheight(const HDCAM hd_cam ) ///< [in] camera handle
{
	DEB_MEMBER_FUNCT();

    DCAMERR err          ;
	double  generic_value;

    // image width
	err = dcamprop_getvalue( hd_cam, DCAM_IDPROP_IMAGE_HEIGHT, &generic_value );
	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_getimageheight", err, "dcamprop_getvalue()", "IDPROP=DCAM_IDPROP_IMAGE_HEIGHT");
		return 0;
	}
	else
    {
		return static_cast<long>(generic_value);
    }
}

//-----------------------------------------------------------------------------
/// Get the number of bits per channel
/*!
@return the number of bits per pixel of 0 if failed
*/
//-----------------------------------------------------------------------------
long Camera::dcamex_getbitsperchannel( HDCAM hd_cam )    ///< [in] camera handle
{
	DEB_MEMBER_FUNCT();

    DCAMERR err             ;
	double  generic_value   ;
    long    bits_nb      = 0;
	
    err = dcamprop_getvalue( hd_cam, DCAM_IDPROP_IMAGE_PIXELTYPE, &generic_value );

	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_getbitsperchannel", err, "dcamprop_getvalue()", "IDPROP=DCAM_IDPROP_IMAGE_PIXELTYPE");
	}
    else
    {
	    switch (static_cast<int32>(generic_value))
	    {
            case DCAM_PIXELTYPE_MONO8  : bits_nb = 8 ; break;
            case DCAM_PIXELTYPE_MONO16 : bits_nb = 16; break;
            case DCAM_PIXELTYPE_MONO12 : bits_nb = 12; break;
            case DCAM_PIXELTYPE_MONO12P: bits_nb = 12; break;
            case DCAM_PIXELTYPE_RGB24  : bits_nb = 24; break;
            case DCAM_PIXELTYPE_RGB48  : bits_nb = 48; break;
            case DCAM_PIXELTYPE_BGR24  : bits_nb = 24; break;
            case DCAM_PIXELTYPE_BGR48  : bits_nb = 48; break;
            default : 
            {
			    DEB_ERROR() << "No compatible image type";
			    THROW_HW_ERROR(Error) << "No compatible image type";
            }
	    }
    }

    return bits_nb;
}

//-----------------------------------------------------------------------------
/// Set the image pixel type
//-----------------------------------------------------------------------------
void Camera::dcamex_setimagepixeltype( HDCAM hd_cam    , ///< [in] camera handle
                                       int   pixel_type) ///< [in] pixel type
{
	DEB_MEMBER_FUNCT();

    DCAMERR err  ;
	double  temp = static_cast<double>(pixel_type);
	
    err = dcamprop_setvalue( hd_cam, DCAM_IDPROP_IMAGE_PIXELTYPE, temp );

	if( failed(err) )
	{
        std::string description;
        
        switch (pixel_type)
        {
            case DCAM_PIXELTYPE_MONO8  : description = "DCAM_PIXELTYPE_MONO8"  ; break;
            case DCAM_PIXELTYPE_MONO16 : description = "DCAM_PIXELTYPE_MONO16" ; break;
            case DCAM_PIXELTYPE_MONO12 : description = "DCAM_PIXELTYPE_MONO12" ; break;
            case DCAM_PIXELTYPE_MONO12P: description = "DCAM_PIXELTYPE_MONO12P"; break;
            case DCAM_PIXELTYPE_RGB24  : description = "DCAM_PIXELTYPE_RGB24"  ; break;
            case DCAM_PIXELTYPE_RGB48  : description = "DCAM_PIXELTYPE_RGB48"  ; break;
            case DCAM_PIXELTYPE_BGR24  : description = "DCAM_PIXELTYPE_BGR24"  ; break;
            case DCAM_PIXELTYPE_BGR48  : description = "DCAM_PIXELTYPE_BGR48"  ; break;
            default : 
            {
		        DEB_ERROR() << "Unkown image type";
		        THROW_HW_ERROR(Error) << "Unkown image type";
            }
        }

        manage_error( deb, "Error in dcamex_setimagepixeltype", err, "dcamprop_setvalue()", "IDPROP=DCAM_IDPROP_IMAGE_PIXELTYPE");
	    THROW_HW_ERROR(Error) << "Could not change the image pixel type to " << description;
	}
}

//-----------------------------------------------------------------------------
/// Get the settings of a feature
/*!
@return true if the feature settings could be obtained
*/
//-----------------------------------------------------------------------------
bool Camera::dcamex_getfeatureinq( HDCAM          hd_cam      ,       ///< [in ] camera handle
                                   const string   feature_name,       ///< [in ] feature name
								   long           id_feature  ,       ///< [in ] feature id
								   FeatureInfos & feature_obj ) const ///< [out] feature informations class	
{
	DEB_MEMBER_FUNCT();

    DCAMERR       err ;
	DCAMPROP_ATTR attr;

    feature_obj.m_name = feature_name;

	memset( &attr, 0, sizeof(DCAMPROP_ATTR) );
	attr.cbSize	= sizeof(DCAMPROP_ATTR); // [in] size of this structure
	attr.iProp	= id_feature           ; //	DCAMIDPROPERTY
    attr.option = DCAMPROP_OPTION_NONE ; //	DCAMPROPOPTION

    err = dcamprop_getattr( hd_cam, &attr );

	if( failed(err) )
	{
        manage_error( deb, "Error in dcamex_getfeatureinq", err,  "dcamprop_getattr()", "IDPROP=0x%08x", id_feature);
		return false;
	}

    feature_obj.m_has_range        = (( attr.attribute & DCAMPROP_ATTR_HASRANGE     ) != 0); ///< range supported ?
    feature_obj.m_has_step         = (( attr.attribute & DCAMPROP_ATTR_HASSTEP      ) != 0); ///< step supported ?
    feature_obj.m_has_default      = (( attr.attribute & DCAMPROP_ATTR_HASDEFAULT   ) != 0); ///< default value supported ?
    feature_obj.m_is_writable      = (( attr.attribute & DCAMPROP_ATTR_WRITABLE     ) != 0); ///< is writable ?
    feature_obj.m_is_readable      = (( attr.attribute & DCAMPROP_ATTR_READABLE     ) != 0); ///< is readable ?
    feature_obj.m_has_view         = (( attr.attribute & DCAMPROP_ATTR_HASVIEW      ) != 0); ///< has view ?
    feature_obj.m_has_auto_rounding = (( attr.attribute & DCAMPROP_ATTR_AUTOROUNDING ) != 0); ///< has auto rounding ?
    feature_obj.m_max_view         = feature_obj.m_has_view ? attr.nMaxView : 0;

    // range
    if( attr.attribute & DCAMPROP_ATTR_HASRANGE )
    {
        feature_obj.m_min = attr.valuemin;
        feature_obj.m_max = attr.valuemax;
    }

    // step
    if( attr.attribute & DCAMPROP_ATTR_HASSTEP )
    {
        feature_obj.m_step = attr.valuestep;
    }

    // default
    if( attr.attribute & DCAMPROP_ATTR_HASDEFAULT )
    {
        feature_obj.m_default_value = attr.valuedefault;
    }

    // array of values ?
	if( attr.attribute2 & DCAMPROP_ATTR2_ARRAYBASE )
    {
        if(!dcamex_getpropertyvalues( hd_cam, attr, feature_obj.m_vect_values))
        {
            manage_error( deb, "Error in dcamex_getfeatureinq", DCAMERR_NONE,  "dcamex_getpropertyvalues()", "IDPROP=0x%08x", id_feature);
    		return false; // error is also managed by the dcamex_getpropertyvalues method.
        }
    }

    // list of mode values ?
	if( (attr.attribute & DCAMPROP_TYPE_MASK) == DCAMPROP_TYPE_MODE )
    {
        if(!dcamex_getmodevalues( hd_cam, attr, feature_obj.m_vect_mode_labels, feature_obj.m_vect_mode_values))
        {
            manage_error( deb, "Error in dcamex_getfeatureinq", DCAMERR_NONE,  "dcamex_getmodevalues()", "IDPROP=0x%08x", id_feature);
    		return false; // error is also managed by the dcamex_getpropertyvalues method.
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
/// Get the possible values of a property
/*!
@return true if the feature settings could be obtained
*/
//-----------------------------------------------------------------------------
bool Camera::dcamex_getpropertyvalues( HDCAM            hd_cam     ,       ///< [in]  camera handle
                                       DCAMPROP_ATTR    attr      ,       ///< [in]  attribut which contains the array base
                                       vector<double> & vect_values) const ///< [out] contains possible values of the property
{
	DEB_MEMBER_FUNCT();

    DCAMERR err          ;
    bool    result = true;
    int32   i_prop  = 0  ;
    double  v            ;
    int     element_index;
    char    text[ 64 ]   ;

	// get number of array
	i_prop = attr.iProp_NumberOfElement;
	err    = dcamprop_getvalue( hd_cam, i_prop, &v );

	if( !failed(err) )
	{
		int32 nArray = (int32)v;

        manage_trace( deb, "dcamex_getpropertyvalues", DCAMERR_NONE, "dcamprop_getvalue()", "Number of elements %d for property 0x%08x", nArray, attr.iProp);

		for( element_index = 1; element_index < nArray; element_index++ )
		{
			// get property name of array element
			i_prop = attr.iProp + element_index * attr.iPropStep_Element;
			err    = dcamprop_getname( hd_cam, i_prop, text, sizeof(text) );

			if( failed(err) )
			{
                manage_error( deb, "Error in dcamex_getpropertyvalues", err, "dcamprop_getname()", "IDPROP=0x%08x", i_prop);
        		result = false;
			}
            else
            {
            	err = dcamprop_getvalue( hd_cam, i_prop, &v );

			    if( failed(err) )
			    {
                    manage_error( deb, "Error in dcamex_getpropertyvalues", err, "dcamprop_getvalue()", "IDPROP=0x%08x", i_prop);
        		    result = false;
			    }
                else
                {
                    vect_values.push_back(v); // add the value to the container
                    manage_trace( deb, "dcamex_getpropertyvalues", DCAMERR_NONE, NULL, "value : %lf - %s", v, text);
                }
            }
		}
	}
    else
	{
        manage_error( deb, "Error in dcamex_getpropertyvalues", err, "dcamprop_getvalue()", "IDPROP=0x%08x", i_prop);
		result = false;
	}

    return result;
}

//-----------------------------------------------------------------------------
/// Get the possible values of a mode
/*!
@return true if the feature settings could be obtained
*/
//-----------------------------------------------------------------------------
bool Camera::dcamex_getmodevalues( HDCAM            hd_cam     ,       ///< [in]  camera handle
                                   DCAMPROP_ATTR    attr      ,       ///< [in]  attribut which contains the array base
                                   vector<string> & vect_label ,       ///< [out] contains possible text values of the mode
                                   vector<double> & vect_values) const ///< [out] contains possible values of the mode
{
	DEB_MEMBER_FUNCT();

	DCAMERR            err          ;
	char               pv_text[ 64 ];
	DCAMPROP_VALUETEXT pvt          ;
	int32              pv_index     = 0            ;
    double             v            = attr.valuemin; // first valid value
    int32              i_prop       = attr.iProp   ;

	do
	{
		// get value text
		memset( &pvt, 0, sizeof(DCAMPROP_VALUETEXT) );
		pvt.cbSize		= sizeof(DCAMPROP_VALUETEXT);
		pvt.iProp		= i_prop                    ;
		pvt.value		= v                         ;
		pvt.text		= pv_text                   ;
		pvt.textbytes	= sizeof(pv_text)           ;

		pv_index++;
		err = dcamprop_getvaluetext( hd_cam, &pvt );
		if( !failed(err) )
		{
            // fill the vectors
            vect_label.push_back (string(pv_text)); // add the value to the container
            vect_values.push_back(v); // add the value to the container
        }
        else
        {
            manage_error( deb, "Error in dcamex_getmodevalues", err, 
                          "dcamprop_getvaluetext()", "IDPROP=0x%08x, index:%d", i_prop, pv_index);
            return false;
		}

		// get next value
		err = dcamprop_queryvalue( hd_cam, i_prop, &v, DCAMPROP_OPTION_NEXT );
	} 
    while( !failed(err) );

    return true;
}

//-----------------------------------------------------------------------------
/// Trace the general informations of a property
/*!traceFeatureGeneralInformations
*/
//-----------------------------------------------------------------------------
void Camera::traceFeatureGeneralInformations( HDCAM          hd_cam      ,       ///< [in ] camera handle
                                              const string   feature_name,       ///< [in ] feature name
                                              long           id_feature  ,       ///< [in ] feature id
                                              FeatureInfos * opt_feature ) const ///< [out] optional feature object to receive data
{
	DEB_MEMBER_FUNCT();

    FeatureInfos feature_obj;

    if(opt_feature == NULL)
    {
        opt_feature = &feature_obj;
    }

    if( !dcamex_getfeatureinq( hd_cam, feature_name, id_feature, *opt_feature ) )
    {
        string txt = "Failed to get " + feature_name;
        manage_error( deb, txt.c_str());
        THROW_HW_ERROR(Error) << txt.c_str();
    }

    opt_feature->traceGeneralInformations();
}

//=============================================================================
// FEATURE INFORMATIONS CLASS
//=============================================================================
//-----------------------------------------------------------------------------
/// feature informations class constructor
//-----------------------------------------------------------------------------
Camera::FeatureInfos::FeatureInfos()
{
    m_min          = 0.0; 
    m_max          = 0.0; 
    m_step         = 0.0; 
    m_default_value = 0.0; 
}

//-----------------------------------------------------------------------------
/// Search a value in the property value array
/*! checkifValueExists
@return true if found
*/
//-----------------------------------------------------------------------------
bool Camera::FeatureInfos::checkifValueExists(const double value_to_check) const ///< [in] contains the value we need to check the existance
{
	DEB_MEMBER_FUNCT();

    bool   bFound   = false                  ;
    size_t vectSize = m_vect_mode_values.size();

    if(vectSize > 0)
    {
        for (size_t i = 0 ; i < vectSize ; i++)
	    {
		    if (value_to_check == m_vect_mode_values.at(i))
		    {
			    bFound = true;
			    break;
		    }
	    }
    }

    return bFound;
}

//-----------------------------------------------------------------------------
/// Trace the possible values of a mode property
/*!traceModePossibleValues
*/
//-----------------------------------------------------------------------------
void Camera::FeatureInfos::traceModePossibleValues(void) const
{
	DEB_MEMBER_FUNCT();

    size_t vectModeLabelsSize = m_vect_mode_labels.size();
    size_t vectModeValuesSize = m_vect_mode_values.size();

    DEB_TRACE() << "checking " << m_name << " property values:";

    if(vectModeLabelsSize != vectModeValuesSize)
    {
    	DEB_TRACE() << "Incoherent mode labels and mode values numbers!";
    }
    else
    if(vectModeLabelsSize == 0)
    {
    	DEB_TRACE() << "no mode values found.";
    }
    else
    {
        size_t ValueIndex;

        for(ValueIndex = 0 ; ValueIndex < vectModeLabelsSize ; ValueIndex++)
        {
            DEB_TRACE() << "value " << ValueIndex
                        << " (" << m_vect_mode_values[ValueIndex] << ") " 
                        << m_vect_mode_labels[ValueIndex];
        }
    }
}

//-----------------------------------------------------------------------------
/// Trace the general informations of a property
/*!traceGeneralInformations
*/
//-----------------------------------------------------------------------------
void Camera::FeatureInfos::traceGeneralInformations(void) const
{
	DEB_MEMBER_FUNCT();
    const string option_enabled  = "YES";
    const string option_disabled = "NO" ;

    DEB_TRACE() << "checking " << m_name  << " property informations:";

    DEB_TRACE() << "Min         : " << m_min          ;
    DEB_TRACE() << "Max         : " << m_max          ;
    DEB_TRACE() << "Step        : " << m_step         ;
    DEB_TRACE() << "Default     : " << m_default_value;
    DEB_TRACE() << "Range       : " << (m_has_range         ? option_enabled : option_disabled);
    DEB_TRACE() << "Step        : " << (m_has_step          ? option_enabled : option_disabled);
    DEB_TRACE() << "Default     : " << (m_has_default       ? option_enabled : option_disabled);
    DEB_TRACE() << "Writable    : " << (m_is_writable       ? option_enabled : option_disabled);
    DEB_TRACE() << "Readable    : " << (m_is_readable       ? option_enabled : option_disabled);
    DEB_TRACE() << "View        : " << (m_has_view          ? option_enabled : option_disabled);
    DEB_TRACE() << "AutoRounding: " << (m_has_auto_rounding ? option_enabled : option_disabled);
}

//-----------------------------------------------------------------------------
/// Round the value using the min-max and step properties
/*!RoundValue
*/
//-----------------------------------------------------------------------------
void Camera::FeatureInfos::RoundValue(int & inout_value) const ///< [in-out] contains the value we need to round to the nearest correct value
{
	DEB_MEMBER_FUNCT();

    // calculate the last value
    if(m_has_step)
    {
        inout_value /= static_cast<int>(m_step);
        inout_value *= static_cast<int>(m_step);
    }

    if(inout_value < static_cast<int>(m_min)) inout_value = static_cast<int>(m_min); else
    if(inout_value > static_cast<int>(m_max)) inout_value = static_cast<int>(m_max);
}

//-----------------------------------------------------------------------------
/// Trace the informations about the trigger
//-----------------------------------------------------------------------------
void Camera::TraceTriggerData() const
{
	DEB_MEMBER_FUNCT();

    DCAMERR err ;
	double  temp;
    int     trigger_source   = -1;
    int     trigger_active   = -1;
    int     trigger_mode     = -1;
    int     trigger_polarity = -1;

    // Trigger source
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_TRIGGERSOURCE, &temp);
    if( failed(err) )
    {
        manage_error( deb, "Cannot get trigger option", err, 
                      "dcamprop_getvalue", "IDPROP=DCAM_IDPROP_TRIGGERSOURCE");
        THROW_HW_ERROR(Error) << "Cannot get trigger option";
    }
    trigger_source = static_cast<int>(temp);

    // Trigger active
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_TRIGGERACTIVE, &temp);
    if( failed(err) )
    {
        manage_error( deb, "Cannot set trigger option", err, 
                      "dcamprop_getvalue", "IDPROP=DCAM_IDPROP_TRIGGERACTIVE");
        THROW_HW_ERROR(Error) << "Cannot get trigger option";
    }
    trigger_active = static_cast<int>(temp);

    // Trigger mode
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_TRIGGER_MODE, &temp);
    if( failed(err) )
    {
        manage_error( deb, "Cannot set trigger option", err, 
                      "dcamprop_getvalue", "IDPROP=DCAM_IDPROP_TRIGGER_MODE");
        THROW_HW_ERROR(Error) << "Cannot get trigger option";
    }
    trigger_mode = static_cast<int>(temp);

    // Trigger polarity
    err = dcamprop_getvalue( m_camera_handle, DCAM_IDPROP_TRIGGERPOLARITY, &temp);
    if( failed(err) )
    {
        manage_error( deb, "Cannot set trigger polarity", err, 
                      "dcamprop_getvalue", "IDPROP=DCAM_IDPROP_TRIGGERPOLARITY");
        THROW_HW_ERROR(Error) << "Cannot get trigger option";
    }
    trigger_polarity = static_cast<int>(temp);

    // writing the informations
    string txt_trigger_source   = "undefined";
    string txt_trigger_active   = "undefined";
    string txt_trigger_mode     = "undefined";
    string txt_trigger_polarity = "undefined";

    if(trigger_source   == DCAMPROP_TRIGGERSOURCE__INTERNAL   ) txt_trigger_source   = "DCAMPROP_TRIGGERSOURCE__INTERNAL"   ; else
    if(trigger_source   == DCAMPROP_TRIGGERSOURCE__EXTERNAL   ) txt_trigger_source   = "DCAMPROP_TRIGGERSOURCE__EXTERNAL"   ; else
    if(trigger_source   == DCAMPROP_TRIGGERSOURCE__SOFTWARE   ) txt_trigger_source   = "DCAMPROP_TRIGGERSOURCE__SOFTWARE"   ; else
    if(trigger_source   == DCAMPROP_TRIGGERSOURCE__MASTERPULSE) txt_trigger_source   = "DCAMPROP_TRIGGERSOURCE__MASTERPULSE";

    if(trigger_active   == DCAMPROP_TRIGGERACTIVE__EDGE       ) txt_trigger_active   = "DCAMPROP_TRIGGERACTIVE__EDGE"       ; else
    if(trigger_active   == DCAMPROP_TRIGGERACTIVE__LEVEL      ) txt_trigger_active   = "DCAMPROP_TRIGGERACTIVE__LEVEL"      ; else
    if(trigger_active   == DCAMPROP_TRIGGERACTIVE__SYNCREADOUT) txt_trigger_active   = "DCAMPROP_TRIGGERACTIVE__SYNCREADOUT"; else
    if(trigger_active   == DCAMPROP_TRIGGERACTIVE__POINT      ) txt_trigger_active   = "DCAMPROP_TRIGGERACTIVE__POINT"      ; 

    if(trigger_mode     == DCAMPROP_TRIGGERACTIVE__EDGE       ) txt_trigger_mode     = "DCAMPROP_TRIGGER_MODE__NORMAL"      ; else
    if(trigger_mode     == DCAMPROP_TRIGGER_MODE__PIV         ) txt_trigger_mode     = "DCAMPROP_TRIGGER_MODE__PIV"         ; else
    if(trigger_mode     == DCAMPROP_TRIGGER_MODE__START       ) txt_trigger_mode     = "DCAMPROP_TRIGGER_MODE__START"       ; else
    if(trigger_mode     == DCAMPROP_TRIGGER_MODE__MULTIGATE   ) txt_trigger_mode     = "DCAMPROP_TRIGGER_MODE__MULTIGATE"   ; else
    if(trigger_mode     == DCAMPROP_TRIGGER_MODE__MULTIFRAME  ) txt_trigger_mode     = "DCAMPROP_TRIGGER_MODE__MULTIFRAME"  ;

    if(trigger_polarity == DCAMPROP_TRIGGERPOLARITY__NEGATIVE ) txt_trigger_polarity = "DCAMPROP_TRIGGERPOLARITY__NEGATIVE" ; else
    if(trigger_polarity == DCAMPROP_TRIGGERPOLARITY__POSITIVE ) txt_trigger_polarity = "DCAMPROP_TRIGGERPOLARITY__POSITIVE" ;

    DEB_TRACE() << "TRIGGER SOURCE   : " << txt_trigger_source  ;
    DEB_TRACE() << "TRIGGER ACTIVE   : " << txt_trigger_active  ;
    DEB_TRACE() << "TRIGGER MODE     : " << txt_trigger_mode    ;
    DEB_TRACE() << "TRIGGER POLARITY : " << txt_trigger_polarity;
}

//-----------------------------------------------------------------------------
/// Set the trigger polarity
//-----------------------------------------------------------------------------
void Camera::setTriggerPolarity(enum Trigger_Polarity in_trigger_polarity) const ///< [in] type of trigger polarity
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(in_trigger_polarity);

    DCAMERR  err    ;
    int      mode   ;
    string   txt_mode;

    if(in_trigger_polarity == Trigger_Polarity_Negative)
    {
        mode     = DCAMPROP_TRIGGERPOLARITY__NEGATIVE;
        txt_mode = "Negative";
    }
    else
    if(in_trigger_polarity == Trigger_Polarity_Positive)
    {
        mode     = DCAMPROP_TRIGGERPOLARITY__POSITIVE;
        txt_mode = "Positive";
    }
    else
	{
        manage_error( deb, "Unable to set the trigger polarity", DCAMERR_NONE, 
                           "", "in_trigger_polarity is unknown %d", static_cast<int>(in_trigger_polarity));
        THROW_HW_ERROR(Error) << "Unable to set the trigger polarity";
	}

    // set the mode
	err = dcamprop_setvalue( m_camera_handle, DCAM_IDPROP_TRIGGERPOLARITY, static_cast<double>(mode) );

    if( failed(err) )
	{
        if((err == DCAMERR_INVALIDPROPERTYID)||(err == DCAMERR_NOTSUPPORT))
        {
            manage_trace( deb, "Unable to set the SyncReadout blank mode", err, 
                               "dcamprop_setvalue", "DCAM_IDPROP_SYNCREADOUT_SYSTEMBLANK %d", mode);
        }
        else
        {
            manage_error( deb, "Unable to set the trigger polarity", err, 
                               "dcamprop_setvalue", "DCAM_IDPROP_TRIGGERPOLARITY %d", mode);
            THROW_HW_ERROR(Error) << "Unable to set the trigger polarity";
        }
	}
    else
    {
        manage_trace( deb, "Set the trigger polarity", DCAMERR_NONE, NULL, "Polarity : %s", txt_mode.c_str());
    }
}
