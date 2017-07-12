// console/propertylist
//

#include "../misc/console4.h"
#include "../misc/common.h"

#define SHOW_PROPERTY_ATTRIBUTE			0	// show the attribute of the property
#define SHOW_PROPERTY_MODEVALUELIST		0	// show the value list of mode
#define SHOW_PROPERTY_ARRAYELEMENT		0	// show the element of array when the property is base element.

void dcamcon_show_arrayelement( HDCAM hdcam, DCAMPROP_ATTR attr )
{
	printf( "Array Element:\n" );

	int32 iPropBase = attr.iProp;

	DCAMERR err;

	int32 iProp = 0;
	double v;

	// get number of array
	iProp = attr.iProp_NumberOfElement;
	err = dcamprop_getvalue( hdcam, iProp, &v );
	if( !failed(err) )
	{
		int32 nArray = (int32)v;
		printf( "\tNumber of element: %d\n", nArray );

		int i;
		for( i = 1; i < nArray; i++ )
		{
			char	text[ 64 ];

			// get property name of array element
			iProp = attr.iProp + i * attr.iPropStep_Element;
			err = dcamprop_getname( hdcam, iProp, text, sizeof(text) );
			if( failed(err) )
			{
				dcamcon_show_dcamerr( hdcam, err, "dcamprop_getname()", "IDPROP=%08lx", iProp );
				return;
			}

			printf( "\t0x%08x: %s\n", iProp, text );
		}
	}
}

void dcamcon_show_supportmodevalues( HDCAM hdcam, int32 iProp, double v )
{
	printf( "Support:\n" );

	DCAMERR err;

	int32 pv_index = 0;

	do
	{
		// get value text
		char	pv_text[ 64 ];

		DCAMPROP_VALUETEXT pvt;
		memset( &pvt, 0, sizeof(pvt) );
		pvt.cbSize		= sizeof(pvt);
		pvt.iProp		= iProp;
		pvt.value		= v;
		pvt.text		= pv_text;
		pvt.textbytes	= sizeof(pv_text);

		pv_index++;
		err = dcamprop_getvaluetext( hdcam, &pvt );
		if( !failed(err) )
		{
			printf( "\t%d:\t%s\n", pv_index, pv_text );
		}

		// get next value
		err = dcamprop_queryvalue( hdcam, iProp, &v, DCAMPROP_OPTION_NEXT );
	} while( !failed(err) );
}

void printf_attr( int32& count, const char* name )
{
	if( count == 0 )
		printf( "%s", name );
	else
		printf( " | %s", name );

	count++;
}

void dcamcon_show_propertyattr( DCAMPROP_ATTR attr )
{
	int32 count = 0;

	printf( "ATTR:\t" );

	// attribute
	if( attr.attribute & DCAMPROP_ATTR_WRITABLE )				printf_attr( count, "WRITABLE" );
	if( attr.attribute & DCAMPROP_ATTR_READABLE )				printf_attr( count, "READABLE" );
	if( attr.attribute & DCAMPROP_ATTR_DATASTREAM )				printf_attr( count, "DATASTREAM" );
	if( attr.attribute & DCAMPROP_ATTR_ACCESSREADY )			printf_attr( count, "ACCESSREADY" );
	if( attr.attribute & DCAMPROP_ATTR_ACCESSBUSY )				printf_attr( count, "ACCESSBUSY" );
	if( attr.attribute & DCAMPROP_ATTR_HASVIEW )				printf_attr( count, "HASVIEW" );
	if( attr.attribute & DCAMPROP_ATTR_HASCHANNEL )				printf_attr( count, "HASCHANNEL" );
	if( attr.attribute & DCAMPROP_ATTR_HASRATIO )				printf_attr( count, "HASRATIO" );
	if( attr.attribute & DCAMPROP_ATTR_VOLATILE )				printf_attr( count, "VOLATILE" );
	if( attr.attribute & DCAMPROP_ATTR_AUTOROUNDING )			printf_attr( count, "AUTOROUNDING" );
	if( attr.attribute & DCAMPROP_ATTR_STEPPING_INCONSISTENT )	printf_attr( count, "STEPPING_INCONSISTENT" );

	// attribute2
	if( attr.attribute2 & DCAMPROP_ATTR2_ARRAYBASE )			printf_attr( count, "ARRAYBASE" );
	if( attr.attribute2 & DCAMPROP_ATTR2_ARRAYELEMENT )			printf_attr( count, "ARRAYELEMENT" );

	if( count == 0 )	printf( "none" );
	printf( "\n" );

	// mode
	switch( attr.attribute & DCAMPROP_TYPE_MASK )
	{
	case DCAMPROP_TYPE_MODE:	printf( "TYPE:\tMODE\n" );	break;
	case DCAMPROP_TYPE_LONG:	printf( "TYPE:\tLONG\n" );	break;
	case DCAMPROP_TYPE_REAL:	printf( "TYPE:\tREAL\n" );	break;
	default:					printf( "TYPE:\tNONE\n" );	break;
	}

	// range
	if( attr.attribute & DCAMPROP_ATTR_HASRANGE )
	{
		printf( "min:\t%f\n", attr.valuemin );
		printf( "max:\t%f\n", attr.valuemax );
	}
	// step
	if( attr.attribute & DCAMPROP_ATTR_HASSTEP )
	{
		printf( "step:\t%f\n", attr.valuestep );
	}
	// default
	if( attr.attribute & DCAMPROP_ATTR_HASDEFAULT )
	{
		printf( "default:\t%f\n", attr.valuedefault );
	}
}

