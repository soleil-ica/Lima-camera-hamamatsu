// WndBitmap.h : header file
//

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CWndBitmap window

class CWndBitmap : public CWnd
{
// Construction
public:
	CWndBitmap();

// Attributes
public:
	struct var_wndBitmap*	pvar_wndBitmap;

// Operations
public:
			BOOL	allocbits( const BITMAPINFOHEADER& bmih, RGBQUAD*& rgb, BYTE*& bottomleft );
			BOOL	unlockbits();
			void	freebits();

			void	set_zoom( double fZoomH, double fZoomV );
			void	get_zoom( double& fZoomH, double& fZoomV ) const;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWndBitmap)
	public:
	virtual BOOL Create( DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CWndBitmap();

protected:
			void	recalc_layout();

	// Generated message map functions
protected:
	//{{AFX_MSG(CWndBitmap)
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
