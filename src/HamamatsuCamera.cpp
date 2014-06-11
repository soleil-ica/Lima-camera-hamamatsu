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
#include <Timestamp.h>

using namespace lima;
using namespace lima::Hamamatsu;
using namespace std;

//-----------------------------------------------------------------------------
/// utility thread
//-----------------------------------------------------------------------------

class Camera::_AcqThread : public Thread
{
    DEB_CLASS_NAMESPC(DebModCamera, "Camera", "_AcqThread");
public:
    _AcqThread(Camera &aCam);
    virtual ~_AcqThread();
    
protected:
    virtual void threadFunction();
    
private:
	bool copyFrames(const int iFrameBegin,			///< [in] index of the frame where to begin copy
					const int iFrameEnd,			///< [in] index of the frame where to end copy
					 StdBufferCbMgr& buffer_mgr );	///< [in] buffer manager object
	
	bool			m_bAbortAcquisition;
    Camera&			m_cam;
};


//-----------------------------------------------------------------------------
///  Ctor
//-----------------------------------------------------------------------------
Camera::Camera(const std::string& config_path, int camera_number)
    : m_status(Ready),
      m_wait_flag(true),
      m_quit(false),
      m_thread_running(true),
      m_image_number(0),
	  m_depth(16),
      m_latency_time(0.),
      m_bin(1,1),
      m_camera_handle(0),
      m_fasttrigger(0),
      m_exp_time(1.),
	  m_read_mode(2),
	  m_lostFramesCount(0),
	  m_fps(0.0),
	  m_fpsUpdatePeriod(100)
{
    DEB_CONSTRUCTOR();

    m_config_path = config_path;
    m_camera_number = camera_number;
  
	m_map_triggerMode[IntTrig]		  = "IntTrig";
	m_map_triggerMode[ExtGate]		  = "ExtGate";
	m_map_triggerMode[ExtStartStop]   = "ExtStartStop";
	m_map_triggerMode[ExtTrigReadout] = "ExtTrigReadout";

    // --- Get available cameras and select the choosen one.	
	m_camera_handle = dcam_init_open(camera_number);
	if (NULL != m_camera_handle)
	{
		// --- Initialise deeper parameters of the controller                
		initialiseController();            
	    	           
		// --- BIN already set to 1,1 above. @@ Pas Vu?
		// --- Hamamatsu sets the ROI by starting coordinates at 1 and not 0 !!!!
		Size sizeMax;
		getDetectorImageSize(sizeMax);
		Roi aRoi = Roi(0,0, sizeMax.getWidth(), sizeMax.getHeight());
	    
		// --- setRoi applies both bin and roi
		DEB_TRACE() << "Set the ROI to full frame: "<< aRoi;
		setRoi(aRoi);	    
	    
		// --- Get the maximum exposure time allowed and set default
		setExpTime(m_exp_time);
	    
		// --- Set detector for software single image mode    
		setTrigMode(IntTrig);
	    
		m_nb_frames = 1;
	    
		// --- finally start the acq thread
		m_acq_thread = new _AcqThread(*this);
		m_acq_thread->start();
	}
	else
	{
		HANDLE_DCAMERROR(NULL, "Unable to initialize the camera (Check that the camera is switched ON & check that is no other software is currently using it).");
	}
}


//-----------------------------------------------------------------------------
///  Dtor
//-----------------------------------------------------------------------------
Camera::~Camera()
{
    DEB_DESTRUCTOR();

	// Stop capture if still running
	if(m_status != Camera::Ready)
	{
		_stopAcq(false);
	}

    // Stop and destroy Acq thread
	m_quit = true;
	m_cond.broadcast();
    delete m_acq_thread;
    m_acq_thread = NULL;

	DEB_TRACE() << "Thread terminated";
                
    // Close camera
	if (NULL != m_camera_handle)
	{
		DEB_TRACE() << "dcam_close()";
		if (dcam_close( m_camera_handle ))
		{
			m_camera_handle = NULL;

			DEB_TRACE() << "dcam_uninit()";
			dcam_uninit( NULL );
		}
		else
		{
			HANDLE_DCAMERROR(m_camera_handle, "dcam_close() failed !");
		}		
	}
   
    DEB_TRACE() << "Shutdown camera";	    
}


