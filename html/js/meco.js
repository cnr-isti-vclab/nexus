/*
Nexus
Copyright (c) 2012-2018, Visual Computing Lab, ISTI - CNR
All rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

onmessage = function(job) {
	if(typeof(job.data) == "string") return;
	var node = job.data.node;
	var signature = job.data.signature;
	var patches = job.data.patches;
//	var now =new Date().getTime();

	var size;
	if(!node.buffer) return;
	else size = node.buffer.byteLength;
	var buffer;
	for(var i =0 ; i < 1; i++) {
		var coder = new MeshCoder(signature, node, patches);
		buffer = coder.decode(node.buffer);
	}
	node.buffer = buffer;
	node.owner = job.owner;
//	var elapsed = new Date().getTime() - now;
//	var t = node.nface;
//	console.log("Z Time: " + elapsed + " Size: " + size + " KT/s " + (t/(elapsed)) + " Mbps " + (8*1000*node.buffer.byteLength/elapsed)/(1<<20));
	postMessage(node);
}

//	actually bitstreams expects a little endian uin64 type. convert it to 2 uint32

BitStream = function(array) { 
	this.a = array;
	for(var i = 0; i < array.length; i += 2) {
		var s = array[i];
		array[i] = array[i+1];
		array[i+1] = s;
	}
	this.position = 0;
	this.bitsPending = 0;
};
 
BitStream.prototype = { 
	read: function(bits) {
		var bitBuffer = 0;
		while(bits > 0) {
			var partial;
			var bitsConsumed;
			if (this.bitsPending > 0) {
				var byte = (this.a[this.position - 1] & (0xffffffff >>> (32 - this.bitsPending)))>>>0;
				bitsConsumed = Math.min(this.bitsPending, bits);
				this.bitsPending -= bitsConsumed;
				partial = byte >>> this.bitsPending;
			} else {
				bitsConsumed = Math.min(32, bits);
				this.bitsPending = 32 - bitsConsumed;
				partial = this.a[this.position++] >>> this.bitsPending;
			}
			bits -= bitsConsumed;
			bitBuffer = ((bitBuffer << bitsConsumed) | partial)>>>0;
		}
		return bitBuffer; 
	},
	replace: function(bits, value) {
		//zero last part
		value = (value & (0xffffffff >>> 32 - bits)) >>> 0;
		value = (value | read(bits)) >>> 0;
		return value;
	}
};

Stream = function(buffer) {
	this.data = buffer;
	this.buffer = new Uint8Array(buffer);
	this.pos = 0;
}

Stream.prototype = {
	readChar: function() {
		var c = this.buffer[this.pos++];
		if(c > 127) c -= 256;
		return c;
	},
	readUChar: function() {
		return this.buffer[this.pos++];
	},	
	readInt: function() {
		var c = this.buffer[this.pos + 3]
		c <<= 8;
		c |= this.buffer[this.pos + 2];
		c <<= 8;
		c |= this.buffer[this.pos + 1];
		c <<= 8;
		c |= this.buffer[this.pos + 0];
		this.pos += 4;
		return c;
	},
	readArray: function(n) {
		var a = this.buffer.subarray(this.pos, this.pos+n);
		this.pos += n;
		return a;
	},
	readBitStream:function() {
		var n = this.readInt();
		var pad = this.pos & 0x3;
		if(pad != 0)
			this.pos += 4 - pad;
		var b = new BitStream(new Uint32Array(this.data, this.pos, n*2));
		this.pos += n*8;
		return b;
	}
};

function Tunstall(wordsize, lookup_size) {
	this.wordsize = wordsize? wordsize : 8;
	this.lookup_size = lookup_size? lookup_size : 8;
}

Tunstall.prototype = {
	decompress: function(stream) {
		var nsymbols = stream.readUChar();
		this.probabilities = stream.readArray(nsymbols*2);
		this.createDecodingTables();
		var size = stream.readInt();
		var data = new Uint8Array(size);
		var compressed_size = stream.readInt();
		var compressed_data = stream.readArray(compressed_size);
		if(size)
			this._decompress(compressed_data, compressed_size, data, size);
		return data;
	}, 

	createDecodingTables: function() {
		//read symbol,prob,symbol,prob as uchar.
		//Here probabilities will range from 0 to 0xffff for better precision

		var n_symbols = this.probabilities.length/2;
		if(n_symbols <= 1) return;

		var queues = []; //array of arrays
		var buffer = []; 

		//initialize adding all symbols to queues
		for(var i = 0; i < n_symbols; i++) {
			var symbol = this.probabilities[i*2];
			var s = [(this.probabilities[i*2+1])<<8, buffer.length, 1]; //probability, position in the buffer, length
			queues[i] = [s];
			buffer.push(this.probabilities[i*2]); //symbol
		}
		var dictionary_size = 1<<this.wordsize;
		var n_words = n_symbols;
		var table_length = n_symbols;

		//at each step we grow all queues using the most probable sequence
		while(n_words < dictionary_size - n_symbols +1) {
			//Should use a stack or something to be faster, but we have few symbols
			//find highest probability word
			var best = 0;
			var max_prob = 0;
			for(var i = 0; i < n_symbols; i++) {
				var p = queues[i][0][0]; //front of queue probability.
				if(p > max_prob) {
					best = i;
					max_prob = p;
				}
			}
			var symbol = queues[best][0];
			var pos = buffer.length;
			
			for(var i = 0; i < n_symbols; i++) {
				var sym = this.probabilities[i*2];
				var prob = this.probabilities[i*2+1]<<8;
				var s = [((prob*symbol[0])>>>16), pos, symbol[2]+1]; //combine probabilities, keep track of buffer, keep length of queue

				for(var k  = 0; k < symbol[2]; k++)
					buffer[pos+k] = buffer[symbol[1] + k]; //copy sequence of symbols

				pos += symbol[2];
				buffer[pos++] = sym; //append symbol
				queues[i].push(s);
			}
			table_length += (n_symbols-1)*(symbol[2] + 1) +1; 
			n_words += n_symbols -1;
			queues[best].shift(); //remove first thing
		}

		this.index = new Uint32Array(n_words);
		this.lengths = new Uint32Array(n_words);
		this.table = new Uint8Array(table_length);
		var word = 0;
		var pos = 0;
		for(i = 0; i < queues.length; i++) {
			var queue = queues[i];
			for(var k = 0; k < queue.length; k++) {
				var s = queue[k];
				this.index[word] = pos;
				this.lengths[word] = s[2]; //length
				word++;

				for(var j = 0; j < s[2]; j++)
					this.table[pos + j] = buffer[s[1] + j]; //buffer of offset
				pos += s[2]; //length
			}
		}
	},
	_decompress: function(input, input_size, output, output_size) {
		var input_pos = 0;
		var output_pos = 0;
		if(this.probabilities.length == 2) {
			var symbol = this.probabilities[0];
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
}

ZPoint = function(h, l) {
	this.lo = l;
	this.hi = h;
}

ZPoint.prototype = {
	copy: function(z) {
		this.lo = z.lo;
		this.hi = z.hi;
	},
	setBit: function(d) {
		if(d < 32)
			this.lo = (this.lo | (1<<d))>>>0;
		else
			this.hi = (this.hi | (1<<(d-32)))>>>0;
	},
	toPoint: function(min, step, buffer, pos) {
		var x = this.morton3(this.lo, this.hi>>>1);
		var y = this.morton3(this.lo>>>1, this.hi>>>2);
		var z = this.morton3((this.lo>>>2 | (this.hi & 0x1)<<30 )>>>0, this.hi>>>3); //first hi bit needs to go into low.

		buffer[pos+0] = (x + min[0])*step;
		buffer[pos+1] = (y + min[1])*step;
		buffer[pos+2] = (z + min[2])*step;
	},
	morton3: function(lo, hi) {
		lo = ( lo                & 0x49249249)>>>0;
        lo = ((lo | (lo >>> 2 )) & 0xc30c30c3)>>>0;
        lo = ((lo | (lo >>> 4 )) & 0x0f00f00f)>>>0;
        lo = ((lo | (lo >>> 8 )) & 0xff0000ff)>>>0;
        lo = ((lo | (lo >>> 16)) & 0x0000ffff)>>>0;

		hi = ( hi                & 0x49249249)>>>0;
        hi = ((hi | (hi >> 2 ))  & 0xc30c30c3)>>>0;
        hi = ((hi | (hi >> 4 ))  & 0x0f00f00f)>>>0;
        hi = ((hi | (hi >> 8 ))  & 0xff0000ff)>>>0;
        hi = ((hi | (hi >> 16))  & 0x0000ffff)>>>0;

		return ((hi<<11) | lo)>>>0;
	}
};

//node is an object with nvert, nface
//patches is an array of offsets in the index, triangle are grouped by those offsets
//signature tells wether mesh has indices, normals, colors, etc. {'colors': true, 'normals':true, 'indices': true }

function MeshCoder(signature, node, patches) {
	this.sig = signature;
	this.node = node;
	this.patches = patches;

	this.last = new Int32Array(this.node.nvert);
	this.last_count = 0;
}

MeshCoder.prototype = {
	//assumes input is an ArrayBuffer
decode: function(input) {
	var t = this;

	t.buffer = new ArrayBuffer(t.node.nvert*(12 + t.sig.texcoords*8 + t.sig.normals*6 + t.sig.colors*4) + t.node.nface*t.sig.indices*6);

	var size = t.node.nvert*12; //float
	t.coords = new Float32Array(t.buffer, 0, t.node.nvert*3);

	if(t.sig.texcoords) {
		t.texcoords = new Float32Array(t.buffer, size, t.node.nvert*2);
		size += t.node.nvert*8; //float
	}
	if(t.sig.normals) {
		t.normals = new Int16Array(t.buffer, size, t.node.nvert*3);
		size += t.node.nvert*6; //short
	}
	if(t.sig.colors) {
		t.colors = new Uint8ClampedArray(t.buffer, size, t.node.nvert*4);
		size += t.node.nvert*4; //chars
	}
	if(t.sig.indices) {
		t.faces = new Uint16Array(t.buffer, size, t.node.nface*3);
		size += t.node.nface*6; //short
	}

	t.stream = new Stream(input);
	
	t.stack = new Float32Array(12); //min0, min1, min2, step, tmin0, tmin1, tstep

	t.stack[3] = t.stream.readInt();
	t.stack[4] = t.stream.readInt();
	t.stack[5] = t.stream.readInt();

	t.coord_q = t.stream.readChar();
	t.coord_bits = t.stream.readChar()*3;

	t.stack[6] = Math.pow(2.0, t.coord_q);

	if(t.sig.texcoords) {
		t.stack[9] = t.stream.readInt();
		t.stack[10] = t.stream.readInt();

		t.texcoord_q = t.stream.readChar();
		t.texcoord_bits = t.stream.readChar()*2;
		t.stack[11] = Math.pow(2.0, t.texcoord_q);
	}

	if(t.sig.indices) {
		t.decodeFaces();

//	var faces = window.performance.now() - start;
//	start += faces;
	} else {
		t.decodeCoordinates();
		
//	var coords = window.performance.now() - start;
//	start += coords;
	}

	if(t.sig.normals)
		t.decodeNormals();
//	var normals = window.performance.now() - start;
//	start += normals;
	if(t.sig.colors)
		t.decodeColors();
//	var colors = window.performance.now() - start;
//	start += colors;
//	console.log("Decode " + (faces + coords + normals + colors) + "ms. C: " + coords + " F: " + faces + " N: " + normals + " C: " + colors);

	return t.buffer;
},

decodeCoordinates: function() {
	var t = this;
	t.min = [t.stack[3], t.stack[4], t.stack[5]];

	var step = Math.pow(2.0, t.coord_q);

	var hi_bits = Math.max(t.coord_bits - 32, 0);
	var lo_bits = Math.min(t.coord_bits, 32);

    var bitstream = t.stream.readBitStream();

	var tunstall = new Tunstall;
	var diffs = tunstall.decompress(t.stream);	

	var hi = bitstream.read(hi_bits);
	var lo = bitstream.read(lo_bits);
	var p = new ZPoint(hi, lo);
	var count = 0;
	p.toPoint(t.min, step, t.coords, count);
	count += 3;
    for(var i = 1; i < t.node.nvert; i++) {
		var d = diffs[i-1];
		p.setBit(d, 1);
		if(d > 32) {
			p.hi = (p.hi & ~((1<<(d-32))-1))>>>0;
			var e = bitstream.read(d - 32);
			p.hi = (p.hi | e)>>>0;
			p.lo = bitstream.read(32);
		} else {

			if(d == 32) {
				p.lo = bitstream.read(d);
			} else {
				var e = bitstream.read(d);
				p.lo = (p.lo & ~((1<<d) -1))>>>0;
				p.lo = (p.lo | e)>>>0;
			}
		}
		p.toPoint(t.min, step, t.coords, count);
		count += 3;
	}
},

decodeFaces: function() {
	if(!this.node.nface) return;
	
	this.vertex_count = 0;
	var start = 0;
	for(var p = 0; p < this.patches.length; p++) {
		var end = this.patches[p];
		this.decodeConnectivity(end - start, start*3);
		start = end;
	}
	//dequantize positions
	var tot = this.node.nvert*3;
	var coords = this.coords;
	var stack = this.stack;
	for(var i = 0; i < tot; ) {
		coords[i] = (coords[i] + stack[3])*stack[6]; i++;
		coords[i] = (coords[i] + stack[4])*stack[6]; i++;
		coords[i] = (coords[i] + stack[5])*stack[6]; i++;
	}
	if(this.sig.texcoords) {
		var t_tot = this.node.nvert*2;
		var t_coords = this.texcoords;
		for(var i = 0; i < tot; ) {
			t_coords[i] = (t_coords[i] + stack[9])*stack[11]; i++;
			t_coords[i] = (t_coords[i] + stack[10])*stack[11]; i++;
		}		
	}
},

decodeNormals: function() {
	var norm_q = this.stream.readChar();

	var dtunstall = new Tunstall;
	var diffs = dtunstall.decompress(this.stream);

	var stunstall = new Tunstall;
	var signs = stunstall.decompress(this.stream);
	var bitstream = this.stream.readBitStream();

	var side = (1<<(16 - norm_q))>>>0;
	var diffcount = 0;
	var signcount = 0;

	if(!this.sig.indices) {
		for(var k = 0; k < 2; k++) {
			var on = 0;
			for(var i = 0; i < this.node.nvert; i++) {
				var d = this.decodeDiff(diffs[diffcount++], bitstream);
				on = on + d;
				this.normals[3*i + k] = on*side;
			}
		}
		for(var i = 0; i < this.node.nvert; i++) {
			var offset = i*3;
			var x = this.normals[offset + 0];
			var y = this.normals[offset + 1];
			var z = 32767.0*32767.0 - x*x - y*y;

        	if(z < 0) z = 0;
        	z = Math.sqrt(z);
			if(z > 32767) z = 32767;
			if(signs[i] == 0)
				z = -z;
			this.normals[offset + 2] = z;
		}
		return;
	}

	var boundary = this.markBoundary();
	this.computeNormals();

	if(this.sig.texcoords) //hack, fixing normals makes it worse actually
		return;

	var stat = 0;
	//get difference between original and predicted
	for(var i = 0; i < this.node.nvert; i++) {
		if(!boundary[i]) continue;
		var offset = i*3;
		var x = (this.normals[offset + 0]/side);
		var y = (this.normals[offset + 1]/side);
		var dx = this.decodeDiff(diffs[diffcount++], bitstream);
		var dy = this.decodeDiff(diffs[diffcount++], bitstream);
		x = (x + dx)*side;
		y = (y + dy)*side;

        var z = 32767.0*32767.0 - x*x - y*y;

        if(z < 0) z = 0;
        z = Math.sqrt(z);
        //sign
        if(z > 32767.0) z = 32767.0;
        var signbit = signs[signcount++];
//        if(this.normals[offset+2] < 0 != signbit)
        if((this.normals[offset+2] < 0 && signbit == 0) || (this.normals[offset+2] > 0 && signbit == 1))
        	z = -z;
		this.normals[offset + 0] = x;
		this.normals[offset + 1] = y;
        this.normals[offset + 2] = z;
	}
},

decodeColors: function() {
	var color_q = [];
	for(var k = 0; k < 4; k++)
		color_q[k] = this.stream.readChar();

	var diffs = [];
	for(var k = 0; k < 4; k++) {
		var tunstall = new Tunstall;;
		diffs[k] = tunstall.decompress(this.stream);
	}
	var bitstream = this.stream.readBitStream();

	var count = 0;
	if(this.sig.indices) {
		for(var i = 0; i < this.node.nvert; i++) {
			var last  = this.last[i]*4;
			var offset = i*4;

			for(var k = 0; k < 4; k++) {
				var c = this.decodeDiff(diffs[k][count], bitstream);

				if(last >= 0)
					c += this.colors[last + k];
				this.colors[offset] = c;
				offset++;
			}
			count++;
		}
	} else {
		for(var k = 0; k < 4; k++)
			this.colors[k] = this.decodeDiff(diffs[k][count], bitstream); 
		count++;

		var offset = 4;
		for(var i = 1; i < this.node.nvert; i++) {
			for(var k = 0; k < 4; k++) {
				var d = this.decodeDiff(diffs[k][count], bitstream); 
				this.colors[offset] = this.colors[offset-4] + d;
				offset ++;
			}
			count++;
		}
	}

	var steps = [];
	for(var k = 0; k < 4; k++)
		steps[k] = (1<<(8 - color_q[k]));

	//convert to rgb
	for(var i = 0; i < this.node.nvert; i++) {
		var offset = i*4;

		var e0 = this.colors[offset + 0] * steps[0];
		var e1 = this.colors[offset + 1] * steps[1];
		var e2 = this.colors[offset + 2] * steps[2];

		this.colors[offset + 0] = (e2 + e0)&0xff;
		this.colors[offset + 1] = e0;
		this.colors[offset + 2] = (e1 + e0)&0xff;
	}
},

//how to determine if a vertex is a boundary without topology:
//for each edge a vertex is in, add or subtract the id of the other vertex depending on order
//for internal vertices sum is zero.
//unless we have strange configurations and a lot of sfiga, zero wont happen. //TODO think about this
markBoundary: function() {
//	var boundary = new Uint8Array(this.node.nvert);
	var count = new Uint32Array(this.node.nvert);

	var offset = 0;
	for(var i = 0; i < this.node.nface; i++) {
		count[this.faces[offset + 0]] += this.faces[offset + 1] - this.faces[offset + 2];
		count[this.faces[offset + 1]] += this.faces[offset + 2] - this.faces[offset + 0];
		count[this.faces[offset + 2]] += this.faces[offset + 0] - this.faces[offset + 1];
		offset += 3;
	}
	return count;
//	for(var i = 0; i < this.node.nvert; i++)
//		if(count[i] != 0)
//			boundary[i] = true;
//	return boundary;
},

norm: function(buffer, a, b, c) { //a b c offsets in the buffer
	var ba0 = buffer[b+0] - buffer[a+0];
	var ba1 = buffer[b+1] - buffer[a+1];
	var ba2 = buffer[b+2] - buffer[a+2];

	var ca0 = buffer[c+0] - buffer[a+0];
	var ca1 = buffer[c+1] - buffer[a+1];
	var ca2 = buffer[c+2] - buffer[a+2];

	var p = [];
	p[0] = ba1*ca2 - ba2*ca1;
	p[1] = ba2*ca0 - ba0*ca2;
	p[2] = ba0*ca1 - ba1*ca0;
	return p;
},

normalize: function(buffer, offset) {
	var x = buffer[offset + 0];
	var y = buffer[offset + 1];
	var z = buffer[offset + 2];
	var n = Math.sqrt(x*x + y*y + z*z);
	if(n > 0) {
		buffer[offset + 0] = x/n;
		buffer[offset + 1] = y/n;
		buffer[offset + 2] = z/n;
	}
},

computeNormals:function() {
	var tmp_normals = new Float32Array(this.node.nvert*3);

	var offset = 0;
	for(var i = 0; i < this.node.nface; i++) {
		var a = 3*this.faces[offset + 0];
		var b = 3*this.faces[offset + 1];
		var c = 3*this.faces[offset + 2];

		var buffer = this.coords;
		var ba0 = buffer[b+0] - buffer[a+0];
		var ba1 = buffer[b+1] - buffer[a+1];
		var ba2 = buffer[b+2] - buffer[a+2];

		var ca0 = buffer[c+0] - buffer[a+0];
		var ca1 = buffer[c+1] - buffer[a+1];
		var ca2 = buffer[c+2] - buffer[a+2];

		var n0 = ba1*ca2 - ba2*ca1;
		var n1 = ba2*ca0 - ba0*ca2;
		var n2 = ba0*ca1 - ba1*ca0;

		tmp_normals[a + 0] += n0;
		tmp_normals[a + 1] += n1;
		tmp_normals[a + 2] += n2;
		tmp_normals[b + 0] += n0;
		tmp_normals[b + 1] += n1;
		tmp_normals[b + 2] += n2;
		tmp_normals[c + 0] += n0;
		tmp_normals[c + 1] += n1;
		tmp_normals[c + 2] += n2;
		offset += 3;
	}

	//normalize
	var offset = 0;
	for(var i = 0; i < this.node.nvert; i++) {
		var x = tmp_normals[offset + 0];
		var y = tmp_normals[offset + 1];
		var z = tmp_normals[offset + 2];
		var n = Math.sqrt(x*x + y*y + z*z);
		if(n > 0) {
			tmp_normals[offset + 0] = x/n;
			tmp_normals[offset + 1] = y/n;
			tmp_normals[offset + 2] = z/n;
		}
		this.normals[offset + 0] = tmp_normals[offset + 0]*32767;
		this.normals[offset + 1] = tmp_normals[offset + 1]*32767;
		this.normals[offset + 2] = tmp_normals[offset + 2]*32767;
		offset += 3;
	}
},

decodeDiff: function(diff, bitstream) {
	var val;
	if(diff == 0) {
		val = 1;
	} else {
		val = 1<<(diff);
		val |= bitstream.read(diff);
	};
	val--; //vall is always >= 1
	if(val & 0x1)
		val = -((val+1)>>1);
	else
		val = val>>1;
	return val;
},

/* an edge is:   uint16_t face, uint16_t side, uint32_t prev, next, bool deleted
I do not want to create millions of small objects, I will use aUint32Array.
Problem is how long, sqrt(nface) we will over blow using nface.
*/

