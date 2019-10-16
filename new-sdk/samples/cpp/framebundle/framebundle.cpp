/**
 @file framebundle.cpp
 @brief	sample code to use framebundle.
 @details	This program accesses the captured each image and metadata with framebundle.
 @remarks	dcamprop_setvalue
 @remarks	dcambuf_lockframe
 @remarks	dcambuf_copyframe
 @remarks	dcambuf_copymetadata
 */


#include "../misc/console4.h"
#include "../misc/common.h"

/**
 @brief set subarray
 @param hdcam:		DCAM handle
 @param hpos:		horizontal offset
 @param hsize:		horizontal size
 @param vpos:		vertical offset
 @param vsize:		vertical size
 @return	result of setting to subarray paramter
 */
BOOL set_subarray( HDCAM hdcam, int32 hpos, int32 hsize, int32 vpos, int32 vsize )
{
	DCAMERR err;

	// set subarray mode off. This setting is not mandatory, but you have to control the setting order of offset and size when mode is on. 
	err = dcamprop_setvalue( hdcam, DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__OFF );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_setvalue()", "IDPROP:SUBARRAYMODE, VALUE:OFF" );
		return FALSE;
	}

	err = dcamprop_setvalue( hdcam, DCAM_IDPROP_SUBARRAYHPOS, hpos );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_setvalue()", "IDPROP:SUBARRAYHPOS, VALUE:%d", hpos );
		return FALSE;
	}

	err = dcamprop_setvalue( hdcam, DCAM_IDPROP_SUBARRAYHSIZE, hsize );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_setvalue()", "IDPROP:SUBARRAYHSIZE, VALUE:%d", hsize );
		return FALSE;
	}

	err = dcamprop_setvalue( hdcam, DCAM_IDPROP_SUBARRAYVPOS, vpos );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_setvalue()", "IDPROP:SUBARRAYVPOS, VALUE:%d", vpos );
		return FALSE;
	}

	err = dcamprop_setvalue( hdcam, DCAM_IDPROP_SUBARRAYVSIZE, vsize );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_setvalue()", "IDPROP:SUBARRAYVSIZE, VALUE:%d", vsize );
		return FALSE;
	}

	// set subarray mode on. The combination of offset and size is checked on this timing.
	err = dcamprop_setvalue( hdcam, DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__ON );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_setvalue()", "IDPROP:SUBARRAYMODE, VALUE:ON" );
		return FALSE;
	}

	return TRUE;
}

/**
 @brief set subarray
 @param hdcam:		DCAM handle
 @param iProp:		target property ID
 @param vMax:		stored maximum value of target property
 @return	result of setting to get the maximum value of target property
 */
BOOL get_propertyvaluemax( HDCAM hdcam, int32 iProp, double& vMax )
{
	DCAMERR err;

	DCAMPROP_ATTR	propattr;
	memset( &propattr, 0, sizeof(propattr) );
	propattr.cbSize = sizeof(propattr);
	propattr.iProp = iProp;

	err = dcamprop_getattr( hdcam, &propattr );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_getattr()", "IDPROP:0x%08x", iProp );
		return FALSE;
	}

	if( ! (propattr.attribute & DCAMPROP_ATTR_HASRANGE ) )
	{
		printf( "This property(0x%08x) doesn't have the value range\n", iProp );
		return FALSE;
	}

	vMax = propattr.valuemax;

	return TRUE;
}

/**
 @brief set framebundle
 @param hdcam:		DCAM handle
 @param nBundle:	target property ID
 @return	result of setting to framebundle parameter
 */
BOOL set_framebundle( HDCAM hdcam, int32 nBundle )
{
	DCAMERR err;

	err = dcamprop_setvalue( hdcam, DCAM_IDPROP_FRAMEBUNDLE_NUMBER, nBundle );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_setvalue()", "IDPROP:FRAMEBUNDLE_NUMBER, VALUE:%d", nBundle );
		return FALSE;
	}

	err = dcamprop_setvalue( hdcam, DCAM_IDPROP_FRAMEBUNDLE_MODE, DCAMPROP_MODE__ON );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_setvalue()", "IDPROP:FRAMEBUNDLE_MODE, VALUE:ON" );
		return FALSE;
	}

	return TRUE;
}

/**
 @brief get information of framebundle
 @param hdcam:				DCAM handle
 @param number_of_bundle:	stored the number of bundled frame
 @param width:				stored width of single frame in the bundled image
 @param height:				stored height of single frame in the bundled image
 @param rowbytes:			stored rowbytes of single frame in the bundled image
 @param	totalframebytes:	stored the total data size of bundled images
 @param framestepbytes:		stored the byte size up to next frame
 @return	result of getting information of framebundle
 */
