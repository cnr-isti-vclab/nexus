/*
Nexus
Copyright (c) 2012-2020, Visual Computing Lab, ISTI - CNR
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

Nexus = function() {

/* WORKER INITIALIZED ONCE */

var meco;
var corto;

var scripts = document.getElementsByTagName('script');
var i, j, k;
var path;
for(i = 0; i < scripts.length; i++) {
	var attrs = scripts[i].attributes;
	for(j = 0; j < attrs.length; j++) {
		var a = attrs[j];
		if(a.name != 'src') continue;
		if(!a.value) continue;
		if(a.value.search('nexus.js') >= 0) {
			path = a.value;
			break;
		}
	}
}

var meco = null;
function loadMeco() {

	meco = new Worker(path.replace('nexus.js', 'meco.js'));

	meco.onerror = function(e) { console.log(e); }
	meco.requests = {};
	meco.count = 0;
	meco.postRequest = function(sig, node, patches) {
		var signature = {
			texcoords: sig.texcoords ? 1 : 0,
			colors   : sig.colors    ? 1 : 0,
			normals  : sig.normals   ? 1 : 0,
			indices  : sig.indices   ? 1 : 0
		};
		meco.postMessage({
			signature:signature,
			node:{ nface: node.nface, nvert: node.nvert, buffer:node.buffer, request:this.count},
			patches:patches
		});
		node.buffer = null;
		this.requests[this.count++] = node;
	};
	meco.onmessage = function(e) {
		var node = this.requests[e.data.request];
		delete this.requests[e.data.request];
		node.buffer = e.data.buffer;
		readyNode(node);
	};
}

var corto = null;
function loadCorto() {

	corto = new Worker(path.replace('nexus.js', 'corto.js'));
	corto.requests = {};
	corto.count = 0;
	corto.postRequest = function(node) {
		corto.postMessage({ buffer: node.buffer, request:this.count, rgba_colors: true, short_index: true, short_normals: true });
		node.buffer = null;
		this.requests[this.count++] = node;
	}
	corto.onmessage = function(e) {
		var node = this.requests[e.data.request];
		delete this.requests[e.data.request];
		node.model = e.data.geometry || e.data.model;  //for old corto.js compatibility
		readyNode(node);
	};
}

/* UTILITIES */

function getUint64(view) {
	var s = 0;
	var lo = view.getUint32(view.offset, true);
	var hi = view.getUint32(view.offset + 4, true);
	view.offset += 8;
	return ((hi * (1 << 32)) + lo);
}

function getUint32(view) {
	var s = view.getUint32(view.offset, true);
	view.offset += 4;
	return s;
}

function getUint16(view) {
	var s = view.getUint16(view.offset, true);
	view.offset += 2;
	return s;
}

function getFloat32(view) {
	var s = view.getFloat32(view.offset, true);
	view.offset += 4;
	return s;
}

/* MATRIX STUFF */

function vecMul(m, v, r) {
	var w = m[3]*v[0] + m[7]*v[1] + m[11]*v[2] + m[15];

	r[0] = (m[0]*v[0]  + m[4]*v[1]  + m[8 ]*v[2] + m[12 ])/w;
	r[1] = (m[1]*v[0]  + m[5]*v[1]  + m[9 ]*v[2] + m[13 ])/w;
	r[2] = (m[2]*v[0]  + m[6]*v[1]  + m[10]*v[2] + m[14])/w;
}


function matMul(a, b, r) {
	r[ 0] = a[0]*b[0] + a[4]*b[1] + a[8]*b[2] + a[12]*b[3];
	r[ 1] = a[1]*b[0] + a[5]*b[1] + a[9]*b[2] + a[13]*b[3];
	r[ 2] = a[2]*b[0] + a[6]*b[1] + a[10]*b[2] + a[14]*b[3];
	r[ 3] = a[3]*b[0] + a[7]*b[1] + a[11]*b[2] + a[15]*b[3];

	r[ 4] = a[0]*b[4] + a[4]*b[5] + a[8]*b[6] + a[12]*b[7];
	r[ 5] = a[1]*b[4] + a[5]*b[5] + a[9]*b[6] + a[13]*b[7];
	r[ 6] = a[2]*b[4] + a[6]*b[5] + a[10]*b[6] + a[14]*b[7];
	r[ 7] = a[3]*b[4] + a[7]*b[5] + a[11]*b[6] + a[15]*b[7];

	r[ 8] = a[0]*b[8] + a[4]*b[9] + a[8]*b[10] + a[12]*b[11];
	r[ 9] = a[1]*b[8] + a[5]*b[9] + a[9]*b[10] + a[13]*b[11];
	r[10] = a[2]*b[8] + a[6]*b[9] + a[10]*b[10] + a[14]*b[11];
	r[11] = a[3]*b[8] + a[7]*b[9] + a[11]*b[10] + a[15]*b[11];

	r[12] = a[0]*b[12] + a[4]*b[13] + a[8]*b[14] + a[12]*b[15];
	r[13] = a[1]*b[12] + a[5]*b[13] + a[9]*b[14] + a[13]*b[15];
	r[14] = a[2]*b[12] + a[6]*b[13] + a[10]*b[14] + a[14]*b[15];
	r[15] = a[3]*b[12] + a[7]*b[13] + a[11]*b[14] + a[15]*b[15];
}

