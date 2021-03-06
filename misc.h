
#include "RanCode.h"
#include <limits>
//#include "RanCodeAdp.h"

template <typename T, typename T2>
void gather(
	const T* in, int lineOffsetBytes,
	T2 values[8][8]
	)
{
	const T* src = in;
	for (size_t i=0; i<8; ++i) {
		for (size_t j=0; j<8; ++j) {
			values[i][j] = src[j];
		}
		OffsetPtr(src, lineOffsetBytes);
	}
}

template <typename T, typename T2>
void scatter(
	T* out, int lineOffsetBytes,
	T2 values[8][8]
	)
{
	T* dest = out;
	for (size_t i=0; i<8; ++i) {
		for (size_t j=0; j<8; ++j) {
			dest[j] = values[i][j];
		}
		OffsetPtr(dest, lineOffsetBytes);
	}
}

int paethPredictor(int left, int above, int upperLeft)
{
	int initial = left + (above - upperLeft);
	int diffLeft = std::abs(initial - left);
	int diffAbove = std::abs(initial - above);
	int diffUpperLeft = std::abs(initial - upperLeft);
	if (diffLeft <= diffAbove && diffLeft <= diffUpperLeft) {
		return left;
	}else if (diffAbove <= diffUpperLeft) {
		return above;
	}else {
		return upperLeft;
	}
}

int LOCO_IPredictor(int left, int above, int upperLeft)
{
	int minLeftAbove = std::min<int>(left, above);
	int maxLeftAbove = std::max<int>(left, above);
	if (upperLeft > maxLeftAbove) {
		return minLeftAbove;
	}else if (upperLeft < minLeftAbove) {
		return maxLeftAbove;
	}else {
		return left + above - upperLeft;
	}
}

class BitWriter
{
public:
	BitWriter(unsigned char* dest)
		:
		nBits_(0),
		dest_(dest),
		initialDest_(dest),
		counter_(0)
	{
		*dest_ = 0;
	}
	void putBit(bool b)
	{
		int v = b ? 1 : 0;
		(*dest_) |= (v << (7 - counter_));
		++counter_;
		if (counter_ == 8) {
			++dest_;
			*dest_ = 0;
			counter_ = 0;
		}
		++nBits_;
	}
	size_t nBits() const { return nBits_; }
	size_t nBytes() const { return (dest_ - initialDest_) + (counter_ ? 1 : 0); }
private:
	size_t nBits_;
	unsigned char counter_;
	unsigned char* dest_;
	unsigned char* initialDest_;
};

class BitReader
{
public:
	BitReader(const unsigned char* src)
		:
		src_(src),
		initialSrc_(src),
		counter_(0)
	{
	}
	
	bool getBit()
	{
		bool ret = *src_ & (1 << (7-counter_));
		++counter_;
		if (counter_ == 8) {
			counter_ = 0;
			++src_;
		}
		return ret;
	}
	
	size_t nBytes() const { return (src_ - initialSrc_) + (counter_ ? 1 : 0); }
	
private:
	unsigned char counter_;
	const unsigned char* src_;
	const unsigned char* initialSrc_;
};

class RiceCoder
{
public:
	RiceCoder(size_t shift)
		:
		shift_(shift)
	{
	}
	
	void Encode(size_t value, BitWriter& writer)
	{
		size_t p = value >> shift_;
		for (size_t i=0; i<p; ++i) {
			writer.putBit(false);
		}
		writer.putBit(true);
		int v = 1;
		for (size_t i=0; i<shift_; ++i) {
			writer.putBit(value & v);
			v <<= 1;
		}
	}

	size_t Decode(BitReader& reader)
	{
		size_t q = 0;
		while (!reader.getBit()) ++q;
		size_t ret = q << shift_;
		for (size_t i=0; i<shift_; ++i) {
			if (reader.getBit()) {
				ret += 1 << i;
			}
		}
		return ret;
	}
	
private:
	size_t shift_;
};