//-----------------------------------------------------------------------------
/// Set detector for single image acquisition
//-----------------------------------------------------------------------------
void Camera::prepareAcq()
{
	DEB_MEMBER_FUNCT();

	if (!dcam_precapture( m_camera_handle, DCAM_CAPTUREMODE_SEQUENCE))
	{
		HANDLE_DCAMERROR(m_camera_handle, "Can not prepare the camera for image capturing. (dcam_precapture())");
	}
}


//-----------------------------------------------------------------------------
///  start the acquistion
//-----------------------------------------------------------------------------
void Camera::startAcq()
{
    DEB_MEMBER_FUNCT();
    m_image_number = 0;
	m_fps = 0;

    m_bAbortCapture=false;

    // --- check first the acquisition is idle
	_DWORD	status;

	// check status (and stop capturing if capturing is already started. TODO: check )
	if(!dcam_getstatus( m_camera_handle, &status ))
	{
		HANDLE_DCAMERROR(m_camera_handle, "Cannot get camera status.");
	}
	
	if (status != DCAM_STATUS_STABLE) // STABLE is the state obtained after precapture()
    {
        _setStatus(Camera::Fault, false);
		DEB_ERROR() << "Cannot start acquisition, camera is not idle";
        THROW_HW_ERROR(Error) << "Cannot start acquisition, camera is not idle";
    }   

    // Wait running state of acquisition thread
    AutoMutex aLock(m_cond.mutex());
    m_wait_flag = false;
    m_cond.broadcast();
    while(!m_thread_running)
        m_cond.wait();
        
    StdBufferCbMgr& buffer_mgr = m_buffer_ctrl_obj.getBuffer();
    buffer_mgr.setStartTimestamp(Timestamp::now());
}


//-----------------------------------------------------------------------------
/// stop the acquisition
//-----------------------------------------------------------------------------
void Camera::stopAcq()
{
    _stopAcq(false);
}


//-----------------------------------------------------------------------------
/// private method
//-----------------------------------------------------------------------------
void Camera::_stopAcq(bool internalFlag)
{
    DEB_MEMBER_FUNCT();

	if(m_status != Camera::Ready)
    {
		if (!dcam_idle(m_camera_handle))
		{
			HANDLE_DCAMERROR(m_camera_handle, "Cannot abort acquisition.");
		}
		
        //Let the acq thread stop the acquisition
        if (!internalFlag)
		{
			m_abortLock.lock();
			m_bAbortCapture = true;
			m_abortLock.unlock();
			return;
		}
            
        // Stop acquisition
        DEB_TRACE() << "Stop acquisition";

		_setStatus(Camera::Ready, false);
	}
}


