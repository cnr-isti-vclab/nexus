/*
Corto
Copyright (c) 2017-2020, Visual Computing Lab, ISTI - CNR
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

BitStream = function(array) {
	var t = this;
	t.a = array;
	t.current = array[0];
	t.position = 0; //position in the buffer
	t.pending = 32;  //bits still to read
};

BitStream.prototype = { 
	read: function(bits) {
		var t = this;
		if(bits > t.pending) {
			t.pending = bits - t.pending;
			var result = (t.current << t.pending)>>>0; //looks the same.
			t.pending = 32 - t.pending;

			t.current = t.a[++t.position];
			result |= (t.current >>> t.pending);
			t.current = (t.current & ((1<<t.pending)-1))>>>0; //slighting faster than mask.
			return result;
		} else { //splitting result in branch seems faster.
			t.pending -= bits;
			var result = (t.current >>> t.pending);
			t.current = (t.current & ((1<<t.pending)-1))>>>0; //slighting faster than mask, 
			return result;
		}
	}
};

Stream = function(buffer, byteOffset, byteLength) {
	var t = this;
	t.data = buffer;
	t.buffer = new Uint8Array(buffer);
	t.pos = byteOffset?byteOffset:0;
	t.view = new DataView(buffer);
};

Stream.prototype = {
	logs: new Uint8Array(16768),

	readChar: function() {
		var c = this.buffer[this.pos++];
		if(c > 127) c -= 256;
		return c;
	},

	readUChar: function() {
		return this.buffer[this.pos++];
	},

	readShort: function() {
		this.pos += 2;
		return this.view.getInt16(this.pos-2, true);
	},

	readFloat: function() {
		this.pos += 4;
		return this.view.getFloat32(this.pos-4, true);
	},

	readInt: function() {
		this.pos += 4;
		return this.view.getInt32(this.pos-4, true);
	},

	readArray: function(n) {
		var a = this.buffer.subarray(this.pos, this.pos+n);
		this.pos += n;
		return a;
	},

	readString: function() {
		var n = this.readShort();
		var s = String.fromCharCode.apply(null, this.readArray(n-1));
		this.pos++; //null terminator of string.
		return s;
	},

	readBitStream:function() {
		var n = this.readInt();
		var pad = this.pos & 0x3;
		if(pad != 0)
			this.pos += 4 - pad;
		var b = new BitStream(new Uint32Array(this.data, this.pos, n));
		this.pos += n*4;
		return b;
	},

	//make decodearray2,3 later //TODO faster to create values here or passing them?
	decodeArray: function(N, values) {
		var t = this;
		var bitstream = t.readBitStream();

		var tunstall = new Tunstall;
		while(t.logs.length < values.length)
			t.logs = new Uint8Array(values.length);

		tunstall.decompress(this, t.logs);

		for(var i = 0; i < t.logs.readed; i++) {
			var diff = t.logs[i];
			if(diff == 0) {
				for(var c = 0; c < N; c++)
					values[i*N + c] = 0;
				continue;
			}
			var max = (1<<diff)>>>1;
			for(var c = 0; c < N; c++)
				values[i*N + c] = bitstream.read(diff) - max;
		}
		return t.logs.readed;
	},

	//assumes values alread allocated
	decodeValues: function(N, values) {
		var t = this;
		var bitstream = t.readBitStream();
		var tunstall = new Tunstall;
		var size = values.length/N;
		while(t.logs.length < size)
			t.logs = new Uint8Array(size);

		for(var c = 0; c < N; c++) {
			tunstall.decompress(this, t.logs);

			for(var i = 0; i < t.logs.readed; i++) {
				var diff = t.logs[i];
				if(diff == 0) {
					values[i*N + c] = 0;
					continue;
				}

				var val = bitstream.read(diff);
				var middle = (1<<(diff-1))>>>0;
				if(val < middle)
					val = -val -middle;
				values[i*N + c] = val;
			}
		}
		return t.logs.readed;
	},

	//assumes values alread allocated
	decodeDiffs: function(values) {
		var t = this;
		var bitstream = t.readBitStream();
		var tunstall = new Tunstall;
		var size = values.length;
		while(t.logs.length < size)
			t.logs = new Uint8Array(size);

		tunstall.decompress(this, t.logs);

		for(var i = 0; i < t.logs.readed; i++) {
			var diff = t.logs[i];
			if(diff == 0) {
				values[i] = 0;
				continue;
			}
			var max = (1<<diff)>>>1;
			values[i] = bitstream.read(diff) - max;
		}
		return t.logs.readed;
	},

	//assumes values alread allocated
	decodeIndices: function(values) {
		var t = this;
		var bitstream = t.readBitStream();

		var tunstall = new Tunstall;
		var size = values.length;
		while(t.logs.length < size)
			t.logs = new Uint8Array(size);

		tunstall.decompress(this, t.logs);

		for(var i = 0; i < t.logs.readed; i++) {
			var ret = t.logs[i];
			if(ret == 0) {
				values[i] = 0;
				continue;
			}
			values[i] = (1<<ret) + bitstream.read(ret) -1;
		}
		return t.logs.readed;
	}
};

function Tunstall() {
}

Tunstall.prototype = {
	wordsize: 8,
	dictionary_size: 256,
	starts: new Uint32Array(256), //starts of each queue
	queue: new Uint32Array(512), //array of n symbols array of prob, position in buffer, length //limit to 8k*3
	index: new Uint32Array(512),
	lengths: new Uint32Array(512),
	table: new Uint8Array(8192), //worst case for 2 

	decompress: function(stream, data) {
		var nsymbols = stream.readUChar();
		this.probs = stream.readArray(nsymbols*2);
		this.createDecodingTables();
		var size = stream.readInt();
		if(size > 100000000) throw("TOO LARGE!");
		if(!data)
			data = new Uint8Array(size);
		if(data.length < size)
			throw "Array for results too small";
		data.readed = size;

		var compressed_size = stream.readInt();
		if(size > 100000000) throw("TOO LARGE!");
		var compressed_data = stream.readArray(compressed_size);
		if(size)
			this._decompress(compressed_data, compressed_size, data, size);
		return data;
	},

	createDecodingTables: function() {
		var t = this;
		var n_symbols = t.probs.length/2;
		if(n_symbols <= 1) return;

		var queue = Tunstall.prototype.queue;

		var end = 0; //keep track of queue end
		var pos = 0; //keep track of buffer first free space
		var n_words = 0;

		//Here probs will range from 0 to 0xffff for better precision
		for(var i = 0; i < n_symbols; i++)
			queue[i] = t.probs[2*i+1] << 8;

		var max_repeat = Math.floor((t.dictionary_size - 1)/(n_symbols - 1));
		var repeat = 2;
		var p0 = queue[0];
		var p1 = queue[1];
		var prob = (p0*p0)>>>16;
		while(prob > p1 && repeat < max_repeat) {
			prob = (prob*p0)>>> 16;
			repeat++;
		}

		if(repeat >= 16) { //Very low entropy results in large tables > 8K.
			t.table[pos++] = t.probs[0];
			for(var k = 1; k < n_symbols; k++) {
				for(var i = 0; i < repeat-1; i++)
					t.table[pos++] = t.probs[0];
				t.table[pos++] = t.probs[2*k];
			}
			t.starts[0] = (repeat-1)*n_symbols;
			for(var k = 1; k < n_symbols; k++)
				t.starts[k] = k;

			for(var col = 0; col < repeat; col++) {
				for(var row = 1; row < n_symbols; row++) {
					var off = (row + col*n_symbols);
					if(col > 0)
						queue[off] = (prob * queue[row]) >> 16;
					t.index[off] = row*repeat - col;
					t.lengths[off] = col+1;
				}
				if(col == 0)
					prob = p0;
				else
					prob = (prob*p0) >>> 16;
			}
			var first = ((repeat-1)*n_symbols);
			queue[first] = prob;
			t.index[first] = 0;
			t.lengths[first] = repeat;

			n_words = 1 + repeat*(n_symbols - 1);
			end = repeat*n_symbols;
		} else {
			//initialize adding all symbols to queues
			for(var i = 0; i < n_symbols; i++) {
				queue[i] = t.probs[i*2+1]<<8;
				t.index[i] = i;
				t.lengths[i] = 1;
	
				t.starts[i] = i;
				t.table[i] = t.probs[i*2];
			}
			pos = n_symbols;
			end = n_symbols;
			n_words = n_symbols;
		}

		//at each step we grow all queues using the most probable sequence
		while(n_words < t.dictionary_size) {
			//find highest probability word
			var best = 0;
			var max_prob = 0;
			for(var i = 0; i < n_symbols; i++) {
				var p = queue[t.starts[i]]; //front of queue probability.
				if(p > max_prob) {
					best = i;
					max_prob = p;
				}
			}
			var start = t.starts[best];
			var offset = t.index[start];
			var len = t.lengths[start];

			for(var i = 0; i < n_symbols; i++) {
				queue[end] = (queue[i] * queue[start])>>>16;
				t.index[end] = pos;
				t.lengths[end] = len + 1;
				end++;

				for(var k  = 0; k < len; k++)
					t.table[pos + k] = t.table[offset + k]; //copy sequence of symbols
				pos += len;
				t.table[pos++] = t.probs[i*2]; //append symbol
				if(i + n_words == t.dictionary_size - 1)
					break;
			}
			if(i == n_symbols)
				t.starts[best] += n_symbols; //move one column
			n_words += n_symbols -1;
		}

		var word = 0;
		for(i = 0, row = 0; i < end; i ++, row++) {
			if(row >= n_symbols)
				row  = 0;
			if(t.starts[row] > i) continue; //skip deleted words

			t.index[word] = t.index[i];
			t.lengths[word] = t.lengths[i];
			word++;
		}
	},

	_decompress: function(input, input_size, output, output_size) {
		//TODO optimize using buffer arrays
		var input_pos = 0;
		var output_pos = 0;
		if(this.probs.length == 2) {
			var symbol = this.probs[0];
			for(var i = 0; i < output_size; i++)
				output[i] = symbol;
			return;
		}

		while(input_pos < input_size-1) {
			var symbol = input[input_pos++];
			var start = this.index[symbol];
			var end = start + this.lengths[symbol];
			for(var i = start; i < end; i++) 
				output[output_pos++] = this.table[i];
		}

		//last symbol might override so we check.
		var symbol = input[input_pos];
		var start = this.index[symbol];
		var end = start + output_size - output_pos;
		var length = output_size - output_pos;
		for(var i = start; i < end; i++)
			output[output_pos++] = this.table[i];

		return output;
	}
};

function Attribute(name, q, components, type, strategy) {
	var t = this;
	t.name = name;
	t.q = q; //float
	t.components = components; //integer
	t.type = type;
	t.strategy = strategy;
};

Attribute.prototype = {
	Type: { UINT32:0, INT32:1, UINT16:2, INT16:3, UINT8:4, INT8:5, FLOAT:6, DOUBLE:7 },

	Strategy: { PARALLEL:1, CORRELATED:2 },

	init: function(nvert, nface) {
		var t = this;
		var n = nvert*t.components;
		t.values = new Int32Array(n);  //local workspace 

		//init output buffers
		switch(t.type) {
		case t.Type.UINT32:
		case t.Type.INT32: t.values = t.buffer = new Int32Array(n); break; //no point replicating.
		case t.Type.UINT16:
		case t.Type.INT16: t.buffer = new Int16Array(n); break;
		case t.Type.UINT8: t.buffer = new Uint8Array(n); break;
		case t.Type.INT8: t.buffer  = new Int8Array(n); break;
		case t.Type.FLOAT:
		case t.Type.DOUBLE: t.buffer = new Float32Array(n); break;
		default: throw "Error if reading";
		}
	},

	decode: function(nvert, stream) {
		var t = this;
		if(t.strategy & t.Strategy.CORRELATED) //correlated
			stream.decodeArray(t.components, t.values);
		else
			stream.decodeValues(t.components, t.values);
	},

	deltaDecode: function(nvert, context) {
		var t = this;
		var values = t.values;
		var N = t.components;

		if(t.strategy & t.Strategy.PARALLEL) { //parallel
			var n = context.length/3;
			for(var i = 1; i < n; i++) {
				for(var c = 0; c < N; c++) {
					values[i*N + c] += values[context[i*3]*N + c] + values[context[i*3+1]*N + c] - values[context[i*3+2]*N + c];
				}
			}
		} else if(context) {
			var n = context.length/3;
			for(var i = 1; i < n; i++)
				for(var c = 0; c < N; c++)
					values[i*N + c] += values[context[i*3]*N + c];
		} else {
			for(var i = N; i < nvert*N; i++)
				values[i] += values[i - N];
		}
	},

	postDelta: function() {},

	dequantize: function(nvert) {
		var t= this;
		var n = t.components*nvert;
		switch(t.type) {
		case t.Type.UINT32:
		case t.Type.INT32: break;
		case t.Type.UINT16:
		case t.Type.INT16: 
		case t.Type.UINT8: 
		case t.Type.INT8: 
			for(var i = 0; i < n; i++)
				t.buffer[i] = t.values[i]*t.q;
			break;
		case t.Type.FLOAT:
		case t.Type.DOUBLE: 
			for(var i = 0; i < n; i++)
				t.buffer[i] = t.values[i]*t.q;
			break;
		}
	}
};

/* COLOR ATTRIBUTE */

