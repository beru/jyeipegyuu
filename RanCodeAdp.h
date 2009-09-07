// RanCode.cpp : Defines the entry point for the console application.
//
//This is DEMO version of arithmetic/range encoder written for research purposes by 
//Andrew Polar (www.ezcodesample.com, Jan. 10, 2007). The algorithm was published by
//G.N.N. Martin. In March 1979. Video & Data Recording Conference, Southampton, UK. 
//The program was tested for many statistically different data samples, however you 
//use it on own risk. Author do not accept any responsibility for use or misuse of 
//this program. Any data sample that cause crash or wrong result can be sent to author 
//for research. The e-mail may be obtained by WHOIS www.ezcodesample.com.
//The correction of July 03, 2007. The processing of non-zero minimum has been added.
//User can pass data with non-zero minimum value, however the min/max limits must be 
//specified in function call.
//The correction of August 17, 2007. The additional means of computational statbility
//has been added. As it was discovered and investigated by Bill Dorsey in beta-testing
//the addition operation must have treatment for carry propagation. The solution is
//to normalize data to vary from 1 to max-min+1 and use value 0 as a means of output
//of the head of operation_buffer bits. This zero value is used as synchronization 
//flag for output buffer and simply excluded from decoded stream.
//Last correction 09/07/2007 by Andrew Polar, making range coder adoptive
//The missing functionality is big-endian processor capabilities.

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
//#include <windows.h>

///////////////////////////////////////////////////////////////////////
// Test data preparation
///////////////////////////////////////////////////////////////////////

double entropy24(int* data, int size) {
	
	int max_size = 1 << 24;
	int min, counter;
	int* buffer;
	double entropy;
	double log2 = log(2.0);
	double prob;

	min = data[0];
	for (counter=0; counter<size; counter++) {
		if (data[counter] < min)
			min = data[counter];
	}

	for (counter=0; counter<size; counter++) {
		data[counter] -= min;
	}

	buffer = (int*)malloc(max_size*sizeof(int));
	memset(buffer, 0x00, max_size*sizeof(int));
	for (counter=0; counter<size; counter++) {
		buffer[data[counter]]++;
	}

	entropy = 0.0;
	for (counter=0; counter<max_size; counter++) {
		if (buffer[counter] > 0) {
			prob = (double)buffer[counter];
			prob /= (double)size;
			entropy += log(prob)/log2*prob;
		}
	}
	entropy *= -1.0;

	for (counter=0; counter<size; counter++) {
		data[counter] += min;
	}

	if (buffer)
		free(buffer);

	return  entropy;
}

double round(double x) {
	if ((x - floor(x)) >= 0.5)
		return ceil(x);
	else
		return floor(x);
}

double MakeData(int* data, int data_size, int min, int max, int redundancy_factor) {

	int counter, cnt, high, low;
	double buf;

	if (redundancy_factor <= 1)
		redundancy_factor = 1;

	if (max <= min)
		max = min + 1;

	srand((unsigned)time(0)); 
	for (counter=0; counter<data_size; counter++) {
		buf = 0;
		for (cnt=0; cnt<redundancy_factor; cnt++) {
			buf += (double)rand();
		}
		data[counter] = ((int)buf)/redundancy_factor;
	}

	low  = data[0];
	high = data[0];
	for (counter=0; counter<data_size; counter++) {
		if (data[counter] > high)
			high = data[counter];
		if (data[counter] < low)
			low = data[counter];
	}

	for (counter=0; counter<data_size; counter++) {
		buf = (double)(data[counter] - low);
		buf /= (double)(high - low);
		buf *= (double)(max - min);
		buf = round(buf);
		data[counter] = (int)buf + min;
	}

	return entropy24(data, data_size);
}

///////////////////////////////////////////////////////////////////
// End of data preparation start of encoding, decoding functions
///////////////////////////////////////////////////////////////////

const unsigned MAX_BASE = 15;   //value is optional but 15 is recommended
unsigned       current_byte;    //position of current byte in result data
unsigned char  current_bit;     //postion of current bit in result data

//Some look up tables for fast processing
static int           output_mask[8][32];
static unsigned char bytes_plus [8][64];

static unsigned char edge_mask[8]   = {0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01};
static unsigned char shift_table[8] = {24, 16, 8, 0};

