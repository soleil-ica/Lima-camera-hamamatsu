
// ExCapView.cpp : implementation of the CExCapView class
//

#include "stdafx.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "ExCap4.h"
#endif

#include "ExCapApp.h"
#include "ExCapDoc.h"
#include "ExCapView.h"

#include "WndBitmap.h"
#include "luttable.h"
#include "image.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

enum {
	IDT_EXCAPVIEW_UPDATEIMAGE = 1,
};

// CExCapView

IMPLEMENT_DYNCREATE(CExCapView, CView)

BEGIN_MESSAGE_MAP(CExCapView, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_CURRENTFRAME, OnUpdateCurrentframe)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_FRAMES, OnUpdateFrames)
	ON_UPDATE_COMMAND_UI_RANGE(ID_FRAME_HEAD, ID_FRAME_TAIL, OnUpdateFrames)
	ON_COMMAND_RANGE(ID_FRAME_HEAD,ID_FRAME_TAIL,OnCommandFrames)
END_MESSAGE_MAP()

// CExCapView construction/destruction

CExCapView::CExCapView()
{
	// TODO: add construction code here

	m_wndBitmap = new CWndBitmap;
	m_luttable = NULL;	// new luttable;

	m_nHOffset = 0;
	m_nVOffset = 0;

	memset( &m_frame, 0, sizeof( m_frame ) );
	m_frame.current = m_frame.total - 1;
	m_frame.last_draw = -1;
}

CExCapView::~CExCapView()
{
	delete m_wndBitmap;

	if( m_luttable != NULL )
		delete m_luttable;
}

// CExCapView draw bitmap

void CExCapView::update_bitmap()
{
	CExCapDoc* pDoc = GetDocument();
	BOOL	bClearBitmap = TRUE;

	BITMAPINFOHEADER	bmih;
	bmih.biSize = sizeof( bmih );
	if( pDoc->get_bitmapinfoheader( bmih ) )
	{
		CRect rc;
		GetClientRect( &rc );

		double	fZoomH, fZoomV;
		m_wndBitmap->get_zoom( fZoomH, fZoomV );
		ASSERT( fZoomH > 0.0 );
		ASSERT( fZoomV > 0.0 );

		long	hpos = long( m_nHOffset / fZoomH );
		if( hpos < 0 )	hpos = 0;
		long	vpos = long( m_nVOffset / fZoomV );
		if( vpos < 0 )	vpos = 0;

		if( bmih.biWidth > long( rc.Width() / fZoomH ) )
			bmih.biWidth = long( rc.Width() / fZoomH );
		if( bmih.biHeight > long( rc.Height() / fZoomV ) )
			bmih.biHeight = long( rc.Height() / fZoomV );
		if( bmih.biHeight <= 0 )
			bmih.biHeight = 1;

		ASSERT( bmih.biWidth > 0 );
		ASSERT( bmih.biHeight > 0 );

		BYTE*		bottomleft;
		RGBQUAD*	rgb;
		if( m_wndBitmap->allocbits( bmih, rgb, bottomleft ) )
		{
			const BYTE*	lut = ( m_luttable == NULL ? NULL : m_luttable->gettable() );
			if( pDoc->copy_dibits( bottomleft, bmih, m_frame.current, hpos, vpos, rgb, lut ) )
			{
				m_frame.last_draw = m_frame.current;
				bClearBitmap = FALSE;
			}

			m_wndBitmap->unlockbits();
		}
	}

	if( bClearBitmap )
		m_wndBitmap->freebits();

	m_wndBitmap->Invalidate();
}

void CExCapView::update_luttable()
{
	CExCapDoc* pDoc = GetDocument();
	image*	pImage = pDoc->get_image();

	luttable*	new_luttable = NULL;
	if( pImage != NULL )
	{
		long	bitperchannel = pImage->pixelperchannel();

		ASSERT( 8 <= bitperchannel && bitperchannel <= 16 );
		new_luttable = new luttable( bitperchannel );
	}

	if( m_luttable != NULL )
		delete m_luttable;

	m_luttable = new_luttable;

	{
		CExCapApp*	app = afxGetApp();

		HDCAM		hdcam;
		HDCAMWAIT   hwait;
		CExCapDoc*	doc;
		luttable*	lut;

		app->get_active_objects( hdcam, hwait, doc, lut );
		if( doc == GetDocument() && hdcam == doc->get_hdcam() )
			app->set_active_objects( hdcam, hwait, doc, m_luttable );
	}
}