void dcamcon_show_property_list( HDCAM hdcam )
{
	printf( "\nShow Property List( ID: name" );
#if SHOW_PROPERTY_ATTRIBUTE
	printf( "\n\t-attribute" );
#endif
#if SHOW_PROPERTY_MODEVALUELIST
	printf( "\n\t-mode value list" );
#endif
#if SHOW_PROPERTY_ARRAYELEMENT
	printf( "\n\t-array element" );
#endif
	printf( " )\n" );

	int32	iProp = 0;	// property IDs

	DCAMERR err;
	err = dcamprop_getnextid( hdcam, &iProp, DCAMPROP_OPTION_SUPPORT );
	if( failed(err) )
	{
		dcamcon_show_dcamerr( hdcam, err, "dcamprop_getnextid()", "IDPROP=0x%08x, OPTION=SUPPORT", 0 );
		return;
	}

	do
	{
		// get property name
		char	text[ 64 ];
		err = dcamprop_getname( hdcam, iProp, text, sizeof(text) );
		if( failed(err) )
		{
			dcamcon_show_dcamerr( hdcam, err, "dcamprop_getname()", "IDPROP=0x%08x", iProp );
			return;
		}

		printf( "0x%08x: %s\n", iProp, text );

		// get property attribute
		DCAMPROP_ATTR	attr;
		memset( &attr, 0, sizeof(attr) );
		attr.cbSize	= sizeof(attr);
		attr.iProp	= iProp;

		err = dcamprop_getattr( hdcam, &attr );
		if( !failed(err) )
		{
#if SHOW_PROPERTY_ATTRIBUTE
			// show property attribute
			dcamcon_show_propertyattr( attr );
#endif

#if SHOW_PROPERTY_MODEVALUELIST
			// show mode value list of property
			if( (attr.attribute & DCAMPROP_TYPE_MASK) == DCAMPROP_TYPE_MODE )
				dcamcon_show_supportmodevalues( hdcam, iProp, attr.valuemin );
#endif

#if SHOW_PROPERTY_ARRAYELEMENT
			// show array element
			if( attr.attribute2 & DCAMPROP_ATTR2_ARRAYBASE )
				dcamcon_show_arrayelement( hdcam, attr );
#endif
		}

		// get next property id
		err = dcamprop_getnextid( hdcam, &iProp, DCAMPROP_OPTION_SUPPORT );
		if( failed(err) )
		{
			// no more supported property id
			return;
		}

	} while( iProp != 0 );
}

int main( int argc, char* const argv[] )
{
	printf( "PROGRAM START\n" );

	int	ret = 0;

	// initialize DCAM-API and open device
	HDCAM hdcam;
	hdcam = dcamcon_init_open();
	if( hdcam != NULL )
	{
		dcamcon_show_dcamdev_info( hdcam );

		// show all property list that the camera supports. 
		dcamcon_show_property_list( hdcam );

		// close device
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