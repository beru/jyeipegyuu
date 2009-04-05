#include "stdafx.h"
#include "Quantizer.h"

static const int quant8_scan[16] = {
	0,3,4,3, 3,1,5,1, 4,5,2,5, 3,1,5,1
};

static const int dequant8_scale[6][6] = {
	{ 20, 18, 32, 19, 25, 24 },
	{ 22, 19, 35, 21, 28, 26 },
	{ 26, 23, 42, 24, 33, 31 },
	{ 28, 25, 45, 26, 35, 33 },
	{ 32, 28, 51, 30, 40, 38 },
	{ 36, 32, 58, 34, 46, 43 },
};
static const int quant8_scale[6][6] = {
	{ 13107, 11428, 20972, 12222, 16777, 15481 },
	{ 11916, 10826, 19174, 11058, 14980, 14290 },
	{ 10082,  8943, 15978,  9675, 12710, 11985 },
	{  9362,  8228, 14913,  8931, 11984, 11259 },
	{  8192,  7346, 13159,  7740, 10486,  9777 },
	{  7282,  6428, 11570,  6830,  9118,  8640 }
};

static const unsigned char quant8_intra_matrix[8][8] = {
	6,	10,	13,	16,	18,	23,	25,	27,
	10,	11,	16,	18,	23,	25,	27,	29,
	13,	16,	18,	23,	25,	27,	29,	31,
	16,	18,	23,	25,	27,	29,	31,	33,
	18,	23,	25,	27,	29,	31,	33,	36,
	23,	25,	27,	29,	31,	33,	36,	38,
	25,	27,	29,	31,	33,	36,	38,	40,
	27,	29,	31,	33,	36,	38,	40,	42,
};

static const unsigned char quant8_inter_matrix[8][8] = {
	9,	13,	15,	17,	19,	21,	22,	24,
	13,	13,	17,	19,	21,	22,	24,	25,
	15,	17,	19,	21,	22,	24,	25,	27,
	17,	19,	21,	22,	24,	25,	27,	28,
	19,	21,	22,	24,	25,	27,	28,	30,
	21,	22,	24,	25,	27,	28,	30,	32,
	22,	24,	25,	27,	28,	30,	32,	33,
	24,	25,	27,	28,	30,	32,	33,	35,
};

namespace {

int div(int n, int d)
{
	return (n + (d>>1)) / d;
}

}

void Quantizer::init(size_t qp, size_t hBlockness, size_t vBlockness, bool useQuantMatrix)
{
	QP = qp;
	QP_shift = QP / 6;
	QP_remain = QP % 6;
	
	for (size_t i=0; i<64; ++i) {
		int j = quant8_scan[((i>>1)&12) | (i&3)];
//		printf("%d,", j);
		quant8_table[i] = quant8_scale[QP_remain][j];
		dequant8_table[i] = dequant8_scale[QP_remain][j] << QP_shift;
	}
	
	// 量子化テーブル操作
	for (size_t i=0; i<8; ++i) {
		if (i>=(8-hBlockness)) {
			for (size_t j=0; j<8; ++j) {
				quant8_table[j*8+i] = 0;
			}
		}
		if (i>=(8-vBlockness)) {
			for (size_t j=0; j<8; ++j) {
				quant8_table[i*8+j] = 0;
			}
		}
	}
	
	// 高周波を荒くする量子化マトリクスを適用
	if (useQuantMatrix) {
		for (size_t i=0; i<8; ++i) {
			for (size_t j=0; j<8; ++j) {
				quant8_table[i*8+j] = div(quant8_table[i*8+j] << 4, quant8_intra_matrix[i][j]);
				dequant8_table[i*8+j] *= quant8_intra_matrix[i][j];
			}
		}
	}else {
		for (size_t i=0; i<8*8; ++i) {
			quant8_table[i] = quant8_table[i];
			dequant8_table[i] <<= 4;
		}
	}
	
}

void Quantizer::quantize(int values[8][8])
{
	for (size_t i=0; i<8; ++i) {
		for (size_t j=0; j<8; ++j) {
			int value = values[i][j];
			if (value == 0) {
				continue;
			}else if (value > 0) {
				value += (1 << 5);
			}else {
				value -= (1 << 5);
			}
			value >>= 6;
			value *= quant8_table[i*8+j];
			value += (1 << (11+QP_shift));
			value >>= (12+QP_shift);
//			assert(value >= -32768 && value <= 32767);
			values[i][j] = value;
		}
	}
}

void Quantizer::dequantize(int values[8][8])
{
	for (size_t i=0; i<8; ++i) {
		for (size_t j=0; j<8; ++j) {
			values[i][j] *= dequant8_table[i*8+j];
		}
	}
}

