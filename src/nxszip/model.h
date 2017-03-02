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
#ifndef NX_RANGET_MODEL_H
#define NX_RANGET_MODEL_H

#include <assert.h>

#include <vector>
#include <iostream>

#include "math_class.h"

/* very simple static and adaptive model, good for very limited alphabed (up to 10 elements or so) */

class StaticModel {
public:
	int *prob;
	int size;
	int tot;

	StaticModel(int alphabet = 0): prob(NULL), size(0) {
		if(alphabet)
			initAlphabet(alphabet);
		//    prob.resize(alphabet, 1);
		//    tot = alphabet;
		/*    if(alphabet > 100) {
	  //prob[0] += 100;
	  //tot += 100;
	  for(int i = 1; i < alphabet/2-1; i++) {
		int d = (int)1000/pow(i, 1.5);
		prob[2*i-1] += d;
		tot += d;
		d =  (int)10000/pow(i, 1.5);
		prob[2*i] += d;
		tot += d;
	  }
	}*/
	}
	~StaticModel(){
		if(prob) delete []prob;
	}

	void initAlphabet(int alphabet) {
		if(alphabet != size) {
			if(prob) delete []prob;
			prob = new int[alphabet];
			size = alphabet;
		}
		for(int i = 0; i < size; i++)
			prob[i] = 1;
		//    prob.clear();
		//    prob.resize(alphabet, 1);
		tot = alphabet;
	}
	void setProbability(int symbol, int value) {
		prob[symbol] += value;
		tot += value;
	}

	int maxRange() { return tot; }

	void getRange(int symbol, int &low, int &high) {
		low = 0;
		assert(symbol < size);
		for(int i = 0; i < symbol; i++)
			low += prob[i];
		high = low + prob[symbol];
	}

	int getSymbol(int count) {
		int c = prob[0];
		int symbol = 0;
		while(c <= count) {
			symbol++;
			c += prob[symbol];
		}
		//    assert(symbol < (int)prob.size());
		return symbol;
	}
	void dump() {
		cout << "Probs:\n";
		for(int i = 0; i < size; i++)
			cout << i << "," << prob[i] << endl;
	}
	float entropy() {
		double e = 0;
		for(int i = 0; i < size; i++) {
			double p = (double)prob[i]/(double)tot;
			e += p*Math::log2(p);
		}
		return (float)e;
	}
	void update(int /*symbol*/) {}
};


class AdaptiveModel: public StaticModel {
public:
	int max;

	AdaptiveModel(int alphabet = 0, int _max = (1<<16)):
		StaticModel(alphabet), max(_max) {
	}
	void setMax(int _max) {
		max = _max;
	}
	void update(int symbol) {
		prob[symbol]++;
		tot++;
		if(tot >= max) {
			tot = 0;
			for(int i = 0; i < size; i++) {
				prob[i] >>= 1;
				prob[i]++;
				tot += prob[i];
			}
		}
	}
};

/* THIS WOULD MAKE probabilities converging faster: too bad get symbol gets in the way.
 *
 * each level gets 1<<size prob initially.
 *
 * each symbol gets prob[log2(s)] + (s - 1<<log2(s))*prob[log2(s)]>>log2(s)   initially.
 * each level change prob in 1<<size increments.
 */

class AdaptiveLogModel: public AdaptiveModel {
public:
	double bits;
	int base;
	AdaptiveLogModel(int alphabet = 0):
		AdaptiveModel(alphabet, 1<<(16 - alphabet)) { //this accounts for zero
		base = 1<<alphabet;
		bits = 0.0;
	}

	int getSymbol(int low) {
		int c = prob[0]*base;
		int symbol = 0;
		while(c <= low) {
			symbol++;
			assert(symbol < size);
			c += base*prob[symbol];
		}
		c -= prob[symbol]*base;
		//now c > count.
		int diff = (1<<symbol)*(low - c)/(base*prob[symbol]);
		symbol = (1<<symbol) + diff;

		//    assert(symbol < (int)prob.size());
		return symbol-1;
	}

	int maxRange() { return tot*base; }

	void getRange(int symbol, int &low, int &high) {
		symbol++;
		int p = Math::log2(symbol);
		assert(p < size);
		low = 0;
		for(int i = 0; i < p; i++)
			low += prob[i]*base;
		int step = prob[p]*base/(1<<p);
		low += (symbol - (1<<p))*step;
		high = low + step;

		bits += -log2((high - low)/(double)maxRange());

		assert(0 <= low && low < high && high <= maxRange());
	}

	void update(int symbol) {
		int p = Math::log2(symbol+1);
		prob[p]++;
		tot++;
		if(tot >= max) {
			tot = 0;
			for(int i = 0; i < size; i++) {
				prob[i] >>= 1;
				prob[i]++;
				tot += prob[i];
			}
		}
	}
};

/* Keep cumulative probabilities (faster encoding)
   allows for binary search in decoding */

/*
class ProbabilisticModel {
 public:
  int *cumulative;
  int size;
  int tot;

  StaticModel(int alphabet = 0): cumulative(NULL), size(0) {
	if(alphabet)
	  initAlphabet(alphabet);
  }
  ~StaticModel(){
	if(cumulative) delete []cumulative;
  }

  void initAlphabet(int alphabet) {
	if(alphabet != size) {
	  if(cumulative) delete []cumulative;
	  cumulative = new int[alphabet+1];
	  size = alphabet;
	}
	for(int i = 0; i < size+1; i++)
	  cumulative[i] = i+1;
//    prob.clear();
//    prob.resize(alphabet, 1);
	tot = alphabet;
  }

  int maxRange() { return tot; }

  void getRange(int symbol, int &low, int &high) {
	low = cumulative[i];
	high = cumulative[i+1];
  }

  int getSymbol(int count) {

	int c = prob[0];
	int symbol = 0;
	while(c <= count) {
	  symbol++;
	  c += prob[symbol];
	}
//    assert(symbol < (int)prob.size());
	return symbol;
  }
  void dump() {
	cout << "Probs:\n";
	for(unsigned int i = 0; i < size; i++)
	  cout << i << "," << prob[i] << endl;
  }
  float entropy() {
	double e = 0;
	for(unsigned int i = 0; i < size; i++) {
	  double p = (double)prob[i]/(double)tot;
	  e += p*Math::log2(p);
	}
	return (float)e;
  }
  void update(int ) {}
};*/

#endif // NX_MODEL_H
