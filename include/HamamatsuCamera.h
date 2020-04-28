//###########################################################################
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
#include <dcamapi4.h>
#include <dcamprop.h>

#include <stdlib.h>
#include <limits>
#include <stdarg.h>

#include "lima/HwMaxImageSizeCallback.h"
#include "lima/HwBufferMgr.h"
#include "lima/ThreadUtils.h"
#include "lima/Timestamp.h"
#include "lima/HwEventCtrlObj.h"

#include <ostream>

using namespace std;

#define REPORT_EVENT(desc)  {   \
                                Event *my_event = new Event(Hardware,Event::Info, Event::Camera, Event::Default,desc); \
                                m_cam->getEventCtrlObj()->reportEvent(my_event); \
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

    //-----------------------------------------------------------------------------
	public:
	    enum Status 
        {
		    Ready, Exposure, Readout, Latency, Fault
	    };

        typedef std::map<TrigMode, bool> trigOptionsMap;

        enum SyncReadOut_BlankMode
        {
            SyncReadOut_BlankMode_Standard, // The blank of syncreadout trigger is standard.
            SyncReadOut_BlankMode_Minimum , // The blank of syncreadout trigger is minimum.
        };

        enum Trigger_Polarity
        {
            Trigger_Polarity_Negative, // Falling edge or LOW level.
            Trigger_Polarity_Positive, // Rising edge or HIGH level.
        };

        // relative to DCAM_IDPROP_SENSORCOOLER
        enum Cooler_Mode
        {
            Cooler_Mode_Not_Supported, 
            Cooler_Mode_Off          , // DCAMPROP_SENSORCOOLER__OFF
            Cooler_Mode_On           , // DCAMPROP_SENSORCOOLER__ON
            Cooler_Mode_Max          , // DCAMPROP_SENSORCOOLER__MAX
        };

        // relative to DCAM_IDPROP_SENSORTEMPERATURE_STATUS
        enum Temperature_Status
        {
            Temperature_Status_Not_Supported, 
            Temperature_Status_Normal       , // DCAMPROP_SENSORTEMPERATURE_STATUS__NORMAL
            Temperature_Status_Warning      , // DCAMPROP_SENSORTEMPERATURE_STATUS__WARNING
            Temperature_Status_Protection   , // DCAMPROP_SENSORTEMPERATURE_STATUS__PROTECTION
        };

        // relative to DCAM_IDPROP_SENSORCOOLERSTATUS
        enum Cooler_Status
        {
            Cooler_Status_Not_Supported, 
            Cooler_Status_Error4       , // DCAMPROP_SENSORCOOLERSTATUS__ERROR4
            Cooler_Status_Error3       , // DCAMPROP_SENSORCOOLERSTATUS__ERROR3
            Cooler_Status_Error2       , // DCAMPROP_SENSORCOOLERSTATUS__ERROR2
            Cooler_Status_Error1       , // DCAMPROP_SENSORCOOLERSTATUS__ERROR1
            Cooler_Status_None         , // DCAMPROP_SENSORCOOLERSTATUS__NONE
            Cooler_Status_Off          , // DCAMPROP_SENSORCOOLERSTATUS__OFF
            Cooler_Status_Ready        , // DCAMPROP_SENSORCOOLERSTATUS__READY
            Cooler_Status_Busy         , // DCAMPROP_SENSORCOOLERSTATUS__BUSY
            Cooler_Status_Always       , // DCAMPROP_SENSORCOOLERSTATUS__ALWAYS
            Cooler_Status_Warning      , // DCAMPROP_SENSORCOOLERSTATUS__WARNING
        };

	//-----------------------------------------------------------------------------
	public:
	    Camera(const std::string& config_path,int camera_number=0, int frame_buffer_size=10);
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
        HwEventCtrlObj * getEventCtrlObj ();
    
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
		void getReadoutSpeed(short int& readout_speed);		///< [out] current readout speed
		void setReadoutSpeed(const short int readout_speed); ///< [in]  new readout speed
		void getLostFrames(unsigned long int& lost_frames);	///< [out] current lost frames
		void getFPS(double& fps);							///< [out] last computed fps
   
        void setSyncReadoutBlankMode(enum SyncReadOut_BlankMode in_sync_read_out_mode); ///< [in] type of sync-readout trigger's blank

        int  getNumberofViews   (void);
        int  getMaxNumberofViews(void);

        void setViewExpTime(int    view_index,     ///< [in] view index [0...m_max_views[
                            double exp_time  );    ///< [in] exposure time to set

        void getViewExpTime(int      view_index,   ///< [in] view index [0...m_max_views[
                            double & exp_time  );  ///< [out] current exposure time

        void getMinViewExpTime(double& exp_time); ///< [out] current exposure time

        void setViewExpTime1(double exp_time   ); ///< [in] exposure time to set
        void setViewExpTime2(double exp_time   ); ///< [in] exposure time to set
        void getViewExpTime1(double & exp_time ); ///< [out] current exposure time
        void getViewExpTime2(double & exp_time ); ///< [out] current exposure time

        void setViewMode(bool   flag);
        void getViewMode(bool & flag);
        void setViewMode(bool in_view_mode_activated,  ///< [in] view mode activation or not
                         int  in_views_number       ); ///< [in] number of views if view mode activated

        void traceAllRoi(void);
        void checkingROIproperties(void);
        
        double getSensorTemperature(void);

        std::string getCoolerModeLabel        (void);
        std::string getTemperatureStatusLabel (void);
        std::string getCoolerStatusLabel      (void);
        bool        getHighDynamicRangeEnabled(void);
        void        setHighDynamicRangeEnabled(const bool & in_enabled);

        bool isSensorTemperatureSupported(void);
        bool isTemperatureStatusSupported(void);
        bool isCoolerModeSupported       (void);
        bool isCoolerStatusSupported     (void);
        bool isHighDynamicRangeSupported (void);

	private:
        enum Camera::Cooler_Mode getCoolerMode(void);
        std::string getCoolerModeLabelFromMode(enum Camera::Cooler_Mode in_cooler_mode);

        enum Camera::Temperature_Status getTemperatureStatus(void);
        std::string getTemperatureStatusLabelFromStatus(enum Camera::Temperature_Status in_temperature_status);

        enum Camera::Cooler_Status getCoolerStatus(void);
        std::string getCoolerStatusLabelFromStatus(enum Camera::Cooler_Status in_cooler_status);

	//-----------------------------------------------------------------------------
	private:
		//-----------------------------------------------------------------------------
        // CameraThread class
		//-----------------------------------------------------------------------------
        class CameraThread: public CmdThread
		{
			DEB_CLASS_NAMESPC(DebModCamera, "CameraThread", "Hamamatsu");
		public:
			// Status
            enum
			{ 
				Ready    = MaxThreadStatus, 
                Exposure                  , 
                Readout                   , 
                Latency                   ,
                Fault                     ,
			};

			// Cmd
            enum
			{ 
				StartAcq = MaxThreadCmd, 
			};

			CameraThread(Camera * cam);

            // destructor
            virtual ~CameraThread();

			virtual void start();
            virtual void abort();

            void abortCapture(void);
			volatile bool m_force_stop;

		protected:
			virtual void init   ();
			virtual void execCmd(int cmd);
            
		private:
			void execStartAcq();

            bool copyFrames(const int index_frame_begin,	///< [in] index of the frame where to begin copy
							const int nb_frames_count  ,	///< [in] number of frames to copy
							StdBufferCbMgr& buffer_mgr );	///< [in] buffer manager object

            void checkStatusBeforeCapturing() const;

            void createWaitHandle (HDCAMWAIT & wait_handle) const;
            void releaseWaitHandle(HDCAMWAIT & wait_handle) const;

            void getTransfertInfo(int32 & frame_index,
                                  int32 & frame_count);

			Camera*   m_cam        ;
            HDCAMWAIT m_wait_handle;

		};
		friend class CameraThread;

		//-----------------------------------------------------------------------------
        // Feature class used to get data informations of a property 
		//-----------------------------------------------------------------------------
		class FeatureInfos
		{
			DEB_CLASS_NAMESPC(DebModCamera, "FeatureInfos", "Hamamatsu");

    		friend class Camera; // Camera class can access directly to private attributs like if they were public declared

        public:
            FeatureInfos();

            bool checkifValueExists(const double value_to_check) const; ///< [in] contains the value we need to check the existance

            void traceModePossibleValues (void) const;
            void traceGeneralInformations(void) const;

            void RoundValue(int & inout_value)  const;

		private:
            string         m_name             ; ///< name of the feature	
            double         m_min              ; ///< min value of the feature	
            double         m_max              ; ///< max value of the feature
            double         m_step             ; ///< minimum stepping between a value and the next
            double         m_default_value    ; ///< default value of the feature
            vector<double> m_vect_values      ; ///< contains possible values of the property
            vector<string> m_vect_mode_labels ; ///< contains possible text values of the mode
            vector<double> m_vect_mode_values ; ///< contains possible values of the mode
            bool           m_has_range        ; ///< range supported ?
            bool           m_has_step         ; ///< step supported ?
            bool           m_has_default      ; ///< default value supported ?
            bool           m_is_writable      ; ///< is writable ?
            bool           m_is_readable      ; ///< is readable ?
            bool           m_has_view         ; ///< has view ?
            bool           m_has_auto_rounding; ///< has auto rounding ?
            int32 		   m_max_view         ; ///< max view if supported
        };

		//-----------------------------------------------------------------------------
		// DCAM-SDK Helper
		//-----------------------------------------------------------------------------
		// TRACE METHODS
        static std::string string_format_arg(const char* format, va_list args);
        static std::string string_format    (const char* format, ...);

        string dcam_get_string( HDCAM hd_cam ,        ///< [in] camera handle 
                                int32 id_str ) const; ///< [in] string identifier

        void manage_trace( DebObj     & deb                     ,             ///< [in] trace object
                           const char * opt_desc  = NULL        ,             ///< [in] optional description (NULL if not used)
                           int32        id_str    = DCAMERR_NONE,             ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                           const char * fct       = NULL        ,             ///< [in] function name which returned the error (NULL if not used)
                           const char * opt       = NULL        , ...) const; ///< [in] optional string to concat to the error string (NULL if not used)

        static void static_manage_trace( const Camera     * const cam              ,       ///< [in] camera object
                                         DebObj           & deb                    ,       ///< [in] trace object
                                         const char       * opt_desc = NULL        ,       ///< [in] optional description (NULL if not used)
                                         int32              id_str   = DCAMERR_NONE,       ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                                         const char       * fct      = NULL        ,       ///< [in] function name which returned the error (NULL if not used)
                                         const char       * opt      = NULL        , ...); ///< [in] optional string to concat to the error string (NULL if not used)

        void manage_error( DebObj     & deb                     ,             ///< [in] trace object
                           const char * opt_desc  = NULL        ,             ///< [in] optional description (NULL if not used)
                           int32        id_str    = DCAMERR_NONE,             ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                           const char * fct       = NULL        ,             ///< [in] function name which returned the error (NULL if not used)
                           const char * opt       = NULL        , ...) const; ///< [in] optional string to concat to the error string (NULL if not used)

        static std::string static_manage_error( const Camera     * const cam              ,       ///< [in] camera object
                                                DebObj           & deb                    ,       ///< [in] trace object
                                                const char       * opt_desc = NULL        ,       ///< [in] optional description (NULL if not used)
                                                int32              id_str   = DCAMERR_NONE,       ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                                                const char       * fct      = NULL        ,       ///< [in] function name which returned the error (NULL if not used)
                                                const char       * opt      = NULL        , ...); ///< [in] optional string to concat to the error string (NULL if not used)

        static std::string static_trace_string_va_list( const Camera     * const cam,  ///< [in] camera object
                                                        DebObj           & deb      ,  ///< [in] trace object
                                                        const char       * opt_desc ,  ///< [in] optional description (NULL if not used)
                                                        int32              id_str   ,  ///< [in] error string identifier (DCAMERR_NONE if no hd_cam error to trace)
                                                        const char       * fct      ,  ///< [in] function name which returned the error (NULL if not used)
                                                        const char       * opt      ,  ///< [in] optional string to concat to the error string (NULL if not used)
                                                        va_list            args     ,  ///< [in] optional args (printf style) to merge with the opt string (NULL if not used)
                                                        bool               is_error ); ///< [in] true if traced like an error, false for a classic info trace

		//-----------------------------------------------------------------------------
        void   execStopAcq();

        void   showCameraInfo      (HDCAM hd_cam); ///< [in] camera device id
        void   showCameraInfoDetail(HDCAM hd_cam); ///< [in] camera device id

        HDCAM  dcam_init_open(long camera_number); ///< [in] id of the camera to open

        bool   dcamex_setsubarrayrect( HDCAM hd_cam    ,  ///< [in] camera handle
                                       long  left     ,  ///< [in] left  (x)
                                       long  top      ,  ///< [in] top   (y)
                                       long  width    ,  ///< [in] horizontal size
                                       long  height   ,  ///< [in] vertical size
                                       int   view_index); ///< [in] View index [0...max view[. Use g_get_sub_array_do_not_use_view for general subarray 

		bool   dcamex_getsubarrayrect( HDCAM   hd_cam    ,  ///< [in] camera handle
									   int32 & left     ,  ///< [in] left  (x)
									   int32 & top      ,  ///< [in] top   (y)
									   int32 & width    ,  ///< [in] horizontal size
									   int32 & height   ,  ///< [in] vertical size
                                       int     view_index); ///< [in] View index [0...max view[. Use -1 for general subarray 

		long   dcamex_getimagewidth (const HDCAM hd_cam ); ///< [in] camera handle
		
		long   dcamex_getimageheight(const HDCAM hd_cam ); ///< [in] camera handle

		long   dcamex_getbitsperchannel( HDCAM hd_cam );    ///< [in] camera handle

        void   dcamex_setimagepixeltype( HDCAM hd_cam   ,  ///< [in] camera handle
                                        int   pixel_type); ///< [in] pixel type

        bool   dcamex_getfeatureinq( HDCAM          hd_cam      ,        ///< [in ] camera handle
                                     const string   feature_name,        ///< [in ] feature name
                                     long           id_feature  ,        ///< [in ] feature id
                                     FeatureInfos & feature_obj ) const; ///< [out] feature informations class	

        bool dcamex_getpropertyvalues( HDCAM            hd_cam     ,        ///< [in ] camera handle
                                       DCAMPROP_ATTR    attr       ,        ///< [in ] attribut which contains the array base
                                       vector<double> & vect_values) const; ///< [out] contains possible values of the property

        bool dcamex_getmodevalues( HDCAM            hd_cam     ,        ///< [in ] camera handle
                                   DCAMPROP_ATTR    attr       ,        ///< [in ] attribut which contains the array base
                                   vector<string> & vect_label ,        ///< [out] contains possible text values of the mode
                                   vector<double> & vect_values) const; ///< [out] contains possible values of the mode

        bool getTriggerMode(const TrigMode trig_mode) const; ///< [in]  lima trigger mode value

        void traceFeatureGeneralInformations( HDCAM          hd_cam      ,        ///< [in ] camera handle
                                              const string   feature_name,        ///< [in ] feature name
                                              long           id_feature  ,        ///< [in ] feature id
                                              FeatureInfos * opt_feature ) const; ///< [out] optional feature object to receive data

        void TraceTriggerData() const;

        void setTriggerPolarity(enum Trigger_Polarity in_trigger_polarity) const; ///< [in] type of trigger polarity

		// DCAM-SDK Helper end
		bool  isBinningSupported(const int   bin_value); /// Check if a binning value is supported
        int32 GetBinningMode    (const int   bin_value); ///< [in] binning value to chck for
        int   GetBinningFromMode(const int32 bin_mode );	///< [in] binning mode to chck for

		vector<int> m_vectBinnings; /// list of available binning modes
		
		//-----------------------------------------------------------------------------
	    //- lima stuff
	    SoftBufferCtrlObj	        m_buffer_ctrl_obj;
        HwEventCtrlObj              m_event_ctrl_obj ;
	    int                         m_nb_frames      ;    
	    Camera::Status              m_status         ;
	    int                         m_image_number   ;
	    int                         m_timeout        ;
	    double                      m_latency_time   ;
	    Roi                         m_roi            ; /// current roi parameters
	    Bin                         m_bin            ; /// current binning paramenters
	    Bin                         m_bin_max        ; /// maximum bining parameters
	    TrigMode                    m_trig_mode      ;
		map<int, string>			m_map_triggerMode;

		// Specific
		unsigned int long			m_lost_frames_count;
		double						m_fps              ;
      
	    //- camera stuff 
	    string                      m_detector_model;
	    string                      m_detector_type ;
		long						m_depth         ;
        long                        m_bytes_per_pixel;
		long						m_max_image_width, m_max_image_height;
    
	    //- Hamamatsu SDK stuff
	    string                      m_config_path        ;
	    int                         m_camera_number      ;
	    HDCAM						m_camera_handle      ;
	    DWORD				        m_camera_capabilities;
	    string                      m_camera_error_str   ;
	    int                         m_camera_error       ;
        int                         m_frame_buffer_size  ; // number of images in the DCAM internal buffer 
    	   
	    bool                        m_fasttrigger        ; // ?
	    int                         m_temperature_sp     ;   
	    int                         m_read_mode          ;
	    int                         m_acq_mode           ;  
	    double                      m_exp_time           ;
	    double                      m_exp_time_max       ;

		CameraThread 				m_thread             ;
		Mutex						m_mutex_force_stop   ;

	    trigOptionsMap              m_map_trig_modes     ;

        FeatureInfos                m_feature_pos_x      ; // property data to check the ROI
        FeatureInfos                m_feature_pos_y      ; // property data to check the ROI
        FeatureInfos                m_feature_size_x     ; // property data to check the ROI
        FeatureInfos                m_feature_size_y     ; // property data to check the ROI

        //- W-View management
        bool                        m_view_mode_enabled  ; // W-View mode with splitting image
        int                         m_view_number        ; // number of W-Views
        int                         m_max_views          ; // maximum number of views for this camera (if > 1 then W-View mode is supported) 
	    double                    * m_view_exp_time      ; // array of exposure value by view

        bool                        m_hdr_enabled        ; // high dynamic range activation latest value

		//-----------------------------------------------------------------------------
        // Constants
		//-----------------------------------------------------------------------------
        static const double g_orca_pixel_size              ;
        static const int    g_dcam_str_msg_size            ;
        static const int    g_get_sub_array_do_not_use_view;

        static const string g_trace_line_separator       ;
        static const string g_trace_little_line_separator;
	};
    } // namespace Hamamatsu
} // namespace lima


#endif // HamamatsuCAMERA_H
