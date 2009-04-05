#pragma once

namespace IntraPrediction {

enum Mode {
	Mode_Vertical = 0,
	Mode_Horizontal = 1,
	Mode_DC = 2,
	Mode_DiagonalDownLeft = 3,
	Mode_DiagonalDownRight = 4,
	Mode_VerticalRight = 5,
	Mode_HorizontalDown = 6,
	Mode_VerticalLeft = 7,
	Mode_HorizontalUp = 8,
};

class Predictor8
{
public:
	void predict(Mode mode, const int original[8][8], int processed[8][8])
	{
		switch (mode) {
		case Mode_Vertical:
			vertical(original, processed);
			break;
		case Mode_Horizontal:
			horizontal(original, processed);
			break;
		case Mode_DC:
			dc(original, processed);
			break;
		case Mode_DiagonalDownLeft:
			diagonalDownLeft(original, processed);
			break;
		case Mode_DiagonalDownRight:
			diagonalDownRight(original, processed);
			break;
		case Mode_VerticalRight:
			verticalRight(original, processed);
			break;
		case Mode_HorizontalDown:
			horizontalDown(original, processed);
			break;
		case Mode_VerticalLeft:
			verticalLeft(original, processed);
			break;
		case Mode_HorizontalUp:
			horizontalUp(original, processed);
			break;
		}
	}
	
	void vertical(const int original[8][8], int processed[8][8]);
	void horizontal(const int original[8][8], int processed[8][8]);
	void dc(const int original[8][8], int processed[8][8]);
	void diagonalDownLeft(const int original[8][8], int processed[8][8]);
	void diagonalDownRight(const int original[8][8], int processed[8][8]);

	void verticalRight(const int original[8][8], int processed[8][8]);
	void verticalRight_Sub(size_t offset, const int original[8][8], int processed[8][8]);

	void horizontalDown(const int original[8][8], int processed[8][8]);

	void verticalLeft(const int original[8][8], int processed[8][8]);
	void verticalLeft_Sub(size_t offset, const int original[8][8], int processed[8][8]);

	void horizontalUp(const int original[8][8], int processed[8][8]);
	
	int cols[17];	// bottom row
	int rows[17];	// right-most columns
	int average;
};

}	// namespace IntraPrediction