function ColorAttr(name, q, components, type, strategy) {
	Attribute.call(this, name, q, components, type, strategy);
	this.qc = [];
	this.outcomponents = 3;
};

ColorAttr.prototype = Object.create(Attribute.prototype);

ColorAttr.prototype.decode = function(nvert, stream) {
	for(var c = 0; c < 4; c++)
		this.qc[c] = stream.readUChar();
	Attribute.prototype.decode.call(this, nvert, stream);
};

ColorAttr.prototype.dequantize = function(nvert) {
	var t = this;

	for(var i = 0; i < nvert; i++) {
		var offset = i*4;
		var rgboff = i*t.outcomponents;

		var e0 = t.values[offset + 0];
		var e1 = t.values[offset + 1];
		var e2 = t.values[offset + 2];

		t.buffer[rgboff + 0] = ((e2 + e0)* t.qc[0])&0xff;
		t.buffer[rgboff + 1] = e0* t.qc[1];
		t.buffer[rgboff + 2] = ((e1 + e0)* t.qc[2])&0xff;
		t.buffer[offset + 3] = t.values[offset + 3] * t.qc[3];
	}
};

/* NORMAL ATTRIBUTE */

function NormalAttr(name, q, components, type, strategy) {
	Attribute.call(this, name, q, components, type, strategy);
};