void CExCapView::update_scrollbar( int width, int height )
{
	SCROLLINFO hInfo, vInfo;

	memset( &hInfo , 0, sizeof( hInfo ) );
	hInfo.cbSize = sizeof( hInfo );
	hInfo.fMask	= SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;
	GetScrollInfo( SB_HORZ, &hInfo );

	memset( &vInfo, 0, sizeof( vInfo ) );
	vInfo.cbSize = sizeof( vInfo );
	vInfo.fMask	= SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;
	GetScrollInfo( SB_VERT, &vInfo );

	CRect	rc;
	GetClientRect( &rc );

	double	fZoomH, fZoomV;
	m_wndBitmap->get_zoom( fZoomH, fZoomV );
	ASSERT( fZoomH > 0.0 );
	ASSERT( fZoomV > 0.0 );

	width	= long( width * fZoomH );
	height	= long( height* fZoomV );

	if( width > 0 && height > 0 )
	{
		if( hInfo.nMin < 0 )
			hInfo.nMin = 0;

		hInfo.nMax = width;
		hInfo.nPage = rc.Width();

		if( (int)( hInfo.nPos + hInfo.nPage - hInfo.nMax ) > 0 )
		{
			hInfo.nPos = hInfo.nMax -  hInfo.nPage;
			m_nHOffset = hInfo.nPos;
		}

		if( width == rc.Width() )
			hInfo.nMax = 0;

		if( vInfo.nMin < 0 )
			vInfo.nMin = 0;

		vInfo.nMax = height;
		vInfo.nPage = rc.Height();

		if( (int)( vInfo.nPos + vInfo.nPage - vInfo.nMax ) > 0 )
		{
			vInfo.nPos = vInfo.nMax -  vInfo.nPage;
			m_nVOffset = vInfo.nPos;
		}

		if( height == rc.Height() )
			vInfo.nMax = 0;	
	}
	else
	{
		hInfo.nMin = 0;
		hInfo.nMax = 0;
		hInfo.nPage = rc.Width();

		vInfo.nMin = 0;
		vInfo.nMax = 0;
		vInfo.nPage = rc.Height();
	}

	SetScrollInfo( SB_HORZ, &hInfo );
	SetScrollInfo( SB_VERT, &vInfo );
}
BOOL CExCapView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	cs.style |= WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL ;

	return CView::PreCreateWindow(cs);
}

// CExCapView drawing

void CExCapView::OnDraw(CDC* /*pDC*/)
{
	CExCapDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// TODO: add draw code for native data here
}


// CExCapView diagnostics

#ifdef _DEBUG
void CExCapView::AssertValid() const
{
	CView::AssertValid();
}

void CExCapView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CExCapDoc* CExCapView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CExCapDoc)));
	return (CExCapDoc*)m_pDocument;
}
#endif //_DEBUG


// CExCapView message handlers

int CExCapView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// TODO: Add your specialized creation code here

	CRect	rc;
	GetClientRect( &rc );
	VERIFY( m_wndBitmap->Create( WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, rc, this, 0 ) );

	SetTimer( IDT_EXCAPVIEW_UPDATEIMAGE, 16, NULL );

	return 0;
}

void CExCapView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView) 
{
	// TODO: Add your specialized code here and/or call the base class

	if( bActivate )
	{
		CExCapDoc*	doc = GetDocument();
		CExCapApp*	app = afxGetApp();
		app->set_active_objects( doc->get_hdcam(), doc->get_hwait(),  doc, m_luttable );
		app->update_availables();
	}
	
	CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}

