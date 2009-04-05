#pragma once

// http://en.wikipedia.org/wiki/Hadamard_transform

// integer 4x4 forward Discrete Hadamard Transform
inline void dht_4x4(short d[4][4], short t[4][4])
{
	for (size_t i=0; i<4; ++i) {
		int s01 = d[i][0] + d[i][1];
		int d01 = d[i][0] - d[i][1];
		int s23 = d[i][2] + d[i][3];
		int d23 = d[i][2] - d[i][3];

		t[0][i] = s01 + s23;
		t[1][i] = s01 - s23;
		t[2][i] = d01 - d23;
		t[3][i] = d01 + d23;
	}

	for (size_t i=0; i<4; ++i) {
		int s01 = t[i][0] + t[i][1];
		int d01 = t[i][0] - t[i][1];
		int s23 = t[i][2] + t[i][3];
		int d23 = t[i][2] - t[i][3];
		
		d[i][0] = s01 + s23;
		d[i][1] = s01 - s23;
		d[i][2] = d01 - d23;
		d[i][3] = d01 + d23;
	}
}

inline void dht_8x8(int d[8][8], int t[8][8])
{
/*

1  ----------------
2  --------________
3  ----________----
4  ----____----____
5  --____----____--
6  --____--__----__
7  --__--____--__--
8  --__--__--__--__

*/
	for (size_t i=0; i<8; ++i) {
		int s01 = d[i][0] + d[i][1];
		int d01 = d[i][0] - d[i][1];
		int s23 = d[i][2] + d[i][3];
		int d23 = d[i][2] - d[i][3];
		int s45 = d[i][4] + d[i][5];
		int d45 = d[i][4] - d[i][5];
		int s67 = d[i][6] + d[i][7];
		int d67 = d[i][6] - d[i][7];
		
		int a0 = s01 + s23;
		int a1 = s01 - s23;
		int a2 = d01 + d23;
		int a3 = d01 - d23;
		int a4 = s45 + s67;
		int a5 = s45 - s67;
		int a6 = d45 + d67;
		int a7 = d45 - d67;

		t[0][i] = a0 + a4;
		t[1][i] = a0 - a4;
		t[2][i] = a1 - a5;
		t[3][i] = a1 + a5;
		t[4][i] = a3 + a7;
		t[5][i] = a3 - a7;
		t[6][i] = a2 - a6;
		t[7][i] = a2 + a6;
	}
	for (size_t i=0; i<8; ++i) {
		int s01 = t[i][0] + t[i][1];
		int d01 = t[i][0] - t[i][1];
		int s23 = t[i][2] + t[i][3];
		int d23 = t[i][2] - t[i][3];
		int s45 = t[i][4] + t[i][5];
		int d45 = t[i][4] - t[i][5];
		int s67 = t[i][6] + t[i][7];
		int d67 = t[i][6] - t[i][7];
		
		int a0 = s01 + s23;
		int a1 = s01 - s23;
		int a2 = d01 + d23;
		int a3 = d01 - d23;
		int a4 = s45 + s67;
		int a5 = s45 - s67;
		int a6 = d45 + d67;
		int a7 = d45 - d67;

		d[i][0] = a0 + a4;
		d[i][1] = a0 - a4;
		d[i][2] = a1 - a5;
		d[i][3] = a1 + a5;
		d[i][4] = a3 + a7;
		d[i][5] = a3 - a7;
		d[i][6] = a2 - a6;
		d[i][7] = a2 + a6;
	}
}

// integer 8x8 forward Discrete Cosine Transform
inline void dct_8x8(int d[8][8], int t[8][8])
{
	// horizontal
	for (size_t i=0; i<8; ++i) {
		int p0 = d[i][0];
		int p1 = d[i][1];
		int p2 = d[i][2];
		int p3 = d[i][3];
		int p4 = d[i][4];
		int p5 = d[i][5];
		int p6 = d[i][6];
		int p7 = d[i][7];

		int a0 = p0 + p7;
		int a1 = p1 + p6;
		int a2 = p2 + p5;
		int a3 = p3 + p4;

		int b0 = (a0 + a3) << 1;
		int b1 = (a1 + a2) << 1;
		int b2 = (a0 - a3) << 1;
		int b3 = (a1 - a2) << 1;

		a0 = p0 - p7;
		a1 = p1 - p6;
		a2 = p2 - p5;
		a3 = p3 - p4;

		int b4 = ((a1 + a2) << 1) + (a0 * 3);
		int b5 = ((a0 - a3) << 1) - (a2 * 3);
		int b6 = ((a0 + a3) << 1) - (a1 * 3);
		int b7 = ((a1 - a2) << 1) + (a3 * 3);
		
		t[i][0] = (b0 + b1) << 2;
		t[i][1] = (b4 << 2) + b7;
		t[i][2] = (b2 << 2) + (b3 << 1);
		t[i][3] = (b5 << 2) + b6;
		t[i][4] = (b0 - b1) << 2;
		t[i][5] = (b6 << 2) - b5;
		t[i][6] = (b2 << 1) - (b3 << 2);
		t[i][7] = b4 - (b7 << 2);
	}
	// vertical
	for (size_t i=0; i<8; ++i) {
		int p0 = t[0][i];
		int p1 = t[1][i];
		int p2 = t[2][i];
		int p3 = t[3][i];
		int p4 = t[4][i];
		int p5 = t[5][i];
		int p6 = t[6][i];
		int p7 = t[7][i];

		int a0 = p0 + p7;
		int a1 = p1 + p6;
		int a2 = p2 + p5;
		int a3 = p3 + p4;

		int b0 = (a0 + a3) << 1;
		int b1 = (a1 + a2) << 1;
		int b2 = (a0 - a3) << 1;
		int b3 = (a1 - a2) << 1;

		a0 = p0 - p7;
		a1 = p1 - p6;
		a2 = p2 - p5;
		a3 = p3 - p4;

		int b4 = ((a1 + a2) << 1) + (a0 * 3);
		int b5 = ((a0 - a3) << 1) - (a2 * 3);
		int b6 = ((a0 + a3) << 1) - (a1 * 3);
		int b7 = ((a1 - a2) << 1) + (a3 * 3);
		
		d[0][i] = (b0 + b1) << 2;
		d[1][i] = (b4 << 2) + b7;
		d[2][i] = (b2 << 2) + (b3 << 1);
		d[3][i] = (b5 << 2) + b6;
		d[4][i] = (b0 - b1) << 2;
		d[5][i] = (b6 << 2) - b5;
		d[6][i] = (b2 << 1) - (b3 << 2);
		d[7][i] = b4 - (b7 << 2);
	}
}

