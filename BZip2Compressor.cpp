#include "stdafx.h"

#include "BZip2Compressor.h"

#include "bzlib.h"
#pragma comment(lib, "libbz2.lib")

size_t BZip2Compressor::Compress(
	const unsigned char* src,
	size_t srcLen,
	unsigned char* dest,
	size_t destLen
)
{
	size_t compressedLen = destLen;
	BZ2_bzBuffToBuffCompress((char*)dest, &compressedLen, (char*)src, srcLen, 9, 0, 30);
	return compressedLen;
}

size_t BZip2Compressor::Decompress(
	const unsigned char* src,
	size_t srcLen,
	unsigned char* dest,
	size_t destLen
)
{
	size_t decompressedLen = destLen;
	BZ2_bzBuffToBuffDecompress((char*)dest, &decompressedLen, (char*)src, srcLen, 0, 0);
	return decompressedLen;
}

