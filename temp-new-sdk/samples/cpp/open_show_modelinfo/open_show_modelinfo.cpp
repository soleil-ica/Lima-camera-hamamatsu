// console/open_show_modelinfo
//

#include "../misc/console4.h"
#include "../misc/common.h"

int main( int argc, char* const argv[] )
{
	printf( "PROGRAM START\n" );

	int	ret = 0;
	DCAMERR err;

	// initialize DCAM-API
	DCAMAPI_INIT	paraminit;
	memset( &paraminit, 0, sizeof(paraminit) );
	paraminit.size	= sizeof(paraminit);

	err = dcamapi_init( &paraminit );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( NULL, err, "dcamapi_init()" );
		ret = 1;
	}
	else
	{
		int32	nDevice = paraminit.iDeviceCount;
		printf( "dcamapi_init() found %d device(s).\n", nDevice );

		int32 iDevice;
		for( iDevice = 0; iDevice < nDevice; iDevice++ )
		{
			printf( "#%d: ", iDevice );

			// open device
			DCAMDEV_OPEN	paramopen;
			memset( &paramopen, 0, sizeof(paramopen) );
			paramopen.size	= sizeof(paramopen);
			paramopen.index	= iDevice;

			err = dcamdev_open( &paramopen );
			if( failed(err) )
			{
				dcamcon_show_dcamerr( (HDCAM)iDevice, err, "dcamdev_open()" );
				ret = 1;
			}
			else
			{
				HDCAM hdcam = paramopen.hdcam;

				dcamcon_show_dcamdev_info( hdcam );
				
				// close device
				dcamdev_close( hdcam );
			}
		}
	}

	// finalaize DCAM-API
	dcamapi_uninit();	// recommended call dcamapi_uninit() when dcamapi_init() is called even if it failed.

	printf( "PROGRAM END\n" );
	return ret;
}

