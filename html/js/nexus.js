/*
Nexus

Copyright (c) 2014-2016, Federico Ponchio - Visual Computing Lab, ISTI - CNR
All rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

Nexus = function() {

// WORKERS 

var meco;
var corto;

var scripts = document.getElementsByTagName('script');
var i, j, k;


function getCurrentDir() {
	var path;
	for(i = 0; i < scripts.length; i++) {
		var attrs = scripts[i].attributes;
		for(j = 0; j < attrs.length; j++) {
			var a = attrs[j];
			if(a.name != 'src' || !a.value) continue;
			if(a.value.search('nexus.js') >= 0) {
				path = a.value;
				break;
			}
		}
	}
	return path;
}


var path = getCurrentDir();

meco = new Worker(path.replace('nexus.js', 'meco.js'));
meco.requests = {};
meco.count = 0;
meco.postRequest = function(sig, node, patches) {
	var signature = {
		texcoords: sig.texcoords?1:0, colors: sig.colors?1:0,
		normals: sig.normals?1:0, indices: sig.indices?1:0
	};
	meco.postMessage({ 
		signature:signature,
		node:{ nface: node.nface, nvert: node.nvert, buffer:node.buffer, request:this.count}, 
		patches:patches
	});
	this.requests[this.count++] = node;
};

meco.onmessage = function(e) {
	var node = this.requests[e.data.request];
	node.buffer = e.data.buffer;
	node.context.readyNode(node);
};

corto = new Worker(path.replace('nexus.js', 'corto.js'));
corto.requests = {};
corto.count = 0;
corto.postRequest = function(node) {
	corto.postMessage({ buffer: node.buffer, request:this.count });
	this.requests[this.count++] = node;
}
corto.onmessage = function(e) {
	var node = this.requests[e.data.request];
	node.patch = e.data.model;
	node.context.readyNode(node);
};


// UTILITIES 

function httpRequest(url, start, end, load, error, type) {
	if(!type) type = 'arraybuffer';
	var that = this;
	var r = new XMLHttpRequest();
	r.open('GET', url, true);
	r.responseType = type;
	r.setRequestHeader("Range", "bytes=" + start + "-" + (end -1));
	r.onload = function() {
		switch (this.status){
			case 206:
				load.bind(this)();
			break;
			case 200:
				console.log("200 response; server does not support byte range requests.");
				error.bind(this)();
			break;
		}
	};
	r.onerror = error;
	r.onabort = error;
	r.send();
}

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

// MATRIX STUFF 

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


// PRIORITY QUEUE 

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


// HEADER AND PARSING 

var padding = 256;
var Debug = { 
	nodes: false,    //color each node
	culling: false,  //visibility culling disabled
	draw: false,     //final rendering call disabled
	extract: false,  //no extraction
	request: false,  //no network requests
	worker: false    //no web workers
};

var glP = WebGLRenderingContext.prototype;
var attrGlMap = [glP.NONE, glP.BYTE, glP.UNSIGNED_BYTE, glP.SHORT, glP.UNSIGNED_SHORT, glP.INT, glP.UNSIGNED_INT, glP.FLOAT, glP.DOUBLE];
var attrSizeMap = [0, 1, 1, 2, 2, 4, 4, 4, 8];


Model = function(url, context, options) {
	if(!options)
		options = {};

	var t = this;
	t.context = context;
	t._url = url;
	t.onLoad = options.onLoad;
	t.onError = options.onError;
	t.isReady = false;
	t.isFailed = false;

	t.modelViewInv     = new Float32Array(16);
	t.modelViewProj    = new Float32Array(16);
	t.modelViewProjInv = new Float32Array(16);
	t.planes           = new Float32Array(24);
	t.viewpoint        = new Float32Array(4);

	httpRequest(t.url(), 0, 88, 
		function() {
			var view = new DataView(this.response);
			view.offset = 0;
			var header = t.importHeader(view);
			if(!header) {
				if(t.onError) t.onError();
				t.isFailed = true;
				return;
			}

			for(i in header)
				t[i] = header[i];

			t.vertex     = t.signature.vertex;
			t.face       = t.signature.face;
			t.compressed = (t.signature.flags & (2 | 4)); //meco or corto
			t.meco       = (t.signature.flags & 2); 
			t.corto      = (t.signature.flags & 4); 
			t.requestIndex();
		},
		function() { t.isFailed = true; if(t.onError) t.onError(); }
	);
	t.context.models.push(t);
}

Model.prototype = {

dispose: function() {
},

url: function() {
	var url = this._url;
	//Safari PATCH
	if (this.context.cachePatch) url = this._url + '?' + Math.random();
	//Safari PATCH
	return url;
},

requestIndex: function() {
	var t = this;
	var end = 88 + t.nodesCount*44 + t.patchesCount*12 + t.texturesCount*68;
	httpRequest(t.url(), 88, end, function() { t.handleIndex(this.response); });
},

handleIndex: function(buffer) {
	var t = this;
	var view = new DataView(buffer);
	view.offset = 0;

	var n = t.nodesCount;
	t.noffsets  = new Uint32Array(n);
	t.nvertices = new Uint32Array(n);
	t.nfaces    = new Uint32Array(n);
	t.nerrors   = new Float32Array(n);
	t.nspheres  = new Float32Array(n*5);
	t.nsize     = new Float32Array(n);
	t.nfirstpatch = new Uint32Array(n);

		for(i = 0; i < n; i++) {
			t.noffsets[i] = padding*getUint32(view); //offset
			t.nvertices[i] = getUint16(view);        //verticesCount
			t.nfaces[i] = getUint16(view);           //facesCount
			t.nerrors[i] = getFloat32(view);
			view.offset += 8;                        //skip cone
			for(k = 0; k < 5; k++)
				t.nspheres[i*5+k] = getFloat32(view);       //sphere + tight
			t.nfirstpatch[i] = getUint32(view);          //first patch
		}
		t.sink = n -1;

		t.patches = new Uint32Array(view.buffer, view.offset, t.patchesCount*3); //noded, lastTriangle, texture
		t.nroots = t.nodesCount;
		for(j = 0; j < t.nroots; j++) {
			for(i = t.nfirstpatch[j]; i < t.nfirstpatch[j+1]; i++) {
				if(t.patches[i*3] < t.nroots)
					t.nroots = t.patches[i*3];
			}
		t.nerrors[j] = 1e20;
	}

	view.offset += t.patchesCount*12;

	t.textures = new Uint32Array(t.texturesCount);
	t.texref = new Uint32Array(t.texturesCount);
	for(i = 0; i < t.texturesCount; i++) {
		t.textures[i] = padding*getUint32(view);
		view.offset += 16*4; //skip proj matrix
	}

	t.vsize = 12 + (t.vertex.normal?6:0) + (t.vertex.color?4:0) + (t.vertex.texCoord?8:0);
	t.fsize = 6;
	//problem: I have no idea how much space a texture is needed in GPU. 10x factor assumed. next version add width/height

	var tmptexsize = new Uint32Array(n-1);
	var tmptexcount = new Uint32Array(n-1);
	for(var i = 0; i < n-1; i++) {
		for(var p = t.nfirstpatch[i]; p != t.nfirstpatch[i+1]; p++) {
			var tex = t.patches[p*3+2];
			tmptexsize[i] += t.textures[tex+1] - t.textures[tex];
			tmptexcount[i]++;
		}
		t.nsize[i] = t.vsize*t.nvertices[i] + t.fsize*t.nfaces[i];
	}
	for(var i = 0; i < n-1; i++)
		t.nsize[i] += 10*tmptexsize[i]/tmptexcount[i];

	t.status    = new Uint8Array(n); //0 for none, 1 for ready, 2+ for waiting data
	t.frames    = new Uint32Array(n);
	t.errors    = new Float32Array(n); //biggest error of instances
	t.texids    = new Uint32Array(n);

	t.vertices  = new Array(n);
	t.indices   = new Array(n);
	t.materials = new Array(n);

	t.isReady = true;
	if(t.onLoad)
		t.onLoad();
	if(t.context.onUpdate)
		t.context.onUpdate();
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

importVertex: function(view) {	//enum POSITION, NORMAL, COLOR, TEXCOORD, DATA0
	var e = this.importElement(view);
	var color = e[2]; 
	if(color) {
		color.type = 2; //unsigned byte
		color.glType = attrGlMap[2];
	}
	return { position: e[0], normal: e[1], color: e[2], texCoord: e[3], data: e[4] };
},

	//enum INDEX, NORMAL, COLOR, TEXCOORD, DATA0
importFace: function(view) {
	var e = this.importElement(view);
	var color = e[2];
	if(color) {
		color.type = 2; //unsigned byte
		color.glType = attrGlMap[2];
	}
	return { index: e[0], normal: e[1], color: e[2], texCoord: e[3], data: e[4] };
},

importSignature: function(view) {
	var s = {};
	s.vertex = this.importVertex(view);
	s.face = this.importFace(view);
	s.flags = getUint32(view);
	return s;
},

importHeader: function(view) {
	var magic = getUint32(view);
	if(magic != 0x4E787320) return null;
	var h = {};
	h.version = getUint32(view);
	h.verticesCount = getUint64(view);
	h.facesCount = getUint64(view);
	h.signature = this.importSignature(view);
	h.nodesCount = getUint32(view);
	h.patchesCount = getUint32(view);
	h.texturesCount = getUint32(view);
	h.sphere = { 
		center: [getFloat32(view), getFloat32(view), getFloat32(view)],
		radius: getFloat32(view)
	};
	return h;
},

traversal : function (targetError, drawBudget) {
	var t = this;
	if(!t.isReady) return;

	t.targetError = targetError;
	t.drawBudget = drawBudget;
	t.currentError = 1e20;
	t.drawSize = 0;


	var n = t.nodesCount;
	t.visited  = new Uint8Array(n); //easiest way to cleanup.
	t.blocked  = new Uint8Array(n);
	t.selected = new Uint8Array(n);

	t.visitQueue = new PriorityQueue(n);
	for(var i = 0; i < t.nroots; i++)
		t.insertNode(i);

	var candidatesCount = 0;
	var nblocked = 0;
	var requested = 0;
	while(t.visitQueue.size && nblocked < t.context.maxBlocked) {
		var error = t.visitQueue.error[0];
		var node = t.visitQueue.pop();
		if ((requested < t.context.maxPending) && (t.status[node] == 0)) {
			t.context.candidates.push({id:node, model:t, frame:t.context.frame, error:error});
			requested++;
		}

		var blocked = t.blocked[node] || !t.expandNode(node, error);
		if (blocked) 
			nblocked++;
		else {
			t.selected[node] = 1;
			t.currentError = error;
		}
		t.insertChildren(node, blocked);
	}
},

insertNode: function (node) { //targeterror, frame
	var t = this;
	t.visited[node] = 1;

	var error = t.nodeError(node);
//	if(node > t.nroots && error < t.targetError) return;
	var errors = t.errors;
	var frames = t.frames;
	if(frames[node] != t.context.frame || errors[node] < error) {
		errors[node] = error;
		frames[node] = t.context.frame;
	}
	t.visitQueue.push(node, error);
},

insertChildren : function (node, block) {
	var t = this;
	for(var i = t.nfirstpatch[node]; i < t.nfirstpatch[node+1]; ++i) {
		var child = t.patches[i*3];
		if (child == t.sink) return;
		if (block) t.blocked[child] = 1;
		if (!t.visited[child])
			t.insertNode(child);
	}
},

expandNode : function (node, error) {  //targeterror 
	var t = this;
	if(node > 0 && error < t.targetError) {//error
//		console.log("Reached target error");
		return false;
	}

	if(t.drawSize > t.drawBudget) {
//		console.log("Reached draw budget");
		return false;
	}

	var sp = t.nspheres;
	var off = node*5;
	if(t.isVisible(sp[off], sp[off+1], sp[off+2], sp[off+3])) //expanded radius
		t.drawSize += t.nvertices[node]*0.8; 
		//we are adding half of the new faces. (but we are using the vertices so *2)
	return t.status[node] == 1; //not ready return false
},

nodeError : function (n) { //viewpoint, resolution
	var t = this;
	var spheres = t.nspheres;
	var b = t.viewpoint;
	var off = n*5;
	var cx = spheres[off+0];
	var cy = spheres[off+1];
	var cz = spheres[off+2];
	var r  = spheres[off+3];
	var d0 = b[0] - cx;
	var d1 = b[1] - cy;
	var d2 = b[2] - cz;
	var dist = Math.sqrt(d0*d0 + d1*d1 + d2*d2) - r;
	if (dist < 0.1) dist = 0.1;

	var error = t.nerrors[n]/(t.resolution*dist);

	if (!t.isVisible(cx, cy. cz, r))
		error /= 1000.0;
	return error;
},

isVisible : function (x, y, z, r) {
	var p = this.planes;
	for (i = 0; i < 16; i +=4) {
		if(p[i]*x + p[i+1]*y + p[i+2]*z +p[i+3] + r < 0) //ax+by+cz+w = 0;
			return false;
	}
	return true;
},

updateView: function(viewport, projection, modelView) {
	var t = this;

	matMul(projection, modelView, t.modelViewProj);
	matInv(t.modelViewProj, t.modelViewProjInv);
	matInv(modelView, t.modelViewInv);

	t.viewpoint[0] = t.modelViewInv[12]; 
	t.viewpoint[1] = t.modelViewInv[13];
	t.viewpoint[2] = t.modelViewInv[14]; 
	t.viewpoint[3] = 1.0;

	var m = t.modelViewProj;
	var mi = t.modelViewProjInv;
	var p = t.planes;

	//left, right, bottom, top as Ax + By + Cz + D = 0;
	p[0]  =  m[0] + m[3]; p[1]  =  m[4] + m[7]; p[2]  =  m[8] + m[11]; p[3]  =  m[12] + m[15];
	p[4]  = -m[0] + m[3]; p[5]  = -m[4] + m[7]; p[6]  = -m[8] + m[11]; p[7]  = -m[12] + m[15];
	p[8]  =  m[1] + m[3]; p[9]  =  m[5] + m[7]; p[10] =  m[9] + m[11]; p[11] =  m[13] + m[15];
	p[12] = -m[1] + m[3]; p[13] = -m[5] + m[7]; p[14] = -m[9] + m[11]; p[15] = -m[13] + m[15];
	p[16] = -m[2] + m[3]; p[17] = -m[6] + m[7]; p[18] = -m[10] + m[11]; p[19] = -m[14] + m[15];
	p[20] = -m[2] + m[3]; p[21] = -m[6] + m[7]; p[22] = -m[10] + m[11]; p[23] = -m[14] + m[15];

	for(var i = 0; i < 16; i+= 4) {
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

	t.resolution = (2*side/dist)/ viewport[2];
},

removeNode: function(node) {
	var t = this;
	var n = node.id;
	var gl = t.context.gl;
	t.status[n] = 0;
	gl.deleteBuffer(t.vbo[n]);
	gl.deleteBuffer(t.ibo[n]);
	t.vbo[n] = t.ibo[n] = null;

	if(!t.vertex.texCoord) return;
	var tex = t.patches[t.nfirstpatch[n]*3+2];  //TODO assuming one texture per node
	t.texref[tex]--;

	if(t.texref[tex] == 0 && t.texids[tex]) {
		gl.deleteTexture(t.texids[tex]);
		t.texids[tex] = null;
	}
},

loadNode: function(node) {
	var m = this;	
	var n = node.id;

	var nv = m.nvertices[n];
	var nf = m.nfaces[n];

	var vertices;
	var indices;
	if(!m.corto) {
		vertices = new Uint8Array(node.buffer, 0, nv*m.vsize);
		//dangerous alignment 16 bits if we had 1b attribute
		indices  = new Uint8Array(node.buffer, nv*m.vsize,  nf*m.fsize);

	} else {
		indices = node.model.index;
		vertices = new ArrayBuffer(nv*m.vsize);
		var v = new Float32Array(vertices, 0, nv*3);
		v.set(model.position, 0, nv*12);
		var off = nv*12;
		if(model.uv) {
			var u = new Float32Array(vertices, off, nv*2);
			u.set(model.uv, off, off+nv*8); off += nv*8;
		}
		if(model.normal) {
			var n = new Int16Array(vertices, off, nv*3);
			vertices.set(model.normal, off, off+nv*6); off += nv*6; 
		}
		if(model.color) {
			var n = new Uint8Array(vertices, off, nv*4);
			vertices.set(model.color, off, off+nv*4); off += nv*4;  
		}
	}

	var gl = t.context.gl;
	var vbo = m.vbo[n] = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
	gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
	var ibo = m.ibo[n] = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW); 
},

loadTexture: function(img, texid) {
	var m = this;
	var gl = this.context.gl;

	var flip = gl.getParameter(gl.UNPACK_FLIP_Y_WEBGL);
	gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
	var tex = m.texids[texid] = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, tex);
	var s = gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
	gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, flip);
},

currentTargetError: function() {
	var t = this;
	var c = t.context;
	var factor = c.fps? c.targetErrorFactor: 1.0;
	var error = t.targetError? t.targetError : c.targetError;
	return factor * error;
}

};  //prototype


// defaults 

function Context(options) {
	var t = this;
	t.targetError   = 3.0;           //approximate error target in rendering
	t.targetErrorFactor = 1.0;
	t.maxTargetError = 10;           //if targetFps enabled
	t.targetFps     = 20;            //automatically adjust targetError to match frameraate (if null, does nothing)
	t.maxPending    = 3;             //max number of enqueued http requests
	t.maxBlocked    = 20;             //amount of prefetch
	t.maxCacheSize  = 256*(1<<20);   //global cache size (256MB)
	t.drawBudget    = 5*(1<<20);     //max single model budget for rendering
	t.cachePatch    = true;          //set false for mac bug.

	for(var i in options)
		t[i] = options[i];

	t.models = [];                   //list of nexus models
	t.frame = 0;                     //keep tracks of unused nodes
	t.cacheSize = 0;
	t.candidates = [];               //keep tracks of to be loaded nodes
	t.pending = 0;
	t.lastUpdated = 0;               //used to keep track of fps
}

Context.prototype = {

updateCache: function() {
	var t = this;
	var best = null;
	t.candidates.forEach(function(c) {
		if(c.model.status[c.id] != 0)
			return;
		if(!best || c.error > best.error)
			best = c;
	});

	if(!best) return;
//	console.log("Best id: " + best.id);

	while(t.cacheSize > t.maxCacheSize) {
		var worst = null;
		//find worst in cache
		t.models.forEach(function(m) {
			var n = m.nodesCount;
			for(i = 0; i < n; i++)
				if(!worst || (m.status[i] == 1 && m.errors[i] < worst.error))
					worst = {error: m.errors[i], frame: m.frames[i], model:m, id:i};
		});

		if(!worst || (worst.error >= best.error && worst.frame == best.frame))
			return;
		t.removeNode(worst);
	}
	t.requestNode(best);
 
	if(t.pending < t.maxPending)
		t.updateCache();
},

update: function() {
	var t = this;
	t.updateCache();

	t.frame++;
	t.candidates = [];

	var now = performance.now();
	var elapsed = now - t.lastUpdated;
	t.lastUpdated = now;

	var fps = 1000/elapsed;

	if(fps && t.targetFps) {
		var r = t.targetFps/fps;
		if(r > 1.1)
			t.targetErrorFactor *= 1.05;
		if(r < 0.9)
			t.targetErrorFactor *= 0.95;
	
		t.targetErrorFactor = Math.max(t.targetErrorFactor, 1.0);
		if(t.targetErrorFactor*t.targetError > t.maxTargetError)
			t.targetErrorFactor = t.maxTargetError/t.targetError;
	}
//	console.log("elapsed: ", elapsed, "current", t.targetErrorFactor, "fps", fps, "targetFps", t.targetFps);
},

removeNode: function(node) {
	var n = node.id;
	var m = node.model;
	t.cacheSize -= m.nsize[n];

	m.removeNode(node);
},

requestNode: function(node) {
//	console.log("Request node: " + node.id);
	var t = this;
	var n = node.id;
	var m = node.model;
	m.status[n] = 2; //pending
	this.pending++;
	this.cacheSize += m.nsize[n];

	httpRequest(m.url(), m.noffsets[n], m.noffsets[n+1], 
		function() { t.loadNode(this, node); },
		function () { console.log("Failed loading node for: " + m.url); t.pending--;},
		'arraybuffer'
	);
	if(!m.vertex.texCoord) return;

	var tex = m.patches[m.nfirstpatch[n]*3+2];
	m.texref[tex]++;
	if(m.texids[tex])
		return;

	//TODO: tex might have been already requested (in practice does not happen).
	m.status[n]++;
	httpRequest(m.url(), m.textures[tex], m.textures[tex+1], 
		function() { t.loadTexture(this, node, tex); },
		function () { console.log("Failed loading node for: " + m.url); m.texids[tex] = null; },
		'blob'
	);
},

loadNode: function(request, node) {
//	console.log("Load node: " + node.id);
	var n = node.id;
	var m = node.model;

	node.buffer = request.response;
	node.nvert = m.nvertices[n];
	node.nface = m.nfaces[n];
	node.context = this;

	if(!m.compressed)
		readyNode(node);
	else if(m.meco) {
		var sig = { texcoords: m.vertex.texCoord, normals:m.vertex.normal, colors:m.vertex.color, indices: m.face.index }
		var patches = [];
		for(var k = m.nfirstpatch[n]; k < m.nfirstpatch[n+1]; k++)
			patches.push(m.patches[k*3+1]);
		meco.postRequest(sig, node, patches);
	} else {
		corto.postRequest(node);
	}
},

loadTexture: function(request, node, texid) {
//	console.log("Texture load: " + node.id);
	var t = this;
	var m = node.model;
	var n = node.id;
	if(m.status[n] == 0) return; //aborted

	var blob = request.response; 
	var urlCreator = window.URL || window.webkitURL;
	var img = document.createElement('img');
	img.onerror = function(e) { console.log("Failed loading texture."); };
	img.src = urlCreator.createObjectURL(blob);


	img.onload = function() { 
		urlCreator.revokeObjectURL(img.src); 

		m.status[n]--;
		m.loadTexture(img, texid); 

		t.updateCache();
		if(t.onUpdate)
			t.onUpdate();
	}
},


readyNode: function(node) {
//	console.log("Ready node: " + node.id);
	var t = this;
	var m = node.model;
	var n = node.id;


	m.status[n]--;
	m.loadNode(node);


	t.pending--;
	if(m.status[n] == 1 && t.onUpdate)
		t.onUpdate();
	t.updateCache(); //call anyway even if waiting for texture
}

}; //context prototype

return {
	Debug: Debug,
	Model: Model, 
	Context: Context
};

}();

/*


THREE.Nexus.loader = new THREE.FileLoader();
Nexus.httpRequest = function(url, start, end, load, error) {
	var loader = THREE.Nexus.loader;
	loader.setRequestHeader({ Range: "bytes=" + start + "-"  + (end-1) });
	THREE.Nexus.loader.load(url, load, null, error);
}

THREE.Nexus.prototype = {

	constructor: THREE.Nexus,

	onBeforeRender: function(renderer, scene, camera, geometry, material, group) {
		if(!isReady) return;
		var s = renderer.getSize();
		var t = this;
		t.updateView([0, 0, s.width, s.height],
			camera.projectionMatrix.elements, 
			mesh.modelViewMatrix.elements);
		t.traversal();

		var mesh = t.mesh;
		for(var n = 0; n < mesh.nodesCount; n++) {
			if(!instance.selected[n]) continue;
			var skip = true;
			for(var p = mesh.nfirstpatch[n]; p < mesh.nfirstpatch[n+1]; p++) {
				var child = mesh.patches[p*3];
				if(!instance.selected[child]) {
					skip = false;
					break;
				}
			}
			var sp = mesh.nspheres;
			var off = n*5;
			if(!t.isVisible(sp[off], sp[off+1], sp[off+2], sp[off+4])) //tight radius
				skip = true;

			var node = t.models[n];
			if(skip) {
				node.visible = false;
				continue;
			}


			var offset = 0;
			var end = 0;
			var last = m.nfirstpatch[n+1]-1;
			node.geometry.clearGroups();
			for (var p = m.nfirstpatch[n]; p < m.nfirstpatch[n+1]; ++p) {
				var child = m.patches[p*3];

				if(!t.selected[child]) { 
					end = m.patches[p*3+1];
					if(p < last) //if textures we do not join. TODO: should actually check for same texture of last one. 
						continue;
				} 
				if(end > offset) {
					var texid = 0;
					if(m.vertex.texCoord) {
						var id = m.patches[p*3+2];
						texid = m.texids[texid];
						if(texid < 0) texid = 0;
					}
					node.geometry.addGroup(offset, end - offset, texid); 
					rendered += end - offset;
				}
				offset = m.patches[p*3+1];
			}
		}
	},

	open: function(url) {
		var t = this;
		var mesh;
		context.models.forEach(function(m) {
			if(m.url == url)
			t.mesh = m;
		});

		if(!t.mesh) {
			t.mesh = new Model();
			t.mesh.onLoad = function() { t.renderMode = t.mesh.renderMode; t.mode = t.renderMode[0]; t.onLoad(); }
			t.mesh.open(url);
			context.models.push(t.mesh);
		}


	},

	close: function() {
		//remove instance from mesh
	},

	get isReady() { return this.mesh.isReady; },
	get datasetRadius() { if(!this.isReady) return 1.0;       return this.mesh.sphere.radius; },
	get datasetCenter() { if(!this.isReady) return [0, 0, 0]; return this.mesh.sphere.center; },

	update: function(camera) {
	},

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

		//left, right, bottom, top as Ax + By + Cz + D = 0;
		p[0]  =  m[0] + m[3]; p[1]  =  m[4] + m[7]; p[2]  =  m[8] + m[11]; p[3]  =  m[12] + m[15];
		p[4]  = -m[0] + m[3]; p[5]  = -m[4] + m[7]; p[6]  = -m[8] + m[11]; p[7]  = -m[12] + m[15];
		p[8]  =  m[1] + m[3]; p[9]  =  m[5] + m[7]; p[10] =  m[9] + m[11]; p[11] =  m[13] + m[15];
		p[12] = -m[1] + m[3]; p[13] = -m[5] + m[7]; p[14] = -m[9] + m[11]; p[15] = -m[13] + m[15];
		p[16] = -m[2] + m[3]; p[17] = -m[6] + m[7]; p[18] = -m[10] + m[11]; p[19] = -m[14] + m[15];
		p[20] = -m[2] + m[3]; p[21] = -m[6] + m[7]; p[22] = -m[10] + m[11]; p[23] = -m[14] + m[15];

		for(var i = 0; i < 16; i+= 4) {
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

		t.resolution = (2*side/dist)/ t.viewport[2];
	},

	traversal : function () {
		var t = this;
		if(Debug.extract == true)
			return;

		if(!t.isReady) return;

		var n = t.mesh.nodesCount;
		t.visited  = new Uint8Array(n);
		t.blocked  = new Uint8Array(n);
		t.selected = new Uint8Array(n);

		t.visitQueue = new PriorityQueue(n);
		for(var i = 0; i < t.mesh.nroots; i++)
			t.insertNode(i);

		var candidatesCount = 0;
		t.targetError = t.context.targetErrorFactor;
		t.currentError = 1e20;
		t.drawSize = 0;

		var nblocked = 0;
		var requested = 0;
		while(t.visitQueue.size && nblocked < maxBlocked) {
			var error = t.visitQueue.error[0];
			var node = t.visitQueue.pop();
			if ((requested < maxPending) && (t.mesh.status[node] == 0)) {
				context.candidates.push({id: node, instance:t, mesh:t.mesh, frame:t.context.frame, error:error});
				requested++;
			}

			var blocked = t.blocked[node] || !t.expandNode(node, error);
			if (blocked) 
				nblocked++;
			else {
				t.selected[node] = 1;
				t.currentError = error;
			}
			t.insertChildren(node, blocked);
		}
	},

	insertNode: function (node) {
		var t = this;
		t.visited[node] = 1;

		var error = t.nodeError(node);
		if(node > 0 && error < t.targetError) return;  //2% speed TODO check if needed

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
			var child = t.mesh.patches[i*3];
			if (child == t.mesh.sink) return;
			if (block) t.blocked[child] = 1;
			if (!t.visited[child])
				t.insertNode(child);
		}
	},

	expandNode : function (node, error) {
		var t = this;
		if(node > 0 && error < t.targetError) {
//			console.log("Reached error", error, t.targetError);
			return false;
		}

		if(t.drawSize > t.drawBudget) {
//			console.log("Reached drawsize", t.drawSize, t.drawBudget);
			return false;
		}

		if(t.mesh.status[node] != 1) { //not ready
//			console.log("Still not loaded (cache?)");
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
		var error = t.mesh.nerrors[n]/(t.resolution*dist); //in pixels

		if (!t.isVisible(cx, cy. cz, spheres[off+4]))
			error /= 1000.0;
		return error;
	},

	isVisible : function (x, y, z, r) {
		var p = this.planes;
		for (i = 0; i < 24; i +=4) {
			if(p[i]*x + p[i+1]*y + p[i+2]*z +p[i+3] + r < 0) //ax+by+cz+w = 0;
				return false;
		}
		return true;
	}
};


function beginFrame(fps) { //each context has a separate frame count.
	var c = context;
	c.frame++;
	c.candidates = [];
	if(fps && c.targetFps) {
		var r = c.targetFps/fps;
		if(r > 1.1)
			c.targetErrorFactor *= 1.05;
		if(r < 0.9)
			c.targetErrorFactor *= 0.95;
	
		if(c.targetErrorFactor < c.targetError)
			c.targetErrorFactor = c.targetError;
		if(c.targetErrorFactor > 20) c.targetErrorFactor = 20;
	}
//	console.log("current", c.targetErrorFactor, "fps", fps, "targetFps", c.targetFps, "rendered", c.rendered);
	c.rendered = 0;
}

function endFrame(gl) {
	updateCache(gl);
}

function removeNode(context, node) {
	var n = node.id;
	var m = node.mesh;
	m.status[n] = 0;
	context.cacheSize -= m.nsize[n];
	context.gl.deleteBuffer(m.vbo[n]);
	context.gl.deleteBuffer(m.ibo[n]);
	m.vbo[n] = m.ibo[n] = null;

	if(!m.vertex.texCoord) return;
	var tex = m.patches[m.nfirstpatch[n]*3+2];  //TODO assuming one texture per node
	m.texref[tex]--;

	if(m.texref[tex] == 0 && m.texids[tex]) {
		context.gl.deleteTexture(m.texids[tex]);
		m.texids[tex] = null;
	}
}

function requestNode(context, node) {
	var n = node.id;
	var m = node.mesh;
	m.status[n] = 2; //pending
	context.pending++;
	context.cacheSize += m.nsize[n];

	Nexus.httpRequest(m.url(), m.noffsets[n], m.noffsets[n+1], 
		function() { loadNode(this, context, node); },
		function () { console.log("Failed loading node for: " + m.url); context.pending--;},
		'arraybuffer'
	);
	if(!m.vertex.texCoord) return;
	var tex = m.patches[m.nfirstpatch[n]*3+2];
	m.texref[tex]++;
	if(m.texids[tex])
		return;

	m.status[n]++;
	Nexus.httpRequest(m.url(), m.textures[tex], m.textures[tex+1], 
		function() { loadTexture(this, context, node, tex); },
		function () { console.log("Failed loading node for: " + m.url); m.texids[tex] = null; },
		'blob'
	);
}

function loadNode(request, context, node) {
	var n = node.id;
	var m = node.mesh;

	node.context = context;
	node.buffer = request.response;
	node.nvert = m.nvertices[n];
	node.nface = m.nfaces[n];

	if(!m.compressed)
		readyNode(node);
	else if(m.meco) {
		var sig = { texcoords: m.vertex.texCoord, normals:m.vertex.normal, colors:m.vertex.color, indices: m.face.index }
		var patches = [];
		for(var k = m.nfirstpatch[n]; k < m.nfirstpatch[n+1]; k++)
			patches.push(m.patches[k*3+1]);
		worker.postRequest(sig, node, patches);
	} else {
		corto.postRequest(node);
	}
}

function loadTexture(request, context, node, texid) {
	var m = node.mesh;
	var n = node.id;
	if(m.status[n] == 0) return; //aborted

	var blob = request.response; 
	var urlCreator = window.URL || window.webkitURL;
	var img = document.createElement('img');
	img.onerror = function(e) { console.log("Failed loading texture."); };
	img.src = urlCreator.createObjectURL(blob);

	var gl = context.gl;
	img.onload = function() { 
		urlCreator.revokeObjectURL(img.src); 

		var flip = gl.getParameter(gl.UNPACK_FLIP_Y_WEBGL);
		gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
		var tex = m.texids[texid] = gl.createTexture();
		gl.bindTexture(gl.TEXTURE_2D, tex);
		var s = gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
		gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, flip);

		m.status[n]--;
		updateCache(gl);
		node.instance.onUpdate();
	}
}

function readyNode(context, node) {
	var m = node.mesh;
	var n = node.id;
	var nv = m.nvertices[n];
	var nf = m.nfaces[n];
	var model = node.model;

	if(!m.corto) {
	var vertices = new Uint8Array(node.buffer, 0, nv*m.vsize);
		//dangerous alignment 16 bits if we had 1b attribute
	var indices  = new Uint8Array(node.buffer, nv*m.vsize,  nf*m.fsize);
	} else {
		var indices = node.model.index;
		var vertices = new ArrayBuffer(nv*m.vsize);
		var v = new Float32Array(vertices, 0, nv*3);
		v.set(model.position, 0, nv*12);
		var off = nv*12;
		if(model.uv) {
			var u = new Float32Array(vertices, off, nv*2);
			u.set(model.uv, off, off+nv*8); off += nv*8;
		}
		if(model.normal) {
			var n = new Int16Array(vertices, off, nv*3);
			vertices.set(model.normal, off, off+nv*6); off += nv*6; 
		}
		if(model.color) {
			var n = new Uint8Array(vertices, off, nv*4);
			vertices.set(model.color, off, off+nv*4); off += nv*4;  
		}
	}

	var gl = context.gl;
	var vbo = m.vbo[n] = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
	gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
	var ibo = m.ibo[n] = gl.createBuffer();
	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
	gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);

	m.status[n]--;
	context.pending--;
	if(m.status[n] == 1)
		node.instance.onUpdate();
	updateCache(gl); //call anyway even if waiting for texture
}

*/
