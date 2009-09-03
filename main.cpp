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
	
	Quantizer quantizer;
	quantizer.init(6*8+1, 0, 0, false);
	
	const size_t hBlockCount = width / 8 + ((width % 8) ? 1 : 0);
	const size_t vBlockCount = height / 8 + ((height % 8) ? 1 : 0);
	const size_t totalBlockCount = hBlockCount * vBlockCount;
	
	encode(quantizer, hBlockCount, vBlockCount, &in[0], width, &work[0], width*sizeof(int));

	if (0) {	
		int cutoff_table[8][8] = {
			-1,-1,-1,-1,-1,-1, 1, 1,
			-1,-1,-1,-1,-1,-1, 1, 1,
			-1,-1,-1,-1,-1,-1, 1, 1,
			-1,-1,-1,-1,-1,-1, 1, 1,
			-1,-1,-1,-1,-1,-1, 1, 1,
			-1,-1,-1,-1,-1,-1, 1, 1,
			 1, 1, 1, 1, 1, 1, 1, 1,
			 1, 1, 1, 1, 1, 1, 1, 1,	
		};
		for (size_t i=0; i<8; ++i) {
			for (size_t j=0; j<8; ++j) {
				if (cutoff_table[i][j] < 0) {
					cutoff_table[i][j] = boost::integer_traits<int>::const_max;
				}
			}
		}
		cutoff(hBlockCount, vBlockCount, &work[0], width*sizeof(int), cutoff_table);
	}
	
	int* pWork = &work[0];
	
	reorderByFrequency(hBlockCount, vBlockCount, &work[0], &work2[0]);
	
	std::vector<unsigned char> zeroOneInfos(totalBlockCount);
	unsigned char* pZeroOneInfos = &zeroOneInfos[0];
	size_t zeroOneLimit = 1;
	findZeroOneInfos(hBlockCount, vBlockCount, &work2[0], &zeroOneInfos[0], zeroOneLimit);
	
	BZip2Compressor compressor;
	size_t compressedLen = 0;
	
	size_t storageSize = work.size()*4*1.1+600;
	std::vector<unsigned char> work3(storageSize);
	std::vector<unsigned char> work4(storageSize);
	std::vector<unsigned char> compressed(storageSize);
	compressedLen = compress(compressor, hBlockCount, vBlockCount, pZeroOneInfos, zeroOneLimit, &work2[0], (unsigned char*)&work[0], &work3[0], &work4[0], &compressed[0], compressed.size());
	
	std::fill(work.begin(), work.end(), 0);
	std::fill(work2.begin(), work2.end(), 0);

	std::vector<unsigned char> tmp(compressed.size());
	decompress(compressor, hBlockCount, vBlockCount, &compressed[0], compressedLen, &tmp[0], &work[0], &work2[0], work2.size());
	
	decode(quantizer, hBlockCount, vBlockCount, &work2[0], width*sizeof(int), &out[0], width);
	unsigned char* pOutput = &out[0];

	//FILE* of = _tfopen(_T("out.raw"), _T("wb"));
	//fwrite(pOutput, 1, size, of);
	//fclose(of);

	_tprintf(_T("%f%% %d bytes"), (100.0 * compressedLen) / size, compressedLen);
	return 0;
}
