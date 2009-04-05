#pragma once

template <int shifts, typename T>
T div(T value)
{
	return (value + (1 << (shifts - 1))) >> shifts;
}


void decodeRightMostColumn(Quantizer& quantizer, int* rows, int values[8][8], int tmps[8][8])
{
	quantizer.dequantize(values);
	idct_8x8(values, tmps);
	for (size_t i=0; i<8; ++i) {
		rows[i] = div<22>(values[i][7]);
	}
}

void decodeBottomRow(Quantizer& quantizer, int* cols, int values[8][8], int tmps[8][8])
{
	quantizer.dequantize(values);
	idct_8x8(values, tmps);
	for (size_t i=0; i<8; ++i) {
		cols[i] = div<22>(values[7][i]);
	}
}

void decodeRightMostColumnAndBottomRow(Quantizer& quantizer, int* rows, int* cols, int values[8][8], int tmps[8][8])
{
	quantizer.dequantize(values);
	idct_8x8(values, tmps);

	for (size_t i=0; i<8; ++i) {
		rows[i] = div<22>(values[i][7]);
	}
	for (size_t i=0; i<8; ++i) {
		cols[i] = div<22>(values[7][i]);
	}
}

template <typename T>
void decode(
	Quantizer& quantizer,
	size_t hBlocks, size_t vBlocks,
	const int* in, int inLineOffsetBytes,
	T* out, int outLineOffsetBytes
	)
{
	int values[8][8];
	int tmps[8][8];
	
	const int* src = in;
	T* dest = out;

	for (size_t y=0; y<vBlocks; ++y) {
		
		const int* srcLine = src;
		T* destLine = dest;

		for (size_t x=0; x<hBlocks; ++x) {
			
			gather(srcLine, inLineOffsetBytes, values);
			
			quantizer.dequantize(values);
			idct_8x8(values, tmps);
			for (size_t i=0; i<8; ++i) {
				for (size_t j=0; j<8; ++j) {
					values[i][j] = div<22>(values[i][j]);
				}
			}
			
			scatter(destLine, outLineOffsetBytes, values);

			srcLine += 8;
			destLine += 8;
		}

		OffsetPtr(src, inLineOffsetBytes*8);
		OffsetPtr(dest, outLineOffsetBytes*8);
	}
}