BOOL get_framebundle_information( HDCAM hdcam, int32& number_of_bundle, int32& width, int32& height, int32& rowbytes, int32& totalframebytes, int32& framestepbytes )
{
	DCAMERR err;
	double v;

	err = dcamprop_getvalue( hdcam, DCAM_IDPROP_FRAMEBUNDLE_MODE, &v );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_getvalue()", "IDPROP:FRAMEBUNDLE_MODE" );
		return FALSE;
	}

	if( v == DCAMPROP_MODE__OFF )
	{
		printf( "framebundle mode is off\n" );
		return FALSE;
	}

	err = dcamprop_getvalue( hdcam, DCAM_IDPROP_FRAMEBUNDLE_NUMBER, &v );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_getvalue()", "IDPROP:FRAMEBUNDLE_NUMBER" );
		return FALSE;
	}

	number_of_bundle = (int32)v;

	err = dcamprop_getvalue( hdcam, DCAM_IDPROP_IMAGE_WIDTH, &v );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_getvalue()", "IDPROP:IMAGE_WIDTH" );
		return FALSE;
	}

	width = (int32)v;

	err = dcamprop_getvalue( hdcam, DCAM_IDPROP_IMAGE_HEIGHT, &v );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_getvalue()", "IDPROP:IMAGE_HEIGHT" );
		return FALSE;
	}

	height = (int32)v;

	err = dcamprop_getvalue( hdcam, DCAM_IDPROP_FRAMEBUNDLE_ROWBYTES, &v );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_getvalue()", "IDPROP:FRAMEBUNDLE_ROWBYTES" );
		return FALSE;
	}

	rowbytes = (int32)v;

	err = dcamprop_getvalue( hdcam, DCAM_IDPROP_IMAGE_FRAMEBYTES, &v );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_getvalue()", "IDPROP:IMAGE_FRAMEBYTES" );
		return FALSE;
	}

	totalframebytes = (int32)v;

	err = dcamprop_getvalue( hdcam, DCAM_IDPROP_FRAMEBUNDLE_FRAMESTEPBYTES, &v );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_getvalue()", "IDPROP:FRAMEBUNDLE_FRAMESTEPBYTES" );
		return FALSE;
	}

	framestepbytes = (int32)v;

	return TRUE;
}

/**
 @brief access meta data of bundled frame
 @param hdcam:				DCAM handle
 @param number_of_buffer:	number of alloced buffer
 @param number_of_bundle:	number of bundled frame
 */
void access_bundledframe_metadata( HDCAM hdcam, int32 number_of_buffer, int32 number_of_bundle )
{
	DCAMERR err;

	// transferinfo param
	DCAMCAP_TRANSFERINFO captransferinfo;
	memset( &captransferinfo, 0, sizeof(captransferinfo) );
	captransferinfo.size	= sizeof(captransferinfo);

	// get number of captured image
	err = dcamcap_transferinfo( hdcam, &captransferinfo );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamcap_transferinfo()" );
		return;
	}

	int32	iEnd = captransferinfo.nFrameCount * number_of_bundle;
	int32	iStart = ( iEnd <= number_of_buffer ? 0 : iEnd - number_of_buffer * number_of_bundle );
	int32	framecount = iEnd - iStart;

	int32	size;
	DCAM_TIMESTAMPBLOCK*	tsb = NULL;
	size = sizeof(*tsb) + sizeof(*tsb->timestamps) * framecount;
	tsb = (DCAM_TIMESTAMPBLOCK*)new char[ size ];
	if( tsb == NULL )
	{
		printf( "failed to allocate timestamp array.\n" );
		return;
	}

	memset( tsb, 0, size );

	tsb->hdr.size = sizeof(*tsb);				// whole structure size
	tsb->hdr.iKind = DCAMBUF_METADATAKIND_TIMESTAMPS;
	tsb->hdr.in_count = framecount;

	tsb->timestamps = (DCAM_TIMESTAMP*)(tsb + 1);
	tsb->timestampsize = sizeof(*tsb->timestamps);

	tsb->hdr.iFrame = iStart;

	err = dcambuf_copymetadata( hdcam, (DCAM_METADATAHDR*)tsb );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcambuf_copymetadata()", "TIMESTAMPBLOCK::iFrame:%d\n", tsb->hdr.iFrame );
	}
	else
	{
		// TODO: add your process to time stamp information
		/* e.g.
		int32 outcount = tsb->hdr.outcount;
		DCAM_TIMESTAMP* ts = tsb->timestamps;

		int32 i;
		for( i=0; i<outcount; i++ )
		{
			
			printf( "%d.%06d\n", ts[i].sec, ts[i].microsec );
		}
		*/
	}

	delete tsb;
}

