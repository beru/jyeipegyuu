#pragma once

#include <stdlib.h>

class ICompressor
{
public:
	virtual size_t Compress(
		const unsigned char* src,
		size_t srcLen,
		unsigned char* dest,
		size_t destLen
	) = 0;
	
	virtual size_t Decompress(
		const unsigned char* src,
		size_t srcLen,
		unsigned char* dest,
		size_t destLen
	) = 0;
};