void CExCapView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// TODO: Add your specialized code here and/or call the base class

	switch( lHint )
	{
	case CExCapDoc::image_updated:
		memset( &m_frame, 0, sizeof( m_frame ) );
		m_frame.last_draw = -1;
		{
			CExCapDoc*	doc = GetDocument();
			if( doc != NULL )
			{
				image* img = doc->get_image();
				m_frame.total = img->numberof_frames();
			}
		}
		update_luttable();
		update_bitmap();
		break;

	case CExCapDoc::stop_capture:
		memset( &m_frame, 0, sizeof( m_frame ) );
		{
			CExCapDoc*	doc = GetDocument();
			if( doc != NULL )
				m_frame.total = doc->numberof_capturedframes();
			m_frame.current = m_frame.total - 1;
			m_frame.last_draw = -1;
		}
		break;

	case CExCapDoc::start_capture:
		{
			memset( &m_frame, 0, sizeof( m_frame ) );
			m_frame.current = m_frame.total - 1;
			m_frame.last_draw = -1;

			update_luttable();

			CExCapDoc*	doc = GetDocument();
			double	dBinning = 1;

			DCAMERR err;

			err = dcamprop_getvalue( doc->get_hdcam(), DCAM_IDPROP_BINNING, &dBinning );
			VERIFY( !failed(err) );

			int32 nBin = (int32)dBinning;
			long	hbin, vbin;
			hbin = vbin = nBin;
			if( nBin > 100 )
			{
				hbin = nBin / 100;
				vbin = nBin % 100;
			}

			double	f;
			err = dcamprop_getvalue( doc->get_hdcam(), DCAM_IDPROP_BINNING_HORZ, &f );
			if( !failed(err) )
				hbin = (long)f;

			err = dcamprop_getvalue( doc->get_hdcam(), DCAM_IDPROP_BINNING_VERT, &f );
			if( !failed(err) )
				vbin = (long)f;

			m_wndBitmap->set_zoom( hbin, vbin );


			double cx;
			err = dcamprop_getvalue( doc->get_hdcam(), DCAM_IDPROP_IMAGE_WIDTH, &cx);
			VERIFY( !failed(err) );
			double cy;
			err = dcamprop_getvalue( doc->get_hdcam(), DCAM_IDPROP_IMAGE_HEIGHT, &cy);
			VERIFY( !failed(err) );

			long width = static_cast<long>(cx);
			long height = static_cast<long>(cy);

#ifdef _EXCAP_SUPPORTS_VIEWS_
			if( doc->is_show_allview() )
			{
				err = dcamprop_getvalue( doc->get_hdcam(), DCAM_IDPROP_NUMBEROF_VIEW, &f );
				if( !failed(err) && f > 1 )
					height *= static_cast<int32>(f);
			}
#endif // _EXCAP_SUPPORTS_VIEWS_ !
			update_scrollbar( width, height );
		}
		break;

	default:
		CView::OnUpdate( pSender, lHint, pHint );
	}
}

void CExCapView::OnTimer(UINT_PTR nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default

	CExCapDoc*	doc = GetDocument();

	switch( nIDEvent )
	{
	case IDT_EXCAPVIEW_UPDATEIMAGE:
		if( (m_frame.total > 0 && m_frame.last_draw != m_frame.current )
		 ||	doc->is_bitmap_updated()
		 || m_luttable->is_updated() )
		{
			update_bitmap();
		}
		break;
	}

	CView::OnTimer(nIDEvent);
}

void CExCapView::OnSize(UINT nType, int cx, int cy) 
{
	CView::OnSize(nType, cx, cy);
	
	// TODO: Add your message handler code here

	if( IsWindow( m_wndBitmap->GetSafeHwnd() ) )
		m_wndBitmap->SetWindowPos( NULL, 0, 0, cx, cy, SWP_NOZORDER );

	CExCapDoc* pDoc = GetDocument();

	BITMAPINFOHEADER	bmih;
	bmih.biSize = sizeof( bmih );

	if( pDoc->get_image() != NULL && pDoc->get_bitmapinfoheader( bmih ) )
	{
		update_scrollbar( bmih.biWidth, bmih.biHeight );
		update_bitmap();
	}
	else
		update_scrollbar( 0, 0 );
}

