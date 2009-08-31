
template <typename T>
void gather(
	const T* in, int lineOffsetBytes,
	int values[8][8]
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

template <typename T>
void scatter(
	T* out, int lineOffsetBytes,
	int values[8][8]
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

void cutoff(
	size_t hBlockCount, size_t vBlockCount,
	int* data, int lineOffsetBytes,
	const int table[8][8]
	)
{

	int* srcLine = data;
	for (size_t y=0; y<vBlockCount; ++y) {
		int* workSrcLine = srcLine;
		for (size_t yi=0; yi<8; ++yi) {
			int* pWork = workSrcLine;
			const int* tableLine = table[yi];
			for (size_t x=0; x<hBlockCount; ++x) {
				for (size_t xi=0; xi<8; ++xi) {
					int value = *pWork;
					int newValue = std::min(tableLine[xi], std::abs(value));
					*pWork = (value < 0) ? -newValue : newValue;
					++pWork;
				}
			}
			OffsetPtr(workSrcLine, lineOffsetBytes);
		}
		OffsetPtr(srcLine, lineOffsetBytes * 8);
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
	
	size_t nBytes() const { return src_ - initialSrc_; }
	
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

enum RiceCoderFlag {
	None = 0xF,
};

size_t repeationCompress(
	const unsigned char* src, size_t srcLen,
	unsigned char* tmp
	)
{
	int repeatHist[1024] = {0};
	int repeat = 0;

	BitWriter bitWriter(tmp);
	RiceCoder riceCoder(2);
	
	for (size_t i=0; i<srcLen; ++i) {
		unsigned char d = src[i];
		for (size_t j=0; j<8; ++j) {
			if (d & (1 << (7-j))) {
				++repeat;
			}else {
				riceCoder.Encode(repeat, bitWriter);
				++repeatHist[repeat];
				repeat = 0;
			}
		}
	}
	if (repeat) {
		riceCoder.Encode(repeat, bitWriter);
		++repeatHist[repeat];
		repeat = 0;			
	}
	
	size_t len = bitWriter.nBytes();
	return len;
}

size_t repeationDecompress(
	const unsigned char* src, size_t srcLen,
	unsigned char* dst
	)
{
	RiceCoder riceCoder(2);
	BitReader bitReader(src);
	BitWriter bitWriter(dst);
	while (bitReader.nBytes() <= srcLen) {
		size_t v = riceCoder.Decode(bitReader);
		for (size_t i=0; i<v; ++i) {
			bitWriter.putBit(true);
		}
		bitWriter.putBit(false);
	}
	return bitWriter.nBytes();
}

size_t compressSub(
	ICompressor& compressor,
	int* src, size_t srcLen,
	unsigned char* dest, size_t destLen,
	unsigned char* tmp,
	unsigned char* tmp2
	)
{
	unsigned char* initialDest = dest;
	
	assert(destLen > 4);
	
	int mini = boost::integer_traits<int>::const_max;
	int maxi = boost::integer_traits<int>::const_min;
	
	int hists[1024] = {0};
	BitWriter signFlags(dest+4);
	for (size_t i=0; i<srcLen; ++i) {
		int val = src[i];
		mini = std::min(val, mini);
		maxi = std::max(val, maxi);
		if (val == 0) {
			;
		}else if (val < 0) {
			signFlags.putBit(false);
		}else {
			signFlags.putBit(true);
		}
		val = std::abs(val);
		src[i] = val;
		
		++hists[val];
	}
	
	uint32_t signFlagBytes = signFlags.nBytes();
	*((size_t*)dest) = signFlagBytes;
	
	dest += (4 + signFlagBytes);
	destLen -= (4 + signFlagBytes);
	
	int b = 0;
	size_t max = std::max(maxi, std::abs(mini));
	
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
	}else if (max > 128) {
		b = 2;
	}else if (max > 64) {
		b = 1;
	}else {
		b = 0;
	}
	
	size_t initialCompressedLen = compressor.Compress((const unsigned char*)src, srcLen*4, tmp, -1);
	
	BitWriter bitWriter(tmp2);
	RiceCoder riceCoder(b);
	for (size_t i=0; i<srcLen; ++i) {
		riceCoder.Encode(src[i], bitWriter);
	}
	size_t bytesLen = bitWriter.nBytes();
	
	size_t compressedLen = 0;

	compressedLen = compressor.Compress(tmp2, bytesLen, dest+6, -1);
	unsigned char* dest2 = (compressedLen < initialCompressedLen) ? tmp : (dest+6);
	size_t repeationCompressedLen = repeationCompress(tmp2, bytesLen, dest2);
	
	size_t len = 0;
	if (initialCompressedLen < compressedLen && initialCompressedLen < bytesLen && initialCompressedLen < repeationCompressedLen) {
		*dest++ = 1;
		*dest++ = RiceCoderFlag::None;
		len = initialCompressedLen;
		memcpy(dest+4, tmp, len);
	}else if (compressedLen < bytesLen && compressedLen < repeationCompressedLen) {
		*dest++ = 1;
		*dest++ = b;
		len = compressedLen;
	}else if (bytesLen < repeationCompressedLen) {
		*dest++ = 0;
		*dest++ = b;
		len = bytesLen;
		memcpy(dest+4, tmp2, len);
	}else {
		*dest++ = 2;
		*dest++ = b;
		len = repeationCompressedLen;
		memcpy(dest+4, dest2, len);
	}
	*((size_t*)dest) = len;
	dest += 4;
	dest += len;
	
	size_t destDiff = dest - initialDest;
	printf(
		"[%d %d %d %d %d] %d %d %d\n",
		hists[0], hists[1], hists[2], hists[3], hists[4],
		max, b, destDiff);
	
	return destDiff;
}

size_t compress(
	ICompressor& compressor,
	size_t hBlockCount,
	size_t vBlockCount,
	const int* src,
	int* tmp,
	unsigned char* tmp2,
	unsigned char* tmp3,
	unsigned char* dest, size_t destLen
	)
{
	const int totalBlockCount = static_cast<int>(hBlockCount * vBlockCount);
	const size_t fromWidth = hBlockCount * 8;
	const size_t fromWidth8 = fromWidth * 8;

	const int* from = src;
	int* to = tmp;

	int readPos;
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
	
	// paeth prediction to DC0
	to -= totalBlockCount;
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
		*to = cur - paethPredictor(0, above, 0);
		left = cur;
		upperLeft = above;
		++to;
		int fromPos = fromLineOffset + 8;
		for (size_t x=1; x<hBlockCount; ++x) {
			above = from[fromPos];
			cur = *to;
			*to = cur - paethPredictor(left, above, upperLeft);
			++to;
			left = cur;
			upperLeft = above;
			fromPos += 8;
		}
		fromLineOffset += fromWidth8;
	}

	size_t progress = 0;
	progress += compressSub(compressor, tmp, totalBlockCount, dest+progress, destLen-progress, tmp2, tmp3);
	to = tmp;

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
	progress += compressSub(compressor, tmp, totalBlockCount*3, dest+progress, destLen-progress, tmp2, tmp3);
	to = tmp;

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
	progress += compressSub(compressor, tmp, totalBlockCount*5, dest+progress, destLen-progress, tmp2, tmp3);
	to = tmp;

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
	progress += compressSub(compressor, tmp, totalBlockCount*7, dest+progress, destLen-progress, tmp2, tmp3);
	to = tmp;
	
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
	progress += compressSub(compressor, tmp, totalBlockCount*9, dest+progress, destLen-progress, tmp2, tmp3);
	to = tmp;
	
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
	progress += compressSub(compressor, tmp, totalBlockCount*11, dest+progress, destLen-progress, tmp2, tmp3);
	to = tmp;
	
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
	progress += compressSub(compressor, tmp, totalBlockCount*13, dest+progress, destLen-progress, tmp2, tmp3);
	to = tmp;
	
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
	progress += compressSub(compressor, tmp, totalBlockCount*15, dest+progress, destLen-progress, tmp2, tmp3);
	
	return progress;
}

size_t decompressSub(ICompressor& compressor, const unsigned char* src, unsigned char* tmp, int* dest, size_t destLen)
{
	const unsigned char* initialSrc = src;
	
	size_t signFlagBytes = *(size_t*)src;
	src += 4;
	BitReader signFlags(src);
	src += signFlagBytes;
	
	unsigned char compressFlag = *src++;
	unsigned char b = *src++;
	size_t len = *(size_t*)src;
	src += 4;
	
	size_t len2 = 0;
	switch (compressFlag) {
	case 0:
		memcpy(tmp, src, len);
		len2 = len;
		break;
	case 1:
		len2 = compressor.Decompress(src, len, tmp, -1);
		break;
	case 2:
		len2 = repeationDecompress(src, len, tmp);
		break;
	}
		
	if (b == RiceCoderFlag::None) {
		memcpy(dest, tmp, len2);
		for (size_t i=0; i<len2/4; ++i) {
			int val = dest[i];
			if (val != 0) {
				if (!signFlags.getBit()) {
					dest[i] = -val;
				}
			}
		}

	}else {
		RiceCoder riceCoder(b);
		BitReader bitReader(tmp);
		for (size_t i=0; i<destLen; ++i) {
			int val = riceCoder.Decode(bitReader);
			if (val != 0) {
				if (!signFlags.getBit()) {
					val = -val;
				}
			}
			dest[i] = val;
		}
	}
//	showMinus(dest, dest, destLen);
		
	return 4 + signFlagBytes + + 6 + len;
}

void decompress(
	ICompressor& decompressor,
	size_t hBlockCount,
	size_t vBlockCount,
	const unsigned char* src, size_t srcLen,
	unsigned char* tmp, int* tmp2,
	int* dest, size_t destLen
	)
{
	int* initialDest = dest;
	const unsigned char* initialSrc = src;

	const int totalBlockCount = static_cast<int>(hBlockCount * vBlockCount);
	const size_t toWidth = hBlockCount * 8;
	const size_t toWidth8 = toWidth * 8;
	
	int* from = tmp2;
	int* to = dest;

	int writePos;
	
	src += decompressSub(decompressor, src, tmp, from, totalBlockCount);
	// DC0
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			assert(toPos < destLen);
			to[toPos] = *from++;
			toPos += 8;
		}
		writePos += toWidth8;
	}
	
	// paeth predictor to DC0
	// first row
	int left = *to;
	int above = 0;
	int upperLeft = 0;
	int cur = 0;
	writePos = 8;
	for (size_t i=1; i<hBlockCount; ++i) {
		cur = to[writePos] + left;
		assert(writePos < destLen);
		to[writePos] = cur;
		left = cur;
		writePos += 8;
	}
	int toLineOffset = toWidth8;
	int paeth = 0;
	for (size_t y=1; y<vBlockCount; ++y) {
		above = to[toLineOffset - toWidth8];
		paeth = to[toLineOffset];
		cur = paeth + paethPredictor(0, above, 0);
		assert(toLineOffset < destLen);
		to[toLineOffset] = cur;
		writePos = toLineOffset + 8;
		left = cur;
		upperLeft = above;
		for (size_t x=1; x<hBlockCount; ++x) {
			above = to[writePos - toWidth8];
			paeth = to[writePos];
			cur = paeth + paethPredictor(left, above, upperLeft);
			assert(writePos < destLen);
			to[writePos] = cur;
			left = cur;
			upperLeft = above;
			writePos += 8;
		}
		toLineOffset += toWidth8;
	}
	
	src += decompressSub(decompressor, src, tmp, from, totalBlockCount*3);
	// AC1
	writePos = 0;
	for (size_t y=0; y<vBlockCount; ++y) {
		int toPos = writePos;
		for (size_t x=0; x<hBlockCount; ++x) {
			to[toPos+toWidth*0+1] = *from++;
			to[toPos+toWidth*1+0] = *from++;
			to[toPos+toWidth*1+1] = *from++;
			assert(toPos+toWidth*1+1 < destLen);
			toPos += 8;
		}
		writePos += toWidth8;
	}

	src += decompressSub(decompressor, src, tmp, from, totalBlockCount*5);
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

	src += decompressSub(decompressor, src, tmp, from, totalBlockCount*7);
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

	src += decompressSub(decompressor, src, tmp, from, totalBlockCount*9);
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

	src += decompressSub(decompressor, src, tmp, from, totalBlockCount*11);
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

	src += decompressSub(decompressor, src, tmp, from, totalBlockCount*13);
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

	src += decompressSub(decompressor, src, tmp, from, totalBlockCount*15);
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
			assert(toPos+toWidth*7+7 < destLen);
			toPos += 8;
		}
		writePos += toWidth8;
	}
}