void findZeroOneInfos(
	size_t hBlockCount,
	size_t vBlockCount,
	const int* src,
	int* dst,
	int* dst2,
	size_t limit
	)
{
	const int totalBlockCount = static_cast<int>(hBlockCount * vBlockCount);
	const int* from = src;
	
	// pass DC0
	from += totalBlockCount;
	
	std::fill(dst, dst+totalBlockCount, 1);
	std::fill(dst2, dst2+totalBlockCount*8, 0);
	
	for (size_t i=1; i<8; ++i) {
		unsigned char blockSize = 1 + i*2;
		if (i<limit) {
			from += blockSize * totalBlockCount;
			continue;
		}
		int* arr = dst2 + totalBlockCount * i;
		for (size_t j=0; j<totalBlockCount; ++j) {
			for (size_t k=0; k<blockSize; ++k) {
				int v = abs(from[k]);
				if (v > 1) {
					dst[j] = 0;
					arr[j] = -1;
					break;
				}
			}
			from += blockSize;
		}
		int hoge = 0;
	}
	
	for (size_t i=0; i<totalBlockCount; ++i) {
		int sum = 0;
		for (size_t j=1; j<8; ++j) {
			sum |= std::abs(dst2[j*totalBlockCount + i]) << (j-limit);
		}
		dst2[i] = sum;
	}
	int hoge = 0;
}

struct CompressInfo
{
	int mini;
	int maxi;
	size_t max;
	int hists[1024];
	int phists[1024];
	int mhists[1024];
	int zeroRepeatHist[1024*128];
	
	size_t zeroOneOnlyAreaHist[2];
	size_t nonZeroOneOnlyAreaHist[1024];

	
	size_t srcCount;
	size_t signFlagsCount;
	size_t initialCompressedLen;
	size_t totalLen;
};

static const size_t DC_BLOCKCOUNT_BORDER = 8192;

#define COMBINE_ZERO_FLAGS 0	// 各周波数の01領域をまとめて圧縮するテスト。逆に圧縮率が落ちてしまう。。

#if COMBINE_ZERO_FLAGS
std::vector<size_t> zeroFlagCompressedSizes;
std::vector<int> zeroFlagValues;
#endif