decodeConnectivity: function(length, start) {

	var t = this;
	var ctunstall = new Tunstall;
	var clers = ctunstall.decompress(this.stream);
	var cler_count = 0;

	var dtunstall = new Tunstall;
	var diffs = dtunstall.decompress(this.stream);
	var diff_count = 0;

	var tdiffs;
	var tdiff_count = 0;
	if(t.sig.texcoords) {
		var ttunstall = new Tunstall;
		tdiffs = ttunstall.decompress(this.stream);	
	}

	var bitstream = this.stream.readBitStream(bitstream);

	var current_face = 0;          //keep track of connected component start
	//t.vertex_count = 0;
	var front = new Uint32Array(this.node.nface*18);
	var front_count = 0; //count each integer so it's front_back*5
	function addFront(_v0, _v1, _v2, _prev, _next) {
		front[front_count++] = _v0;
		front[front_count++] = _v1;
		front[front_count++] = _v2;
		front[front_count++] = _prev;
		front[front_count++] = _next;
		front[front_count++] = 0; //deleted
	}
	function _next(t) {
		t++;
		if(t == 3) t = 0;
		return t;
	}
	function _prev(t) {
		t--;
		if(t == -1) t = 2;
		return t;
	}

	var delayed = [];
	var faceorder = [];

	var faces_count = start; //count indices in this.faces array
	var totfaces = length;
//	var estimated = [0, 0, 0]; //no! use stack.
	var stack = this.stack;
	var coords = this.coords;
	var texcoords = this.texcoords;
	var hasTexCoords = t.sig.texcoords;

	while(totfaces > 0) {
		if(!faceorder.length && !delayed.length) {
			if(current_face == this.node.nface) break; //no more faces to encode exiting

			stack[0] = stack[1] = stack[2] = 0;
			stack[7] = stack[8] = 0; //texcoords
			var last_index = -1;
			var index = [];
			for(var k = 0; k < 3; k++) {
				this.last[this.last_count++] = last_index;
				var diff = diffs[diff_count++];
				var tdiff = diff && hasTexCoords? tdiffs[tdiff_count++] : 0;
				var v = this.decodeVertex(bitstream, diff, tdiff);
				index[k] = v; 
				this.faces[faces_count++] = v;
				stack[0] = coords[v*3];
				stack[1] = coords[v*3+1];
				stack[2] = coords[v*3+2]; 
				if(t.sig.texcoords) {
					stack[7] = texcoords[v*2];
					stack[8] = texcoords[v*2+1];
				}
				last_index = v;
			}
			var current_edge = front_count;
			for(var k = 0; k < 3; k++) {
				faceorder.push(front_count);
				front[front_count++] = index[_next(k)];
				front[front_count++] = index[_prev(k)]; 
				front[front_count++] = index[k];
				front[front_count++] = current_edge + _prev(k)*6;
				front[front_count++] = current_edge + _next(k)*6;
				front_count++;
//				addFront(index[_next(k)], index[_prev(k)], index[k], current_edge + _prev(k)*6, current_edge + _next(k)*6);
			}
			current_face++;
			totfaces--;
			continue;
		}
		var f;
		if(faceorder.length) 
			f = faceorder.shift();
		else 
			f = delayed.pop();
		
		var edge_start = f;

		if(front[edge_start + 5]) continue; //deleted
		front[edge_start + 5] = 1; //set edge as deleted anyway

		var c = clers[cler_count++];
		if(c == 4) continue; //BOUNDARY

		var v0   = front[edge_start + 0];
		var v1   = front[edge_start + 1];
		var v2   = front[edge_start + 2];
		var prev = front[edge_start + 3];
		var next = front[edge_start + 4];

		var first_edge = front_count; //points to new edge to be inserted
		var opposite = -1;
		if(c == 0) { //VERTEX
			//predict position based on v0, v1 and v2
			for(var k = 0; k < 3; k++) 
				stack[k] = coords[v0*3 + k] + coords[v1*3 + k] - coords[v2*3 + k];

			if(hasTexCoords)
				for(var k = 0; k < 2; k++)
					stack[7+k] = texcoords[v0*2 + k]  + texcoords[v1*2 + k] - texcoords[v2*2 + k];
			
			var diff = diffs[diff_count++];
			var tdiff = diff && hasTexCoords? tdiffs[tdiff_count++] : 0;
			opposite = this.decodeVertex(bitstream, diff, tdiff);
			if(diff != 0)
				this.last[this.last_count++] = v1;

			front[prev + 4] = first_edge;
			front[next + 3] = first_edge + 6;
			faceorder.unshift(front_count);

			front[front_count++] = v0;
			front[front_count++] = opposite;
			front[front_count++] = v1;
			front[front_count++] = prev;
			front[front_count++] = first_edge+6;
			front_count++; 
//			addFront(v0, opposite, v1, prev, first_edge + 6);

			faceorder.push(front_count);

			front[front_count++] = opposite;
			front[front_count++] = v1;
			front[front_count++] = v0;
			front[front_count++] = first_edge; 
			front[front_count++] = next;
			front_count++; 
//			addFront(opposite, v1, v0, first_edge, next);

		} else if(c == 3) { //END
			front[prev + 5] = 1;
			front[next + 5] = 1;
			front[front[prev + 3] + 4] = front[next + 4];
			front[front[next + 4] + 3] = front[prev + 3];
			opposite = front[prev + 0];

		} else if(c == 1) { //LEFT
			front[prev + 5] = 1; //deleted
			front[front[prev + 3] + 4] = first_edge;
			front[next + 3] = first_edge;
			opposite = front[prev + 0];

			faceorder.unshift(front_count);

			front[front_count++] = opposite;
			front[front_count++] = v1;
			front[front_count++] = v0;
			front[front_count++] = front[prev +3];
			front[front_count++] = next;
			front_count++; 
//			addFront(opposite, v1, v0, front[prev + 3], next);

		} else if(c == 2) { //RIGHT
			front[next + 5] = 1;
			front[front[next + 4] + 3] = first_edge;
			front[prev + 4] = first_edge;
			opposite = front[next + 1];


			faceorder.unshift(front_count);

			front[front_count++] = v0;
			front[front_count++] = opposite;
			front[front_count++] = v1;
			front[front_count++] = prev;
			front[front_count++] = front[next+4];
			front_count++; 
//			addFront(v0, opposite, v1, prev, front[next + 4]);

		} else if(c == 5) { //DELAY
			front[edge_start + 5] = 0;
			delayed.push(edge_start);
			continue;
		}
		this.faces[faces_count++] = v1;
		this.faces[faces_count++] = v0;
		this.faces[faces_count++] = opposite;
		totfaces--;
	}
},
   
decodeVertex: function(bitstream, diff, tdiff) {
	if(diff == 0) 
		return bitstream.read(16);

	var v = this.vertex_count++;

	var max = 1<<(diff-1);

	for(var k = 0; k < 3; k++) {
		var d = bitstream.read(diff) - max;
		this.coords[v*3+k] = this.stack[k] + d; //stack 0-3 is used as extimated
	}
	if(this.sig.texcoords) {
		var tmax = 1<<(tdiff-1);
		for(var k = 0; k < 2; k++) {
			var d = bitstream.read(tdiff) - tmax;
			this.texcoords[v*2+k] = this.stack[7+k] + d; //stack 7-9 is used as extimated
		}
	}
	return v;
},

decodeDiff: function(diff, bitstream) {
	var val;
	if(diff == 0) {
		return 0;
	} 
	val = 1<<diff;
	val += bitstream.read(diff);


	if(val & 0x1) 
		val >>>= 1;
	else 
		val = -(val>>>1);

	return val;
}

};

var tot = 0;