NormalAttr.prototype = Object.create(Attribute.prototype);

NormalAttr.prototype.Prediction = { DIFF: 0, ESTIMATED: 1, BORDER: 2 };

NormalAttr.prototype.init = function(nvert, nface) {
	var t = this;
	var n = nvert*t.components;
	t.values = new Int32Array(2*nvert);  //local workspace 

	//init output buffers
	switch(t.type) {
	case t.Type.INT16: t.buffer = new Int16Array(n); break;
	case t.Type.FLOAT:
	case t.Type.DOUBLE: t.buffer = new Float32Array(n); break;
	default: throw "Error if reading";
	}
};

NormalAttr.prototype.decode = function(nvert, stream) {
	var t = this;
	t.prediction = stream.readUChar();

	stream.decodeArray(2, t.values);
};

NormalAttr.prototype.deltaDecode = function(nvert, context) {
	var t = this;
	if(t.prediction != t.Prediction.DIFF)
		return;

	if(context) {
		for(var i = 1; i < nvert; i++) {
			for(var c = 0; c < 2; c++) {
				var d = t.values[i*2 + c];
				t.values[i*2 + c] += t.values[context[i*3]*2 + c];
			}
		}
	} else { //point clouds assuming values are already sorted by proximity.
		for(var i = 2; i < nvert*2; i++) {
			var d = t.values[i];
			t.values[i] += t.values[i-2];
		}
	}
};