static long long     overflow_indicator = 0xffffffffffffffff << (MAX_BASE * 2 - 1);

void make_look_ups() {

	int sign_mask[8] = {
		0xffffffff, 0x7fffffff, 0x3fffffff, 0x1fffffff,
		0x0fffffff, 0x07ffffff, 0x03ffffff, 0x01ffffff
	};
	
	for (int i=0; i<8; i++) {
		for (int j=0; j<32; j++) {
			output_mask[i][j] = sign_mask[i];
			output_mask[i][j] >>= (32-i-j);
			output_mask[i][j] <<= (32-i-j);
		}
	}

	for (int i=0; i<8; i++) {
		for (int j=0; j<64; j++) {
			bytes_plus[i][j] = j/8;
			if ((i + j%8) > 7)
				++bytes_plus[i][j];
		}
	}
}

//The key function that takes product of x*y and convert it to 
//mantissa and exponent. Mantissa have number of bits equal to
//length of the mask.
inline int SafeProduct(int x, int y, int mask, int& shift) {

	int p = x * y;
	if ((p >> shift) > mask) {
		p >>= shift;
		while (p > mask) {
			p >>= 1;
			++shift;
		}
	}
	else {
		while (shift >= 0) {
			--shift;
			if ((p >> shift) > mask) {
				break;
			}
		}
		++shift;
		p >>= shift;
	}
	return p;
}

inline int readBits(int operation_buffer, int bits, unsigned char* code) {

	unsigned end_byte = current_byte + bytes_plus[current_bit][bits];
	int buffer = (code[current_byte] & edge_mask[current_bit]);
	unsigned char total_bits = 8 - current_bit;
	for (unsigned i=current_byte+1; i<=end_byte; ++i) {
		(buffer <<= 8) |= code[i];
		total_bits += 8;
	}
	buffer >>= (total_bits - bits);
	operation_buffer <<= bits;
	operation_buffer |= buffer;
	current_byte = end_byte;
	current_bit  = (bits + current_bit)%8; 
	return operation_buffer;
}

inline void writeBits(long long operation_buffer, int bits, unsigned char* result) {

	int buffer = (int)((operation_buffer >> current_bit) >> 32);
	buffer &= output_mask[current_bit][bits];
	unsigned bytes_plus2 = bytes_plus[current_bit][bits]; 
	current_bit = (bits + current_bit)%8;
	for (unsigned i=0; i<=bytes_plus2; ++i) {
		result[current_byte + i] |= (buffer >> shift_table[i]);
	}
	current_byte += bytes_plus2;
}

void update_frequencies(int* buffer, int* freq, int* cumulative, int range, int denominator) {
	
	memcpy(freq, buffer, range*sizeof(int));
	for (int i=0; i<range; i++)
		buffer[i] = 1;

	cumulative[0] = 0;
	for (int i=1; i<range; i++) {
		cumulative[i] = cumulative[i-1] + freq[i-1];
	}
	cumulative[range] = denominator;
}