//-----------------------------------------------------------------------------
/// thread function for acquisition for sequence mode
/// Version 3: sequence capture with cycling buffer
//-----------------------------------------------------------------------------
void Camera::_AcqThread::threadFunction()
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cam.m_cond.mutex());
    StdBufferCbMgr& buffer_mgr = m_cam.m_buffer_ctrl_obj.getBuffer();

    while(!m_cam.m_quit)
    {
        while(m_cam.m_wait_flag && !m_cam.m_quit)
        {
            DEB_TRACE() << "Wait";
            m_cam.m_thread_running = false;
            m_cam.m_cond.broadcast();
            m_cam.m_cond.wait();
        }

        DEB_TRACE() << "Run";
        m_cam.m_thread_running = true;
        if (m_cam.m_quit) return;
    
        m_cam.m_status = Camera::Exposure;
        m_cam.m_cond.broadcast();
        aLock.unlock();

        bool continueAcq = true;
	
		FrameDim frame_dim = buffer_mgr.getFrameDim();
		Size frame_size    = frame_dim.getSize();

		Timestamp T0, T1, DeltaT;
		T0 = Timestamp::now();

		// Set camera to Readout state
        m_cam._setStatus(Camera::Readout,false);

		// Release the capture frame (in case thread funtion has been exited by "if (m_cam.m_quit) return;"
		if (!dcam_freeframe( m_cam.m_camera_handle))
		{
			HANDLE_DCAMERROR(m_cam.m_camera_handle, "begin acq: Unable to free capture frame (dcam_freeframe())");
		}

		// Allocate frames to capture
		int bufferSize = 10; // capture buffer size
		if (!dcam_allocframe( m_cam.m_camera_handle, bufferSize)) 
		{
			HANDLE_DCAMERROR(m_cam.m_camera_handle, "Cannot allocate frame for capturing (dcam_allocframe()).");
		}

		// Check the number of allocated frames
		int32 Count = 0;
		if ( !dcam_getframecount(m_cam.m_camera_handle, &Count) )
		{
			HANDLE_DCAMERROR(m_cam.m_camera_handle, "Could not get the number of allocated frames. (dcam_getframecount())");
		}
		else
		{
			DEB_TRACE() << "Allocated frames: " << Count;
			if (Count != bufferSize) 
			{
				DEB_ERROR() << "Only " << Count << " frames could be allocated";
				THROW_HW_ERROR(Error) << "Only " << Count << " frames could be allocated";
			}
		}

		_DWORD	status;
		// Check the status and stop capturing if capturing is already started.
		if (!dcam_getstatus( m_cam.m_camera_handle, &status ))
		{
			HANDLE_DCAMERROR(m_cam.m_camera_handle, "Cannot get status");
		}

		if (DCAM_STATUS_READY != status) // After allocframe, the camera status should be READY
		{
			DEB_ERROR() << "Camera could not be set in the proper state for image capture";
			THROW_HW_ERROR(Error) << "Camera could not be set in the proper state for image capture";
		}

		// Start the real capture (this function returns immediately)
		if (!dcam_capture(m_cam.m_camera_handle))
		{
			HANDLE_DCAMERROR(m_cam.m_camera_handle, "Frame capture failed");
		}

		// Transfer the images as they are beeing captured from dcam_sdk buffer to Lima
		m_cam.m_lostFramesCount = 0;
		int32 lastFrameCount = 0, frameCount = 0, lastFrameIndex = -1, frameIndex = 0;
		
		// Main acquisition loop
        while (		( continueAcq )
				&&  ( (0==m_cam.m_nb_frames) || (m_cam.m_image_number < m_cam.m_nb_frames) )
			  )
        {
			// Check first if acq. has been stopped
			if (m_cam.m_wait_flag)
			{
				continueAcq = false;
				continue;
			}

			// Wait for the next image to become available or the end of the capture by user
			_DWORD  waitEventMask = DCAM_EVENT_FRAMEEND | DCAM_EVENT_CAPTUREEND;
			if (!dcam_wait( m_cam.m_camera_handle, &waitEventMask, DCAM_WAIT_INFINITE, NULL ) )
			{
				string strErr;
				int err = getLastErrorMsg(m_cam.m_camera_handle, strErr);

				// If capture was aborted (by stopAcq() -> dcam_idle())
				if (DCAMERR_ABORT == err)
				{
					DEB_TRACE() << "DCAMERR_ABORT";
					continueAcq = false;
				}
				else 
				{					
					HANDLE_DCAMERROR(m_cam.m_camera_handle, "dcam_wait() failed.")
					m_cam._stopAcq(true);
				}
			}
			else
			{
				if (DCAM_EVENT_CAPTUREEND == waitEventMask)
				{
					DEB_TRACE() << "DCAM_EVENT_CAPTUREEND";
					continueAcq = false;
					continue;
				}

				// check DCAM error code
				long err = dcam_getlasterror( m_cam.m_camera_handle );
				switch (err)
				{
					case DCAMERR_ABORT:
					{
						DEB_TRACE() << "DCAMERR_ABORT";					
						continueAcq = false;
						continue;
					}

					case DCAMERR_TIMEOUT:
					{
						DEB_TRACE() << "DCAMERR_TIMEOUT";
						continue;
					}

					case DCAMERR_LOSTFRAME:
					case DCAMERR_MISSINGFRAME_TROUBLE:
					{
						DEB_TRACE() << "DCAMERR_LOSTFRAME || DCAMERR_MISSINGFRAME_TROUBLE";
						++m_cam.m_lostFramesCount;
						continue;
					}

					// no specific management for other cases
				}
			}


			// Transfert the new images
			if (continueAcq && (!m_cam.m_bAbortCapture))
			{
			
				if (dcam_gettransferinfo( m_cam.m_camera_handle, &frameIndex, &frameCount ))
				{
					int32 deltaFrames = frameCount-lastFrameCount;
					DEB_TRACE() << "m_image_number > " << m_cam.m_image_number;
					DEB_TRACE() << "lastFrameIndex > " << lastFrameIndex;
					DEB_TRACE() << "frameIndex     > " << frameIndex;
					DEB_TRACE() << "frameCount     > " << frameCount << " (delta: " << deltaFrames << ")";

					if (0 == frameCount)
					{
						m_cam._stopAcq(true);
						DEB_ERROR() << "No image captured.";
						THROW_HW_ERROR(Error) << "No image captured.";
					}
					if (deltaFrames > bufferSize)
					{
						m_cam.m_lostFramesCount += deltaFrames;
					}
					lastFrameCount = frameCount;
				}
				else
				{
					m_cam._stopAcq(true);
					HANDLE_DCAMERROR(m_cam.m_camera_handle, "Cannot get transfer info. (dcam_gettransferinfo())");
				}

				// See which frames indexes must be copied from the buffer
				int part1Begin = 0, part1End = 0;
				if (frameIndex < lastFrameIndex)	// 2 possible partitions (frameIndex has looped)
				{
					part1Begin = (lastFrameIndex+1 == bufferSize)?0:lastFrameIndex+1;  // handle the case where lastFrameIndex was the end of the buffer
					if (0 == part1Begin)	// only one partition in this case
					{
						part1End = frameIndex;
						continueAcq = copyFrames(part1Begin, part1End, buffer_mgr);
					}
					else					// 2 partitions
					{
						int part2Begin = 0, part2End;
						part1End = bufferSize - 1;
						part2End = frameIndex;

						continueAcq = copyFrames(part1Begin, part1End, buffer_mgr);
						if ( (0==m_cam.m_nb_frames) || (m_cam.m_image_number < m_cam.m_nb_frames) )
						{
							continueAcq = copyFrames(part2Begin, part2End, buffer_mgr);
						}
					}
				}
				else								// only 1 partition (contiguous images)
				{
					if (lastFrameIndex != frameIndex)	// lastFrameIndex == frameIndex can occur when delta is greater than caputure buffer (in this nothing is done)
					{
						continueAcq = copyFrames(lastFrameIndex + 1, frameIndex, buffer_mgr);
					}				
				}

				lastFrameIndex = frameIndex;
			}

			// Update fps
			{
				T1 = Timestamp::now();

				DeltaT = T1 - T0;
				m_cam.m_fps = 1.0 * m_cam.m_image_number / DeltaT;
			}
        }

		DEB_TRACE() << "exiting threadfunction mainloop";

        m_cam._stopAcq(true);

		// Release the capture frame
		if (!dcam_freeframe( m_cam.m_camera_handle))
		{
			HANDLE_DCAMERROR(m_cam.m_camera_handle, "Unable to free capture frame (dcam_freeframe())");
		}


		DEB_TRACE() << "Total time (s): " << (Timestamp::now() - T0);
		DEB_TRACE() << "FPS           : " << int(m_cam.m_image_number / (Timestamp::now() - T0) );
		DEB_TRACE() << "Lost frames   : " << m_cam.m_lostFramesCount;

        aLock.lock();
        m_cam.m_wait_flag = true;
    }
}


