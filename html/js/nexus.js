/*
3DHOP - 3D Heritage Online Presenter
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

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

Nexus = function() {

/* WORKER INITIALIZED ONCE */

var worker;

var scripts = document.getElementsByTagName('script');
var i, j, k;
for(i = 0; i < scripts.length; i++) {
	var attrs = scripts[i].attributes;
	for(j = 0; j < attrs.length; j++) {
		var a = attrs[j];
		if(a.name != 'src') continue;
		if(!a.value) continue;
		if(a.value.search('nexus.js') >= 0) {
			var path = a.value.replace('nexus.js', 'meshcoder_worker.js');
			worker = new Worker(path);
			worker.requests = {};
			worker.count = 0;
			worker.postRequest = function(sig, node, patches) {
				var signature = { texcoords: sig.texcoords?1:0, colors: sig.colors?1:0, normals: sig.normals?1:0, indices: sig.indices?1:0 };
				worker.postMessage({ signature:signature, node:{nface: node.nface, nvert: node.nvert, buffer:node.buffer, request:this.count}, patches:patches});
				this.requests[this.count++] = node;
			};
			worker.onmessage = function(e) {
				var node = this.requests[e.data.request];
				node.buffer = e.data.buffer;
				readyNode(node.context, node);
			};
			break;
		}
	}
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

//normalized projection
function matMulV(a, b) {
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


var targetError   = 3.0;
var targetFps     = 20;
var maxPending    = 3;
var maxBlocked    = 3;
var maxCacheSize  = 256*(1<<20); //TODO DEBUG
var drawBudget    = 1*(1<<20);
var minDrawBudget = drawBudget/4;
var cachePatch    = true; //set false for mac.


/* MESH DEFINITION */

Mesh = function() {
	var t = this;
	t.onLoad = null;
}

Mesh.prototype = {
	open: function(url) {
		var mesh = this;
		mesh._url = url;
		mesh.httpRequest(0, 88, function() {
			var view = new DataView(this.response);
			view.offset = 0;
			var header = mesh.importHeader(view);
			if(!header) return null;

			for(i in header)
				mesh[i] = header[i];
			mesh.vertex = mesh.signature.vertex;
			mesh.face = mesh.signature.face;
			mesh.renderMode = mesh.face.index?["FILL", "POINT"]:["POINT"];
			mesh.compressed = (mesh.signature.flags & 2);
			mesh.requestIndex();
		});
	},

	url: function() {
		var url = this._url;
		//Safari PATCH
		if (cachePatch) url = this._url + '?' + Math.random();
		//Safari PATCH
		return url;
	},

	httpRequest: function(start, end, load, error, abort, type) {
		if(!type) type = 'arraybuffer';
		var that = this;
		var r = new XMLHttpRequest();
		r.open('GET', this.url(), true);
		r.responseType = type;
		r.setRequestHeader("Range", "bytes=" + start + "-" + (end -1));
		r.onload = function(){
			switch (this.status){
				case 206:
                			load.bind(this)();
                			break;
				case 200:
					console.log("200 response; server does not support byte range requests.")
			}
        	};
		r.onerror = error;
		r.onabort = abort;
		r.send();
	},

	requestIndex: function() {
		var mesh = this;
		var end = 88 + mesh.nodesCount*44 + mesh.patchesCount*12 + mesh.texturesCount*68;
		mesh.httpRequest(88, end, function() { mesh.handleIndex(this.response); });
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
			t.nvertices[i] = getUint16(view);          //verticesCount
			t.nfaces[i] = getUint16(view);          //facesCount
			t.nerrors[i] = getFloat32(view);
			view.offset += 8;                             //skip cone
			for(k = 0; k < 5; k++)
				t.nspheres[i*5+k] = getFloat32(view);       //sphere + tight
			t.nfirstpatch[i] = getUint32(view);          //first patch
		}
		t.sink = n -1;

		t.patches = new Uint32Array(view.buffer, view.offset, t.patchesCount*3); //noded, lastTriangle, texture
		view.offset += t.patchesCount*12;

		t.textures = new Uint32Array(t.texturesCount);
		t.texref = new Uint32Array(t.texturesCount);
		for(i = 0; i < t.texturesCount; i++) {
			t.textures[i] = padding*getUint32(view);
			view.offset += 16*4; //skip proj matrix
		}

		t.vsize = 12 + (t.vertex.normal?6:0) + (t.vertex.color?4:0) + (t.vertex.texCoord?8:0);
		t.fsize = 6;
		//problem: I have no idea how much space a texture is needed in GPU. 10x factor assumed.

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
		for(var i = 0; i < n-1; i++) {
			t.nsize[i] += 10*tmptexsize[i]/tmptexcount[i];
		}

		t.status = new Uint8Array(n); //0 for none, 1 for ready, 2+ for waiting data
		t.frames = new Uint32Array(n);
		t.errors = new Float32Array(n); //biggest error of instances
		t.ibo    = new Array(n);
		t.vbo    = new Array(n);
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
	}
}; 

Instance = function(gl) {
	this.gl = gl;
	this.onLoad = function() {};
	this.onUpdate = function() {};
	this.drawBudget = drawBudget;
	this.minDrawBudget = minDrawBudget;
	this.attributes = { 'position':0, 'normal':1, 'color':2, 'uv':3 };
}

Instance.prototype = {
	open: function(url) {
		var t = this;
		t.context = getContext(t.gl);
		var mesh;
		t.context.meshes.forEach(function(m) {
			if(m.url == url)
			t.mesh = m;
			//m.instances.push(t);
		});
		if(!t.mesh) {
			t.mesh = new Mesh();
			t.mesh.onLoad = function() { t.renderMode = t.mesh.renderMode; t.mode = t.renderMode[0]; t.onLoad(); }
			t.mesh.open(url);
			t.context.meshes.push(t.mesh);
		}

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
		t.viewpoint[0] = t.modelViewInv[12]; t.viewpoint[1] = t.modelViewInv[13];
		t.viewpoint[2] = t.modelViewInv[14]; t.viewpoint[3] = 1.0;

		var m = t.modelViewProj;
		var mi = t.modelViewProjInv;
		var p = t.planes;

		//left, right, bottom, top as Ax + By + Cz + D = 0;
		p[0]  =  m[0] + m[3]; p[1]  =  m[4] + m[7]; p[2]  =  m[8] + m[11]; p[3]  =  m[12] + m[15];
		p[4]  = -m[0] + m[3]; p[5]  = -m[4] + m[7]; p[6]  = -m[8] + m[11]; p[7]  = -m[12] + m[15];
		p[8]  =  m[1] + m[3]; p[9]  =  m[5] + m[7]; p[10] =  m[9] + m[11]; p[11] =  m[13] + m[15];
		p[12] = -m[1] + m[3]; p[13] = -m[5] + m[7]; p[14] = -m[9] + m[11]; p[15] = -m[13] + m[15];

		for(var i = 0; i < 16; i+= 4) {
			var l = Math.sqrt(p[i]*p[i] + p[i+1]*p[i+1] + p[i+2]*p[i+2]);
			p[i] /= l; p[i+1] /= l; p[i+2] /= l; p[i+3] /= l;
		}
		//side is M'(1,0,0,1) - M'(-1,0,01) and they lie on the planes
		var dx = p[0]*p[3] - p[4]*p[7];
		var dy = p[1]*p[3] - p[5]*p[7];
		var dz = p[2]*p[3] - p[6]*p[7];
		var side = Math.sqrt(dx*dx + dy*dy + dz*dz);

		//center of the scene is M'*(0, 0, 0, 1)
		var rx = mi[3]/mi[15] - t.viewpoint[0];
		var ry = mi[7]/mi[15] - t.viewpoint[1];
		var rz = mi[11]/mi[15] - t.viewpoint[2];
		var dist = Math.sqrt(rx*rx + ry*ry + rz*rz);

		t.resolution = 2*side/(t.viewport[2]*dist); //2* brings it back to the value of the orig nexus.
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
		t.insertNode(0);

		var candidatesCount = 0;
		t.targetError = t.context.currentError;
		t.currentError = 1e20;
		t.drawSize = 0;

		var nblocked = 0;
		var requested = 0;
		while(t.visitQueue.size && nblocked < maxBlocked) {
			var error = t.visitQueue.error[0];
			var node = t.visitQueue.pop();
			if ((requested < maxPending) && (t.mesh.status[node] == 0)) {
				t.context.candidates.push({id: node, instance:t, mesh:t.mesh, frame:t.context.frame, error:error});
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
		if(node > 0 && error < t.targetError*0.8) return;  //2% speed TODO check if needed

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
		if(node > 0 && error < t.targetError) //error
			return false;

		if(t.drawSize > t.drawBudget)
			return false;

		var sp = t.mesh.nspheres;
		var off = node*5;
		if(t.isVisible(sp[off], sp[off+1], sp[off+2], sp[off+3])) //expanded radius
			t.drawSize += t.mesh.nvertices[node]*0.8; 
			//we are adding half of the new faces. (but we are using the vertices so *2)
		return t.mesh.status[node] == 1; //not ready return false
	},

	nodeError : function (n) {
		var t = this;
		var spheres = t.mesh.nspheres;
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

		var error = t.mesh.nerrors[n]/(t.resolution*dist);

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

	renderNodes: function() {
		var t = this;
		var m = t.mesh;
		var gl = t.gl;
		var attr = t.attributes;

		var vertexEnabled = gl.getVertexAttrib(attr.position, gl.VERTEX_ATTRIB_ARRAY_ENABLED);
		var normalEnabled = gl.getVertexAttrib(attr.normal, gl.VERTEX_ATTRIB_ARRAY_ENABLED);
		var colorEnabled = attr.color >= 0? gl.getVertexAttrib(attr.color, gl.VERTEX_ATTRIB_ARRAY_ENABLED): false;
		var uvEnabled = attr.uv >= 0? gl.getVertexAttrib(attr.uv, gl.VERTEX_ATTRIB_ARRAY_ENABLED): false;

		gl.enableVertexAttribArray(attr.position);
		if(m.vertex.texCoord && attr.uv >= 0) gl.enableVertexAttribArray(attr.uv);
		if(m.vertex.normal && attr.normal >= 0) gl.enableVertexAttribArray(attr.normal);
		if(m.vertex.color && attr.color >= 0) gl.enableVertexAttribArray(attr.color);
		gl.vertexAttrib4fv(2, [0.8, 0.8, 0.8, 1.0]);

		if(Debug.nodes) {
			gl.disableVertexAttribArray(2);
			gl.disableVertexAttribArray(3);
		}

		var rendered = 0;
		var last_texture = -1;
		for(var n = 0; n < m.nodesCount; n++) {
			if(!t.selected[n]) continue;

			var skip = true;
			for(var p = m.nfirstpatch[n]; p < m.nfirstpatch[n+1]; p++) {
				var child = m.patches[p*3];
				if(!t.selected[child]) {
					skip = false;
					break;
				}
			}
			if(skip) continue;


			var sp = t.mesh.nspheres;
			var off = n*5;
			if(!t.isVisible(sp[off], sp[off+1], sp[off+2], sp[off+4])) //tight radius
				continue;

			gl.bindBuffer(gl.ARRAY_BUFFER, m.vbo[n]);
			if(t.mode != "POINT")
				gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, m.ibo[n]);

			var nv = m.nvertices[n];

			gl.vertexAttribPointer(attr.position, 3, gl.FLOAT, false, 12, 0);
			var offset = nv*12;
			if(m.vertex.texCoord && attr.uv >= 0)
				gl.vertexAttribPointer(attr.uv, 2, gl.FLOAT, false, 8, offset), offset += nv*8;
			if(m.vertex.normal && attr.normal >= 0)
				gl.vertexAttribPointer(attr.normal, 3, gl.SHORT, true, 6, offset), offset += nv*6;
			if(m.vertex.color && attr.color >= 0)
				gl.vertexAttribPointer(attr.color, 4, gl.UNSIGNED_BYTE, true, 4, offset);

			if (Debug.nodes)
				gl.vertexAttrib4fv(2, [(n*200 %255)/255.0, (n*140 %255)/255.0,(n*90 %255)/255.0, 1]);

			if (Debug.draw) continue;

			if(t.mode == "POINT") {
				var error = t.nodeError(n); //because we lost it due to the instance problem
				var pointsize = Math.floor(0.30*error);
				if(pointsize > 1) pointsize = 1;
				gl.vertexAttrib1fv(4, [pointsize]);

				var fraction = (error/t.currentError - 1);
				if(fraction > 1) fraction = 1;

				var count = fraction * nv;
				if(count != 0) {
					if(m.vertex.texCoord) {
						var texid = m.patches[m.nfirstpatch[n]*3+2];
						if(texid != -1 && texid != last_texture) { //bind texture
							var tex = m.texids[texid];
							gl.activeTexture(gl.TEXTURE0);
							gl.bindTexture(gl.TEXTURE_2D, tex);
						}
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
				var child = m.patches[p*3];

				if(!t.selected[child]) { 
					end = m.patches[p*3+1];
					if(p < last) //if textures we do not join. TODO: should actually check for same texture of last one. 
						continue;
				} 
				if(end > offset) {
					if(m.vertex.texCoord) {
						var texid = m.patches[p*3+2];
						if(texid != -1 && texid != last_texture) { //bind texture
							var tex = m.texids[texid];
							gl.activeTexture(gl.TEXTURE0);
							gl.bindTexture(gl.TEXTURE_2D, tex);
							last_texture = texid;
						}
					}
					gl.drawElements(gl.TRIANGLES, (end - offset) * 3, gl.UNSIGNED_SHORT, offset * 6);
					rendered += end - offset;
				}
				offset = m.patches[p*3+1];
			}
		} 

		t.context.rendered += rendered;
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
	if(!gl.isTexture) throw "PORCA PALETTA";
	contexts.forEach(function(g) { 
		if(g.gl == gl) c = g;
	});
	if(c) return c;
	c = { gl:gl, meshes:[], frame:0, cacheSize:0, candidates:[], pending:0, 
		targetFps: targetFps, targetError: targetError, currentError: targetError };
	contexts.push(c);
	return c;
}



function beginFrame(gl, fps) { //each context has a separate frame count.
	var c = getContext(gl);

	c.frame++;
	c.candidates = [];
	if(fps && c.targetFps) {
		var r = c.targetFps/fps;
		if(r > 1.1)
			c.currentError *= 1.05;
		if(r < 0.9)
			c.currentError *= 0.95;
	
		if(c.currentError < c.targetError)
			c.currentError = c.targetError;
		if(c.currentError > 20) c.currentError = 20;
	}
//	console.log("current", c.currentError, "fps", fps, "targetFps", c.targetFps, "rendered", c.rendered);
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

	m.httpRequest(m.noffsets[n], m.noffsets[n+1], 
		function() { loadNode(this, context, node); },
		function () { console.log("Failed loading node for: " + m.url); context.pending--;},
		function () { console.log("Abort!"); m.status[n] = 0; context.pending--; },
		'arraybuffer'
	);
	if(!m.vertex.texCoord) return;
	var tex = m.patches[m.nfirstpatch[n]*3+2];
	m.texref[tex]++;
	if(m.texids[tex])
		return;

	m.status[n]++;
	m.httpRequest(m.textures[tex], m.textures[tex+1], 
		function() { loadTexture(this, context, node, tex); },
		function () { console.log("Failed loading node for: " + m.url); },
		function () { console.log("Abort!"); m.texids[tex] = null; },
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
		readyNode(context, node);
	else {
		var sig = { texcoords: m.vertex.texCoord, normals:m.vertex.normal, colors:m.vertex.color, indices: m.face.index }
		var patches = [];
		for(var k = m.nfirstpatch[n]; k < m.nfirstpatch[n+1]; k++)
			patches.push(m.patches[k*3+1]);
		worker.postRequest(sig, node, patches);
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

	var vertices = new Uint8Array(node.buffer, 0, nv*m.vsize);
	var indices  = new Uint8Array(node.buffer, nv*m.vsize,  nf*m.fsize);

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

function updateCache(gl) {
	var context = getContext(gl);

	var best = null;
	context.candidates.forEach(function(e) {
		if(e.mesh.status[e.id] == 0 && (!best || e.error > best.error)) best = e;
	});
	context.candidates = [];
	if(!best) return;

	while(context.cacheSize > maxCacheSize) {
		var worst = null;
		//find worst in cache
		context.meshes.forEach(function(m) {
			var n = m.nodesCount;
			for(i = 0; i < n; i++)
				if(!worst || (m.status[i] == 1 && m.errors[i] < worst.error))
					worst = {error: m.errors[i], frame: m.frames[i], mesh:m, id:i};
		});

		if(!worst || (worst.error >= best.error && worst.frame == best.frame))
			return;
		removeNode(context, worst);
	}
	requestNode(context, best);
 
	if(context.pending < maxPending)
		updateCache(gl);
}

//nodes are loaded asincronously, just update mesh content (VBO) cache size is kept globally.
//but this could be messy.

function setTargetError(gl, error) {
	var context = getContext(gl);
	context.targetError = error;
}
function setTargetFps(gl, fps) {
	var context = getContext(gl);
	context.targetFps = fps;
}
function setMaxCacheSize(gl, size) {
	var context = getContext(gl);
	context.maxCacheSize = size;
}


return { Mesh: Mesh, Renderer: Instance, Renderable: Instance, Instance:Instance,
	Debug: Debug, contexts: contexts, beginFrame:beginFrame, endFrame:endFrame, 
	setTargetError:setTargetError, setTargetFps:setTargetFps, setMaxCacheSize:setMaxCacheSize };

}();
