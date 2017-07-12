// WndBitmap.cpp : implementation file
//

#include "stdafx.h"
#include "WndBitmap.h"

#include "bitmap.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// var_wndBitmap

struct var_wndBitmap
{
	BITMAPINFOHEADER	bmih;
    RGBQUAD             bmiColors[256];

	BYTE*	dibits;
//	long	rowbytes;
	long	dibitsize;

	double	m_fZoomH;
	double	m_fZoomV;

public:
	~var_wndBitmap()
	{
		if( dibits != NULL )
			delete dibits;
	}
	var_wndBitmap()
	{
		memset( &bmih, 0, sizeof( bmih ) );
		bmih.biSize = sizeof( bmih );

		memset( &bmiColors, 0, sizeof( bmiColors ) );

		dibits = NULL;
	//	rowbytes = 0;
		dibitsize = 0;

		m_fZoomH = m_fZoomV = 1;
	}
};

/////////////////////////////////////////////////////////////////////////////
// CWndBitmap

CWndBitmap::CWndBitmap()
{
	pvar_wndBitmap = new var_wndBitmap;
}

CWndBitmap::~CWndBitmap()
{
	delete pvar_wndBitmap;
}


BEGIN_MESSAGE_MAP(CWndBitmap, CWnd)
	//{{AFX_MSG_MAP(CWndBitmap)
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CWndBitmap::allocbits( const BITMAPINFOHEADER& bmih, RGBQUAD*& rgb, BYTE*& bottomleft )
{
	long	newrowbytes = abs( getrowbytes( bmih ) );

	long	newdibitsize = newrowbytes * abs( bmih.biHeight );
	if( pvar_wndBitmap->dibitsize < newdibitsize )
	{
		if( pvar_wndBitmap->dibits != NULL )
			delete pvar_wndBitmap->dibits;

		pvar_wndBitmap->dibits = new BYTE[ newdibitsize ];
		pvar_wndBitmap->dibitsize = newdibitsize;
	}

	if( pvar_wndBitmap->dibits == NULL )
		return FALSE;

	memcpy( &pvar_wndBitmap->bmih, &bmih, sizeof( bmih ) );
	memset( pvar_wndBitmap->dibits, 0xFF, newdibitsize );

	bottomleft = pvar_wndBitmap->dibits;
	rgb = pvar_wndBitmap->bmiColors;

	return TRUE;
}

BOOL CWndBitmap::unlockbits()
{
	return TRUE;
}

void CWndBitmap::freebits()
{
	if( pvar_wndBitmap->dibits != NULL )
	{
		delete pvar_wndBitmap->dibits;
		pvar_wndBitmap->dibits = NULL;
	}

	pvar_wndBitmap->dibitsize = 0;
}


void CWndBitmap::get_zoom( double& fZoomH, double& fZoomV ) const
{
	fZoomH = pvar_wndBitmap->m_fZoomH;
	fZoomV = pvar_wndBitmap->m_fZoomV;
}

void CWndBitmap::set_zoom( double fZoomH, double fZoomV )
{
	pvar_wndBitmap->m_fZoomH = fZoomH;
	pvar_wndBitmap->m_fZoomV = fZoomV;

	recalc_layout();
}

void CWndBitmap::recalc_layout()
{
	if( IsWindow( GetSafeHwnd() ) )
	{
	}
}

/////////////////////////////////////////////////////////////////////////////
// CWndBitmap message handlers

BOOL CWndBitmap::Create( DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CWnd::Create( NULL, _T( "" ), dwStyle, rect, pParentWnd, nID, pContext);
}

BOOL CWndBitmap::OnEraseBkgnd(CDC* pDC) 
{
	// TODO: Add your message handler code here and/or call default

	CRect	rc;

	if( pvar_wndBitmap->dibits != NULL )
	{
		long	srcleft = 0;
		long	srctop = 0;
		long	srcwidth = pvar_wndBitmap->bmih.biWidth - srcleft;
		long	srcheight= abs( pvar_wndBitmap->bmih.biHeight ) - srctop;

		GetClientRect( &rc );

		long	dstwidth  = long( srcwidth  * pvar_wndBitmap->m_fZoomH );
		long	dstheight = long( srcheight * pvar_wndBitmap->m_fZoomV );
		long	dstleft	= ( rc.right + rc.left - dstwidth ) / 2;
		long	dsttop	= ( rc.bottom + rc.top - dstheight ) / 2;

		StretchDIBits( pDC->GetSafeHdc()
			, dstleft, dsttop, dstwidth, dstheight
			, srcleft, srctop, srcwidth, srcheight
			, pvar_wndBitmap->dibits, (const BITMAPINFO*)&pvar_wndBitmap->bmih
			, DIB_RGB_COLORS, SRCCOPY );

		pDC->ExcludeClipRect( dstleft, dsttop, dstleft + dstwidth, dsttop + dstheight );
	}

	GetClientRect( &rc );
	pDC->ExtTextOut( 0, 0, ETO_OPAQUE | ETO_CLIPPED, &rc, _T( "" ), 0, NULL );
	return TRUE;
//	return CWnd::OnEraseBkgnd(pDC);
}

void CWndBitmap::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	
	// TODO: Add your message handler code here
	
	recalc_layout();
}