//The result buffer should be allocated outside the function
//The data must be non-negative integers less or equal to max_value
//The actual size of compressed buffer is returned in &result_size
//Initially result_size must contain size of result buffer
void Encode(int* source, int source_size, int max_value, int min_value, unsigned char* result, int& result_size) {
	
	memset(result, 0x00, result_size);
	current_byte = 0;
	current_bit  = 0;

	if (min_value != 0) {
		for (int i=0; i<source_size; i++) {
			source[i] -= min_value;
		}
	}

	//collecting frequencies
	int range = (max_value + 1) - min_value + 1; //we add one more value for flag
	int* frequency  = (int*)malloc(range*sizeof(int));
	int* cumulative = (int*)malloc((range+1)*sizeof(int));
	int* buffer     = (int*)malloc(range * sizeof(int));

	int Denominator = source_size + 1; //we increased size by 1 
	unsigned base = 0;
	while (Denominator > 1) {
		Denominator >>= 1;
		base++;
	}
	if (base > MAX_BASE)
		base = MAX_BASE;
	Denominator = (1 << base);

	for (int i=0; i<range; ++i) {
		buffer[i] = 1;
	}
	int cnt = 1; //start from 1 on purpose
	for (int i=0; i<Denominator-range; ++i) {
		buffer[cnt++]++;
		if (cnt == range)
			cnt = 1;
	}
	
	update_frequencies(buffer, frequency, cumulative, range, Denominator);

	//saving frequencies in result buffer
	memcpy(result, &range, 4);
	memcpy(result + 4, &Denominator, 4);
	current_byte += 8;
	//data is ready

	//encoding
	make_look_ups();
	int mask = 0xFFFFFFFF >> (32 - base);
	//First 64 bits are not used for data, we use them for saving data_size and
	//any other 32 bit value that we want to save
	long long operation_buffer = source_size;
	operation_buffer <<= 32;
	//we save minimum value always as positive number and use last
	//decimal position as sign indicator
	int saved_min = min_value * 10;
	if (saved_min < 0) {
		saved_min = 1 - saved_min;
	}
	operation_buffer |= saved_min; //we use another 32 bits of first 64 bit buffer 
	/////////////////////
	int product = 1;
	int shift = 0;
	cnt = 1;
	for (int i=0; i<source_size; ++i) {

		while (((operation_buffer << (base - shift)) & overflow_indicator) == overflow_indicator) {
			printf("Possible buffer overflow is corrected at value %d\n", i);
			//this is zero flag output in order to avoid buffer overflow
			//rarely happen, cumulative[0] = 0, frequency[0] = 1
			writeBits(operation_buffer, base-shift, result); 
			operation_buffer <<= (base - shift);
			operation_buffer += cumulative[0] * product;
			product = SafeProduct(product, frequency[0], mask, shift);
			//in result of this operation shift = 0
		}

		writeBits(operation_buffer, base-shift, result); 
		operation_buffer <<= (base - shift);
		operation_buffer += cumulative[source[i]+1] * product;
		product = SafeProduct(product, frequency[source[i]+1], mask, shift);

		//This is adoptive frequency updates
		buffer[source[i]+1]++;
		cnt++;
		if (cnt == (Denominator-range)) {
			update_frequencies(buffer, frequency, cumulative, range, Denominator);
			cnt = 1;
		}
	}
	//flushing remained 64 bits
	writeBits(operation_buffer, 24, result);
	operation_buffer <<= 24;
	writeBits(operation_buffer, 24, result);
	operation_buffer <<= 24;
	writeBits(operation_buffer, 16, result);
	operation_buffer <<= 16;
	result_size = current_byte + 1;
	//end encoding

	if (min_value != 0) {
		for (int i=0; i<source_size; i++) {
			source[i] += min_value;
		}
	}

	if (buffer)
		free(buffer);

	if (cumulative)
		free(cumulative);

	if (frequency)
		free(frequency);
}

void prepare_symbols(int* cumulative, int range, int denominator, int* symbol) {

	memset(symbol, 0x00, denominator*sizeof(int));
	for (int k=0; k<range; ++k) {
		for (int i=cumulative[k]; i<cumulative[k+1]; i++) {
			symbol[i] = k;
		}
	}
}