function matInv(m, t) {
	var s = 1.0/(
		m[12]* m[9]*m[6]*m[3]-m[8]*m[13]*m[6]*m[3]-m[12]*m[5]*m[10]*m[3]+m[4]*m[13]*m[10]*m[3]+
		m[8]*m[5]*m[14]*m[3]-m[4]*m[9]*m[14]*m[3]-m[12]*m[9]*m[2]*m[7]+m[8]*m[13]*m[2]*m[7]+
		m[12]*m[1]*m[10]*m[7]-m[0]*m[13]*m[10]*m[7]-m[8]*m[1]*m[14]*m[7]+m[0]*m[9]*m[14]*m[7]+
		m[12]*m[5]*m[2]*m[11]-m[4]*m[13]*m[2]*m[11]-m[12]*m[1]*m[6]*m[11]+m[0]*m[13]*m[6]*m[11]+
		m[4]*m[1]*m[14]*m[11]-m[0]*m[5]*m[14]*m[11]-m[8]*m[5]*m[2]*m[15]+m[4]*m[9]*m[2]*m[15]+
		m[8]*m[1]*m[6]*m[15]-m[0]*m[9]*m[6]*m[15]-m[4]*m[1]*m[10]*m[15]+m[0]*m[5]*m[10]*m[15]
	);

	t[ 0] = (m[9]*m[14]*m[7]-m[13]*m[10]*m[7]+m[13]*m[6]*m[11]-m[5]*m[14]*m[11]-m[9]*m[6]*m[15]+m[5]*m[10]*m[15])*s;
	t[ 1] = (m[13]*m[10]*m[3]-m[9]*m[14]*m[3]-m[13]*m[2]*m[11]+m[1]*m[14]*m[11]+m[9]*m[2]*m[15]-m[1]*m[10]*m[15])*s;
	t[ 2] = (m[5]*m[14]*m[3]-m[13]*m[6]*m[3]+m[13]*m[2]*m[7]-m[1]*m[14]*m[7]-m[5]*m[2]*m[15]+m[1]*m[6]*m[15])*s;
	t[ 3] = (m[9]*m[6]*m[3]-m[5]*m[10]*m[3]-m[9]*m[2]*m[7]+m[1]*m[10]*m[7]+m[5]*m[2]*m[11]-m[1]*m[6]*m[11])*s;

	t[ 4] = (m[12]*m[10]*m[7]-m[8]*m[14]*m[7]-m[12]*m[6]*m[11]+m[4]*m[14]*m[11]+m[8]*m[6]*m[15]-m[4]*m[10]*m[15])*s;
	t[ 5] = (m[8]*m[14]*m[3]-m[12]*m[10]*m[3]+m[12]*m[2]*m[11]-m[0]*m[14]*m[11]-m[8]*m[2]*m[15]+m[0]*m[10]*m[15])*s;
	t[ 6] = (m[12]*m[6]*m[3]-m[4]*m[14]*m[3]-m[12]*m[2]*m[7]+m[0]*m[14]*m[7]+m[4]*m[2]*m[15]-m[0]*m[6]*m[15])*s;
	t[ 7] = (m[4]*m[10]*m[3]-m[8]*m[6]*m[3]+m[8]*m[2]*m[7]-m[0]*m[10]*m[7]-m[4]*m[2]*m[11]+m[0]*m[6]*m[11])*s;

	t[ 8] = (m[8]*m[13]*m[7]-m[12]*m[9]*m[7]+m[12]*m[5]*m[11]-m[4]*m[13]*m[11]-m[8]*m[5]*m[15]+m[4]*m[9]*m[15])*s;
	t[ 9] = (m[12]*m[9]*m[3]-m[8]*m[13]*m[3]-m[12]*m[1]*m[11]+m[0]*m[13]*m[11]+m[8]*m[1]*m[15]-m[0]*m[9]*m[15])*s;
	t[10] = (m[4]*m[13]*m[3]-m[12]*m[5]*m[3]+m[12]*m[1]*m[7]-m[0]*m[13]*m[7]-m[4]*m[1]*m[15]+m[0]*m[5]*m[15])*s;
	t[11] = (m[8]*m[5]*m[3]-m[4]*m[9]*m[3]-m[8]*m[1]*m[7]+m[0]*m[9]*m[7]+m[4]*m[1]*m[11]-m[0]*m[5]*m[11])*s;

	t[12] = (m[12]*m[9]*m[6]-m[8]*m[13]*m[6]-m[12]*m[5]*m[10]+m[4]*m[13]*m[10]+m[8]*m[5]*m[14]-m[4]*m[9]*m[14])*s;
	t[13] = (m[8]*m[13]*m[2]-m[12]*m[9]*m[2]+m[12]*m[1]*m[10]-m[0]*m[13]*m[10]-m[8]*m[1]*m[14]+m[0]*m[9]*m[14])*s;
	t[14] = (m[12]*m[5]*m[2]-m[4]*m[13]*m[2]-m[12]*m[1]*m[6]+m[0]*m[13]*m[6]+m[4]*m[1]*m[14]-m[0]*m[5]*m[14])*s;
	t[15] = (m[4]*m[9]*m[2]-m[8]*m[5]*m[2]+m[8]*m[1]*m[6]-m[0]*m[9]*m[6]-m[4]*m[1]*m[10]+m[0]*m[5]*m[10])*s;
}

/* PRIORITY QUEUE */

PriorityQueue = function(max_length) {
	this.error = new Float32Array(max_length);
	this.data = new Int32Array(max_length);
	this.size = 0;
}

PriorityQueue.prototype = {
	push: function(data, error) {
		this.data[this.size] = data;
		this.error[this.size] = error;
		this.bubbleUp(this.size);
		this.size++;
	},

	pop: function() {
		var result = this.data[0];
		this.size--;
		if(this.size > 0) {
			this.data[0] = this.data[this.size];
			this.error[0] = this.error[this.size];
			this.sinkDown(0);
		}
		return result;
	},

	bubbleUp: function(n) {
		var data = this.data[n];
		var error = this.error[n];
		while (n > 0) {
			var pN = ((n+1)>>1) -1;
			var pError = this.error[pN];
			if(pError > error)
				break;
			//swap
			this.data[n] = this.data[pN];
			this.error[n] = pError;
			this.data[pN] = data;
			this.error[pN] = error;
			n = pN;
		}
	},

	sinkDown: function(n) {
		var data = this.data[n];
		var error = this.error[n];

		while(true) {
			var child2N = (n + 1) * 2;
			var child1N = child2N - 1;
			var swap = -1;
			if (child1N < this.size) {
				var child1Error = this.error[child1N];
				if(child1Error > error)
					swap = child1N;
			}
			if (child2N < this.size) {
				var child2Error = this.error[child2N];
				if (child2Error > (swap == -1 ? error : child1Error))
					swap = child2N;
			}

			if (swap == -1) break;

			this.data[n] = this.data[swap];
			this.error[n] = this.error[swap];
			this.data[swap] = data;
			this.error[swap] = error;
			n = swap;
		}
	}
};


/* HEADER AND PARSING */

var padding = 256;
var Debug = {
	verbose : false,  //debug messages
	nodes   : false,  //color each node
	draw    : false,  //final rendering call disabled
	extract : false,  //extraction disabled
//	culling : false,  //visibility culling disabled
//	request : false,  //network requests disabled
//	worker  : false   //web workers disabled
};


var glP = WebGLRenderingContext.prototype;
var attrGlMap = [glP.NONE, glP.BYTE, glP.UNSIGNED_BYTE, glP.SHORT, glP.UNSIGNED_SHORT, glP.INT, glP.UNSIGNED_INT, glP.FLOAT, glP.DOUBLE];
var attrSizeMap = [0, 1, 1, 2, 2, 4, 4, 4, 8];