size_t compressSub(
	int* src,
	const int* pZeroOneInfos,
	size_t zeroOneFlag,
	size_t blockCount, size_t blockSize,
	unsigned char* dest, size_t destLen,
	unsigned char* tmp,
	unsigned char* tmp2,
	unsigned char* tmp3,
	CompressInfo& cinfo
	)
{
	unsigned char* initialDest = dest;
	size_t srcCount = blockCount * blockSize;
	
	assert(destLen > 4);
	
	int mini = cinfo.mini;
	int maxi = cinfo.maxi;
	size_t max = std::max<int>(maxi, std::abs(mini));
	
	int* hists = cinfo.hists;
	int* phists = cinfo.phists;
	int* mhists = cinfo.mhists;
	
	int initialCompressedLen = 0;
	
	// DC成分の記録数が少ない場合はRangeCoderではなくGolomb-Rice符号を使う
	//（成分数が少ない場合にRangeCoderの方が圧縮率が低いのは、エントロピの記録に容量を食ってしまう為かも）
	if (blockSize == 1 && blockCount < DC_BLOCKCOUNT_BORDER) {
		int b = 0;
		
		if (max > 2048) {
			b = 7;
		}else if (max > 1024) {
			b = 6;
		}else if (max > 512) {
			b = 5;
		}else if (max > 256) {
			b = 4;
		}else if (max > 128+64) {
			b = 3;
		}else if (max > 96) {
			b = 2;
		}else if (max > 32) {
			b = 1;
		}else {
			b = 0;
		}
		
		BitWriter bitWriter(tmp);
		RiceCoder riceCoder(b);
		int* from = src;
		for (size_t i=0; i<blockCount; ++i) {
			for (size_t j=0; j<blockSize; ++j) {
				riceCoder.Encode(*from++, bitWriter);	
			}
		}	
		initialCompressedLen = bitWriter.nBytes();
		
		*dest++ = 0;
		*dest++ = b;
		memcpy(dest+4, tmp, initialCompressedLen);
		*((size_t*)dest) = initialCompressedLen;
		dest += 4;
		dest += initialCompressedLen;
		
	}else {
		initialCompressedLen = srcCount * 4;
		Encode(src, srcCount, max, 0, tmp, initialCompressedLen);
		
		int encodedValueSizes[2];
		int* from = src;
		if (pZeroOneInfos) {
			std::vector<int> values;
			for (size_t i=0; i<blockCount; ++i) {
	//			if (!(pZeroOneInfos[i] & zeroOneFlag)) {
				if (pZeroOneInfos[i]) {
					for (size_t j=0; j<blockSize; ++j) {
						int val = src[i*blockSize+j];
						assert(val < 2);
						cinfo.zeroOneOnlyAreaHist[val]++;
						values.push_back(val);
					}
				}
			}
			encodedValueSizes[0] = blockCount*blockSize;
			Encode(&values[0], values.size(), 1, 0, tmp2, encodedValueSizes[0]);
#if COMBINE_ZERO_FLAGS
			zeroFlagValues.insert(zeroFlagValues.end(), values.begin(), values.end());
			zeroFlagCompressedSizes.push_back(encodedValueSizes[0]);
#endif			
			values.clear();
			int maxV = 0;
			for (size_t i=0; i<blockCount; ++i) {
	//			if ((pZeroOneInfos[i] & zeroOneFlag)) {
				if (!pZeroOneInfos[i]) {
					for (size_t j=0; j<blockSize; ++j) {
						int val = src[i*blockSize+j];
						maxV = std::max<int>(maxV, val);
						cinfo.nonZeroOneOnlyAreaHist[val]++;
						values.push_back(val);	
					}
				}
			}
			encodedValueSizes[1] = blockCount*blockSize;
			Encode(&values[0], values.size(), maxV, 0, tmp3, encodedValueSizes[1]);
	/*
			{
				int zeroRepeat = 0;
				int zeroRepeatHist[1024] = {0};
				int repeatMax = 0;
				int val = 0;
				int valMax = 0;
				std::vector<int> zeroRepeatRecs;
				std::vector<int> nonZeroRecs;
				for (size_t i=0; i<values.size(); ++i) {
					val = values[i];
					if (val == 0) {
						++zeroRepeat;
					}else {
						zeroRepeatRecs.push_back(zeroRepeat);
						++zeroRepeatHist[zeroRepeat];
						repeatMax = std::max(repeatMax, zeroRepeat);
						zeroRepeat = 0;
						nonZeroRecs.push_back(val);
						valMax = std::max(valMax, val);
					}
				}
				if (zeroRepeat) {
					zeroRepeatRecs.push_back(zeroRepeat);
					++zeroRepeatHist[zeroRepeat];
					repeatMax = std::max(repeatMax, zeroRepeat);
					zeroRepeat = 0;
					nonZeroRecs.push_back(val);
					valMax = std::max(valMax, val);
				}
				std::vector<int> newRecs;
				int nonZeroRefPos = 0;
				for (size_t i=0; i<zeroRepeatRecs.size(); ++i) {
					int v = zeroRepeatRecs[i];
					if (v) {
						newRecs.push_back(v);
					}
					newRecs.push_back(repeatMax + nonZeroRecs[nonZeroRefPos++]);
				}
				int encodedSize1 = blockCount*blockSize;
				int encodedSize2 = blockCount*blockSize;
				std::vector<unsigned char> recbuff(encodedSize1);
				Encode(&zeroRepeatRecs[0], zeroRepeatRecs.size(), repeatMax, 0, &recbuff[0], encodedSize1);
				Encode(&nonZeroRecs[0], nonZeroRecs.size(), maxV, 1, &recbuff[0], encodedSize2);
				int hoget = 0;
			}
	*/
			
			int totalSize = encodedValueSizes[0] + encodedValueSizes[1];
			size_t len = 0;
			if (totalSize <= initialCompressedLen) {
				*dest++ = 3;
				
				for (size_t i=0; i<2; ++i) {
					*((size_t*)dest) = encodedValueSizes[i];
					dest += 4;
					memcpy(dest, ((i==0)?tmp2:tmp3), encodedValueSizes[i]);
					dest += encodedValueSizes[i];
				}
				
				goto label_end;
			}

		}
		
		*dest++ = 1;
		memcpy(dest+4, tmp, initialCompressedLen);
		*((size_t*)dest) = initialCompressedLen;
		dest += 4;
		dest += initialCompressedLen;
		
	}
		
label_end:	
	size_t destDiff = dest - initialDest;
	
	cinfo.srcCount = srcCount;
	cinfo.mini = mini;
	cinfo.maxi = maxi;
	cinfo.max = max;
	cinfo.initialCompressedLen = initialCompressedLen;
	cinfo.totalLen = destDiff;
	
	return destDiff;
}