void sample_access_framebundle_eachimage( HDCAM hdcam, int32 number_of_buffer )
{
	DCAMERR err;
	
	// frame bundle information
	int32 number_of_bundle, width, height, rowbytes, totalframebytes, framestepbytes;
	if( !get_framebundle_information( hdcam, number_of_bundle, width, height, rowbytes, totalframebytes, framestepbytes ) )
		return;

	// transferinfo param
	DCAMCAP_TRANSFERINFO captransferinfo;
	memset( &captransferinfo, 0, sizeof(captransferinfo) );
	captransferinfo.size	= sizeof(captransferinfo);

	// get number of captured image
	err = dcamcap_transferinfo( hdcam, &captransferinfo );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamcap_transferinfo()" );
		return;
	}

	if( captransferinfo.nFrameCount < 1 )
	{
		printf( "not capture image\n" );
		return;
	}

	char* buf = new char[ rowbytes * height ];

	DCAMBUF_FRAME	bufframe;
	memset( &bufframe, 0, sizeof(bufframe) );
	bufframe.size = sizeof(bufframe);
	
	int iFrame;
	for( iFrame = 0; iFrame < captransferinfo.nFrameCount; iFrame++ )
	{
		bufframe.iFrame = iFrame;

		// lock bundled frames
		err = dcambuf_lockframe( hdcam, &bufframe );
		if( failed(err) )
		{
			dcamcon_show_dcamerr( hdcam, err, "dcambuf_lockframe()", "iFrame:%d", iFrame );
			break;
		}

		char* pTop = (char*)bufframe.buf;

		// access each image
		int32 iBundle;
		for( iBundle = 0; iBundle < number_of_bundle; iBundle++ )
		{
			// copy single frame
			memcpy_s( buf, rowbytes * height, pTop, bufframe.rowbytes * bufframe.height );

			// TODO: add your process to single frame
			/* e.g.
				char str[256];
				sprintf_s( str, sizeof(str), "img%d_%d.raw", iFrame, iBundle );
				output_data( str, buf, rowbytes * height );
			*/

			// shift to the top of next frame
			pTop += framestepbytes;
		}

		// access each meta data
		access_bundledframe_metadata( hdcam, number_of_buffer, number_of_bundle );
	}

	delete buf;
}

int main( int argc, char* const argv[] )
{
	printf( "PROGRAM START\n" );

	int	ret = 0;

	DCAMERR err;

	// initialize DCAM-API and open device
	HDCAM hdcam;
	hdcam = dcamcon_init_open();
	if( hdcam != NULL )
	{
		// show device information
		dcamcon_show_dcamdev_info( hdcam );

		// open wait handle
		DCAMWAIT_OPEN	waitopen;
		memset( &waitopen, 0, sizeof(waitopen) );
		waitopen.size	= sizeof(waitopen);
		waitopen.hdcam	= hdcam;

		err = dcamwait_open( &waitopen );
		if( failed(err) )
		{
			dcamcon_show_dcamerr( hdcam, err, "dcamwait_open()" );
			ret = 1;
		}
		else
		{
			HDCAMWAIT hwait = waitopen.hwait;

			int32 hpos=0, hsize=0, vpos=0, vsize=0;
			// tentative
			{
				// subarray parameter of center 1/4
				double v;
			
				int32 hmax=0, vmax=0;
				if( get_propertyvaluemax( hdcam, DCAM_IDPROP_SUBARRAYHSIZE, v ) )
					hmax = (int32)v;

				if( get_propertyvaluemax( hdcam, DCAM_IDPROP_SUBARRAYVSIZE, v ) )
					vmax = (int32)v;

				hpos	= hmax / 8;
				hsize	= hmax / 4;
				vpos	= vmax / 8;
				vsize	= vmax / 4;
			}

			// set subarray
			if( set_subarray( hdcam, hpos, hsize, vpos, vsize ) )
			{
				int32 number_of_bundle = 4;	// tentative

				// set framebundle
				if( set_framebundle( hdcam, number_of_bundle ) )
				{
					int32 number_of_buffer = 10;
					// allocate the buffer to receive image. the image geometry is fixed.
					err = dcambuf_alloc( hdcam, number_of_buffer );
					if( failed(err) )
					{
						dcamcon_show_dcamerr( hdcam, err, "dcambuf_alloc()" );
						ret = 1;
					}
					else
					{
						// start capture
						err = dcamcap_start( hdcam, DCAMCAP_START_SEQUENCE );
						if( failed(err) )
						{
							dcamcon_show_dcamerr( hdcam, err, "dcamcap_start()" );
							ret = 1;
						}
						else
						{
							printf( "\nStart Capture\n" );

							// set wait param
							DCAMWAIT_START	waitstart;
							memset( &waitstart, 0, sizeof(waitstart) );
							waitstart.size	= sizeof(waitstart);
							waitstart.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
							waitstart.timeout	= 1000;

							// wait image
							err = dcamwait_start( hwait, &waitstart );
							if( failed(err) )
							{
								dcamcon_show_dcamerr( hdcam, err, "dcamwait_start()" );
								ret = 1;
							}

							// stop capture
							dcamcap_stop( hdcam );
							printf( "Stop Capture\n" );

							// access image
							printf( "Access Image\n" );
							sample_access_framebundle_eachimage( hdcam, number_of_buffer );
						}
						
						// release buffer
						dcambuf_release( hdcam );
					}
				}
			}

			// close wait handle
			dcamwait_close( hwait );
		}

		// close DCAM handle
		dcamdev_close( hdcam );
	}
	else
	{
		ret = 1;
	}

	// finalize DCAM-API
	dcamapi_uninit();

	printf( "PROGRAM END\n" );
	return ret;
}