var targetError   = 2.0;    //error won't go lower than this if we reach it.
var maxError      = 15;     //error won't go over this even if fps is low
var minFps        = 15;
var maxPending    = 3;
var maxBlocked    = 3;
var maxReqAttempt = 2;
var maxCacheSize  = 512*(1<<20); //TODO DEBUG
var drawBudget    = 5*(1<<20);


/* MESH DEFINITION */

Mesh = function() {
	var t = this;
	t.headerSize = 4096;
	t.onLoad = null;
	t.reqAttempt = 0;
}

Mesh.prototype = {
	open: function(url) {
		var mesh = this;
		mesh.url = url;
		mesh.httpRequest(
			0,
			mesh.headerSize,
			function() {
				if(Debug.verbose) console.log("Loading header for " + mesh.url);
				var view = new DataView(this.response);
				view.offset = 0;
				mesh.reqAttempt++;
				var header = mesh.importHeader(view);
				if(!header) {
					if(Debug.verbose) console.log("Empty header!");
					if(mesh.reqAttempt < maxReqAttempt) mesh.open(mesh.url + '?' + Math.random()); // BLINK ENGINE CACHE BUG PATCH
					return;
				}
				mesh.reqAttempt = 0;
				for(i in header)
					mesh[i] = header[i];
				mesh.vertex = mesh.signature.vertex;
				mesh.face = mesh.signature.face;
				mesh.renderMode = mesh.face.INDEX?["FILL", "POINT"]:["POINT"];
				mesh.compressed = (mesh.signature.flags & (2 | 4)); //meco or corto
				mesh.meco = (mesh.signature.flags & 2);
				mesh.corto = (mesh.signature.flags & 4);
				mesh.requestIndex();
			},
			function() { console.log("Open request error!");},
			function() { console.log("Open request abort!");}
		);
	},

	httpRequest: function(start, end, load, error, abort, type) {
		if(!type) type = 'arraybuffer';
		var r = new XMLHttpRequest();
		r.open('GET', this.url, true);
		r.responseType = type;
		r.setRequestHeader("Range", "bytes=" + start + "-" + (end -1));
		r.onload = function(){
			switch (this.status){
				case 0:
//					console.log("0 response: server unreachable.");//returned in chrome for local files
				case 206:
//					console.log("206 response: partial content loaded.");
					load.bind(this)();
					break;
				case 200:
//					console.log("200 response: server does not support byte range requests.");
			}
		};
		r.onerror = error;
		r.onabort = abort;
		r.send();
		return r;
	},

	requestIndex: function() {
		var mesh = this;

		mesh.httpRequest(
			mesh.indexStart,
			mesh.indexEnd,
			function() { if(Debug.verbose) console.log("Loading index for " + mesh.url); mesh.handleIndex(this.response); },
			function() { console.log("Index request error!");},
			function() { console.log("Index request abort!");}
		);
	},

	handleIndex: function(buffer) {
		var t = this;
		var view = new DataView(buffer);
		view.offset = 0;

		var n = t.n_nodes;

		t.v3 = (t.version == 3);
		t.noffsets  = new Uint32Array(n);
		t.nsizes    = new Uint32Array(n); //size of the node on disk
		t.nvertices = new Uint32Array(n);
		t.nfaces    = new Uint32Array(n);
		t.nerrors   = new Float32Array(n);
		t.nspheres  = new Float32Array(n*5);
		t.nsize     = new Float32Array(n); //decompressed size in ram.
		t.nfirstpatch = new Uint32Array(n);

		for(i = 0; i < n; i++) {
			t.noffsets[i] = padding*getUint32(view); //offset
			if(t.v3)
				t.nsizes[i] = getUint32(view);
			t.nvertices[i] = getUint16(view);        //verticesCount
			t.nfaces[i] = getUint16(view);           //facesCount
			t.nerrors[i] = getFloat32(view);
			view.offset += 8;                        //skip cone
			for(k = 0; k < 5; k++)
				t.nspheres[i*5+k] = getFloat32(view);       //sphere + tight
			t.nfirstpatch[i] = getUint32(view);          //first patch
		}
		if(!t.v3) {
		    for(i = 0; i < n-1; i++)
			    t.nsizes[i] = t.noffsets[i+1] - t.noffsets[i];
		}

		t.sink = n -1;


		if(t.v3)
			t.patches = new Int32Array(view.buffer, view.offset, t.n_patches*4); //node, lastTriangle, texture, material
		else {
			let tmp = new Int32Array(view.buffer, view.offset, t.n_patches*3); //node, lastTriangle, texture
			t.patches =  new Uint32Array(t.n_patches*4);
			for(let i = 0; i < t.n_patches; i++) {
				t.patches[i*4+0] = tmp[i*3+0];
				t.patches[i*4+1] = tmp[i*3+1];
				t.patches[i*4+2] = tmp[i*3+2];
				t.patches[i*4+3] = 0; //material
			}
		}

		t.nroots = t.n_nodes;
		for(j = 0; j < t.nroots; j++) {
			for(i = t.nfirstpatch[j]; i < t.nfirstpatch[j+1]; i++) {
				if(t.patches[i*4] < t.nroots)
					t.nroots = t.patches[i*4];
			}
		}

		if(t.v3)
			view.offset += t.n_patches*16;
		else
			view.offset += t.n_patches*12;

		//TODO rename n_textures to n_groups
		//texture groups (1 group per material)
		t.groups = new Array(t.n_textures);
		
		if(t.v3) {

			let g = new Uint32Array(view.buffer, view.offset, t.n_textures*2);
			for(let i = 0; i < t.groups.length; i++) {
				t.groups[i] = {
					offset: padding*g[2*i],
					size: g[2*i+1],
					ref: 0,
					textures: []
				};
			}
		} else {
			for(let i = 0; i < t.groups.length; i++) {
				t.groups[i] = {
					offset: padding*getUint32(view),
					size: 0,
					ref: 0,
					textures: []
				};
			}
			for(let i = 0; i < t.groups.length-1; i++)
				t.groups[i].size = t.groups[i+1].offset - t.groups[i].offset;

/*			t.tgroups = new Uint32Array(t.n_textures*2);
			for(let i = 0; i < t.n_textures; i++) {
				t.tgroups[2*i] = padding*getUint32(view);
				view.offset += 16*4; //skip proj matrix
			} 
			for(let i = 0; i < t.n_textures-1; i++)
				t.tgroups[2*i+1] = t.tgroups[2*(i+1)] - t.tgroups[2*i]; */
		}

		t.vsize = 12 + (t.vertex.NORMAL?6:0) + (t.vertex.COLOR_0?4:0) + (t.vertex.UV_0?8:0);
		t.fsize = 6;

		//problem: I have no idea how much space a texture is needed in GPU. 10x factor assumed.
		var tmptexsize = new Uint32Array(n-1);
		var tmptexcount = new Uint32Array(n-1);
		for(var i = 0; i < n-1; i++) {
			for(var p = t.nfirstpatch[i]; p != t.nfirstpatch[i+1]; p++) {
				var tex = t.patches[p*4+2];
				if(tex == -1)
					continue;
				tmptexsize[i] += 10 * t.groups[tex].size;
				tmptexcount[i]++;
			}
			t.nsize[i] = t.vsize*t.nvertices[i] + t.fsize*t.nfaces[i];
		}
		for(var i = 0; i < n-1; i++) {
			if(tmptexcount[i] > 0)
				t.nsize[i] += tmptexsize[i]/tmptexcount[i];
		}

		t.status = new Uint8Array(n); //0 for none, 1 for ready, 2+ for waiting data
		t.frames = new Uint32Array(n);
		t.errors = new Float32Array(n); //biggest error of instances
		t.ibo    = new Array(n);
		t.vbo    = new Array(n);

//		need a place where we store the tex id of each texture (we have the group)
		t.texids = new Array(n);

		t.isReady = true;
		if(t.onLoad) t.onLoad();
	},

	importAttribute: function(view) {
		var a = {};
		a.type = view.getUint8(view.offset++, true);
		a.size = view.getUint8(view.offset++, true);
		a.glType = attrGlMap[a.type];
		a.normalized = a.type < 7;
		a.stride = attrSizeMap[a.type]*a.size;
		if(a.size == 0) return null;
		return a;
	},

	importElement: function(view) {
		var e = [];
		for(i = 0; i < 8; i++)
			e[i] = this.importAttribute(view);
		return e;
	},

	importVertex: function(view) {	//enum POSITION, NORMAL, COLOR_0, UV_0, DATA0
		var e = this.importElement(view);
		var color = e[2];
		if(color) {
			color.type = 2; //unsigned byte
			color.glType = attrGlMap[2];
		}
		return { POSITION: e[0], NORMAL: e[1], COLOR_0: e[2], UV_0: e[3], data: e[4] };
	},

	//enum INDEX, NORMAL, COLOR, UV, DATA0
	importFace: function(view) {
		var e = this.importElement(view);
		var color = e[2];
		if(color) {
			color.type = 2; //unsigned byte
			color.glType = attrGlMap[2];
		}
		return { INDEX: e[0], NORMAL: e[1], COLOR_0: e[2], UV_0: e[3], data: e[4] };
	},

	importSignature: function(view) {
		var s = {};
		s.vertex = this.importVertex(view);
		s.face = this.importFace(view);
		s.flags = getUint32(view);
		return s;
	},

	importHeader: function(view) {
		let magic = getUint32(view);
		if(magic != 0x4E787320) {
			console.log("Not a nexus file.");
			return null;
		}
		this.version = getUint32(view);
		if(this.version == 2)
			return this.importHeader2(view);
		if(this.version == 3)
			return this.importHeader3(view);
		throw "Nexus version: " + this.version + " not supported";
		return null;
	},

	importHeader2: function(view) {
		var h = {};
		h.verticesCount = getUint64(view);
		h.facesCount = getUint64(view);
		h.signature = this.importSignature(view);
		h.n_nodes = getUint32(view);
		h.n_patches = getUint32(view);
		h.n_textures = getUint32(view);
		h.sphere = {
			center: [getFloat32(view), getFloat32(view), getFloat32(view)],
			radius: getFloat32(view)
		};
		this.indexStart = 88;
		this.indexEnd = 88 + h.n_nodes*44 + h.n_patches*12 + h.n_textures*68;
		return h;
	},

	importHeader3: function(view) {
		var h = {};
		let jsonLength = getUint32(view);
		this.headerSize = jsonLength + 12;
		if(this.headerSize < view.length)
			return null;
		
		var str = String.fromCharCode.apply(null, new Uint8Array(view.buffer, 12, jsonLength));
		h = JSON.parse(str);		
		h.version = 3;
		this.indexStart = jsonLength + 12;
		this.indexEnd = this.indexStart + h.n_nodes*48 + h.n_patches*16 + h.n_textures*8;
		return h;
	}
};