//result buffer must be allocated outside of function
//result size must contain the correct size of the buffer
void Decode(unsigned char* code, int code_size, int* result, int& result_size) {

	current_byte = 0;
	current_bit  = 0;

	//reading frequencies
	int range  = 0;
	int Denominator = 0;
	memcpy(&range, code, 4);
	memcpy(&Denominator, code + 4, 4);

	int* frequency   = (int*)malloc(range*sizeof(int));
	int* cumulative  = (int*)malloc((range+1)*sizeof(int));
	int* buffer      = (int*)malloc(range * sizeof(int));

	current_byte += 8;
	unsigned base = 0;
	while (Denominator > 1) {
		Denominator >>= 1;
		base++;
	}
	Denominator = (1 << base);

	for (int i=0; i<range; ++i) {
		buffer[i] = 1;
	}
	int cnt = 1; //start from 1 on purpose
	for (int i=0; i<Denominator-range; ++i) {
		buffer[cnt++]++;
		if (cnt == range)
			cnt = 1;
	}
	
	int* symbol = (int*)malloc(Denominator*sizeof(int));
	update_frequencies(buffer, frequency, cumulative, range, Denominator);
	prepare_symbols(cumulative, range, Denominator, symbol);
	//data is ready

	//decoding
	make_look_ups();
	int mask = 0xFFFFFFFF >> (32 - base);
	long long ID;
	int product = 1;
	int shift = 0;
	int operation_buffer = 0;
	//we skip first 64 bits they contain size and minimum value
	operation_buffer = readBits(operation_buffer, 32, code);
	result_size = (int)operation_buffer; //First 32 bits is data_size 
	operation_buffer = 0;
	operation_buffer = readBits(operation_buffer, 32, code);
	int min_value = (int)operation_buffer;  //Second 32 bits is minimum value;
	//we find sign according to our convention
	if ((min_value % 10) > 0)
		min_value = - min_value;
	min_value /= 10;
	operation_buffer = 0;
	////////////////////////////////////////
	int counter = 0;
	cnt = 1;
	while (counter < result_size) {
		operation_buffer = readBits(operation_buffer, base-shift, code);
		ID = operation_buffer/product;
		operation_buffer -= product * cumulative[symbol[ID]];
		product = SafeProduct(product, frequency[symbol[ID]], mask, shift);
		result[counter] = symbol[ID] + min_value - 1;
		if (result[counter] >= min_value) {
			
			buffer[symbol[ID]]++;
			cnt++;
			if (cnt == (Denominator-range)) {
				update_frequencies(buffer, frequency, cumulative, range, Denominator);
				prepare_symbols(cumulative, range, Denominator, symbol);
				cnt = 1;
			}
			counter++;
		}
	}
	//end decoding

	if (buffer)
		free(buffer);

	if (symbol)
		free(symbol);

	if (cumulative)
		free(cumulative);

	if (frequency)
		free(frequency);
}

#if 0

int main()
{
	printf("Making data for round trip...\n");
	int data_size = 1800000;
	int min_data_value  = -297;
	int max_data_value  = 4000;
	int redundancy_factor = 10;	

	int* source = (int*)malloc(data_size * sizeof(int));
	double entropy = MakeData(source, data_size, min_data_value, max_data_value, redundancy_factor);

	int Bytes = (int)(ceil((double)(data_size) * entropy)/8.0);
	printf("Data size = %d, alphabet = %d\n", data_size, max_data_value - min_data_value + 1);
	printf("Entropy %5.3f, estimated compressed size         %d bytes\n\n", entropy, Bytes);

	SYSTEMTIME st;
	GetSystemTime(&st);
	printf("Encoding is started  %d %d\n", st.wSecond, st.wMilliseconds);

	int result_size = Bytes + Bytes/2 + (max_data_value - min_data_value) * 2 + 4 + 1024;
	unsigned char* result = (unsigned char*)malloc(result_size * sizeof(unsigned char));

	//Min and Max data values can be specified approximately they can make
	//larger data segment, for example Max value can be passed as larger and Min 
	//value can be passed as smaller number. Replace the function below by commented
	//out example and it will work in the same way.
	//Encode(source, data_size, max_data_value + 150, min_data_value - 140, result, result_size);
	Encode(source, data_size, max_data_value, min_data_value, result, result_size);

	GetSystemTime(&st);
	printf("Encoding is finished %d %d\n\n", st.wSecond, st.wMilliseconds);
	printf("Actual size of compressed buffer with frequencies %d bytes\n\n", result_size);

	GetSystemTime(&st);
	printf("Decoding is started  %d %d\n", st.wSecond, st.wMilliseconds);
	int test_data_size = data_size  + data_size/2;
	int* test_data = (int*)malloc(test_data_size*sizeof(int));
	Decode(result, result_size, test_data, test_data_size);

	GetSystemTime(&st);
	printf("Decoding is finished %d %d\n\n", st.wSecond, st.wMilliseconds);

	bool error_occur = false;
	if (test_data_size != data_size) {
		error_occur = true;
	}
	else {
		for (int i=0; i<data_size; i++) {
			if (test_data[i] != source[i]) {
				error_occur = true;
				printf("Mismatch in %d, %d %d\n", i, test_data[i], source[i]);
				break;
			}
		}
	}

	if (error_occur == false) {
		printf("Round trip is correct, 100 percent match\n\n");
	}
	else {
		printf("Round trip test failed, data mismatch\n\n");
	}

	if (test_data)
		free(test_data);

	if (result)
		free(result);

	if (source)
		free(source);

	return 0;
}

#endif