NormalAttr.prototype.postDelta = function(nvert, nface, attrs, index) {
	var t = this;
	//for border and estimate we need the position already deltadecoded but before dequantized
	if(t.prediction == t.Prediction.DIFF)
		return;

	if(!attrs.position)
		throw "No position attribute found. Use DIFF normal strategy instead.";

	var coord = attrs.position;

	t.estimated = new Float32Array(nvert*3);
	t.estimateNormals(nvert, coord.values, nface, index.faces);

	if(t.prediction == t.Prediction.BORDER) {
		t.boundary = new Uint32Array(nvert);
		t.markBoundary(nvert, nface, index.faces, t.boundary);
	}

	t.computeNormals(nvert);
};

NormalAttr.prototype.dequantize = function(nvert) {
	var t = this;
	if(t.prediction != t.Prediction.DIFF)
		return;

	for(var i = 0; i < nvert; i++)
		t.toSphere(i, t.values, i, t.buffer, t.q)
};

NormalAttr.prototype.computeNormals = function(nvert) {
	var t = this;
	var norm = t.estimated;

	if(t.prediction == t.Prediction.ESTIMATED) {
		for(var i = 0; i < nvert; i++) {
			t.toOcta(i, norm, i, t.values, t.q);
			t.toSphere(i, t.values, i, t.buffer, t.q);
		}

	} else { //BORDER
		var count = 0; //here for the border.
		for(var i = 0, k = 0; i < nvert; i++, k+=3) {
			if(t.boundary[i] != 0) {
				t.toOcta(i, norm, count, t.values, t.q);
				t.toSphere(count, t.values, i, t.buffer, t.q);
				count++;

			} else { //no correction
				var len = 1/Math.sqrt(norm[k]*norm[k] + norm[k+1]*norm[k+1] + norm[k+2]*norm[k+2]);
				if(t.type == t.Type.INT16)
					len *= 32767;

				t.buffer[k] = norm[k]*len;
				t.buffer[k+1] = norm[k+1]*len;
				t.buffer[k+2] = norm[k+2]*len;  
			}
		}
	}
};