Instance = function(gl) {
	this.gl = gl;
	this.onLoad = function() {};
	this.onUpdate = null;
	this.drawBudget = drawBudget;
	this.attributes = { position:0, normal:1, color:2, uv:3, size:4 };
	this.samplers = { };
}

Instance.prototype = {
	open: function(url) {
		var t = this;
		t.context = getContext(t.gl);

		t.modelMatrix      = new Float32Array(16);
		t.viewMatrix       = new Float32Array(16);
		t.projectionMatrix = new Float32Array(16);
		t.modelView        = new Float32Array(16);
		t.modelViewInv     = new Float32Array(16);
		t.modelViewProj    = new Float32Array(16);
		t.modelViewProjInv = new Float32Array(16);
		t.planes           = new Float32Array(24);
		t.viewport         = new Float32Array(4);
		t.viewpoint        = new Float32Array(4);

		t.context.meshes.forEach(function(m) {
			if(m.url == url){
				t.mesh = m;
				t.renderMode = t.mesh.renderMode;
				t.mode = t.renderMode[0];
				t.onLoad();
			}
		});

		if(!t.mesh) {
			t.mesh = new Mesh();
			t.mesh.onLoad = function() { t.renderMode = t.mesh.renderMode; t.mode = t.renderMode[0]; t.onLoad(); }
			t.mesh.open(url);
			t.context.meshes.push(t.mesh);
		}
	},

	close: function() {
		//remove instance from mesh.
	},

	get isReady() { return this.mesh.isReady; },
	setPrimitiveMode : function (mode) { this.mode = mode; },
	get datasetRadius() { if(!this.isReady) return 1.0;       return this.mesh.sphere.radius; },
	get datasetCenter() { if(!this.isReady) return [0, 0, 0]; return this.mesh.sphere.center; },

	updateView: function(viewport, projection, modelView) {
		var t = this;

		for(var i = 0; i < 16; i++) {
			t.projectionMatrix[i] = projection[i];
			t.modelView[i] = modelView[i];
		}
		for(var i = 0; i < 4; i++)
			t.viewport[i] = viewport[i];

		matMul(t.projectionMatrix, t.modelView, t.modelViewProj);
		matInv(t.modelViewProj, t.modelViewProjInv);

		matInv(t.modelView, t.modelViewInv);
		t.viewpoint[0] = t.modelViewInv[12];
		t.viewpoint[1] = t.modelViewInv[13];
		t.viewpoint[2] = t.modelViewInv[14];
		t.viewpoint[3] = 1.0;


		var m = t.modelViewProj;
		var mi = t.modelViewProjInv;
		var p = t.planes;


		//frustum plane equation Ax + By + Cz + D = 0;
		p[0]  =  m[0] + m[3]; p[1]  =  m[4] + m[7]; p[2]  =  m[8] + m[11];  p[3]  =  m[12] + m[15]; //left
		p[4]  = -m[0] + m[3]; p[5]  = -m[4] + m[7]; p[6]  = -m[8] + m[11];  p[7]  = -m[12] + m[15]; //right
		p[8]  =  m[1] + m[3]; p[9]  =  m[5] + m[7]; p[10] =  m[9] + m[11];  p[11] =  m[13] + m[15]; //bottom
		p[12] = -m[1] + m[3]; p[13] = -m[5] + m[7]; p[14] = -m[9] + m[11];  p[15] = -m[13] + m[15]; //top
		p[16] =  m[2];        p[17] =  m[6];        p[18] =  m[10];         p[19] =  m[14];        //near
		p[20] = -m[2] + m[3]; p[21] = -m[6] + m[7]; p[22] = -m[10] + m[11]; p[23] = -m[14] + m[15];//far

		//normalize planes to get also distances
		for(var i = 0; i < 24; i+= 4) {
			var l = Math.sqrt(p[i]*p[i] + p[i+1]*p[i+1] + p[i+2]*p[i+2]);
			p[i] /= l; p[i+1] /= l; p[i+2] /= l; p[i+3] /= l;
		}

		//side is M'(1,0,0,1) - M'(-1,0,0,1) and they lie on the planes
		var r3 = mi[3] + mi[15];
		var r0 = (mi[0]  + mi[12 ])/r3;
		var r1 = (mi[1]  + mi[13 ])/r3;
		var r2 = (mi[2]  + mi[14 ])/r3;

		var l3 = -mi[3] + mi[15];
		var l0 = (-mi[0]  + mi[12 ])/l3 - r0;
		var l1 = (-mi[1]  + mi[13 ])/l3 - r1;
		var l2 = (-mi[2]  + mi[14 ])/l3 - r2;

		var side = Math.sqrt(l0*l0 + l1*l1 + l2*l2);

		//center of the scene is M'*(0, 0, 0, 1)
		var c0 = mi[12]/mi[15] - t.viewpoint[0];
		var c1 = mi[13]/mi[15] - t.viewpoint[1];
		var c2 = mi[14]/mi[15] - t.viewpoint[2];
		var dist = Math.sqrt(c0*c0 + c1*c1 + c2*c2);

		var resolution = (2*side/dist)/ t.viewport[2];
		t.currentResolution == resolution ? t.sameResolution = true : t.sameResolution = false;
		t.currentResolution = resolution;
	},

	traversal : function () {
		var t = this;

		if(Debug.extract == true)
			return;

		if(!t.isReady) return;

		if(t.sameResolution)
			if(!t.visitQueue.size && !t.nblocked) return;

		var n = t.mesh.n_nodes;
		t.visited  = new Uint8Array(n);
		t.blocked  = new Uint8Array(n);
		t.selected = new Uint8Array(n);

		t.visitQueue = new PriorityQueue(n);
		for(var i = 0; i < t.mesh.nroots; i++)
			t.insertNode(i);

		t.currentError = t.context.currentError;
		t.drawSize = 0;
		t.nblocked = 0;

		var requested = 0;
		while(t.visitQueue.size && t.nblocked < maxBlocked) {
			var error = t.visitQueue.error[0];
			var node = t.visitQueue.pop();
			if ((requested < maxPending) && (t.mesh.status[node] == 0)) {
				t.context.candidates.push({id: node, instance:t, mesh:t.mesh, frame:t.context.frame, error:error});
				requested++;
			}

			var blocked = t.blocked[node] || !t.expandNode(node, error);
			if (blocked)
				t.nblocked++;
			else {
				t.selected[node] = 1;
			}
			t.insertChildren(node, blocked);
		}
	},

	insertNode: function (node) {
		var t = this;
		t.visited[node] = 1;

		var error = t.nodeError(node);
		if(node > 0 && error < t.currentError) return;  //2% speed TODO check if needed

		var errors = t.mesh.errors;
		var frames = t.mesh.frames;
		if(frames[node] != t.context.frame || errors[node] < error) {
			errors[node] = error;
			frames[node] = t.context.frame;
		}
		t.visitQueue.push(node, error);
	},

	insertChildren : function (node, block) {
		var t = this;
		for(var i = t.mesh.nfirstpatch[node]; i < t.mesh.nfirstpatch[node+1]; ++i) {
			var child = t.mesh.patches[i*4];
			if (child == t.mesh.sink) return;
			if (block) t.blocked[child] = 1;
			if (!t.visited[child])
				t.insertNode(child);
		}
	},

	expandNode : function (node, error) {
		var t = this;
		if(node > 0 && error < t.currentError) {
//			console.log("Reached error", error, t.currentError);
			return false;
		}

		if(t.drawSize > t.drawBudget) {
//			console.log("Reached drawsize", t.drawSize, t.drawBudget);
			return false;
		}

		if(t.mesh.status[node] != 1) { //not ready
//			console.log("Node " + node + " still not loaded (cache?)");
			return false;
		}

		var sp = t.mesh.nspheres;
		var off = node*5;
		if(t.isVisible(sp[off], sp[off+1], sp[off+2], sp[off+3])) //expanded radius
			t.drawSize += t.mesh.nvertices[node]*0.8;
			//we are adding half of the new faces. (but we are using the vertices so *2)

		return true;
	},

	nodeError : function (n, tight) {
		var t = this;
		var spheres = t.mesh.nspheres;
		var b = t.viewpoint;
		var off = n*5;
		var cx = spheres[off+0];
		var cy = spheres[off+1];
		var cz = spheres[off+2];
		var r  = spheres[off+3];
		if(tight)
			r = spheres[off+4];
		var d0 = b[0] - cx;
		var d1 = b[1] - cy;
		var d2 = b[2] - cz;
		var dist = Math.sqrt(d0*d0 + d1*d1 + d2*d2) - r;
		if (dist < 0.1)
			dist = 0.1;

		//resolution is how long is a pixel at distance 1.
		var error = t.mesh.nerrors[n]/(t.currentResolution*dist); //in pixels

		if (!t.isVisible(cx, cy, cz, spheres[off+4]))
			error /= 1000.0;
		return error;
	},

	isVisible : function (x, y, z, r) {
		var p = this.planes;
		for (i = 0; i < 24; i +=4) {
			if(p[i]*x + p[i+1]*y + p[i+2]*z +p[i+3] + r < 0) //plane is ax+by+cz+w = 0;
				return false;
		}
		return true;
	},

	renderNodes: function() {
		var t = this;
		var m = t.mesh;
		var gl = t.gl;
		var attr = t.attributes;

		var vertexEnabled = gl.getVertexAttrib(attr.position, gl.VERTEX_ATTRIB_ARRAY_ENABLED);
		var normalEnabled = attr.normal >= 0? gl.getVertexAttrib(attr.normal, gl.VERTEX_ATTRIB_ARRAY_ENABLED): false;
		var colorEnabled  = attr.color  >= 0? gl.getVertexAttrib(attr.color,  gl.VERTEX_ATTRIB_ARRAY_ENABLED): false;
		var uvEnabled     = attr.uv     >= 0? gl.getVertexAttrib(attr.uv,     gl.VERTEX_ATTRIB_ARRAY_ENABLED): false;

		var rendered = 0;
		var last_texture = -1;
		t.realError = 0.0;
		for(var n = 0; n < m.n_nodes; n++) {
			if(!t.selected[n]) continue;

			if(t.mode != "POINT") {
				var skip = true;
				for(var p = m.nfirstpatch[n]; p < m.nfirstpatch[n+1]; p++) {
					var child = m.patches[p*4];
					if(!t.selected[child]) {
						skip = false;
						break;
					}
				}
				if(skip) continue;
			}


			var sp = m.nspheres;
			var off = n*5;
			if(!t.isVisible(sp[off], sp[off+1], sp[off+2], sp[off+4])) //tight radius
				continue;

			let err = t.nodeError(n, true);
			t.realError = Math.max(err, t.realError);

			gl.bindBuffer(gl.ARRAY_BUFFER, m.vbo[n]);
			if(t.mode != "POINT")
				gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, m.ibo[n]);

			gl.vertexAttribPointer(attr.position, 3, gl.FLOAT, false, 12, 0);
			gl.enableVertexAttribArray(attr.position);

			var nv = m.nvertices[n];
			var offset = nv*12;

			if(m.vertex.UV_0 && attr.uv >= 0){
				gl.vertexAttribPointer(attr.uv, 2, gl.FLOAT, false, 8, offset), offset += nv*8;
				gl.enableVertexAttribArray(attr.uv);
			}
			if(m.vertex.COLOR_0 && attr.color >= 0){
				gl.vertexAttribPointer(attr.color, 4, gl.UNSIGNED_BYTE, true, 4, offset), offset += nv*4;
				gl.enableVertexAttribArray(attr.color);
			}
			if(m.vertex.NORMAL && attr.normal >= 0){
				gl.vertexAttribPointer(attr.normal, 3, gl.SHORT, true, 6, offset);
				gl.enableVertexAttribArray(attr.normal);
			}

			if(Debug.nodes) {
				gl.disableVertexAttribArray(2);
				gl.disableVertexAttribArray(3);

				var error = t.nodeError(n, true);
				var palette = {
					1:    [1, 1, 1, 1], //white
					2:    [1, 0, 1, 1], //magenta
					4:    [0, 1, 1, 1], //cyan
					8:    [1, 1, 0, 1], //yellow
					16:   [0, 0, 1, 1], //blue
					32:   [0, 1, 0, 1], //green
					64:   [1, 0, 0, 1]  //red
				};

				for(i in palette)
					if(i > error) {
						gl.vertexAttrib4fv(attr.color, palette[i]);
						break;
					}
//				gl.vertexAttrib4fv(2, [(n*200 %255)/255.0, (n*140 %255)/255.0,(n*90 %255)/255.0, 1]);
			}

			if (Debug.draw) continue;

			if(t.mode == "POINT") {
				var pointsize = t.pointsize;
				var error = t.nodeError(n);
				if(!pointsize)
					var pointsize = Math.ceil(1.2* Math.min(error, 5));

				if(typeof attr.size == 'object') { //threejs pointcloud rendering
					gl.uniform1f(attr.size, t.pointsize);
					gl.uniform1f(attr.scale, t.pointscale);
				} else
					gl.vertexAttrib1fv(attr.size, [pointsize]);

//				var fraction = (error/t.realError - 1);
//				if(fraction > 1) fraction = 1;

				var count = nv;
				if(count != 0) {
					if(m.vertex.UV_0) {

						let texgroupid = m.patches[m.nfirstpatch[n]*4+2];
						let group = m.groups[texgroupid];
						if(texgroupid != -1 && texgroupid != last_texture) { //bind texture
							
							for(let i = 0; i < group.textures.length; i++) {
								gl.activeTexture(gl.TEXTURE0 + i);
								gl.bindTexture(gl.TEXTURE_2D, group.textures[i]);
							}
						}
						last_texture = texgroupid;
					}
					gl.drawArrays(gl.POINTS, 0, count);
					rendered += count;
				}
				continue;
			}

			//concatenate renderings to remove useless calls. except we have textures.
			var offset = 0;
			var end = 0;
			var last = m.nfirstpatch[n+1]-1;
			for (var p = m.nfirstpatch[n]; p < m.nfirstpatch[n+1]; ++p) {
				var child = m.patches[p*4];

				if(!t.selected[child]) {
					end = m.patches[p*4+1];
					if(p < last) //if textures we do not join. TODO: should actually check for same texture of last one.
						continue;
				}
				if(end > offset) {
					if(m.vertex.UV_0) {
						let texgroupid = m.patches[p*4+2];
						let group = m.groups[texgroupid];
						if(texgroupid != -1 && texgroupid != last_texture) { //bind texture
							let mat = m.patches[p*4 + 3];
							mat = m.materials[mat];
							//we need to map material texture index in group.textures to the gl_texture_0, 1, etx
							mat.mapping.forEach((sampler, tex) => {
								gl.activeTexture(gl.TEXTURE0 + sampler);
								gl.bindTexture(gl.TEXTURE_2D, group.textures[tex]);
							});
						}
						last_texture = texgroupid;
					}
					gl.drawElements(gl.TRIANGLES, (end - offset) * 3, gl.UNSIGNED_SHORT, offset * 6);
					rendered += end - offset;
				}
				offset = m.patches[p*4+1];
			}
		}

		t.context.rendered += rendered;
		t.context.realError = Math.max(t.context.realError, t.realError);

		if(!vertexEnabled) gl.disableVertexAttribArray(attr.position);
		if(!normalEnabled && attr.normal >= 0) gl.disableVertexAttribArray(attr.normal);
		if(!colorEnabled && attr.color >= 0) gl.disableVertexAttribArray(attr.color);
		if(!uvEnabled && attr.uv >= 0) gl.disableVertexAttribArray(attr.uv);

		gl.bindBuffer(gl.ARRAY_BUFFER, null);
		if(t.mode != "POINT")
			gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);
	},

	render: function() {
		this.traversal();
		this.renderNodes();
	}
};


