import { getUint64, getUint32, getUint16, getFloat32 } from './Binary.js'

/* MESH DEFINITION */
let Debug = { verbose: true };

let glP = WebGLRenderingContext.prototype;
let attrGlMap = [glP.NONE, glP.BYTE, glP.UNSIGNED_BYTE, glP.SHORT, glP.UNSIGNED_SHORT, glP.INT, glP.UNSIGNED_INT, glP.FLOAT, glP.DOUBLE];
let attrSizeMap = [0, 1, 1, 2, 2, 4, 4, 4, 8];

//All addresses in the file are n*256 so,  256 * 2^32 is the max size of a Nxs file 
var padding = 256;



let Mesh = function(url) {
    var t = this;
    t.isReady = false;
    t.onLoad = [];
    t.onUpdate = [];
    t.reqAttempt = 0;
    t.georeq = {}; //keeps track of existing httprequests
    t.texreq = {};
    t.frame = 0; //last time this mesh was traversed in rendering.
    if(url)
        t.open(url);
}

Mesh.prototype = {
    open: function(url) {
        let mesh = this;
        mesh.url = url;
        mesh.httpRequest(
            0,
            88,
            function() {
                if(Debug.verbose) console.log("Loading header for " + mesh.url);
                let view = new DataView(this.response);
                view.offset = 0;
                mesh.reqAttempt++;
                const header = mesh.importHeader(view);
                if(!header) {
                    if(Debug.verbose) console.log("Empty header!");
                    if(mesh.reqAttempt < maxReqAttempt) mesh.open(mesh.url + '?' + Math.random()); // BLINK ENGINE CACHE BUG PATCH
                    return;
                }
                mesh.reqAttempt = 0;
                for(let i in header)
                    mesh[i] = header[i];
                mesh.vertex = mesh.signature.vertex;
                mesh.face = mesh.signature.face;
                mesh.renderMode = mesh.face.index?["FILL", "POINT"]:["POINT"];
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
        var end = 88 + mesh.nodesCount*44 + mesh.patchesCount*12 + mesh.texturesCount*68;
        mesh.httpRequest(
            88,
            end,
            function() { if(Debug.verbose) console.log("Loading index for " + mesh.url); mesh.handleIndex(this.response); },
            function() { console.log("Index request error!");},
            function() { console.log("Index request abort!");}
        );
    },

    handleIndex: function(buffer) {
        let t = this;
        let view = new DataView(buffer);
        view.offset = 0;

        const n = t.nodesCount;

        t.noffsets  = new Uint32Array(n);
        t.nvertices = new Uint32Array(n);
        t.nfaces    = new Uint32Array(n);
        t.nerrors   = new Float32Array(n);
        t.nspheres  = new Float32Array(n*5);
        t.nsize     = new Float32Array(n);
        t.nfirstpatch = new Uint32Array(n);

        for(let i = 0; i < n; i++) {
            t.noffsets[i] = padding*getUint32(view); //offset
            t.nvertices[i] = getUint16(view);        //verticesCount
            t.nfaces[i] = getUint16(view);           //facesCount
            t.nerrors[i] = getFloat32(view);
            view.offset += 8;                        //skip cone
            for(let k = 0; k < 5; k++)
                t.nspheres[i*5+k] = getFloat32(view);       //sphere + tight
            t.nfirstpatch[i] = getUint32(view);          //first patch
        }
        t.sink = n -1;

        t.patches = new Uint32Array(view.buffer, view.offset, t.patchesCount*3); //noded, lastTriangle, texture
        t.nroots = t.nodesCount;
        for(let j = 0; j < t.nroots; j++) {
            for(let i = t.nfirstpatch[j]; i < t.nfirstpatch[j+1]; i++) {
                if(t.patches[i*3] < t.nroots)
                    t.nroots = t.patches[i*3];
            }
        }

        view.offset += t.patchesCount*12;

        t.textures = new Uint32Array(t.texturesCount);
        t.texref = new Uint32Array(t.texturesCount);
        for(let i = 0; i < t.texturesCount; i++) {
            t.textures[i] = padding*getUint32(view);
            view.offset += 16*4; //skip proj matrix
        }

        t.vsize = 12 + (t.vertex.normal?6:0) + (t.vertex.color?4:0) + (t.vertex.texCoord?8:0);
        t.fsize = 6;

        //problem: I have no idea how much space a texture is needed in GPU. 10x factor assumed.
        let tmptexsize = new Uint32Array(n-1);
        let tmptexcount = new Uint32Array(n-1);
        for(let i = 0; i < n-1; i++) {
            for(let p = t.nfirstpatch[i]; p != t.nfirstpatch[i+1]; p++) {
                let tex = t.patches[p*3+2];
                tmptexsize[i] += t.textures[tex+1] - t.textures[tex];
                tmptexcount[i]++;
            }
            t.nsize[i] = t.vsize*t.nvertices[i] + t.fsize*t.nfaces[i];
        }
        for(let i = 0; i < n-1; i++) {
            t.nsize[i] += 10*tmptexsize[i]/tmptexcount[i];
        }

        t.status = new Uint8Array(n); //0 for none, 1 for ready, 2+ for waiting data
        t.frames = new Uint32Array(n);
        t.errors = new Float32Array(n); //biggest error of instances
        t.reqAttempt = new Uint8Array(n);
        
        t.isReady = true;
        for(let callback of t.onLoad)
            callback(this);
    },

    importAttribute: function(view) {
        let a = {};
        a.type = view.getUint8(view.offset++, true);
        a.size = view.getUint8(view.offset++, true);
        a.glType = attrGlMap[a.type];
        a.normalized = a.type < 7;
        a.stride = attrSizeMap[a.type]*a.size;
        if(a.size == 0) return null;
        return a;
    },

    importElement: function(view) {
        let e = [];
        for(let i = 0; i < 8; i++)
            e[i] = this.importAttribute(view);
        return e;
    },

    importVertex: function(view) {	//enum POSITION, NORMAL, COLOR, TEXCOORD, DATA0
        const e = this.importElement(view);
        let color = e[2];
        if(color) {
            color.type = 2; //unsigned byte
            color.glType = attrGlMap[2];
        }
        return { position: e[0], normal: e[1], color: e[2], texCoord: e[3], data: e[4] };
    },

    //enum INDEX, NORMAL, COLOR, TEXCOORD, DATA0
    importFace: function(view) {
        const e = this.importElement(view);
        let color = e[2];
        if(color) {
            color.type = 2; //unsigned byte
            color.glType = attrGlMap[2];
        }
        return { index: e[0], normal: e[1], color: e[2], texCoord: e[3], data: e[4] };
    },

    importSignature: function(view) {
        let s = {};
        s.vertex = this.importVertex(view);
        s.face = this.importFace(view);
        s.flags = getUint32(view);
        return s;
    },

    importHeader: function(view) {
        const magic = getUint32(view);
        if(magic != 0x4E787320) return null;
        let h = {};
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
    //OVERRIDE THESE METHOS

    //assemble node and geometry
    createNode: function(id) {},

    createNodeGeometry: function(id, data) {},
    deleteNodeGeometry: function(id) {},

    createTexture: function(id, image) {},
    deleteTexture: function(id) {},
};


export { Mesh }
/*
if (typeof exports === 'object' && typeof module === 'object') {
    module.exports = { NEXUSMesh: NEXUSMesh }
    console.log('a');
} else if (typeof define === 'function' && define['amd']) {
    console.log('b');
	define([], function() {
		return { NEXUSMesh: NEXUSMesh }
	});
} else if (typeof exports === 'object') {
    console.log('c');
    exports["NEXUSMesh"] = NEXUSMesh;
}*/
