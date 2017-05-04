/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/
#ifndef NX_TUNSTALL_H
#define NX_TUNSTALL_H

#include "cstream.h"
#include <vector>


/* SIMPLEST TUNSTALL encoding:
   1) build dictionary:
	  split root, keep track of highest probability, keep splitting until all words are assigned.
	  the final structure for decompression is 2 tables:
		1) index table block -> start in the second table (length is derived from next word start)
		2) words: all the words one after the other
	  structure for compression is 1 table:
	  256 entries (8bit) mapping
		if(lenght < 8bit) maps beginning of the word to length (6 bit e.g.) and code
		else maps to second 8bit table with same approach. each entry longer than 8 bits gets another table.
	  all tables are just concatenated.
*/

namespace meco {

class Tunstall {
public:
	//word size 8 means use 8 bit blocks.
	Tunstall(int _wordsize = 8, int _lookup = 2): wordsize(_wordsize), lookup_size(_lookup) {}

	int compress(CStream &stream, unsigned char *data, int size); //return compressed size
	void decompress(CStream &stream, std::vector<unsigned char> &output); //allocate and decompress

	//computes symbol probabilities from the stream
	void getProbabilities(unsigned char *data, int size);

	//set probabilities explicitly
	void setProbabilities(float *probabilities, int n_symbols);

	//create accelerated structures (need probabilities);
	void createDecodingTables();
	void createEncodingTables();

	//data 1 symbol 1 char, if using binary input convert them, the output is in bits though.
	//set probabilities and create tables before this.
	//output is padded, careful!
	unsigned char *compress(unsigned char *data, int input_size, int &output_size);

	//output_size is the NUMBER of symbols created (and the output is 1 symbol 1 char)
	//we need it because of the padding!
	void decompress(unsigned char *data, int input_size, unsigned char *output, int output_size);
	//we can do without the input size.
	int decompress(unsigned char *data, unsigned char *output, int output_size);

	//return entropy of the dictionary
	float entropy();

	static uint32_t toUint(int i) {
		i *= 2;
		if(i < 0) i = -i -1;
		return (uint32_t)i;
	}
	static int toInt(uint32_t i) {
		int k = i;
		if(k&0x1)
			k = (-k-1)/2;
		else
			k /= 2;
		return k;
	}

	//protected:
	struct Symbol {
		Symbol() {}
		Symbol(unsigned char s, unsigned char p): symbol(s), probability(p) {}
		unsigned char symbol;
		unsigned char probability;
		bool operator<(const Symbol &s) const {
			return probability > s.probability;
			//TODO check if reversgin order is faster
		}
	};

	int wordsize;
	int dictionarysize;
	std::vector<Symbol> probabilities; //sorted

	//decoding structure
	std::vector<int> index;
	std::vector<int> lengths;
	std::vector<unsigned char> table;

	//encoding structure
	int lookup_size;
	std::vector<int> offsets;
	std::vector<unsigned char> remap;  //remaps symbols to probabilities

	//user for encoding structure: returns interval in offset table used by word
	void wordCode(unsigned char *w, int length, int &low, int &high) {
		int n_symbols = probabilities.size();
		//counting in base n_symbols
		int c = 0;
		for(int i = 0; i < length && i < lookup_size; i++) {
			c *= n_symbols;
			c += remap[w[i]];
		}
		low = high = c;
		high++;
		for(int i = length; i < lookup_size; i++) {
			low *= n_symbols;
			high *= n_symbols;
		}
	}



	static   int roundUp(int v) {
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	}

	static void decompose(std::vector<int> &input, std::vector<unsigned char> &output, int threshold);
	static void recompose(std::vector<unsigned char> &input, std::vector<int> &output, int threshold);

};


//reserve 1 symbol (255) to switch state for very low probability symbols
/*class BiTunstall: public Tunstall {
 public:
  vector<char> outliers; //255 entries. points 1 outlier, 0 inlier
  int special;

  void createDecodingTables();
  unsigned char *compress(unsigned char *data, int input_size, int &output_size);
  void decompress(unsigned char *data, int input_size, unsigned char *output, int output_size);
  int decompress(unsigned char *data, unsigned char *output, int output_size);

};*/

} //namespace
#endif // NX_TUNSTALL_H
