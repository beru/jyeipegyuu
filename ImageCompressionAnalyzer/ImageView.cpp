#include "stdafx.h"
#include "ImageView.h"

HBITMAP CreateDIB32(int width, int height, BITMAPINFO& bmi, void*& pBits)
{
	BITMAPINFOHEADER& header = bmi.bmiHeader;
	header.biSize = sizeof(BITMAPINFOHEADER);
	header.biWidth = width;
	header.biHeight = height;
	header.biPlanes = 1;
	header.biBitCount = 32;
	header.biCompression = BI_RGB;
	header.biSizeImage = width * abs(height) * 4;
	header.biXPelsPerMeter = 0;
	header.biYPelsPerMeter = 0;
	header.biClrUsed = 0;
	header.biClrImportant = 0;

	return ::CreateDIBSection(
		(HDC)0,
		&bmi,
		DIB_RGB_COLORS,
		&pBits,
		NULL,
		0
	);
}

void CImageView::SetupDIB()
{
	CWindow wnd(GetDesktopWindow());
	HDC hDC = wnd.GetDC();
	int width = GetDeviceCaps(hDC, HORZRES) + 10;
	int height = GetDeviceCaps(hDC, VERTRES) + 10;
	wnd.ReleaseDC(hDC);
	
	// TopDownŒ`Ž®
	if (m_hBMP) {
		DeleteObject(m_hBMP);
		m_hBMP = 0;
	}
	m_hBMP = CreateDIB32(width, -height, m_bmi, m_pBits);
	
	if (m_memDC.m_hDC) {
		m_memDC.DeleteDC();
	}
	HDC dc = GetDC();
	m_memDC.CreateCompatibleDC(dc);
	m_memDC.SetMapMode(GetMapMode(dc));
	ReleaseDC(dc);
	m_memDC.SelectBitmap(m_hBMP);
	
}


LRESULT CImageView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	m_hBMP = 0;
	SetupDIB();
	return 0;
}

LRESULT CImageView::OnDestroy(void)
{
	if (m_hBMP) {
		DeleteObject(m_hBMP);
		m_hBMP = 0;
	}
	//You should call SetMsgHandled(FALSE) or set bHandled = FALSE for the main window of your application
	SetMsgHandled(FALSE);
	return 0;
}

LRESULT CImageView::OnEraseBkgnd(HDC hdc)
{
	return TRUE; // background is erased
}

LRESULT CImageView::OnSize(UINT state, CSize Size)
{
	if (IsWindow() && Size.cx != 0 && Size.cy != 0 && m_hBMP) {
		Render();
	}
	return 0;
}

LRESULT CImageView::OnDisplayChange(UINT bitsPerPixel, CSize Size)
{
	if (m_bmi.bmiHeader.biWidth < Size.cx || abs(m_bmi.bmiHeader.biHeight) < Size.cy) {
		SetupDIB();
		Render();
	}
	return 0;
}

LRESULT CImageView::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CPaintDC dc(m_hWnd);
	CRect rect = dc.m_ps.rcPaint;
	
	dc.BitBlt(
		rect.left,
		rect.top,
		rect.Width(),
		rect.Height(),
		m_memDC,
		rect.left,
		rect.top,
		SRCCOPY
	);
	return 0;
}

void CImageView::Render()
{
}

void CImageView::SetImage(size_t width, size_t height, const unsigned char* p)
{
	unsigned int* pDest = (unsigned int*)m_pBits;
	int lineOffsetBytes = m_bmi.bmiHeader.biWidth * 4;
	for (size_t y=0; y<height; ++y) {
		for (size_t x=0; x<width; ++x) {
			unsigned char c = *p++;
			pDest[x] = RGB(c,c,c);
		}
		OffsetPtr(pDest, lineOffsetBytes);
	}

	Invalidate();
}
