#include "stdafx.h"

#include <assert.h>

#include "dct.h"
#include "Quantizer.h"

#include <vector>
#include <algorithm>
#include <cmath>

#include <boost/integer_traits.hpp>
using namespace boost;
#include <boost/cstdint.hpp>

#include "Compressor/BZip2Compressor.h"
#include "misc.h"
#include "decode.h"
#include "encode.h"

#include "ReadImage/ReadImage.h"
#include "ReadImage/File.h"

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2) {
		_tprintf(_T("specify filename\n"));
		return 1;
	}
	
	FILE* f = _tfopen(argv[1], _T("rb"));
	if (!f) {
		_tprintf(_T("failed to open file : %s\n"), argv[1]);
		return 1;
	}
	File fo(f);
	ImageInfo imageInfo;
	ReadImageInfo(fo, imageInfo);
	
	size_t width = imageInfo.width;
	size_t height = imageInfo.height;
	assert(imageInfo.bitsPerSample == 8 && imageInfo.samplesPerPixel == 1);
	const size_t size = width * height;
	std::vector<unsigned char> in(size);
	std::vector<int> work(size);
	std::vector<int> work2(size);
	std::vector<unsigned char> out(size);
	
	unsigned char palettes[256 * 4];
	ReadImageData(fo, &in[0], width, palettes);
	fclose(f);
	
	for (size_t i=0; i<size; ++i) {
		in[i] = palettes[4 * in[i]];
	}
	
	const size_t hBlockCount = width / 8 + ((width % 8) ? 1 : 0);
	const size_t vBlockCount = height / 8 + ((height % 8) ? 1 : 0);
	const size_t totalBlockCount = hBlockCount * vBlockCount;
	
	BZip2Compressor compressor;
	
	size_t storageSize = work.size()*4*1.1+600;
	std::vector<unsigned char> work3(storageSize);
	std::vector<unsigned char> work4(storageSize);
	std::vector<unsigned char> encoded(storageSize);
	CompressInfo compressInfos[8] = {0};
	
	Quantizer quantizer;
	quantizer.init(6*8+0, 0, 0, true);
	
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
		
		// TODO: to add option to disable paeth prediction
		unsigned char enablePaethPrediction = 1;
		*dest++ = enablePaethPrediction;
		if (enablePaethPrediction) {
			paethPredictEncode(hBlockCount, vBlockCount, pWork2, pWork);
		}
		
		std::vector<unsigned char> signFlags(totalBlockCount*64);
		unsigned char* pSignFlags = &signFlags[0];
		size_t signFlagCount = collectInfos(pWork2, totalBlockCount, pSignFlags, compressInfos);
		
		size_t zeroOneLimit = 2;
		std::vector<unsigned char> zeroOneInfos(totalBlockCount);
		unsigned char* pZeroOneInfos = &zeroOneInfos[0];
		
		// TODO: to record quantizing zero one limit setting
		
		// quantizing zero one flags
		*dest++ = zeroOneLimit;
		if (zeroOneLimit != 0) {
			findZeroOneInfos(hBlockCount, vBlockCount, pWork2, &zeroOneInfos[0], zeroOneLimit);
			BitWriter bw(dest+4);
			for (size_t i=0; i<totalBlockCount; ++i) {
				bw.putBit(pZeroOneInfos[i]);
			}
			*((uint32_t*)dest) = bw.nBytes();
			dest += 4;
			dest += bw.nBytes();
		}else {
			pZeroOneInfos = 0;
		}
		dest += compress(compressor, hBlockCount, vBlockCount, pZeroOneInfos, zeroOneLimit, compressInfos, pWork2, (unsigned char*)pWork, &work3[0], &work4[0], dest, encoded.size());
		
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
		
		unsigned char enablePaethPrediction = *src++;
		
		// zero one flags
		unsigned char zeroOneLimit = *src++;
		std::vector<unsigned char> zeroOneFlags(totalBlockCount);
		unsigned char* pZeroOneFlags = &zeroOneFlags[0];
		if (zeroOneLimit != 0) {
			uint32_t zeroOneFlagBytes = *(uint32_t*)src;
			src += 4;
			{
				BitReader reader(src);
				assert(totalBlockCount <= zeroOneFlagBytes*8);
				for (size_t i=0; i<zeroOneFlagBytes*8; ++i) {
					zeroOneFlags[i] = reader.getBit();
				}
			}
			src += zeroOneFlagBytes;
		}else {
			pZeroOneFlags = 0;
		}
		src += decompress(compressor, hBlockCount, vBlockCount, pZeroOneFlags, zeroOneLimit, src, compressedLen, &tmp[0], pWork, pWork2, destLen);
		
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
		
		if (enablePaethPrediction) {
			paethPredictDecode(hBlockCount, vBlockCount, pWork2, pWork);
		}
		
		reorderByPosition(hBlockCount, vBlockCount, pWork2, pWork);
		
		decode(quantizer, hBlockCount, vBlockCount, pWork, width*sizeof(int), &out[0], width);
	}
	unsigned char* pOutput = &out[0];
	
	//FILE* of = _tfopen(_T("out.raw"), _T("wb"));
	//fwrite(pOutput, 1, size, of);
	//fclose(of);
	
	_tprintf(_T("%f%% %d bytes"), (100.0 * compressedLen) / size, compressedLen);
	return 0;
}