//-----------------------------------------------------------------------------
// Copy the given frames to the buffer manager
//-----------------------------------------------------------------------------
bool Camera::_AcqThread::copyFrames(const int iFrameBegin,			///< [in] index of the frame where to begin copy
									const int iFrameEnd,			///< [in] index of the frame where to end copy
								    StdBufferCbMgr& buffer_mgr )	///< [in] buffer manager object
{
	DEB_MEMBER_FUNCT();

	// Check for capture abort
	m_cam.m_abortLock.lock();
	if (m_cam.m_bAbortCapture) 
	{
		m_cam.m_abortLock.unlock();
		return true;
	}
	m_cam.m_abortLock.unlock();

	DEB_TRACE() << "copyFrames(" << iFrameBegin << ", " << iFrameEnd << ")";

	FrameDim frame_dim = buffer_mgr.getFrameDim();
	Size frame_size    = frame_dim.getSize();
	int height		   = frame_size.getHeight();

	DEB_TRACE() << "frame_size.getHeight(): " << frame_size.getHeight();

	bool _CopySucess = false;
	for  (int iFrame = iFrameBegin; iFrame <= iFrameEnd; iFrame++)
	{	
		m_cam.m_abortLock.lock();
		if (m_cam.m_bAbortCapture) 
		{
			m_cam.m_abortLock.unlock();
			return true;
		}
		m_cam.m_abortLock.unlock();

		void *dst = buffer_mgr.getFrameBufferPtr(m_cam.m_image_number);
        	
		void* src;
		long int sRowbytes;
		bool bImageCopied;

		if ( dcam_lockdata( m_cam.m_camera_handle, &src, &sRowbytes, iFrame) )
		{
			DEB_TRACE() << "sRowbytes: "  << sRowbytes;
			DEB_TRACE() << "Copy m_image_number > " << m_cam.m_image_number << " (frameIndex: " << iFrame << ")";

			memcpy( dst, src, sRowbytes * height );
			DEB_TRACE() << "memcpy ok";

			if (dcam_unlockdata(m_cam.m_camera_handle))
			{
				bImageCopied = true;
				DEB_TRACE() << "image #" << m_cam.m_image_number <<" acquired !";
			}
			else
			{
				HANDLE_DCAMERROR(m_cam.m_camera_handle, "Unable to unlock frame data (dcam_unlockdata())");
				bImageCopied = false;
			}
		}
		else
		{
			HANDLE_DCAMERROR(m_cam.m_camera_handle, "Unable to lock frame data (dcam_lockdata())");
			bImageCopied = false;
		}

		if (!bImageCopied)
		{
			m_cam._setStatus(Camera::Fault,false);
			_CopySucess = false;
			string strErr = "Cannot get image.";
			HANDLE_DCAMERROR(m_cam.m_camera_handle, strErr);
			break;
		}
		else
		{
			HwFrameInfoType frame_info;
			frame_info.acq_frame_nb = m_cam.m_image_number;

			m_cam.m_abortLock.lock();
			if (m_cam.m_bAbortCapture) 
			{
				m_cam.m_abortLock.unlock();
				return true;
			}
			m_cam.m_abortLock.unlock();

			_CopySucess = buffer_mgr.newFrameReady(frame_info);

			DEB_TRACE() << DEB_VAR1(_CopySucess);

			if ( (0==m_cam.m_nb_frames) || (m_cam.m_image_number < m_cam.m_nb_frames) )
			{
				++m_cam.m_image_number;
			}
			else
			{
				DEB_TRACE() << "All images captured.";
				break;
			}
		}
	}

	return _CopySucess;
}