//keep track of meshes and which GL they belong to. (no sharing between contexts)
var contexts = [];

function getContext(gl) {
	var c = null;
	if(!gl.isTexture) throw "Something wrong";
	contexts.forEach(function(g) {
		if(g.gl == gl) c = g;
	});
	if(c) return c;

	let version = gl.getParameter(gl.SHADING_LANGUAGE_VERSION);
	version = parseInt(version[14]);
	c = { gl:gl, meshes:[], frame:0, cacheSize:0, candidates:[], pending:0, version: version, maxCacheSize: maxCacheSize,
		minFps: minFps, targetError: targetError, currentError: targetError, maxError: maxError, realError: 0 };
	contexts.push(c);
	return c;
}



function beginFrame(gl, fps) { //each context has a separate frame count.
	var c = getContext(gl);

	c.frame++;
	c.candidates = [];
	if(fps && c.minFps) {
		c.currentFps = fps;
		var r = c.minFps/fps;
		if(r > 1.1)
			c.currentError *= 1.05;
		if(r < 0.9)
			c.currentError *= 0.95;

		c.currentError = Math.max(c.targetError, Math.min(c.maxError, c.currentError));

	} else
		c.currentError = c.targetError;

	c.rendered = 0;
	c.realError = 0;
}

function endFrame(gl) {
	updateCache(gl);
}