NormalAttr.prototype.markBoundary = function( nvert,  nface, index, boundary) {
	for(var f = 0; f < nface*3; f += 3) {
		boundary[index[f+0]] ^= index[f+1] ^ index[f+2];
		boundary[index[f+1]] ^= index[f+2] ^ index[f+0];
		boundary[index[f+2]] ^= index[f+0] ^ index[f+1];
	}
};


NormalAttr.prototype.estimateNormals = function(nvert, coords, nface, index) {
	var t = this;
	for(var f = 0; f < nface*3; f += 3) {
		var a = 3*index[f + 0];
		var b = 3*index[f + 1];
		var c = 3*index[f + 2];

		var ba0 = coords[b+0] - coords[a+0];
		var ba1 = coords[b+1] - coords[a+1];
		var ba2 = coords[b+2] - coords[a+2];

		var ca0 = coords[c+0] - coords[a+0];
		var ca1 = coords[c+1] - coords[a+1];
		var ca2 = coords[c+2] - coords[a+2];

		var n0 = ba1*ca2 - ba2*ca1;
		var n1 = ba2*ca0 - ba0*ca2;
		var n2 = ba0*ca1 - ba1*ca0;

		t.estimated[a + 0] += n0;
		t.estimated[a + 1] += n1;
		t.estimated[a + 2] += n2;
		t.estimated[b + 0] += n0;
		t.estimated[b + 1] += n1;
		t.estimated[b + 2] += n2;
		t.estimated[c + 0] += n0;
		t.estimated[c + 1] += n1;
		t.estimated[c + 2] += n2;
	}
};