void CExCapView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	SCROLLINFO info;
	memset( &info, 0, sizeof( info ) );
	info.cbSize = sizeof( info );
	info.fMask	= SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;

	GetScrollInfo( SB_HORZ, &info );

	long dx = 0;
	double	fZoomH, fZoomV;
	m_wndBitmap->get_zoom( fZoomH, fZoomV );
	long	nStep = (long)fZoomH;
	if( nStep < 1 )	nStep = 1;

	switch( nSBCode )
	{
	case SB_LEFT:			dx = -nStep;			break;
	case SB_LINELEFT:		dx = -10 * nStep;		break;
	case SB_PAGELEFT:		dx = -(int)info.nPage;	break;
	case SB_PAGERIGHT:		dx = (int)info.nPage;	break;
	case SB_LINERIGHT:		dx = 10 * nStep;		break;
	case SB_RIGHT:			dx = nStep;				break;
	case SB_ENDSCROLL:		dx = 0;					break;
	case SB_THUMBTRACK:		dx = nPos - info.nPos;	break;
	case SB_THUMBPOSITION:		
		if( long( nPos - info.nPos ) > 10 * nStep )
			dx = 10 * nStep;
		else
		if( long( nPos - info.nPos ) < -10 * nStep )
			dx = -10 * nStep;
		else
			dx = nPos - info.nPos;
		
		break;
	}

	info.nPos += dx;

	if( info.nPos > (int)(info.nMax - info.nPage) )
		info.nPos = info.nMax - info.nPage;

	if( info.nPos < info.nMin )
		info.nPos = info.nMin;

	if( m_nHOffset != info.nPos )
	{
		m_nHOffset = info.nPos;
		SetScrollPos( SB_HORZ, info.nPos );
		update_bitmap();
	}
	
	CView::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CExCapView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	// TODO: Add your message handler code here and/or call default
	SCROLLINFO info;
	memset( &info, 0, sizeof( info ) );
	info.cbSize = sizeof( info );
	info.fMask	= SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS;

	GetScrollInfo( SB_VERT, &info );

	long dy = 0;
	double	fZoomH, fZoomV;
	m_wndBitmap->get_zoom( fZoomH, fZoomV );
	long	nStep = (long)fZoomV;
	if( nStep < 1 )	nStep = 1;

	switch( nSBCode )
	{
		case SB_BOTTOM:			dy = nStep;				break;
		case SB_LINEDOWN:		dy = 10 * nStep;		break;
		case SB_PAGEDOWN:		dy = (int)info.nPage;	break;
		case SB_PAGEUP:			dy = -(int)info.nPage;	break;
		case SB_LINEUP:			dy = -10 * nStep;		break;
		case SB_TOP:			dy = -nStep;			break;
		case SB_ENDSCROLL:		dy = 0;break;
		case SB_THUMBTRACK	:	dy = nPos - info.nPos;	break;
		case SB_THUMBPOSITION:		
			if( long( nPos - info.nPos ) > 10 * nStep )
				dy = 10 * nStep;
			if( long( nPos - info.nPos ) < -10 * nStep )
				dy = -10 * nStep;
			else
				dy = nPos - info.nPos;
			
			break;
	}

	info.nPos += dy;

	if( info.nPos > (int)(info.nMax - info.nPage) )
		info.nPos = info.nMax - info.nPage;

	if( info.nPos < info.nMin )
		info.nPos = info.nMin;

	if( m_nVOffset != info.nPos )
	{
		m_nVOffset = info.nPos;
		SetScrollPos( SB_VERT, info.nPos );
		update_bitmap();
	}
	
	CView::OnVScroll(nSBCode, nPos, pScrollBar);

}

void CExCapView::OnUpdateCurrentframe(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here

	BOOL	bEnable	= FALSE;

	if( m_frame.total > 0 )
	{
		CString	str;
		str.Format( _T("%d"), m_frame.current );
		pCmdUI->SetText( str );

		bEnable	= TRUE;
	}
	
	pCmdUI->Enable( bEnable );
}

void CExCapView::OnUpdateFrames(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here

	BOOL	bEnable	= FALSE;

	if( pCmdUI->m_nID == ID_INDICATOR_FRAMES )
	{
		if( m_frame.total > 0 )
		{
			CString	str;
			str.Format( _T("/ %d"), m_frame.total );
			pCmdUI->SetText( str );

			bEnable	= TRUE;
		}
	}
	else
	{
		switch( pCmdUI->m_nID )
		{
		case ID_FRAME_HEAD:
		case ID_FRAME_PREV:
			if( m_frame.total > 0 && m_frame.current > 0 )
				bEnable = TRUE;
			break;

		case ID_FRAME_TAIL:
		case ID_FRAME_NEXT:
			if( m_frame.total > 0 && m_frame.current < m_frame.total-1 )
				bEnable = TRUE;
			break;

		default:
			ASSERT( 0 );
		}
	}

	pCmdUI->Enable( bEnable );
}

void CExCapView::OnCommandFrames( UINT id ) 
{
	ASSERT( m_frame.total > 0 );

	long	iFrame = m_frame.current;
	switch( id )
	{
	case ID_FRAME_HEAD:	iFrame = 0;	break;
	case ID_FRAME_PREV:	iFrame--;	break;
	case ID_FRAME_TAIL:	iFrame = m_frame.total-1;	break;
	case ID_FRAME_NEXT:	iFrame++;	break;
	}

	m_frame.current = iFrame;

}
