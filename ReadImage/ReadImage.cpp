#include "stdafx.h"

#include "ReadImage.h"

#define TRACE printf
#include <assert.h>
#include <stdio.h>

//#include <windows.h>

#define BI_BITFIELDS 3

typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef long LONG;

typedef struct tagBITMAPINFOHEADER {
    DWORD  biSize;
    LONG   biWidth;
    LONG   biHeight;
    WORD   biPlanes;
    WORD   biBitCount;
    DWORD  biCompression;
    DWORD  biSizeImage;
    LONG   biXPelsPerMeter;
    LONG   biYPelsPerMeter;
    DWORD  biClrUsed;
    DWORD  biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagBITMAPFILEHEADER {
  WORD  bfType;
  DWORD bfSize;         /* DWORD (４バイト) 境界にアラインされていない点に注意．*/
  WORD  bfReserved1;
  WORD  bfReserved2;
  DWORD bfOffBits;      /* DWORD (４バイト) 境界にアラインされていない点に注意．*/
} BITMAPFILEHEADER;


bool Read_BITMAPINFOHEADER(IFile& file, BITMAPINFOHEADER& bmih)
{
	size_t fileSize = file.Size();
	if (fileSize <= 54) {
		TRACE("file size <= 54 bytes.");
		return false;
	}
	BITMAPFILEHEADER bmpFileHeader;
	size_t readBytes = 0;
	file.Read(&bmpFileHeader, 14, readBytes);
	assert(readBytes == 14);
	if (bmpFileHeader.bfType != 19778) {
		TRACE("file hedear bfType != 19778");
		return false;
	}
	
	file.Read(&bmih, 40, readBytes);
	if (readBytes != 40) {
		TRACE("bmih shortage.");
		return false;
	}
	
	return true;
}

bool ReadImageInfo_BMP(IFile& file, ImageInfo& info)
{
	BITMAPINFOHEADER bmih;
	if (!Read_BITMAPINFOHEADER(file, bmih)) {
		return false;
	}
	info.width = bmih.biWidth;
	info.height = bmih.biHeight;
	info.bitsPerSample = 8;
	info.samplesPerPixel = 1;
	return true;
}

bool ReadImageData_BMP(IFile& file, unsigned char* dest, int lineOffset, void* palettes)
{
	file.Seek(0, SEEK_SET);
	BITMAPINFOHEADER bmih;
	if (!Read_BITMAPINFOHEADER(file, bmih)) {
		return false;
	}
	size_t bmiColorsLen = 0;
	switch (bmih.biBitCount) {
	case 1:
		bmiColorsLen = 2;
		break;
	case 4:
		bmiColorsLen = 16;
		break;
	case 8:
		bmiColorsLen = 256;
		break;
	case 16:
		TRACE("16 bit BMP not supported.");
		return false;
		break;
	case 32:
		if (bmih.biCompression == BI_BITFIELDS) {
			bmiColorsLen = 3;
		}
	}
	size_t readBytes = 0;
	if (bmiColorsLen) {
		size_t needBytes = /*sizeof(RGBQUAD)*/4*bmiColorsLen;
		file.Read(palettes, needBytes, readBytes);
		if (readBytes != needBytes) {
			TRACE("bmiColors read failed.");
			return false;
		}
	}
	const int lineBytes = ((((bmih.biWidth * bmih.biBitCount) + 31) & ~31) >> 3);
	const int lineIdx = (bmih.biHeight < 0) ? 0 : (bmih.biHeight-1);
	OffsetPtr(dest, lineOffset * lineIdx);
	lineOffset *= (bmih.biHeight < 0) ? 1 : -1;

	const size_t height = abs(bmih.biHeight);
	for (size_t y=0; y<height; ++y) {
		file.Read(dest, lineBytes, readBytes);
		if (readBytes != lineBytes) {
			TRACE("read bytes != lineBytes.");
			return false;
		}
		OffsetPtr(dest, lineOffset);
	}
	
	return true;
}

bool ReadImageInfo(IFile& file, ImageInfo& info)
{
	return ReadImageInfo_BMP(file, info);
}

bool ReadImageData(IFile& file, unsigned char* dest, int lineOffset, void* palettes)
{
	return ReadImageData_BMP(file, dest, lineOffset, palettes);
}

