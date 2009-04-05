#include "stdafx.h"

#include "IntraPrediction.h"

namespace IntraPrediction {

void Predictor8::vertical(const int original[8][8], int processed[8][8])
{
	for (size_t x=0; x<8; ++x) {
		processed[0][x] = original[0][x] - cols[1+x];
	}
	for (size_t y=1; y<8; ++y) {
		for (size_t x=0; x<8; ++x) {
			processed[y][x] = original[y][x] - original[y-1][x];
		}
	}
}

void Predictor8::horizontal(const int original[8][8], int processed[8][8])
{
	for (size_t y=0; y<8; ++y) {
		processed[y][0] = original[y][0] - rows[1+y];
		for (size_t x=1; x<8; ++x) {
			processed[y][x] = original[y][x] - original[y][x-1];
		}
	}
}

void Predictor8::dc(const int original[8][8], int processed[8][8])
{
	for (size_t y=0; y<8; ++y) {
		for (size_t x=0; x<8; ++x) {
			processed[y][x] = original[y][x] - average;
		}
	}
}

void Predictor8::diagonalDownLeft(const int original[8][8], int processed[8][8])
{
	for (size_t x=0; x<8; ++x) {
		processed[0][x] = original[0][x] - cols[1+x];
	}
	for (size_t y=1; y<8; ++y) {
		for (size_t x=0; x<7; ++x) {
			processed[y][x] = original[y][x] - original[y-1][x+1];
		}
		processed[y][7] = original[y][7] - cols[9+y];
	}
}

void Predictor8::diagonalDownRight(const int original[8][8], int processed[8][8])
{
	for (size_t x=0; x<8; ++x) {
		processed[0][x] = original[0][x] - cols[x];
	}
	for (size_t y=1; y<8; ++y) {
		processed[y][0] = rows[y];
		for (size_t x=1; x=8; ++x) {
			processed[y][x] = original[y][x] - original[y-1][x-1];
		}
	}
}

void Predictor8::verticalRight_Sub(size_t offset, const int original[8][8], int processed[8][8])
{
	processed[offset][0] = original[offset][0] - rows[offset];
	for (size_t x=1; x<8; ++x) {
		processed[offset][x] = original[offset][x] - original[offset-1][x-1];
	}
	for (size_t x=0; x<8; ++x) {
		processed[offset+1][x] = original[offset+1][x] - original[offset][x];
	}
}

void Predictor8::verticalRight(const int original[8][8], int processed[8][8])
{
	for (size_t x=0; x<8; ++x) {
		processed[0][x] = original[0][x] - cols[x];
	}
	for (size_t x=0; x<8; ++x) {
		processed[1][x] = original[1][x] - original[0][x];
	}
	verticalRight_Sub(2, original, processed);
	verticalRight_Sub(4, original, processed);
	verticalRight_Sub(6, original, processed);
}

void Predictor8::horizontalDown(const int original[8][8], int processed[8][8])
{
	for (size_t x=0; x<8; x+=2) {
		processed[0][x] = original[0][x] - cols[x];
		processed[0][x+1] = original[0][x+1] - original[0][x];
	}
	for (size_t y=1; y<8; ++y) {
		processed[y][0] = original[y][0] - rows[y];
		processed[y][1] = original[y][1] - original[y][0];
		for (size_t x=2; x<8; x+=2) {
			processed[y][x] = original[y][x] - original[y-1][x-1];
			processed[y][x+1] = original[y][x+1] - original[y][x];
		}
	}
}

void Predictor8::verticalLeft_Sub(size_t offset, const int original[8][8], int processed[8][8])
{
	for (size_t x=0; x<7; ++x) {
		processed[offset][x] = original[offset][x] - original[offset-1][x+1];
		processed[offset+1][x] = original[offset+1][x] - original[offset][x];
	}
	processed[offset][7] = original[offset][7] - cols[8+offset];
	processed[offset+1][7] = original[offset+1][7] - original[offset][7];
}

void Predictor8::verticalLeft(const int original[8][8], int processed[8][8])
{
	for (size_t x=0; x<8; ++x) {
		processed[0][x] = original[0][x] - cols[x+2];
	}
	for (size_t x=0; x<8; ++x) {
		processed[1][x] = original[1][x] - original[0][x];
	}
	verticalLeft_Sub(2, original, processed);
	verticalLeft_Sub(4, original, processed);
	verticalLeft_Sub(6, original, processed);
}

void Predictor8::horizontalUp(const int original[8][8], int processed[8][8])
{
	for (size_t y=0; y<7; ++y) {
		processed[y][0] = original[y][0] - rows[2+y];
		processed[y][1] = original[y][1] - original[y][0];
		for (size_t x=2; x<8; x+=2) {
			processed[y][x] = original[y][x] - rows[x];
			processed[y][x+1] = original[y][x+1] - original[y][x+1];
		}
	}
	for (size_t x=0; x<8; x+=2) {
		processed[7][x] = original[7][x] - rows[7/*9+x>>1*/];
		processed[7][x+1] = original[7][x+1] - original[7][x];
	}
	
}

}	// namespace namespace IntraPrediction

