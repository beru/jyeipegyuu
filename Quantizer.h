#pragma once

class Quantizer
{
public:
	void init(size_t qp, size_t hBlockness, size_t vBlockness, bool useQuantMatrix);
	
	void quantize(int values[8][8]);
	void dequantize(int values[8][8]);
private:
	int quant8_table[64];
	int dequant8_table[64];
	
	size_t QP;
	size_t QP_shift;
	size_t QP_remain;
};