//-----------------------------------------------------------------------------
/// the acquisition thread Ctor
//-----------------------------------------------------------------------------
Camera::_AcqThread::_AcqThread(Camera &aCam) :
    m_cam(aCam)
{
    pthread_attr_setscope(&m_thread_attr,PTHREAD_SCOPE_PROCESS);
}


//-----------------------------------------------------------------------------
/// the acquisition thread Dtor
//-----------------------------------------------------------------------------
Camera::_AcqThread::~_AcqThread()
{
    AutoMutex aLock(m_cam.m_cond.mutex());
    m_cam.m_quit = true;
    m_cam.m_cond.broadcast();
    aLock.unlock();
    
    join();
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
		xmax = dcamex_getimagewidth(m_camera_handle);
		ymax = dcamex_getimageheight(m_camera_handle);
	}

	if ((0==xmax) || (0==ymax))
    {
		HANDLE_DCAMERROR(m_camera_handle, "Cannot get detector size.");
    }     
    size= Size(xmax, ymax);
}


//-----------------------------------------------------------------------------
/// return the image type  TODO: this is permanently called by the device -> find a way to avoid DCAM acess
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
			case 8:	  type = Bpp8;  break;
			case 16:  type = Bpp16; break;
			case 32:  type = Bpp32; break;
			default:
			{
				DEB_ERROR() << "No compatible image type";
				THROW_HW_ERROR(Error) << "No compatible image type";
			}
		}
	}
	else
	{
		HANDLE_DCAMERROR(m_camera_handle, "Unable to get image type.");
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
			m_depth			= 16;
			break;
		}
		default:
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

	bool valid_mode = false;

	if (DCAM_TRIGMODE_UNKNOWN != getTriggerMode(trig_mode))
	{
		valid_mode = true;
	}

	return valid_mode;
}


//-----------------------------------------------------------------------------
/// Set the new trigger mode
//-----------------------------------------------------------------------------
void Camera::setTrigMode(TrigMode mode) ///< [in] trigger mode to set
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(mode);

	// Get the dcam_sdk mode associated to the given LiMA TrigMode
	int dcam_trigMode = getTriggerMode(mode);
	if (DCAM_TRIGMODE_UNKNOWN!=dcam_trigMode)
	{
		if (!dcam_settriggermode(m_camera_handle, dcam_trigMode))
		{
			HANDLE_DCAMERROR(m_camera_handle, "Cannot set trigger mode (dcam_settriggermode()).");
		}

		m_trig_mode = mode;    
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
    
    if (!dcam_setexposuretime(m_camera_handle, exp_time))
    {
        HANDLE_DCAMERROR(m_camera_handle, "Cannot set exposure time. (dcam_setexposuretime())");
    }
                 
    m_exp_time = exp_time;
}


