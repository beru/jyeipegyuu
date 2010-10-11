#pragma once

struct Setting
{
	size_t qp;
	size_t hBlockness;
	size_t vBlockness;
	bool useQuantMatrix;
	bool reorderByFrequency;
	bool enableDCprediction;
	size_t zeroOneLimit;
};

