
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
private:
	size_t nBits_;
	unsigned char counter_;
	unsigned char* dest_;
};

class BitReader
{
public:
	BitReader(const unsigned char* src)
		:
		src_(src),
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
private:
	unsigned char counter_;
	const unsigned char* src_;
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
	None = 0xFF,
};

size_t compressSub(
	ICompressor& compressor,
	int* src, size_t srcLen,
	unsigned char* dest, size_t destLen,
	unsigned char* tmp, size_t tmpLen
	)
{
	unsigned char* initialDest = dest;
	
	assert(destLen > 4);

	int mini = boost::integer_traits<int>::const_max;
	int maxi = boost::integer_traits<int>::const_min;
//	int table[4096] = {0};

	BitWriter signFlags(dest+4);
	for (size_t i=0; i<srcLen; ++i) {
		int val = src[i];
		if (val == 0) {
		}else if (val < 0) {
			signFlags.putBit(false);
		}else {
			signFlags.putBit(true);
		}
		val = std::abs(val);
		maxi = std::max(val, maxi);
		src[i] = val;
//		++table[val];
	}
	size_t signFlagCount = signFlags.nBits();
	size_t signFlagBytes = signFlagCount/8 + ((signFlagCount%8) ? 1 : 0);
	*((size_t*)dest) = signFlagBytes;
	dest += (4 + signFlagBytes);
	destLen -= (4 + signFlagBytes);
	
	mini = 0;
	size_t max = std::max(maxi, std::abs(mini));
	int b;
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
	
	size_t initialCompressedLen = compressor.Compress((const unsigned char*)src, srcLen*4, tmp, tmpLen);
	
	BitWriter bitWriter(dest);
	RiceCoder riceCoder(b);
	for (size_t i=0; i<srcLen; ++i) {
		riceCoder.Encode(src[i], bitWriter);
	}
	//BitReader bitReader(dest);
	//for (size_t i=0; i<srcLen; ++i) {
	//	size_t val = riceCoder.Decode(bitReader);
	//	assert(src[i] == val);
	//}
	size_t bitsLen = bitWriter.nBits();
	size_t bytesLen = bitsLen / 8;
	if (bitsLen % 8) ++bytesLen;

	memcpy(src, dest, bytesLen);
	size_t compressedLen = compressor.Compress((const unsigned char*)src, bytesLen, dest+2+4, destLen-2-4);
	
	size_t len = 0;
	if (initialCompressedLen < compressedLen && initialCompressedLen < bytesLen) {
		*dest++ = 1;
		*dest++ = RiceCoderFlag::None;
		len = initialCompressedLen;
		memcpy(dest+4, tmp, len);
	}else if (compressedLen < bytesLen) {
		*dest++ = 1;
		*dest++ = b;
		len = compressedLen;
	}else {
		*dest++ = 0;
		*dest++ = b;
		len = bytesLen;
		memcpy(dest+4, src, len);
	}
	*((size_t*)dest) = len;
	dest += 4;
	dest += len;

//	printf("%d %d %d\n", max, b, len + signFlagBytes);

	return dest - initialDest;
}

size_t compress(
	ICompressor& compressor,
	size_t hBlockCount,
	size_t vBlockCount,
	const int* src,
	int* tmp,
	unsigned char* tmp2, size_t tmp2Len,
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
	progress += compressSub(compressor, tmp, totalBlockCount, dest+progress, destLen-progress, tmp2, tmp2Len);
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
	progress += compressSub(compressor, tmp, totalBlockCount*3, dest+progress, destLen-progress, tmp2, tmp2Len);
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
	progress += compressSub(compressor, tmp, totalBlockCount*5, dest+progress, destLen-progress, tmp2, tmp2Len);
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
	progress += compressSub(compressor, tmp, totalBlockCount*7, dest+progress, destLen-progress, tmp2, tmp2Len);
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
	progress += compressSub(compressor, tmp, totalBlockCount*9, dest+progress, destLen-progress, tmp2, tmp2Len);
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
	progress += compressSub(compressor, tmp, totalBlockCount*11, dest+progress, destLen-progress, tmp2, tmp2Len);
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
	progress += compressSub(compressor, tmp, totalBlockCount*13, dest+progress, destLen-progress, tmp2, tmp2Len);
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
	progress += compressSub(compressor, tmp, totalBlockCount*15, dest+progress, destLen-progress, tmp2, tmp2Len);
	
	return progress;
}

size_t decompressSub(ICompressor& compressor, const unsigned char* src, unsigned char* tmp, int* dest, size_t destLen)
{
	size_t signFlagBytes = *(size_t*)src;
	src += 4;
	BitReader signFlags(src);
	src += signFlagBytes;

	bool isCompressed = *src++ != 0;
	unsigned char b = *src++;
	size_t len = *(size_t*)src;
	src += 4;
	
	size_t len2 = 0;
	if (isCompressed) {
		len2 = compressor.Decompress(src, len, tmp, destLen*4);
	}else {
		memcpy(tmp, src, len);
		len2 = len;
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
	
	return 4 + signFlagBytes + 6 + len;
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