// integer 8x8 inverse Discrete Cosine Transform
inline void idct_8x8(int d[8][8], int t[8][8])
{
	// horizontal
	for (size_t i=0; i<8; ++i) {
		int p0 = d[i][0];
		int p1 = d[i][1];
		int p2 = d[i][2];
		int p3 = d[i][3];
		int p4 = d[i][4];
		int p5 = d[i][5];
		int p6 = d[i][6];
		int p7 = d[i][7];

		int a0 = (p0 + p4) << 3;
		int a1 = (p0 - p4) << 3;
		int a2 = (p6 << 3) - (p2 << 2);
		int a3 = (p2 << 3) + (p6 << 2);

		int b0 = a0 + a3;
		int b2 = a1 - a2;
		int b4 = a1 + a2;
		int b6 = a0 - a3;

		a0 = ((-p3 + p5 - p7) << 1) - p7;
		a1 = (( p1 + p7 - p3) << 1) - p3;
		a2 = ((-p1 + p7 + p5) << 1) + p5;
		a3 = (( p3 + p5 + p1) << 1) + p1;

		int b1 = (a0 << 2) + a3;
		int b3 = (a1 << 2) + a2;
		int b5 = (a2 << 2) - a1;
		int b7 = (a3 << 2) - a0;
		
		t[i][0] = b0 + b7;
		t[i][1] = b2 - b5;
		t[i][2] = b4 + b3;
		t[i][3] = b6 + b1;
		t[i][4] = b6 - b1;
		t[i][5] = b4 - b3;
		t[i][6] = b2 + b5;
		t[i][7] = b0 - b7;
	}
	// vertical
	for (size_t i=0; i<8; ++i) {
		int p0 = t[0][i];
		int p1 = t[1][i];
		int p2 = t[2][i];
		int p3 = t[3][i];
		int p4 = t[4][i];
		int p5 = t[5][i];
		int p6 = t[6][i];
		int p7 = t[7][i];
		
		int a0 = (p0 + p4) << 3;
		int a1 = (p0 - p4) << 3;
		int a2 = (p6 << 3) - (p2 << 2);
		int a3 = (p2 << 3) + (p6 << 2);

		int b0 = a0 + a3;
		int b2 = a1 - a2;
		int b4 = a1 + a2;
		int b6 = a0 - a3;

		a0 = ((-p3 + p5 - p7) << 1) - p7;
		a1 = (( p1 + p7 - p3) << 1) - p3;
		a2 = ((-p1 + p7 + p5) << 1) + p5;
		a3 = (( p3 + p5 + p1) << 1) + p1;

		int b1 = (a0 << 2) + a3;
		int b3 = (a1 << 2) + a2;
		int b5 = (a2 << 2) - a1;
		int b7 = (a3 << 2) - a0;
		
		d[0][i] = b0 + b7;
		d[1][i] = b2 - b5;
		d[2][i] = b4 + b3;
		d[3][i] = b6 + b1;
		d[4][i] = b6 - b1;
		d[5][i] = b4 - b3;
		d[6][i] = b2 + b5;
		d[7][i] = b0 - b7;
	}
}

/*
	Y = (R + 2G + B) >> 2
	Co = (R - B) >> 1
	Cg = (-R + 2G - B) >> 2
*/
inline void RGB_2_YCoCg(short r, short g, short b, short& y, short& co, short& cg)
{
	assert(r <= 255);
	assert(g <= 255);
	assert(b <= 255);
	y = r + 2 * g + b;
	co = r - b;
	cg = -r + 2 * g -b;
}

/*
	R = Y + Co - Cg
	G = Y + Cg
	B = Y - Co - Cg
*/
inline void YCoCg_2_RGB(short y, short co, short cg, short& r, short& g, short& b)
{
	short ymcg = y - cg;
	short co2 = 2 * co;
	r = (ymcg + co2) >> 2;
	g = (y + cg) >> 2;
	b = (ymcg - co2) >> 2;
	assert(r <= 255);
	assert(g <= 255);
	assert(b <= 255);
}