//taks input in ingress at i offset, adds out at c offset
NormalAttr.prototype.toSphere = function(i, input, o, out, unit) {

	var t = this;
	var j = i*2;
	var k = o*3;
	var av0 = input[j] > 0? input[j]:-input[j];
	var av1 = input[j+1] > 0? input[j+1]:-input[j+1];
	out[k] = input[j];
	out[k+1] = input[j+1];
	out[k+2] = unit - av0 - av1;
	if (out[k+2] < 0) {
		out[k] = (input[j] > 0)? unit - av1 : av1 - unit;
		out[k+1] = (input[j+1] > 0)? unit - av0: av0 - unit;
	}
	var len = 1/Math.sqrt(out[k]*out[k] + out[k+1]*out[k+1] + out[k+2]*out[k+2]);
	if(t.type == t.Type.INT16)
		len *= 32767;

	out[k] *= len;
	out[k+1] *= len;
	out[k+2] *= len;
};

NormalAttr.prototype.toOcta = function(i, input, o, output, unit) {
	var k = o*2;
	var j = i*3; //input

	var av0 = input[j] > 0? input[j]:-input[j];
	var av1 = input[j+1] > 0? input[j+1]:-input[j+1];
	var av2 = input[j+2] > 0? input[j+2]:-input[j+2];
	var len = av0 + av1 + av2;
	var p0 = input[j]/len;
	var p1 = input[j+1]/len;

	var ap0 = p0 > 0? p0: -p0;
	var ap1 = p1 > 0? p1: -p1;

	if(input[j+2] < 0) {
		p0 = (input[j] >= 0)? 1.0 - ap1 : ap1 - 1;
		p1 = (input[j+1] >= 0)? 1.0 - ap0 : ap0 - 1;
	}
	output[k] += p0*unit;
	output[k+1] += p1*unit;
/*
		Point2f p(v[0], v[1]);
		p /= (fabs(v[0]) + fabs(v[1]) + fabs(v[2]));

		if(v[2] < 0) {
			p = Point2f(1.0f - fabs(p[1]), 1.0f - fabs(p[0]));
			if(v[0] < 0) p[0] = -p[0];
			if(v[1] < 0) p[1] = -p[1];
		}
		return Point2i(p[0]*unit, p[1]*unit);
*/
};

/* INDEX ATTRIBUTE */

function IndexAttr(nvert, nface, type) {
	var t = this;
	if((!type && nface < (1<<16)) || type == 0) //uint16
		t.faces = new Uint16Array(nface*3);
	else if(!type || type == 2) //uint32 
		t.faces = new Uint32Array(nface*3);
	else
		throw "Unsupported type";

	t.prediction = new Uint32Array(nvert*3);
};

IndexAttr.prototype = {
	decode: function(stream) {
		var t = this;

		var max_front = stream.readInt();
		t.front = new Int32Array(max_front*5);

		var tunstall = new Tunstall;
		t.clers = tunstall.decompress(stream);
		t.bitstream = stream.readBitStream();
	},

	decodeGroups: function(stream) {
		var t = this;
		var n = stream.readInt();
		t.groups = new Array(n);
		for(var i = 0; i < n; i++) {
			var end = stream.readInt();
			var np = stream.readUChar();
			var g = { end: end, properties: {} };
			for(var k = 0; k < np; k++) {
				var key = stream.readString();
				g.properties[key] = stream.readString();
			}
			t.groups[i] = g;
		}
	}
};

onmessage = function(job) {
	if(typeof(job.data) == "string") return;

	var buffer = job.data.buffer;
	if(!buffer) return;

	var decoder = new CortoDecoder(buffer);

	if(decoder.attributes.normal && job.data.short_normals)
		decoder.attributes.normal.type = 3;
	if(decoder.attributes.color && job.data.rgba_colors)
		decoder.attributes.color.outcomponents = 4;

	var model = decoder.decode();

	//pass back job
	postMessage({ model: model, buffer: buffer, request: job.data.request});
};


