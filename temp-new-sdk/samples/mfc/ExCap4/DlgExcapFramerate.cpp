// DlgExcapFramerate.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "DlgExcapFramerate.h"
#include "ExCapFramerate.h"
#include "ExCapFiretrigger.h"
#include "ExCapCallback.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void double2text( double v, CString& str )
{
	if( v <= -10000 || 10000 <= v )
	{
		str.Format( _T( "%f" ), v );
	}
	else
	if( v <= -1000 || 1000 <= v )
	{
		str.Format( _T( "%.1f" ), v );
	}
	else
	if( v <= -100 || 100 <= v )
	{
		str.Format( _T( "%.2f" ), v );
	}
	else
	if( v <= -10 || 10 <= v )
	{
		str.Format( _T( "%.3f" ), v );
	}
	else
	{
		str.Format( _T( "%.4f" ), v );
	}
}

/////////////////////////////////////////////////////////////////////////////

class CExCapCallback_DlgExcapFramerate : public CExCapCallback
{
public:
	CExCapCallback_DlgExcapFramerate( CExCapFiretrigger* pFiretrigger )
	{
		ASSERT( pFiretrigger != NULL );
		m_firetrigger = pFiretrigger;

		m_dwEvent = DCAMWAIT_CAPEVENT_TRANSFERRED;
	}

public:
	void on_dcamwait( HDCAM hdcam, HDCAMWAIT hwait, DWORD dwEvent )
	{
		if( dwEvent == m_dwEvent )
			m_firetrigger->firetrigger( hdcam, hwait );
	}
	void on_lostframe( HDCAM hdcam, HDCAMWAIT hwait )
	{
		m_firetrigger->firetrigger( hdcam, hwait );
	}

	void set_callback_event( DWORD dwEvent )
	{
		m_dwEvent = dwEvent;
	}

protected:
	CExCapFiretrigger*	m_firetrigger;
	DWORD				m_dwEvent;
};

/////////////////////////////////////////////////////////////////////////////
// CDlgExcapFramerate dialog


CDlgExcapFramerate::CDlgExcapFramerate(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgExcapFramerate::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDlgExcapFramerate)
	m_strAverageFps = _T("");
	m_strAveragePeriod = _T("");
	m_strFastestFps = _T("");
	m_strFastestPeriod = _T("");
	m_strLatestFps = _T("");
	m_strLatestPeriod = _T("");
	m_strSlowestFps = _T("");
	m_strSlowestPeriod = _T("");
	m_strLostframecount = _T("");
	m_strTotalframecount = _T("");
	m_strEvents = _T("");
	//}}AFX_DATA_INIT

	m_bCreateDialog			= FALSE;
	m_hdcam					= NULL;
	m_hwait					= NULL;
	m_events				= 0;

	m_framerate				= NULL;
	m_firetrigger			= NULL;
	m_callback				= NULL;
}

// ----------------

HDCAM CDlgExcapFramerate::set_hdcamwait( HDCAM hdcam, HDCAMWAIT hwait, int32 events )
{
	HDCAM	old = m_hdcam;
	m_hdcam = hdcam;
	m_hwait = hwait;
	m_events = events;

	if( m_framerate != NULL )
		m_framerate->set_hdcamwait( m_hdcam, hwait );

	if( m_hdcam != NULL && m_callback != NULL )
	{

		if( m_events & DCAMWAIT_CAPEVENT_EXPOSUREEND )
		{
			m_callback->set_callback_event( DCAMWAIT_CAPEVENT_EXPOSUREEND );
		}
		else
		{
			ASSERT( m_events & DCAMWAIT_CAPEVENT_TRANSFERRED );
			m_callback->set_callback_event( DCAMWAIT_CAPEVENT_TRANSFERRED );
		}
	}
/*
	update_values();

	if( IsWindow( GetSafeHwnd() ) && IsWindowVisible() )
	{
		setup_controls();
		update_controls();
	}
*/
	return old;
}

BOOL CDlgExcapFramerate::toggle_visible()
{
	if( ! IsWindow( GetSafeHwnd() ) )
	{
		if( ! Create() )
		{
			ASSERT( 0 );
			return FALSE;
		}
	}
	else
	if( IsWindowVisible() )
	{
		ShowWindow( SW_HIDE );
	}
	else
	{
		SetWindowPos( &CWnd::wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

BOOL CDlgExcapFramerate::Create(CWnd* pParentWnd) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	m_bCreateDialog = CDialog::Create(IDD, pParentWnd );
	return m_bCreateDialog;
}

// ----------------

void CDlgExcapFramerate::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgExcapFramerate)
	DDX_Control(pDX, IDC_EXCAPFRAMERATE_BTNFIRETRIGGERREPEATEDLY, m_btnFiretriggerRepeatedly);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTAVERAGEFPS, m_strAverageFps);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTAVERAGEPERIOD, m_strAveragePeriod);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTFASTESTFPS, m_strFastestFps);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTFASTESTPERIOD, m_strFastestPeriod);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTLATESTFPS, m_strLatestFps);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTLATESTPERIOD, m_strLatestPeriod);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTSLOWESTFPS, m_strSlowestFps);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTSLOWESTPERIOD, m_strSlowestPeriod);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTLOSTFRAMECOUNT, m_strLostframecount);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTTOTALFRAMECOUNT, m_strTotalframecount);
	DDX_Text(pDX, IDC_EXCAPFRAMERATE_TXTEVENTS, m_strEvents);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgExcapFramerate, CDialog)
	//{{AFX_MSG_MAP(CDlgExcapFramerate)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_EXCAPFRAMERATE_BTNFIRETRIGGERREPEATEDLY, OnExcapframerateBtnrepeatedlyfiretrigger)
	ON_BN_CLICKED(IDC_EXCAPFRAMERATE_BTNRESETFRAMECOUNT, OnExcapframerateBtnresetframecount)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////