function removeNode(context, node) {
	var n = node.id;
	var m = node.mesh;
	if(m.status[n] == 0) return;

	if(Debug.verbose) console.log("Removing " + m.url + " node: " + n);
	m.status[n] = 0;

	if (m.georeq.readyState != 4) m.georeq.abort();

	context.cacheSize -= m.nsize[n];
	context.gl.deleteBuffer(m.vbo[n]);
	context.gl.deleteBuffer(m.ibo[n]);
	m.vbo[n] = m.ibo[n] = null;

	if(!m.vertex.UV_0) return;
	if (m.texreq && m.texreq.readyState != 4) m.texreq.abort();

	let texgroup = m.patches[m.nfirstpatch[n]*4+2]; //TODO assuming one texture per node
	let group = m.groups[texgroup];
	group.ref--;

	if(group.ref) {
		for(let i = 0; i < group.textures.length; i++) 
			context.gl.deleteTexture(group.textures[i]);
		group.textures = [];
	}
}

function requestNode(context, node) {
	var n = node.id;
	var m = node.mesh;

	m.status[n] = 2; //pending

	context.pending++;
	context.cacheSize += m.nsize[n];

	node.reqAttempt = 0;
	node.context = context;
	node.nvert = m.nvertices[n];
	node.nface = m.nfaces[n];

//	console.log("Requesting " + m.url + " node: " + n);
	requestNodeGeometry(context, node);
	requestNodeTexture(context, node);
}