function CortoDecoder(data, byteOffset, byteLength) {
	if(byteOffset & 0x3)
		throw "Memory aligned on 4 bytes is mandatory";

	var t = this;
	var stream = t.stream = new Stream(data, byteOffset, byteLength);

	var magic = stream.readInt();
	if(magic != 2021286656) return;

	var version = stream.readInt();
	t.entropy = stream.readUChar();
	//exif
	t.geometry = {};
	var n = stream.readInt();
	for(var i = 0; i < n; i++) {
		var key = stream.readString();
		t.geometry[key] = stream.readString();
	}

	//attributes
	var n = stream.readInt();

	t.attributes = {};
	for(var i = 0; i < n; i++) {
		var a = {};
		var name = stream.readString();
		var codec = stream.readInt();
		var q = stream.readFloat();
		var components = stream.readUChar(); //internal number of components
		var type = stream.readUChar();     //default type (same as it was in input), can be overridden
		var strategy = stream.readUChar();
		var attr;
		switch(codec) {
		case 2: attr = NormalAttr; break;
		case 3: attr = ColorAttr; break;
		case 1: //generic codec
		default: attr = Attribute; break;
		}
		t.attributes[name] = new attr(name, q, components, type, strategy);
	}

//TODO move this vars into an array.
	t.geometry.nvert = t.nvert = t.stream.readInt();
	t.geometry.nface = t.nface = t.stream.readInt();
};