void predictEncode(
	size_t hBlockCount,
	size_t vBlockCount,
	int* src,
	int* tmp
	)
{
	const int totalBlockCount = static_cast<int>(hBlockCount * vBlockCount);
	
	int* from = src;
	int* to = tmp;
	std::copy(from, from+totalBlockCount, to);
	// first row
	int left = *to;
	int above = 0;
	int upperLeft = 0;
	int cur = 0;
	++to;
	for (size_t i=1; i<hBlockCount; ++i) {
		cur = *to;
		*to = cur - left;
		++to;
		left = cur;
	}
	int fromLineOffset = 0;
	for (size_t y=1; y<vBlockCount; ++y) {
		cur = *to;
		above = from[fromLineOffset];
		*to = cur - above;
		left = cur;
		upperLeft = above;
		++to;
		int fromPos = fromLineOffset + 1;
		for (size_t x=1; x<hBlockCount; ++x) {
			above = from[fromPos];
			cur = *to;
			*to = cur - LOCO_IPredictor(left, above, upperLeft);
			++to;
			left = cur;
			upperLeft = above;
			++fromPos;
		}
		fromLineOffset += hBlockCount;
	}
	std::copy(tmp, tmp+totalBlockCount, src);
}

void predictDecode(
	size_t hBlockCount,
	size_t vBlockCount,
	int* src,
	int* tmp
	)
{
	const int totalBlockCount = static_cast<int>(hBlockCount * vBlockCount);
	
	int* to = tmp;
	std::copy(src,src+totalBlockCount, to);
	
	// paeth predictor to DC0
	// first row
	int left = *to;
	int above = 0;
	int upperLeft = 0;
	int cur = 0;

	int writePos;
	writePos = 1;
	for (size_t i=1; i<hBlockCount; ++i) {
		cur = to[writePos] + left;
		to[writePos] = cur;
		left = cur;
		++writePos;
	}
	int toLineOffset = hBlockCount;
	int diff = 0;
	for (size_t y=1; y<vBlockCount; ++y) {
		above = to[toLineOffset - hBlockCount];
		diff = to[toLineOffset];
		cur = diff + above;
		to[toLineOffset] = cur;
		writePos = toLineOffset + 1;
		left = cur;
		upperLeft = above;
		for (size_t x=1; x<hBlockCount; ++x) {
			above = to[writePos - hBlockCount];
			diff = to[writePos];
			cur = diff + LOCO_IPredictor(left, above, upperLeft);
			to[writePos] = cur;
			left = cur;
			upperLeft = above;
			++writePos;
		}
		toLineOffset += hBlockCount;
	}

	std::copy(to, to+totalBlockCount, src);

}