//-----------------------------------------------------------------------------
/// Get the current exposure time
//-----------------------------------------------------------------------------
void Camera::getExpTime(double& exp_time) ///< [out] current exposure time
{
    DEB_MEMBER_FUNCT();

	double exposure;
	if (!dcam_getexposuretime(m_camera_handle, &exposure))
    {
        HANDLE_DCAMERROR(m_camera_handle, "Cannot get acquisition timings (dcam_getexposuretime()).");
    }   
	else
	{
		exp_time = exposure;
	}
    
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
		DEB_ERROR() << "Latency is not supported.";
		THROW_HW_ERROR(Error) << "Latency is not supported.";
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
    
	long	capflags;
	double	step, defaultvalue;
	if( !dcamex_getfeatureinq( m_camera_handle, DCAM_IDFEATURE_EXPOSURETIME, capflags, min_expo, max_expo, step, defaultvalue ) )
	{
        HANDLE_DCAMERROR(m_camera_handle, "Failed to get exposure time");
	}

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
void Camera::getStatus(Camera::Status& status) ///< [out] current camera status
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    status = m_status;
    DEB_RETURN() << DEB_VAR1(DEB_HEX(status));
}


//-----------------------------------------------------------------------------
/// Set the new camera status
//-----------------------------------------------------------------------------
void Camera::_setStatus(Camera::Status status,	///< [in] new camera status
						bool force)				///< [in] true to force the status
{
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    if(force || m_status != Camera::Fault)
        m_status = status;
    m_cond.broadcast();
}


//-----------------------------------------------------------------------------
/// do nothing, hw_roi = set_roi.
//-----------------------------------------------------------------------------
void Camera::checkRoi(const Roi& set_roi,	///< [in]  Roi values to set
						    Roi& hw_roi)	///< [out] Updated Roi values
{
    DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(set_roi);

	hw_roi = set_roi;

    DEB_RETURN() << DEB_VAR1(hw_roi);
}


//-----------------------------------------------------------------------------
/// Set the new roi
//-----------------------------------------------------------------------------
void Camera::setRoi(const Roi& set_roi) ///< [in] New Roi values
{
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(set_roi);

    Point topleft, size;
    //int binx, biny;
    Roi hw_roi, roiMax;
    Size sizeMax;

	Point set_roi_topleft = set_roi.getTopLeft(); 
	Point set_roi_size    = set_roi.getSize();
	cout << "setRoi(): " << set_roi_topleft.x << ", " 
					     << set_roi_topleft.y << ", " 
					     << set_roi_size.x    << ", " 
					     << set_roi_size.y << endl;

    if (m_roi == set_roi) return;

	if ((0 == set_roi_size.x) && (0 == set_roi_size.y))
	{
		cout << "Ignore 0x0 roi" << endl;
		return;
	}

	/*
    getDetectorImageSize(sizeMax);
    roiMax = Roi(0,0, sizeMax.getWidth(), sizeMax.getHeight());
               
    if (set_roi.isActive() && (set_roi != roiMax) )
    {
		// --- a real roi available
		DEB_TRACE() << "--- a real roi available";
		hw_roi = set_roi;
		binx = m_bin.getX(); biny = m_bin.getY();    	
    }
    else
    {
		DEB_TRACE() << "---  either No roi or roi fit with max size!!!	";
		// ---  either No roi or roi fit with max size!!!	
		// --- in that case binning for full size calculation is 1
		hw_roi = roiMax;
		binx=1; biny=1;
    }    
	*/

	hw_roi = set_roi;

	Point hw_roi_topleft = hw_roi.getTopLeft(); 
	Point hw_roi_size    = hw_roi.getSize();
	if ((0==hw_roi_size.x) || (0==hw_roi_size.y)) 
	{
		DEB_ERROR() << "ROI values not supported";
		THROW_HW_ERROR(Error) << "Cannot set detector ROI";
	}

	cout << "calling dcamex_setsubarrayrect(): " << hw_roi_topleft.x << ", " 
											   << hw_roi_topleft.y << ", " 
											   << hw_roi_size.x    << ", " 
											   << hw_roi_size.y << endl;
	if (!dcamex_setsubarrayrect(m_camera_handle, hw_roi_topleft.x, hw_roi_topleft.y, hw_roi_size.x, hw_roi_size.y))
    {
        HANDLE_DCAMERROR(m_camera_handle, "Cannot set detector ROI.");
    }

    m_roi = hw_roi;
}


