#pragma once

#include "IntraPrediction.h"

void copy(int in[8][8], int out[8][8])
{
	for (size_t i=0; i<8; ++i)
	for (size_t j=0; j<8; ++j)
		out[i][j] = in[i][j];
}

size_t sumOfAbsolutes(const int values[8][8])
{
	size_t ret = 0;
	for (size_t i=0; i<8; ++i)
	for (size_t j=0; j<8; ++j)
		ret += std::abs(values[i][j]);
	return ret;
}

template <typename T>
void encode(
	Quantizer& quantizer,
	size_t hBlocks, size_t vBlocks,
	const T* in, int inLineOffsetBytes,
	int* out, int outLineOffsetBytes
	)
{
	int values[8][8];
	int tmps[8][8];
	int tmps2[8][8];
	
	IntraPrediction::Predictor8 predictor;
	int cols[17];
	int rows[17];
	int predictionScores[9];

	const T* src = in;
	int* dest = out;
	
	// top
	{
		const T* srcLine = src;
		int* destLine = dest;

		// top left
		gather(srcLine, inLineOffsetBytes, values);
		dct_8x8(values, tmps);
		quantizer.quantize(values);
		scatter(destLine, outLineOffsetBytes, values);
		srcLine += 8; destLine += 8;
		// top remain
		for (size_t x=1; x<hBlocks; ++x) {
#if 0
			decodeRightMostColumn(quantizer, rows+1, values, tmps);
//			predictor.rows[0] = predictor.rows[1] = 
			int sum = 0;
			for (size_t i=1; i<9; ++i) {
				sum += rows[i];
			}
			std::copy(rows+1, rows+9, predictor.rows+1);
			predictor.average = sum / 8;
			gather(srcLine, inLineOffsetBytes, values);
			predictor.horizontal(values, tmps);
			sum = sumOfAbsolutes(tmps);
			predictor.horizontalUp(values, tmps);
			sum = sumOfAbsolutes(tmps);
			predictor.dc(values, tmps);
			sum = sumOfAbsolutes(tmps);
			dct_8x8(tmps, tmps2);
			quantizer.quantize(tmps);
			scatter(destLine, outLineOffsetBytes, tmps);
#else
			gather(srcLine, inLineOffsetBytes, values);
			dct_8x8(values, tmps);
			quantizer.quantize(values);
			scatter(destLine, outLineOffsetBytes, values);
#endif
			srcLine += 8; destLine += 8;
		}
		OffsetPtr(src, inLineOffsetBytes*8);
		OffsetPtr(dest, outLineOffsetBytes*8);
	}
	
	for (size_t y=1; y<vBlocks; ++y) {
		
		const T* srcLine = src;
		int* destLine = dest;
		
		// left
		gather(srcLine, inLineOffsetBytes, values);
		dct_8x8(values, tmps);
		quantizer.quantize(values);
		scatter(destLine, outLineOffsetBytes, values);
		srcLine += 8; destLine += 8;
		// between
		for (size_t x=1; x<hBlocks-1; ++x) {
			gather(srcLine, inLineOffsetBytes, values);
			dct_8x8(values, tmps);
			quantizer.quantize(values);
			scatter(destLine, outLineOffsetBytes, values);
			srcLine += 8; destLine += 8;
		}
		// right
		gather(srcLine, inLineOffsetBytes, values);
		dct_8x8(values, tmps);
		quantizer.quantize(values);
		scatter(destLine, outLineOffsetBytes, values);
		srcLine += 8; destLine += 8;

		OffsetPtr(src, inLineOffsetBytes*8);
		OffsetPtr(dest, outLineOffsetBytes*8);
	}
}