size_t collectInfos(
	int* src,
	size_t blockCount,
	unsigned char* signFlags,
	CompressInfo compressInfos[8]
	)
{
	size_t count = 0;
	int* from = src;
	for (size_t i=0; i<8; ++i) {
		size_t blockSize = 1 + i * 2;
		CompressInfo& ci = compressInfos[i];
		int oldCount = count;
		int mini = std::numeric_limits<int>::max();
		int maxi = std::numeric_limits<int>::min();
		
		int* hists = ci.hists;
		int* phists = ci.phists;
		int* mhists = ci.mhists;
		int zeroRepeat = 0;
		int* zeroRepeatHist = ci.zeroRepeatHist;
		size_t srcCount = blockCount * blockSize;
		for (size_t j=0; j<srcCount; ++j) {
			int val = from[j];
			mini = std::min<int>(val, mini);
			maxi = std::max<int>(val, maxi);
			if (val == 0) {
				++zeroRepeat;
			}else {
				signFlags[count++] = (val > 0) ? 255 : 0;
				++zeroRepeatHist[zeroRepeat];
				zeroRepeat = 0;
			}
			if (val >= 0) {
				++phists[val];
			}else {
				++mhists[-val];
			}
			val = std::abs(val);
			from[j] = val;
			
			++hists[val];
		}
		if (zeroRepeat) {
			++zeroRepeatHist[zeroRepeat];
			zeroRepeat = 0;			
		}
		ci.mini = mini;
		ci.maxi = maxi;
		ci.max = std::max<int>(maxi, std::abs(mini));
		ci.signFlagsCount = count - oldCount;
		from += srcCount;
	}
	return count;
}

size_t compress(
	size_t hBlockCount,
	size_t vBlockCount,
	const int* pZeroOneInfos,
	size_t zeroOneLimit,
	CompressInfo compressInfos[8],
	int* src,
	unsigned char* tmp1,
	unsigned char* tmp2,
	unsigned char* tmp3,
	unsigned char* dest, size_t destLen
	)
{
	const int totalBlockCount = static_cast<int>(hBlockCount * vBlockCount);
	
	int* from = src;
	size_t progress = 0;
	
	for (size_t i=0; i<8; ++i) {
		const int* p01 = (i < zeroOneLimit) ? 0 : pZeroOneInfos;
		size_t blockSize = 1 + i * 2;
		progress += compressSub(from, p01, 1<<(i-zeroOneLimit), totalBlockCount, blockSize, dest+progress, destLen-progress, tmp1, tmp2, tmp3, compressInfos[i]);
		from += totalBlockCount * blockSize;
	}
	
#if COMBINE_ZERO_FLAGS
	std::vector<unsigned char> tmp(10000);
	int compressedSize = tmp.size();
	Encode(&zeroFlagValues[0], zeroFlagValues.size(), 1, 0, &tmp[0], compressedSize);
#endif

	return progress;
}

size_t decompressSub(
	const unsigned char* src,
	const int* pZeroOneInfos,
	unsigned char* tmp,
	int* tmp2,
	int* dest,
	size_t totalBlockCount,
	size_t blockSize)
{
	size_t destLen = totalBlockCount * blockSize;

	const unsigned char* initialSrc = src;
	
	unsigned char compressFlag = *src++;
	
	if (blockSize == 1 && totalBlockCount < DC_BLOCKCOUNT_BORDER) {
		unsigned char b = *src++;
		
		size_t len = *(size_t*)src;
		src += 4;
		
		RiceCoder riceCoder(b);
		BitReader bitReader(src);
		for (size_t i=0; i<destLen; ++i) {
			dest[i] = riceCoder.Decode(bitReader);
		}
		
		assert(len == bitReader.nBytes());
		src += len;

	}else {
		if (compressFlag != 3) {
			size_t len = *(size_t*)src;
			src += 4;
			
			int len2 = 0;
			switch (compressFlag) {
			case 1:
				len2 = destLen*4;
				Decode((unsigned char*)src, len, (int*)dest, len2);
				break;
			default:
				assert(false);
			}
			src += len;
		}else {
			size_t sizes[2] = {0};
			int resultSize = destLen * 4;
			
			sizes[0] = *(size_t*)src;
			src += 4;
			memcpy(tmp, src, sizes[0]);
			src += sizes[0];
			Decode(tmp, sizes[0], tmp2, resultSize);
			
			size_t count = 0;
			for (size_t i=0; i<totalBlockCount; ++i) {
				if (pZeroOneInfos[i]) {
					for (size_t j=0; j<blockSize; ++j) {
						dest[i*blockSize+j] = tmp2[count++];
					}			
				}
			}
			
			sizes[1] = *(size_t*)src;
			src += 4;
			memcpy(tmp, src, sizes[1]);
			src += sizes[1];
			Decode(tmp, sizes[1], tmp2, resultSize);
			
			count = 0;
			for (size_t i=0; i<totalBlockCount; ++i) {
				if (!pZeroOneInfos[i]) {
					for (size_t j=0; j<blockSize; ++j) {
						dest[i*blockSize+j] = tmp2[count++];
					}			
				}
			}			
							
		}
	}
	return src - initialSrc;
}