CortoDecoder.prototype = {
	decode: function() {
		var t = this;

		t.last = new Uint32Array(t.nvert*3); //for parallelogram prediction
		t.last_count = 0;

		for(var i in t.attributes)
			t.attributes[i].init(t.nvert, t.nface);

		if(t.nface == 0)
			t.decodePointCloud();
		else
			t.decodeMesh();

		return t.geometry;
	},

	decodePointCloud: function() {
		var t = this;
		t.index = new IndexAttr(t.nvert, t.nface, 0);
		t.index.decodeGroups(t.stream);
		t.geometry.groups = t.index.groups;
		for(var i in t.attributes) {
			var a = t.attributes[i];
			a.decode(t.nvert, t.stream);
			a.deltaDecode(t.nvert);
			a.dequantize(t.nvert);
			t.geometry[a.name] = a.buffer;
		}
	},

	decodeMesh: function() {
		var t = this;
		t.index = new IndexAttr(t.nvert, t.nface);
		t.index.decodeGroups(t.stream);
		t.index.decode(t.stream);

		t.vertex_count = 0;
		var start = 0;
		t.cler = 0;
		for(var p = 0; p < t.index.groups.length; p++) {
			var end = t.index.groups[p].end;
			this.decodeFaces(start *3, end *3);
			start = end;
		}
		t.geometry['index'] = t.index.faces;
		t.geometry.groups = t.index.groups;
		for(var i in t.attributes) 
			t.attributes[i].decode(t.nvert, t.stream);
		for(var i in t.attributes) 
			t.attributes[i].deltaDecode(t.nvert, t.index.prediction);
		for(var i in t.attributes) 
			t.attributes[i].postDelta(t.nvert, t.nface, t.attributes, t.index);
		for(var i in t.attributes) { 
			var a = t.attributes[i];
			a.dequantize(t.nvert);
			t.geometry[a.name] = a.buffer;
		}
	},

	/*
	An edge is: uint16_t face, uint16_t side, uint32_t prev, next, bool deleted
	I do not want to create millions of small objects, I will use aUint32Array.
	Problem is how long, sqrt(nface) we will over blow using nface.
	*/

	ilog2: function(p) {
		var k = 0;
		while ( p>>=1 ) { ++k; }
		return k;
	},

	decodeFaces: function(start, end) {

		var t = this;
		var clers = t.index.clers;
		var bitstream = t.index.bitstream;

		var front = t.index.front;
		var front_count = 0; //count each integer so it's front_back*5

		function addFront(_v0, _v1, _v2, _prev, _next) {
			front[front_count] = _v0;
			front[front_count+1] = _v1;
			front[front_count+2] = _v2;
			front[front_count+3] = _prev;
			front[front_count+4] = _next;
			front_count += 5;
		}

		var faceorder = new Uint32Array((end - start));
		var order_front = 0;
		var order_back = 0;

		var delayed = [];

		var splitbits = t.ilog2(t.nvert) + 1;

		var new_edge = -1;

		var prediction = t.index.prediction;

		while(start < end) {

			if(new_edge == -1 && order_front >= order_back && !delayed.length) {

				var last_index = t.vertex_count-1;
				var vindex = [];

				var split = 0;
				if(clers[t.cler++] == 6) { //split look ahead
					split = bitstream.read(3);
				}

				for(var k = 0; k < 3; k++) {
					var v;
					if(split & (1<<k))
						v = bitstream.read(splitbits);
					else {
						prediction[t.vertex_count*3] = prediction[t.vertex_count*3+1] = prediction[t.vertex_count*3+2] = last_index;
						last_index = v = t.vertex_count++;
					}
					vindex[k] = v;
					t.index.faces[start++] = v;
				}

				var current_edge = front_count;
				faceorder[order_back++] = front_count;
				addFront(vindex[1], vindex[2], vindex[0], current_edge + 2*5, current_edge + 1*5);
				faceorder[order_back++] = front_count;
				addFront(vindex[2], vindex[0], vindex[1], current_edge + 0*5, current_edge + 2*5);
				faceorder[order_back++] = front_count;
				addFront(vindex[0], vindex[1], vindex[2], current_edge + 1*5, current_edge + 0*5);
				continue;
			}
			var edge;
			if(new_edge != -1) {
				edge = new_edge;
				new_edge = -1;
			} else if(order_front < order_back) {
				edge = faceorder[order_front++];
			} else {
				edge = delayed.pop();
			}
			if(typeof(edge) == "undefined")
				throw "aarrhhj";

			if(front[edge] < 0) continue; //deleted

			var c = clers[t.cler++];
			if(c == 4) continue; //BOUNDARY

			var v0   = front[edge + 0];
			var v1   = front[edge + 1];
			var v2   = front[edge + 2];
			var prev = front[edge + 3];
			var next = front[edge + 4];

			new_edge = front_count; //points to new edge to be inserted
			var opposite = -1;
			if(c == 0 || c == 6) { //VERTEX
				if(c == 6) { //split
					opposite = bitstream.read(splitbits);
				} else {
					prediction[t.vertex_count*3] = v1;
					prediction[t.vertex_count*3+1] = v0;
					prediction[t.vertex_count*3+2] = v2;
					opposite = t.vertex_count++;
				}

				front[prev + 4] = new_edge;
				front[next + 3] = new_edge + 5;

				front[front_count] = v0;
				front[front_count + 1] = opposite;
				front[front_count + 2] = v1;
				front[front_count + 3] = prev;
				front[front_count + 4] = new_edge+5;
				front_count += 5; 

				faceorder[order_back++] = front_count;

				front[front_count] = opposite;
				front[front_count + 1] = v1;
				front[front_count + 2] = v0;
				front[front_count + 3] = new_edge; 
				front[front_count + 4] = next;
				front_count += 5; 

			} else if(c == 1) { //LEFT

				front[front[prev + 3] + 4] = new_edge;
				front[next + 3] = new_edge;
				opposite = front[prev];

				front[front_count] = opposite;
				front[front_count + 1] = v1;
				front[front_count + 2] = v0;
				front[front_count + 3] = front[prev + 3];
				front[front_count + 4] = next;
				front_count += 5; 

				front[prev] = -1; //deleted

			} else if(c == 2) { //RIGHT
				front[front[next + 4] + 3] = new_edge;
				front[prev + 4] = new_edge;
				opposite = front[next + 1];

				front[front_count] = v0;
				front[front_count + 1] = opposite;
				front[front_count + 2] = v1;
				front[front_count + 3] = prev;
				front[front_count + 4] = front[next+4];
				front_count += 5; 

				front[next] = -1;

			} else if(c == 5) { //DELAY
				delayed.push(edge);
				new_edge = -1;

				continue;

			} else if(c == 3) { //END
				front[front[prev + 3] + 4] = front[next + 4];
				front[front[next + 4] + 3] = front[prev + 3];
				
				opposite = front[prev];

				front[prev] = -1;
				front[next] = -1;
				new_edge = -1;

			} else {
				throw "INVALID CLER!";
			}
			if(v1 >= t.nvert || v0 >= t.nvert || opposite >= t.nvert)
				throw "Topological error";
			t.index.faces[start] = v1;
			t.index.faces[start+1] = v0;
			t.index.faces[start+2] = opposite;
			start += 3;
		}
	}
};


