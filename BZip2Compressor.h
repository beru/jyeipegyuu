#pragma once

#include "ICompressor.h"

class BZip2Compressor : public ICompressor
{
public:
	// overrides
	size_t Compress(
		const unsigned char* src,
		size_t srcLen,
		unsigned char* dest,
		size_t destLen
	);
	
	size_t Decompress(
		const unsigned char* src,
		size_t srcLen,
		unsigned char* dest,
		size_t destLen
	);
	
};