void reorderByFrequency(
	size_t hBlockCount,
	size_t vBlockCount,
	const int* src,
	int* dst
	)
{
	const size_t fromWidth = hBlockCount * 8;
	const size_t fromWidth8 = fromWidth * 8;
	
	int readPos;
	const int* from = src;
	int* to = dst;
	
	// DC0
	readPos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int fromPos = readPos;
		for (size_t x=0; x<hBlockCount; ++x) {
			*to++ = from[fromPos];
			fromPos += 8;
		}
		readPos += fromWidth8;
	}
	
	// AC1
	readPos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int fromPos = readPos;
		for (size_t x=0; x<hBlockCount; ++x) {
			*to++ = from[fromPos+fromWidth*0+1];
			*to++ = from[fromPos+fromWidth*1+0];
			*to++ = from[fromPos+fromWidth*1+1];
			fromPos += 8;
		}
		readPos += fromWidth8;
	}
	
	// AC2
	readPos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int fromPos = readPos;
		for (size_t x=0; x<hBlockCount; ++x) {
			*to++ = from[fromPos+fromWidth*0+2];
			*to++ = from[fromPos+fromWidth*1+2];
			*to++ = from[fromPos+fromWidth*2+0];
			*to++ = from[fromPos+fromWidth*2+1];
			*to++ = from[fromPos+fromWidth*2+2];
			fromPos += 8;
		}
		readPos += fromWidth8;
	}

	// AC3
	readPos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int fromPos = readPos;
		for (size_t x=0; x<hBlockCount; ++x) {
			*to++ = from[fromPos+fromWidth*0+3];
			*to++ = from[fromPos+fromWidth*1+3];
			*to++ = from[fromPos+fromWidth*2+3];
			*to++ = from[fromPos+fromWidth*3+0];
			*to++ = from[fromPos+fromWidth*3+1];
			*to++ = from[fromPos+fromWidth*3+2];
			*to++ = from[fromPos+fromWidth*3+3];
			fromPos += 8;
		}
		readPos += fromWidth8;
	}

	// AC4
	readPos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int fromPos = readPos;
		for (size_t x=0; x<hBlockCount; ++x) {
			*to++ = from[fromPos+fromWidth*0+4];
			*to++ = from[fromPos+fromWidth*1+4];
			*to++ = from[fromPos+fromWidth*2+4];
			*to++ = from[fromPos+fromWidth*3+4];
			*to++ = from[fromPos+fromWidth*4+0];
			*to++ = from[fromPos+fromWidth*4+1];
			*to++ = from[fromPos+fromWidth*4+2];
			*to++ = from[fromPos+fromWidth*4+3];
			*to++ = from[fromPos+fromWidth*4+4];
			fromPos += 8;
		}
		readPos += fromWidth8;
	}


	// AC5
	readPos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int fromPos = readPos;
		for (size_t x=0; x<hBlockCount; ++x) {
			*to++ = from[fromPos+fromWidth*0+5];
			*to++ = from[fromPos+fromWidth*1+5];
			*to++ = from[fromPos+fromWidth*2+5];
			*to++ = from[fromPos+fromWidth*3+5];
			*to++ = from[fromPos+fromWidth*4+5];
			*to++ = from[fromPos+fromWidth*5+0];
			*to++ = from[fromPos+fromWidth*5+1];
			*to++ = from[fromPos+fromWidth*5+2];
			*to++ = from[fromPos+fromWidth*5+3];
			*to++ = from[fromPos+fromWidth*5+4];
			*to++ = from[fromPos+fromWidth*5+5];
			fromPos += 8;
		}
		readPos += fromWidth8;
	}

	// AC6
	readPos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int fromPos = readPos;
		for (size_t x=0; x<hBlockCount; ++x) {
			*to++ = from[fromPos+fromWidth*0+6];
			*to++ = from[fromPos+fromWidth*1+6];
			*to++ = from[fromPos+fromWidth*2+6];
			*to++ = from[fromPos+fromWidth*3+6];
			*to++ = from[fromPos+fromWidth*4+6];
			*to++ = from[fromPos+fromWidth*5+6];
			*to++ = from[fromPos+fromWidth*6+0];
			*to++ = from[fromPos+fromWidth*6+1];
			*to++ = from[fromPos+fromWidth*6+2];
			*to++ = from[fromPos+fromWidth*6+3];
			*to++ = from[fromPos+fromWidth*6+4];
			*to++ = from[fromPos+fromWidth*6+5];
			*to++ = from[fromPos+fromWidth*6+6];
			fromPos += 8;
		}
		readPos += fromWidth8;
	}

	// AC7
	readPos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int fromPos = readPos;
		for (size_t x=0; x<hBlockCount; ++x) {
			*to++ = from[fromPos+fromWidth*0+7];
			*to++ = from[fromPos+fromWidth*1+7];
			*to++ = from[fromPos+fromWidth*2+7];
			*to++ = from[fromPos+fromWidth*3+7];
			*to++ = from[fromPos+fromWidth*4+7];
			*to++ = from[fromPos+fromWidth*5+7];
			*to++ = from[fromPos+fromWidth*6+7];
			*to++ = from[fromPos+fromWidth*7+0];
			*to++ = from[fromPos+fromWidth*7+1];
			*to++ = from[fromPos+fromWidth*7+2];
			*to++ = from[fromPos+fromWidth*7+3];
			*to++ = from[fromPos+fromWidth*7+4];
			*to++ = from[fromPos+fromWidth*7+5];
			*to++ = from[fromPos+fromWidth*7+6];
			*to++ = from[fromPos+fromWidth*7+7];
			fromPos += 8;
		}
		readPos += fromWidth8;
	}
	
}

