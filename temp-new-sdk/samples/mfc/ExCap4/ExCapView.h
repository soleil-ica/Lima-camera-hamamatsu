
// ExCapView.h : interface of the CExCapView class
//

#pragma once


class CExCapView : public CView
{
protected: // create from serialization only
	CExCapView();
	DECLARE_DYNCREATE(CExCapView)

// Attributes
public:
	CExCapDoc* GetDocument() const;

	long m_nHOffset;
	long m_nVOffset;

// Operations
public:

// Overrides
public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);

	class CWndBitmap*	m_wndBitmap;
	class luttable*		m_luttable;

	struct {
		long	total;
		long	current;
		long	last_draw;
	} m_frame;

// Implementation
public:
	virtual ~CExCapView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
			void	update_bitmap();
			void	update_luttable();
			void	update_scrollbar( int width, int height );

// Generated message map functions
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnUpdateCurrentframe(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFrames(CCmdUI* pCmdUI);
	afx_msg void OnCommandFrames( UINT id );
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in ExCapView.cpp
inline CExCapDoc* CExCapView::GetDocument() const
   { return reinterpret_cast<CExCapDoc*>(m_pDocument); }
#endif

