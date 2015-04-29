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
#ifndef HAMAMATSUCAMERA_H
#define HAMAMATSUCAMERA_H

#include "HamamatsuCompatibility.h"
#if defined (__GNUC__) && (__GNUC__ == 3) && defined (__ELF__)
#   define GENAPI_DECL __attribute__((visibility("default")))
#   define GENAPI_DECL_ABSTRACT __attribute__((visibility("default")))
#endif


#include <sys/timeb.h>
#include <time.h>
#include <windows.h>
#include <dcamapi.h>
#include <dcamapi3.h>
#include <dcamprop.h>

#include <stdlib.h>
#include <limits>
#include "lima/HwMaxImageSizeCallback.h"
#include "lima/HwBufferMgr.h"
#include "lima/ThreadUtils.h"
#include "lima/Timestamp.h"

#include <ostream>

using namespace std;

#define C_ORCA_PIXELSIZE	   6.5e-6	
#define DCAM_STRMSG_SIZE	   256
#define DCAM_FRAMEBUFFER_SIZE  10
#define DCAM_TRIGMODE_UNKNOWN -1
#define HANDLE_DCAMERROR(__camHandle__, __userMsg__) { \
					     				string __errMsg__;\
										int32 __errCode__ = getLastErrorMsg(__camHandle__, __errMsg__);\
										DEB_ERROR() << __userMsg__ << " : error #" << __errCode__ << "(" << __errMsg__ << ")";\
										THROW_HW_ERROR(Error) << __userMsg__;\
									   }



namespace lima
{
    namespace Hamamatsu
    {

/*******************************************************************
 * \class Camera
 * \brief object controlling the Hamamatsu camera via DCAM-SDK
 *******************************************************************/
	class LIBHAMAMATSU_API Camera
	{
	    DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Hamamatsu");
	    friend class Interface;
	public:

	    enum Status {
		Ready, Exposure, Readout, Latency, Fault
	    };

	    Camera(const std::string& config_path,int camera_number=0);
	    ~Camera();

	    void startAcq();
	    void stopAcq();
		void prepareAcq();
    
	    // -- detector info object
	    void getImageType(ImageType& type);
	    void setImageType(ImageType type);

	    void getDetectorType(std::string& type);
	    void getDetectorModel(std::string& model);
	    void getDetectorImageSize(Size& size);
		void getDetectorMaxImageSize(Size& size);
    
	    // -- Buffer control object
	    HwBufferCtrlObj* getBufferCtrlObj();
    
	    //-- Synch control object
	    bool checkTrigMode(TrigMode trig_mode);
	    void setTrigMode(TrigMode  mode);
	    void getTrigMode(TrigMode& mode);
    
	    void setExpTime(double  exp_time);
	    void getExpTime(double& exp_time);

	    void setLatTime(double  lat_time);
	    void getLatTime(double& lat_time);

	    void getExposureTimeRange(double& min_expo, double& max_expo) const;
	    void getLatTimeRange(double& min_lat, double& max_lat) const;    

	    void setNbFrames(int  nb_frames);
	    void getNbFrames(int& nb_frames);
	    void getNbHwAcquiredFrames(int &nb_acq_frames);

	    void checkRoi(const Roi& set_roi, Roi& hw_roi);
	    void setRoi(const Roi& set_roi);
	    void getRoi(Roi& hw_roi);    

	    void checkBin(Bin&);
	    void setBin(const Bin&);
	    void getBin(Bin&);
	    bool isBinningAvailable();       

	    void getPixelSize(double& sizex, double& sizey);
    
	    Camera::Status getStatus();
    
	    void reset();

	    // -- Hamamatsu specific
	    long HamamatsuError(string& strErr);
	    void initialiseController();
	    void setFastExtTrigger(bool flag);
	    void getFastExtTrigger(bool& flag);
		void getReadoutSpeed(short int& readoutSpeed);		///< [out] current readout speed
		void setReadoutSpeed(const short int readoutSpeed); ///< [in]  new readout speed
		void getLostFrames(unsigned long int& lostFrames);	///< [out] current lost frames
		void getFPS(double& fps);							///< [out] last computed fps
   