function requestNodeGeometry(context, node) {
	var n = node.id;
	var m = node.mesh;

	m.status[n]++; //pending
	m.georeq = m.httpRequest(
		m.noffsets[n],
		m.noffsets[n] + m.nsizes[n],
		function() { loadNodeGeometry(this, context, node); },
		function() {
			if(Debug.verbose) console.log("Geometry request error!");
			recoverNode(context, node, 0);
		},
		function() {
			if(Debug.verbose) console.log("Geometry request abort!");
			removeNode(context, node);
		},
		'arraybuffer'
	);
}

function requestNodeTexture(context, node) {
	var n = node.id;
	var m = node.mesh;

	if(!m.vertex.UV_0) return;

	var texgroup = m.patches[m.nfirstpatch[n]*4+2];
	m.groups[texgroup].ref++;

	if(m.groups[texgroup].ref > 1) //already in use.
		return;

	m.status[n]++; //pending

	m.texreq = m.httpRequest(
		m.groups[texgroup].offset, 
		m.groups[texgroup].offset + m.groups[texgroup].size, 
		function() { loadNodeTexture(this, context, node, texgroup); },
		function() {
			if(Debug.verbose) console.log("Texture request error!");
			recoverNode(context, node, 1);
		},
		function() {
			if(Debug.verbose) console.log("Texture request abort!");
			removeNode(context, node);
		},
		'arraybuffer'
	);
}

function recoverNode(context, node, id) {
	var n = node.id;
	var m = node.mesh;
	if(m.status[n] == 0) return;

	m.status[n]--;

	if(node.reqAttempt > maxReqAttempt) {
		if(Debug.verbose) console.log("Max request limit for " + m.url + " node: " + n);
		removeNode(context, node);
		return;
	}

	node.reqAttempt++;

	switch (id){
		case 0:
			requestNodeGeometry(context, node);
			if(Debug.verbose) console.log("Recovering geometry for " + m.url + " node: " + n);
			break;
		case 1:
			var texgroup = m.patches[m.nfirstpatch[n]*4+2];
			m.groups[texgroup].ref--;
			requestNodeTexture(context, node);
			if(Debug.verbose) console.log("Recovering texture for " + m.url + " node: " + n);
			break;
	}
}

function loadNodeGeometry(request, context, node) {
	var n = node.id;
	var m = node.mesh;
	if(m.status[n] == 0) return;

	node.buffer = request.response;

	if(!m.compressed)
		readyNode(node);
	else if(m.meco) {
		var sig = { texcoords: m.vertex.UV_0, normals:m.vertex.NORMAL, colors:m.vertex.COLOR_0, indices: m.face.INDEX }
		var patches = [];
		for(var k = m.nfirstpatch[n]; k < m.nfirstpatch[n+1]; k++)
			patches.push(m.patches[k*4+1]);
		if(!meco) loadMeco();
		meco.postRequest(sig, node, patches);
	} else {
		if(!corto) loadCorto();
		corto.postRequest(node);
	}
}

function powerOf2(n) {
    return n && (n & (n - 1)) === 0;
}

