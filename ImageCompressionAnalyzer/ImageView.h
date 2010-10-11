#pragma once

class CImageView : public CWindowImpl<CImageView>
{
	HBITMAP m_hBMP;
	BITMAPINFO m_bmi;
	void* m_pBits;
	CDC m_memDC;
	void SetupDIB();
	void Render();

public:
	DECLARE_WND_CLASS_EX(NULL, CS_NOCLOSE|CS_DBLCLKS|CS_OWNDC, (HBRUSH)GetStockObject(NULL_BRUSH)-1)
	
	BOOL PreTranslateMessage(MSG* pMsg)
	{
		pMsg;
		return FALSE;
	}
	
	BEGIN_MSG_MAP(CImageCompressionAnalyzerView)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_ERASEBKGND(OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MSG_WM_SIZE(OnSize)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct);
	LRESULT OnDestroy(void);
	LRESULT OnEraseBkgnd(HDC hdc);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT state, CSize Size);
	LRESULT OnDisplayChange(UINT bitsPerPixel, CSize Size);

	void SetImage(size_t width, size_t height, const unsigned char* p);
};