	private:
		class CameraThread: public CmdThread
		{
			DEB_CLASS_NAMESPC(DebModCamera, "CameraThread", "Hamamatsu");
		public:
			enum
			{ // Status
				Ready = MaxThreadStatus, Exposure, Readout, Latency,
			};

			enum
			{ // Cmd
				StartAcq = MaxThreadCmd, StopAcq,
			};

			CameraThread(Camera& cam);

			virtual void start();

			volatile bool m_force_stop;

		protected:
			virtual void init();
			virtual void execCmd(int cmd);
		private:
			void execStartAcq();
			bool copyFrames(const int iFrameBegin,			///< [in] index of the frame where to begin copy
							const int iFrameCount,			///< [in] number of frames to copy
							StdBufferCbMgr& buffer_mgr );	///< [in] buffer manager object

			Camera* m_cam;

		};
		friend class CameraThread;

		//-----------------------------------------------------------------------------
		// DCAM-SDK Helper
		HDCAM dcam_init_open(long camera_number);
		bool dcamex_setsubarrayrect( HDCAM hdcam,   ///< [in] camera handle
									 long left,	    ///< [in] left  (x)
									 long top,      ///< [in] top (y)
									 long width,    ///< [in] horizontal size
									 long height ); ///< [in] vertical size
		
		bool dcamex_getsubarrayrect( HDCAM hdcam,    ///< [in] camera handle
									 int32& left,	 ///< [in] left  (x)
									 int32& top,	 ///< [in] top   (y)
									 int32& width,	 ///< [in] horizontal size
									 int32& height ); ///< [in] vertical size
		
		long dcamex_getimagewidth(const HDCAM hdcam ); ///< [in] camera handle
		
		long dcamex_getimageheight(const HDCAM hdcam ); ///< [in] camera handle
		
		bool dcamex_getfeatureinq( HDCAM hdcam,			///< [in]  camera handle
								   long idfeature,		///< [in]  feature id
								   long& capflags,		///< [out] ?
								   double& min,			///< [out] min value of the feature	
								   double& max,			///< [out] max value of the feature
								   double& step,		///< [out] ?
								   double& defaultvalue ) const;	///< [out] default value of the feature
		
		long dcamex_getbitsperchannel( HDCAM hdcam );    ///< [in] camera handle

		int getTriggerMode(const TrigMode trig_mode); ///< [in] lima trigger mode value
		
		void showCameraInfo(const int iDevice);
		static int32 getLastErrorMsg(HDCAM hdcam, string& errMsg); /// Get the last dcamsdk error message

		// DCAM-SDK Helper end

		bool isBinningSupported(const int binValue);	/// Check if a binning value is supported
		vector<int>					 m_vectBinnings; /// list of available binning modes
		
		//-----------------------------------------------------------------------------
	    //- lima stuff
	    SoftBufferCtrlObj	        m_buffer_ctrl_obj;
	    int                         m_nb_frames;    
	    Camera::Status              m_status;
	    int                         m_image_number;
	    int                         m_timeout;
	    double                      m_latency_time;
	    Roi                         m_roi; /// current roi parameters
	    Bin                         m_bin; /// current binning paramenters
	    Bin                         m_bin_max; /// maximum bining parameters
	    TrigMode                    m_trig_mode;
		map<int, string>			m_map_triggerMode;

		// Specific
		unsigned int long			m_lostFramesCount;
		double						m_fps;
		int							m_fpsUpdatePeriod;
      
	    //- camera stuff 
	    string                      m_detector_model;
	    string                      m_detector_type;
		long						m_depth, m_bytesPerPixel;
		long						m_maxImageWidth, m_maxImageHeight;
    
	    //- Hamamatsu SDK stuff
	    string                      m_config_path;
	    int                         m_camera_number;
	    HDCAM						m_camera_handle;
	    DWORD				        m_camera_capabilities;
	    string                      m_camera_error_str;
	    int                         m_camera_error;
    	   
	    bool                        m_fasttrigger;
	    int                         m_temperature_sp;   
	    int                         m_read_mode;
	    int                         m_acq_mode;    
	    map<TrigMode, int>          m_map_trig_modes;
	    double                      m_exp_time;
	    double                      m_exp_time_max;

		CameraThread 				m_thread;
		Mutex						m_mutexForceStop;
	};
    } // namespace Hamamatsu
} // namespace lima


#endif // HamamatsuCAMERA_H