//-----------------------------------------------------------------------------
/// Get the current roi values
//-----------------------------------------------------------------------------
void Camera::getRoi(Roi& hw_roi) ///< [out] Roi values
{
    DEB_MEMBER_FUNCT();

	int32 left, top, width, height;
	if (!dcamex_getsubarrayrect( m_camera_handle, left, top, width,	height ) )
    {
		HANDLE_DCAMERROR(m_camera_handle, "Cannot get detector ROI");
    }    
	DEB_TRACE() << "getRoi(): " << left << ", " << top << ", " <<  width << ", " <<  height;
    hw_roi = Roi(left, top, width, height);
    
    DEB_RETURN() << DEB_VAR1(hw_roi);
}


//-----------------------------------------------------------------------------
/// Check is the given binning values are supported, rise an exception otherwise
//-----------------------------------------------------------------------------
void Camera::checkBin(Bin& hw_bin) ///< [out] binning values to update
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
void Camera::setBin(const Bin& set_bin) ///< [in] binning values objects
{
    DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(set_bin);
    
	// Binning values have been checked by checkBin()
	if (dcam_setbinning( m_camera_handle, set_bin.getX() ) )
	{
		DEB_TRACE() << "dcam_setbinning() ok: " << set_bin.getX() << "x" << set_bin.getX();
		m_bin = set_bin; // update current binning values		
	}
	else
	{
		HANDLE_DCAMERROR(m_camera_handle, "Cannot set detector BIN");
	}
    
    DEB_RETURN() << DEB_VAR1(set_bin);
}