function loadNodeTexture(request, context, node, groupid) {
	var n = node.id;
	var m = node.mesh;
	if(m.status[n] == 0) return;

	let ntex = 1;

	let offset = 0;
	let size = request.response.size;
	let view;
	if(m.v3) {
		view = new DataView(request.response);
		view.offset = 0;
		ntex = getUint32(view); 
	}

	m.status[n] += ntex-1;

	for(let i = 0; i < ntex; i++) {
		if(m.v3) {
			size = getUint32(view);
			offset = view.offset;
			view.offset += size;
		}

		let blob = new Blob([new Uint8Array(request.response, offset, size)]);

		let urlCreator = window.URL || window.webkitURL;
		let img = document.createElement('img');
		img.onerror = function(e) { console.log("Texture loading error!"); };
		img.src = urlCreator.createObjectURL(blob);

		let texnumber = i;
		let gl = context.gl;
		img.onload = function() {
			urlCreator.revokeObjectURL(img.src);

			var flip = gl.getParameter(gl.UNPACK_FLIP_Y_WEBGL);
			gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);

			var tex = gl.createTexture();
			let remap = [0, 1, 2];
			m.groups[groupid].textures[remap[i]] = tex; 
			gl.bindTexture(gl.TEXTURE_2D, tex);
//TODO some textures might be alpha only! save space
			if(context.version > 1 || (powerOf2(img.width) && powerOf2(img.height))) {
				var s = gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
				gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
				gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST_MIPMAP_LINEAR);
				gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
				gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
				gl.generateMipmap(gl.TEXTURE_2D);

			} else {
				var s = gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
				gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
				gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
				gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
				gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);

			}
			gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, flip);

			m.status[n]--;

			if(m.status[n] == 2) {
				m.status[n]--; //ready
				node.reqAttempt = 0;
				node.context.pending--;
				node.instance.onUpdate && node.instance.onUpdate();
				updateCache(gl);
			}
		}
	}
}

function scramble(n, coords, normals, colors) {
	while (n > 0) {
		var i = Math.floor(Math.random() * n);
		n--;
		for(var k =0; k < 3; k++) {
			var v = coords[n*3+k];
			coords[n*3+k] = coords[i*3+k];
			coords[i*3+k] = v;

			if(normals) {
				var v = normals[n*3+k];
				normals[n*3+k] = normals[i*3+k];
				normals[i*3+k] = v;
			}
			if(colors) {
				var v = colors[n*4+k];
				colors[n*4+k] = colors[i*4+k];
				colors[i*4+k] = v;
			}
		}
	}
}

function readyNode(node) {
	var m = node.mesh;
	var n = node.id;
	var nv = m.nvertices[n];
	var nf = m.nfaces[n];
	var model = node.model;

	var vertices;
	var indices;

	if(!m.corto) {
		indices  = new Uint8Array(node.buffer, nv*m.vsize,  nf*m.fsize);
//TODO why vertices is not build using node.buffer?
//suspect: might be with the byte alignmente required:
//coords, uv, color (4 bytes), normal
		vertices = new Uint8Array(nv*m.vsize);
		var view = new Uint8Array(node.buffer, 0, nv*m.vsize);
		var v = view.subarray(0, nv*12);
		vertices.set(v);
		var off = nv*12;
		if(m.vertex.UV_0) {
			var uv = view.subarray(off, off + nv*8);
			vertices.set(uv, off);
			off += nv*8;
		}
		//unfortunately normals comes before color in nxs, but we want the reverse in the buffer

		if(m.vertex.NORMAL && m.vertex.COLOR_0) {
			var no = view.subarray(off, off + nv*6);
			var co = view.subarray(off + nv*6, off + nv*6 + nv*4);
			vertices.set(co, off);
			vertices.set(no, off + nv*4);
		}
		else {
			if(m.vertex.NORMAL) {
				var no = view.subarray(off, off + nv*6);
				vertices.set(no, off);
			}
			if(m.vertex.COLOR_0) {
				var co = view.subarray(off, off + nv*4);
				vertices.set(co, off);
			}
		}
	} else {
		indices = node.model.index;
		vertices = new ArrayBuffer(nv*m.vsize);
		var v = new Float32Array(vertices, 0, nv*3);
		v.set(model.position);
		var off = nv*12;
		if(model.uv) {
			var uv = new Float32Array(vertices, off, nv*2);
			uv.set(model.uv);
			off += nv*8;
		}
		if(model.color) {
			var co = new Uint8Array(vertices, off, nv*4);
			co.set(model.color);
			off += nv*4;
		}
		if(model.normal) {
			var no = new Int16Array(vertices, off, nv*3);
			no.set(model.normal);
		}
	}

	if(nf == 0)
		scramble(nv, v, no, co);

	if(n == 1) {
		m.basev = new Float32Array(vertices, 0, nv*3);
		m.basei = new Uint16Array(indices, 0, nf*3);
	}

	var gl = node.context.gl;
	var vbo = m.vbo[n] = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
	gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
	var ibo = m.ibo[n] = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);

	m.status[n]--;

	if(m.status[n] == 2) {
		m.status[n]--; //ready
		node.reqAttempt = 0;
		node.context.pending--;
		node.instance.onUpdate && node.instance.onUpdate();
		updateCache(gl);
	}
}

function flush(context, mesh) {
	for(var i = 0; i < mesh.n_nodes; i++)
		removeNode(context, {mesh:mesh, id: i });
}

function updateCache(gl) {
	var context = getContext(gl);

	var best = null;
	context.candidates.forEach(function(e) {
		if(e.mesh.status[e.id] == 0 && (!best || e.error > best.error)) best = e;
	});
	context.candidates = [];
	if(!best) return;

	while(context.cacheSize > context.maxCacheSize) {
		var worst = null;
		//find node with smallest error in cache
		context.meshes.forEach(function(m) {
			var n = m.n_nodes;
			for(i = 0; i < n; i++)
				if(m.status[i] == 1 && (!worst ||  m.errors[i] < worst.error))
					worst = {error: m.errors[i], frame: m.frames[i], mesh:m, id:i};
		});
		if(!worst || (worst.error >= best.error && worst.frame == best.frame))
			return;
		removeNode(context, worst);
	}

	if(context.pending < maxPending) {
		requestNode(context, best);
		updateCache(gl);
	}
}

//nodes are loaded asincronously, just update mesh content (VBO) cache size is kept globally.
//but this could be messy.


function getTargetError(gl)  { return getContext(gl).targetError; }
function getMinFps(gl)       { return getContext(gl).minFps; }
function getMaxCacheSize(gl) { return getContext(gl).maxCacheSize; }

function setTargetError(gl, error) { getContext(gl).targetError = error; }
function setMinFps(gl, fps)        { getContext(gl).minFps = fps; }
function setMaxCacheSize(gl, size) { getContext(gl).maxCacheSize = size; }


return { Mesh: Mesh, Renderer: Instance, Renderable: Instance, Instance:Instance,
	Debug: Debug, contexts: contexts, beginFrame:beginFrame, endFrame:endFrame, updateCache: updateCache, flush: flush,
	setTargetError:setTargetError, setMinFps: setMinFps, setMaxCacheSize:setMaxCacheSize };

}();