void reorderByPosition(
	size_t hBlockCount,
	size_t vBlockCount,
	const int* src,
	int* dest
	)
{
	const size_t toWidth = hBlockCount * 8;
	const size_t toWidth8 = toWidth * 8;	
	
	const int* from = src;
	int* to = dest;
	int writePos = 0;
	
	// DC0
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			to[toPos] = *from++;
			toPos += 8;
		}
		writePos += toWidth8;
	}
	
	// AC1
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			to[toPos+toWidth*0+1] = *from++;
			to[toPos+toWidth*1+0] = *from++;
			to[toPos+toWidth*1+1] = *from++;
			toPos += 8;
		}
		writePos += toWidth8;
	}

	// AC2
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			to[toPos+toWidth*0+2] = *from++;
			to[toPos+toWidth*1+2] = *from++;
			to[toPos+toWidth*2+0] = *from++;
			to[toPos+toWidth*2+1] = *from++;
			to[toPos+toWidth*2+2] = *from++;
			toPos += 8;
		}
		writePos += toWidth8;
	}

	// AC3
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			to[toPos+toWidth*0+3] = *from++;
			to[toPos+toWidth*1+3] = *from++;
			to[toPos+toWidth*2+3] = *from++;
			to[toPos+toWidth*3+0] = *from++;
			to[toPos+toWidth*3+1] = *from++;
			to[toPos+toWidth*3+2] = *from++;
			to[toPos+toWidth*3+3] = *from++;
			toPos += 8;
		}
		writePos += toWidth8;
	}

	// AC4
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			to[toPos+toWidth*0+4] = *from++;
			to[toPos+toWidth*1+4] = *from++;
			to[toPos+toWidth*2+4] = *from++;
			to[toPos+toWidth*3+4] = *from++;
			to[toPos+toWidth*4+0] = *from++;
			to[toPos+toWidth*4+1] = *from++;
			to[toPos+toWidth*4+2] = *from++;
			to[toPos+toWidth*4+3] = *from++;
			to[toPos+toWidth*4+4] = *from++;
			toPos += 8;
		}
		writePos += toWidth8;
	}

	// AC5
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			to[toPos+toWidth*0+5] = *from++;
			to[toPos+toWidth*1+5] = *from++;
			to[toPos+toWidth*2+5] = *from++;
			to[toPos+toWidth*3+5] = *from++;
			to[toPos+toWidth*4+5] = *from++;
			to[toPos+toWidth*5+0] = *from++;
			to[toPos+toWidth*5+1] = *from++;
			to[toPos+toWidth*5+2] = *from++;
			to[toPos+toWidth*5+3] = *from++;
			to[toPos+toWidth*5+4] = *from++;
			to[toPos+toWidth*5+5] = *from++;
			toPos += 8;
		}
		writePos += toWidth8;
	}

	// AC6
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			to[toPos+toWidth*0+6] = *from++;
			to[toPos+toWidth*1+6] = *from++;
			to[toPos+toWidth*2+6] = *from++;
			to[toPos+toWidth*3+6] = *from++;
			to[toPos+toWidth*4+6] = *from++;
			to[toPos+toWidth*5+6] = *from++;
			to[toPos+toWidth*6+0] = *from++;
			to[toPos+toWidth*6+1] = *from++;
			to[toPos+toWidth*6+2] = *from++;
			to[toPos+toWidth*6+3] = *from++;
			to[toPos+toWidth*6+4] = *from++;
			to[toPos+toWidth*6+5] = *from++;
			to[toPos+toWidth*6+6] = *from++;
			toPos += 8;
		}
		writePos += toWidth8;
	}

	// AC7
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			to[toPos+toWidth*0+7] = *from++;
			to[toPos+toWidth*1+7] = *from++;
			to[toPos+toWidth*2+7] = *from++;
			to[toPos+toWidth*3+7] = *from++;
			to[toPos+toWidth*4+7] = *from++;
			to[toPos+toWidth*5+7] = *from++;
			to[toPos+toWidth*6+7] = *from++;
			to[toPos+toWidth*7+0] = *from++;
			to[toPos+toWidth*7+1] = *from++;
			to[toPos+toWidth*7+2] = *from++;
			to[toPos+toWidth*7+3] = *from++;
			to[toPos+toWidth*7+4] = *from++;
			to[toPos+toWidth*7+5] = *from++;
			to[toPos+toWidth*7+6] = *from++;
			to[toPos+toWidth*7+7] = *from++;
			toPos += 8;
		}
		writePos += toWidth8;
	}
	
}

size_t decompress(
	size_t hBlockCount,
	size_t vBlockCount,
	const int* pZeroOneInfos,
	size_t zeroOneLimit,
	const unsigned char* src, size_t srcLen,
	unsigned char* tmp, int* tmp2,
	int* dest, size_t destLen
	)
{
	const int totalBlockCount = static_cast<int>(hBlockCount * vBlockCount);
	
	const unsigned char* from = src;
	int* to = dest;
	
	for (size_t i=0; i<8; ++i) {
		const int* p01 = (i < zeroOneLimit) ? 0 : pZeroOneInfos;
		size_t blockSize = 1 + i * 2;
		from += decompressSub(from, p01, tmp, tmp2, to, totalBlockCount, blockSize);
		to += totalBlockCount * blockSize;
	}
	
	return from - src;
}