//-----------------------------------------------------------------------------
/// Get the current binning mode
//-----------------------------------------------------------------------------
void Camera::getBin(Bin &hw_bin) ///< [out] binning values object
{
    DEB_MEMBER_FUNCT();
    
	int32 nBinning;
	if (dcam_getbinning( m_camera_handle, &nBinning) )
	{
		DEB_TRACE() << "dcam_getbinning(): " << nBinning;
		hw_bin = Bin(nBinning, nBinning);
	}
	else
	{
		HANDLE_DCAMERROR(m_camera_handle, "Cannot get detector BIN");
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
    
    sizex = C_ORCA_PIXELSIZE;
    sizey = C_ORCA_PIXELSIZE;
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

	// Create the list of available binning modes from camera capabilities
	if (dcam_getcapability( m_camera_handle, &m_camera_capabilities, 0 ))
	{
															    m_vectBinnings.push_back(1); // Binning 1x1 is always available
		if ( m_camera_capabilities & DCAM_CAPABILITY_BINNING2 ) m_vectBinnings.push_back(2);
		if ( m_camera_capabilities & DCAM_CAPABILITY_BINNING4 ) m_vectBinnings.push_back(4);
		if ( m_camera_capabilities & DCAM_CAPABILITY_BINNING6 ) m_vectBinnings.push_back(6);
		if ( m_camera_capabilities & DCAM_CAPABILITY_BINNING8 ) m_vectBinnings.push_back(8);
		if ( m_camera_capabilities & DCAM_CAPABILITY_BINNING12) m_vectBinnings.push_back(12);
		if ( m_camera_capabilities & DCAM_CAPABILITY_BINNING16) m_vectBinnings.push_back(16);
		if ( m_camera_capabilities & DCAM_CAPABILITY_BINNING32) m_vectBinnings.push_back(32);

		// Create the binning object with the maximum possible value
		int max = *std::max_element(m_vectBinnings.begin(), m_vectBinnings.end());
		m_bin_max = Bin(max, max);

		// Fills the map of available trigger modes
		/*
		IntTrig,		x
		IntTrigMult,	?
		ExtTrigSingle,	?
		ExtTrigMult,	?
		ExtGate,		x
		ExtStartStop,   x
		ExtTrigReadout  x
		*/
		m_map_trig_modes[IntTrig] = DCAM_TRIGMODE_INTERNAL;
		if( m_camera_capabilities & DCAM_CAPABILITY_TRIGGER_EDGE )			m_map_trig_modes[ExtTrigSingle] = DCAM_TRIGMODE_EDGE;

		// (TODO: A proposer/valider: ExtGate)
		// if( m_camera_capabilities & DCAM_CAPABILITY_TRIGGER_LEVEL )			m_map_trig_modes[ExtGate]		= DCAM_TRIGMODE_LEVEL ); 

		//if( m_camera_capabilities & DCAM_CAPABILITY_TRIGGER_SOFTWARE )			m_map_trig_modes[ExtStartStop]  = DCAM_TRIGMODE_SOFTWARE;
		// if( m_camera_capabilities & DCAM_CAPABILITY_TRIGGER_FASTREPETITION ) m_map_trig_modes[] = DCAM_TRIGMODE_FASTREPETITION;
		// if( m_camera_capabilities & DCAM_CAPABILITY_TRIGGER_TDI )			m_map_trig_modes[] = DCAM_TRIGMODE_TDI;
		// if( m_camera_capabilities & DCAM_CAPABILITY_TRIGGER_TDIINTERNAL )	m_map_trig_modes[] = DCAM_TRIGMODE_TDIINTERNAL;
		// if( m_camera_capabilities & DCAM_CAPABILITY_TRIGGER_START )			m_map_trig_modes[] = DCAM_TRIGMODE_START );
		//if( m_camera_capabilities & DCAM_CAPABILITY_TRIGGER_SYNCREADOUT )		m_map_trig_modes[ExtTrigReadout] = DCAM_TRIGMODE_SYNCREADOUT;
	}
	else
	{
        HANDLE_DCAMERROR(m_camera_handle, "Failed to get capabilities.");
	}

	// Retreive exposure time
	long	capflags;
	double	step, defaultvalue, min_expo, max_expo;
	if( dcamex_getfeatureinq( m_camera_handle, DCAM_IDFEATURE_EXPOSURETIME, capflags, min_expo, max_expo, step, defaultvalue ) )
	{
		m_exp_time_max = max_expo;
	}
	else
	{
        HANDLE_DCAMERROR(m_camera_handle, "Failed to get exposure time.");
	}

	// Display obtained values

	// Binning modes
	vector<int>::const_iterator iterBinningMode = m_vectBinnings.begin();
	while (m_vectBinnings.end()!=iterBinningMode)
	{
		DEB_TRACE() << *iterBinningMode;
		++iterBinningMode;
	}

	// Trigger modes
	map<TrigMode, int>::const_iterator iter = m_map_trig_modes.begin();
	DEB_TRACE() << "Trigger modes:";
	while (m_map_trig_modes.end()!=iter)
	{
		DEB_TRACE() << ">" << m_map_triggerMode[iter->first];
		++iter;
	}

	// Exposure time
	DEB_TRACE() << "Min exposure time: " << min_expo;
	DEB_TRACE() << "Max exposure time: " << max_expo;
}


//-----------------------------------------------------------------------------
/// Get the last dcamsdk error message
/*!
@return dcamsdk error code associated to the message
*/
//-----------------------------------------------------------------------------
int32 Camera::getLastErrorMsg(HDCAM hdcam,	  ///< [in] camera handle
							  string& strErr) ///< [out] sdk error message
{
	long	err = -1;
	if (NULL!=hdcam)
	{
		char	msg[DCAM_STRMSG_SIZE];
		memset( msg, 0, sizeof( msg ) );

		err = dcam_getlasterror( hdcam, msg, sizeof( msg ) );
		strErr = msg;
	}

	return err;
}


//-----------------------------------------------------------------------------
/// Get the dcamdsk trigger mode value associated to the given Lima TrigMode 
/*!
@return dcamdsk trigger mode or DCAM_TRIGMODE_UNKNOWN if no associated value found
*/
//-----------------------------------------------------------------------------
int Camera::getTriggerMode(const TrigMode trig_mode) ///< [in] lima trigger mode value
{
	map<TrigMode, int>::const_iterator iterFind = m_map_trig_modes.find(trig_mode);
	if (m_map_trig_modes.end()!=iterFind)
	{
		return iterFind->second;
	}
	else
	{
		return DCAM_TRIGMODE_UNKNOWN;
	}
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

	if (!dcam_setpropertyvalue(m_camera_handle, DCAM_IDPROP_READOUTSPEED, double(readoutSpeed)))
	{
		HANDLE_DCAMERROR(m_camera_handle, "Failed to set readout speed.");
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