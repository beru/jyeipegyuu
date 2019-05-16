#include "stdafx.h"
#include "MainFrm.h"

#include "SettingView.h"
#include "ImageView.h"
#include "AboutDlg.h"

#include <atldlgs.h>

CMainFrame::CMainFrame()
	:
	m_pSettingView(new CSettingView),
	m_pImageView(new CImageView)
{
	m_pSettingView->m_onSettingChangeDelegate.bind(this, &CMainFrame::OnSettingChange);
}


CMainFrame::~CMainFrame()
{
	delete m_pSettingView;
	delete m_pImageView;
}

// virtual
BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	if (m_pSettingView->PreTranslateMessage(pMsg)) {
		return TRUE;
	}
	return m_pImageView->PreTranslateMessage(pMsg);
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// create command bar window
	HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	// attach menu
	m_CmdBar.AttachMenu(GetMenu());
	// load command bar images
	m_CmdBar.LoadImages(IDR_MAINFRAME);
	// remove old menu
	SetMenu(NULL);

	HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

  // ステータスバーを作成
  m_hWndStatusBar = m_wndStatusBar.Create(m_hWnd);
  UIAddStatusBar(m_hWndStatusBar);
	
  HDC hDC = ::GetDC(0);
  auto dpiX = GetDeviceCaps(hDC, LOGPIXELSX);
  auto dpiY = GetDeviceCaps(hDC, LOGPIXELSY);
  ::ReleaseDC(0, hDC);

	// ステータスバーにペインを設定
	int nPanes[] = {ID_DEFAULT_PANE, 1};
	m_wndStatusBar.SetPanes(nPanes, sizeof(nPanes)/sizeof(nPanes[0]));
	m_wndStatusBar.SetPaneWidth(1, ::MulDiv(200, dpiX, 96));
	
	m_hWndClient = m_splitter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_pane.SetPaneContainerExtendedStyle(PANECNT_NOBORDER);
	m_pane.Create(m_splitter, _T("Setting"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_pSettingView->Create(m_pane, rcDefault);
	m_pSettingView->SetFont(AtlGetDefaultGuiFont());
	m_pane.SetClient(*m_pSettingView);

	m_pImageView->Create(m_splitter, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

	m_splitter.SetSplitterPanes(m_pane, *m_pImageView);
	UpdateLayout();
	m_splitter.SetSplitterPos(::MulDiv(190, dpiX, 96));
	m_splitter.SetSplitterExtendedStyle(SPLIT_NONINTERACTIVE);

	UIAddToolBar(hWndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);
	UISetCheck(ID_VIEW_SETTINGPANE, 1);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

#include <assert.h>

#include <vector>
#include <algorithm>
#include <cmath>

#include <limits>
#include <stdint.h>

#include "../dct.h"
#include "../Quantizer.h"
#include "../misc.h"
#include "../decode.h"
#include "../encode.h"

#include "../ReadImage/ReadImage.h"
#include "../ReadImage/File.h"

bool bImageLoaded = false;
ImageInfo imageInfo;
unsigned char palettes[256 * 4];
std::vector<unsigned char> in;
std::vector<unsigned char> out;

bool loadImage(LPCTSTR fileName)
{
	FILE* f = _tfopen(fileName, _T("rb"));
	if (!f) {
		return false;
	}
	File fo(f);
	ReadImageInfo(fo, imageInfo);
	size_t width = imageInfo.width;
	size_t height = imageInfo.height;
	assert(imageInfo.bitsPerSample == 8 && imageInfo.samplesPerPixel == 1);
	const size_t size = width * height;

	in.resize(size);
	ReadImageData(fo, &in[0], width, palettes);
	fclose(f);
	for (size_t i=0; i<size; ++i) {
		in[i] = palettes[4 * in[i]];
	}
	return true;
}

#include "setting.h"

size_t encodeDecode(Setting s)
{
	size_t width = imageInfo.width;
	size_t height = imageInfo.height;
	const size_t size = width * height;
	std::vector<int> work(size);
	std::vector<int> work2(size);
	out.resize(size);
	
	const size_t hBlockCount = width / 8 + ((width % 8) ? 1 : 0);
	const size_t vBlockCount = height / 8;// + ((height % 8) ? 1 : 0);
	const size_t totalBlockCount = hBlockCount * vBlockCount;
	
	size_t storageSize = work.size()*4*1.1+600;
	std::vector<unsigned char> work3(storageSize);
	std::vector<unsigned char> work4(storageSize);
	std::vector<unsigned char> encoded(storageSize);
	std::vector<CompressInfo> compressInfos(8);
	
	Quantizer quantizer;
	quantizer.init(s.qp, s.hBlockness, s.vBlockness, s.useQuantMatrix);
	
	size_t totalLen = 0;
	
	// TODO: to design stream byte formats and various JYEIPEGYUU stream classes and implement serialize methods.
	
	// encode
	{
		unsigned char* dest = &encoded[0];
		unsigned char* initialDest = dest;
		
		// TODO: to record stream signature "jyeipegyuu\0"
		// TODO: to record streams
		
		int* pWork = &work[0];
		int* pWork2 = &work2[0];
		
		// TODO: to record quantizer parameter
		
		// TODO: to add encode options such as quantization table.
		encode(quantizer, hBlockCount, vBlockCount, &in[0], width, pWork, width*sizeof(int));
		
		reorderByFrequency(hBlockCount, vBlockCount, pWork, pWork2);
		
		unsigned char enableDCPrediction = s.enableDCprediction;
		*dest++ = enableDCPrediction;
		if (enableDCPrediction) {
			predictEncode(hBlockCount, vBlockCount, pWork2, pWork);
		}
		
		std::vector<unsigned char> signFlags(totalBlockCount*64);
		unsigned char* pSignFlags = &signFlags[0];
		size_t signFlagCount = collectInfos(pWork2, totalBlockCount, pSignFlags, &compressInfos[0]);
		
		// AC係数が0か1だけの場合にそこを纏めて圧縮する。低周波成分はさすがに01だけでは無い事が多い。ただある程度以上の高周波は含んでいないブロックが多い場合がある。
		size_t zeroOneLimit = s.zeroOneLimit;
		std::vector<int> zeroOneInfos(totalBlockCount);
		std::vector<int> allZeroOneInfos(totalBlockCount * 8);
		int* pZeroOneInfos = &zeroOneInfos[0];
		int* pAllZeroOneInfos = &allZeroOneInfos[0];
		
		// quantizing zero one flags
		*dest++ = zeroOneLimit;
		if (zeroOneLimit != 0) {
			findZeroOneInfos(hBlockCount, vBlockCount, pWork2, pZeroOneInfos, pAllZeroOneInfos, zeroOneLimit);
			int encodedSize = totalBlockCount/4;
			Encode(pZeroOneInfos, totalBlockCount, 1, 0, &work3[0], encodedSize);
			*((uint32_t*)dest) = encodedSize;
			dest += 4;
			memcpy(dest, &work3[0], encodedSize); 
			dest += encodedSize;
			
			//encodedSize = totalBlockCount;
			//Encode(pAllZeroOneInfos, totalBlockCount, (256>>zeroOneLimit)-1, 0, &work3[0], encodedSize);
			//*((uint32_t*)dest) = encodedSize;
			//dest += 4;
			//memcpy(dest, &work3[0], encodedSize); 
			//dest += encodedSize;
			
		}else {
			pZeroOneInfos = 0;
		}
		dest += compress(hBlockCount, vBlockCount, pZeroOneInfos, zeroOneLimit, &compressInfos[0], pWork2, (unsigned char*)pWork, &work3[0], &work4[0], dest, encoded.size());
		
		// TODO: to record DCT coefficients sign predictor setting
		
		BitWriter writer(dest+4);
		for (size_t i=0; i<signFlagCount; ++i) {
			writer.putBit(signFlags[i] != 0);
		}
		*((uint32_t*)dest) = writer.nBytes();
		dest += 4;
		dest += writer.nBytes();
		
		totalLen = dest - initialDest;
		
	}
	
	size_t compressedLen = totalLen;
	
	std::fill(work.begin(), work.end(), 0);
	std::fill(work2.begin(), work2.end(), 0);
	std::vector<unsigned char> tmp(encoded.size());
	
	// decode
	{
		unsigned char* src = &encoded[0];
		int* pWork = &work[0];
		int* pWork2 = &work2[0];
		size_t destLen = work2.size();
		
		unsigned char enableDCPrediction = *src++;
		
		// zero one flags
		unsigned char zeroOneLimit = *src++;
		std::vector<int> zeroOneFlags(totalBlockCount*2);
		int* pZeroOneFlags = &zeroOneFlags[0];
		if (zeroOneLimit != 0) {
			uint32_t zeroOneFlagBytes = *(uint32_t*)src;
			src += 4;
			{
				int flagCount = totalBlockCount;
				Decode(src, zeroOneFlagBytes, pZeroOneFlags, flagCount);
				assert(flagCount == totalBlockCount);
			}
			src += zeroOneFlagBytes;
		}else {
			pZeroOneFlags = 0;
		}
		src += decompress(hBlockCount, vBlockCount, pZeroOneFlags, zeroOneLimit, src, compressedLen, &tmp[0], pWork, pWork2, destLen);
		
		// sign flags
		uint32_t signFlagBytes = *(uint32_t*)src;
		src += 4;
		std::vector<unsigned char> signFlags(signFlagBytes * 8);
		unsigned char* pSignFlags = &signFlags[0];
		{
			BitReader reader(src);
			for (size_t i=0; i<signFlagBytes*8; ++i) {
				pSignFlags[i] = reader.getBit();
			}
			size_t pos = 0;
			for (size_t i=0; i<totalBlockCount*64; ++i) {
				int& val = pWork2[i];
				if (val != 0) {
					val = pSignFlags[pos++] ? val : -val;
				}
			}
		}
		
		if (enableDCPrediction) {
			predictDecode(hBlockCount, vBlockCount, pWork2, pWork);
		}
		
		reorderByPosition(hBlockCount, vBlockCount, pWork2, pWork);
		
		decode(quantizer, hBlockCount, vBlockCount, pWork, width*sizeof(int), &out[0], width);
	}
	unsigned char* pOutput = &out[0];
	
	//FILE* of = _tfopen(_T("out.raw"), _T("wb"));
	//fwrite(pOutput, 1, size, of);
	//fclose(of);z
	
	_tprintf(_T("%f%% %d bytes"), (100.0 * compressedLen) / size, compressedLen);

	return compressedLen;
}

LRESULT CMainFrame::OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog dlg(TRUE);
	if (dlg.DoModal() != IDOK) {
		return 0;
	}
	
	if (!loadImage(dlg.m_ofn.lpstrFile)) {
		return 0;
	}
	bImageLoaded = true;
	
	return 0;
}

void CMainFrame::OnSettingChange(const Setting& s)
{
	if (!bImageLoaded) {
		return;
	}
	size_t compressedLen = encodeDecode(s);
	m_pImageView->SetImage(imageInfo.width, imageInfo.height, &out[0]);

	size_t size = imageInfo.width * imageInfo.height;
	TCHAR buff[64];
	_stprintf(buff, _T("%.2f%%  %d bytes"), (100.0 * compressedLen) / size, compressedLen);

	m_wndStatusBar.SetPaneText(1, buff);

}