void CDlgExcapFramerate::update_controls( BOOL b, double period, CString& strPeriod, CString& strFps )
{
	if( b )
	{
		double2text( period, strPeriod );
	}
	else
	{
		period = 0;
		strPeriod = "-";
	}

	if( period > 0 )
		double2text( 1 / period, strFps );
	else
		strFps = "-";
}

/////////////////////////////////////////////////////////////////////////////
// CDlgExcapFramerate message handlers

#define	IDT_UPDATEFRAMERATE	1

BOOL CDlgExcapFramerate::OnInitDialog() 
{
	m_strAverageFps = _T("");
	m_strAveragePeriod = _T("");
	m_strFastestFps = _T("");
	m_strFastestPeriod = _T("");
	m_strLatestFps = _T("");
	m_strLatestPeriod = _T("");
	m_strSlowestFps = _T("");
	m_strSlowestPeriod = _T("");

	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here

	if( m_framerate == NULL )
		m_framerate = new CExCapFramerate;

	if( m_firetrigger == NULL )
		m_firetrigger = new CExCapFiretrigger;

	if( m_callback == NULL )
		m_callback = new CExCapCallback_DlgExcapFramerate( m_firetrigger );

	set_hdcamwait( m_hdcam, m_hwait,  m_events);

	SetTimer( IDT_UPDATEFRAMERATE, 500, NULL );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgExcapFramerate::OnDestroy() 
{
	CDialog::OnDestroy();
	
	// TODO: Add your message handler code here

	if( m_framerate != NULL )
	{
		m_framerate->set_callback( NULL );
		m_framerate->release();
		m_framerate = NULL;
	}
	if( m_firetrigger != NULL )
	{
		m_firetrigger->release();
		m_firetrigger = NULL;
	}
	if( m_callback != NULL )
	{
		delete m_callback;
		m_callback = NULL;
	}

	m_bCreateDialog	= FALSE;
}

void CDlgExcapFramerate::OnOK() 
{
	// TODO: Add extra validation here
	
	if( ! m_bCreateDialog )
	{
		// If this dialog is called from CWnd::DoModal(), the main routine should set 
		CDialog::OnOK();
		return;
	}

	if( ! UpdateData() )
		return;
}

void CDlgExcapFramerate::OnCancel() 
{
	// TODO: Add extra cleanup here	
	if( m_bCreateDialog )
		DestroyWindow();
	else
		CDialog::OnCancel();
}

void CDlgExcapFramerate::OnTimer(UINT_PTR nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default

	if( m_framerate != NULL )
	{
		BOOL	b;
		double	period;

		m_framerate->enter_critical();

		// latest
		b = m_framerate->get_latestperiod( period );
		update_controls( b, period, m_strLatestPeriod, m_strLatestFps );

		// average
		b = m_framerate->get_averageperiod( period );
		update_controls( b, period, m_strAveragePeriod, m_strAverageFps );

		// fastest
		b = m_framerate->get_minimumperiod( period );
		update_controls( b, period, m_strFastestPeriod, m_strFastestFps );

		// slowest
		b = m_framerate->get_maximumperiod( period );
		update_controls( b, period, m_strSlowestPeriod, m_strSlowestFps );

		// frame count
		long	total, lost;
		m_framerate->get_framecount( total, lost );
		m_strTotalframecount.Format( _T( "%d" ), total );
		m_strLostframecount.Format(  _T( "%d" ), lost );

		long	exposureend, frameend, captureend, unknown;
		m_framerate->get_eventcount( exposureend, frameend, captureend, unknown );
		m_strEvents.Format( _T( "XE:%d, FE:%d, CE:%d, UN:%d" ), exposureend, frameend, captureend, unknown );

		m_framerate->leave_critical();

		UpdateData( FALSE );
	}
	
	CDialog::OnTimer(nIDEvent);
}

void CDlgExcapFramerate::OnExcapframerateBtnrepeatedlyfiretrigger() 
{
	// TODO: Add your control notification handler code here
	
	if( m_framerate != NULL )
	{
		if( m_btnFiretriggerRepeatedly.GetCheck() == 0 )
		{
			m_framerate->set_callback( NULL );
		}
		else
		{
			m_framerate->set_callback( m_callback );
		}
	}
}

void CDlgExcapFramerate::OnExcapframerateBtnresetframecount() 
{
	// TODO: Add your control notification handler code here

	if( m_framerate != NULL )
	{
		m_framerate->reset_framecount();
		m_framerate->reset_timestamp();
	}
}
