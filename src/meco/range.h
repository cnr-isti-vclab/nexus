#ifndef NX_RANGE_H
#define NX_RANGE_H

//Entropy Coding Source code
//By Sachin Garg, 2006
//
//Includes range coder based upon the carry-less implementation
//by Dmitry Subbotin, and arithmetic coder based upon Mark Nelson's
//DDJ code.
//For details:
//http://www.sachingarg.com/compression/entropy_coding/64bit

#include "bytestream.h"

typedef unsigned short uint2;  /* two-byte integer (large arrays)      */
typedef unsigned int   uint4;  /* four-byte integers (range needed)    */
typedef unsigned int   uint;     /* fast unsigned integer, 2 or 4 bytes  */

class RangeCoder {
public:
	const uint4 MaxRange;
	const uint4 Top, Bottom;
	uint4 Low, Range;


protected:
	RangeCoder(): MaxRange((uint4)1<<16),
		Top((uint4)1<<24), Bottom((uint4)1<<16),
		Low(0), Range((uint4)-1) {}

public:
	static bool getBit(uint bit, uint data) {
		return (1<<bit) & data ? 1:0;
	}
	static void setBit(uint bit, uint &data, bool on) {
		if(on)
			data = data | (1<<bit);
		else
			data = data & (~(1<<bit));
	}
	static uint4 toUint(int i) {
		i *= 2;
		if(i < 0) i = -i -1;
		return i;
	}
	static int toInt(uint4 i) {
		int k = i;
		if(k&0x1)
			k = (-k-1)/2;
		else
			k /= 2;
		return k;
	}
};

//STREAM method used is writeByte...

template <class STREAM> class RangeEncoder: public RangeCoder {
private:
	bool flushed;

public:
	STREAM &output;
	RangeEncoder(STREAM &stream): flushed(false),output(stream) {}
	~RangeEncoder() { if (!flushed) flush(); }

	void encodeChar(uint c) { encodeRange(c, c+1, 1<<8); }
	void encodeShort(uint2 c) { encodeRange(c, c+1, 1<<16); }
	void encodeInt(uint4 c) {
		uint2 h = (c>>16);      encodeShort(h);
		uint2 l = (c & 0xffff); encodeShort(l);
	}

	void encodeRange(uint4 symbol_low, uint4 symbol_high, uint4 total_range) {
		//cout << "Encode " << symbol_low << " total: " << total_range << endl;
		//cout << "T: " << Top << "\nB: " << Bottom << "\nL: " << Low << "\nR: " << Range << endl << endl;
		Low += symbol_low*(Range/=total_range);
		//cout << "ML: " << Low << endl;
		Range *= symbol_high - symbol_low;
		//cout << "MR: " << Range << endl;
		while ((Low ^ (Low+Range))<Top || Range<Bottom && ((Range= -Low & (Bottom-1)),1)) {
			//cout << "R: " << Range << " L: " << Low << endl;
			output.writeByte(Low>>24), Range<<=8, Low<<=8;
			//cout << "R: " << Range << " L: " << Low << endl;
			//cout << (Low ^ (Low+Range)) << " < " << Top << endl;
		}
		//cout << "T: " << Top << "\nB: " << Bottom << "\nL: " << Low << "\nR: " << Range << endl << endl;
	}

	void flush() {
		if(flushed) return;

		for(int i = 0; i < 4; i++) {
			output.writeByte(Low>>24);
			Low<<=8;
		}
		flushed = true;
	}

	template <class Model> void encodeSymbol(uint4 sym, Model &model) {
		int low, high;
		model.getRange(sym, low, high);
		encodeRange((uint4)low, (uint4)high, model.maxRange());
		model.update(sym);
	}

	void encodeArray(uint len, unsigned char *c) {
		for(uint i = 0; i < len; i++)
			encodeChar(c[i]);
	}
};

//STREAM method is readByte
template <class STREAM> class RangeDecoder:public RangeCoder {
private:
	uint4 Code;

public:
	STREAM &input;
	RangeDecoder(STREAM &stream): Code(0), input(stream) {
		for(int i=0;i<4;i++)
			Code = (Code << 8) | input.readByte();
	}

	uint4 getCurrentCount(uint4 TotalRange) {
		return (Code-Low)/(Range/=TotalRange);
	}

	void removeRange(uint4 symbol_low, uint4 symbol_high, uint4 /*TotalRange*/) {
		Low += symbol_low*Range;
		Range *= symbol_high - symbol_low;
		while ((Low ^ Low+Range)<Top || Range<Bottom && ((Range= -Low & Bottom-1),1)) {
			Code = Code<<8 | input.readByte(), Range<<=8, Low<<=8;
		}
	}

	uint decodeChar() {
		uint c = getCurrentCount(1<<8);
		removeRange(c, c+1, 1<<8);
		return c;
	}
	uint2 decodeShort() {
		uint2 c = getCurrentCount(1<<16);
		removeRange(c, c+1, 1<<16);
		return c;
	}
	uint4 decodeInt() {
		uint v = decodeShort()<<16;
		v += decodeShort();
		return v;
	}

	void decodeArray(uint len, unsigned char *c) {
		for(uint i = 0; i < len; i++)
			c[i] = decodeChar();
	}

	template <class Model> uint4 decodeSymbol(Model &model) {

		uint4 count = getCurrentCount(model.maxRange());
		uint4 symbol = model.getSymbol(count);
		int low, high;
		model.getRange(symbol, low, high);
		removeRange(low, high, model.maxRange());
		model.update(symbol);
		return symbol;
	}
};

#endif // NX_RANGE_H
