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
    t.availableNodes = 0;
    t.headerSize = 4096; // Large enough for v3 JSON headers
    if(url)
        t.open(url);
}

Mesh.prototype = {
    open: function(url) {
        let mesh = this;
        mesh.url = url;
        mesh.httpRequest(url,
            0,
            mesh.headerSize,
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
                
                // Set version flag for v3
                mesh.v3 = (mesh.version == 3);
                if(Debug.verbose) console.log("Nexus version: " + mesh.version);
                
                // Create default material for v2 if not present
                if(!mesh.materials || mesh.materials.length === 0) {
                    mesh.materials = [{
                        mapping: [0] // Default: texture 0 maps to sampler 0
                    }];
                }
                
                mesh.vertex = mesh.signature.vertex;
                mesh.face = mesh.signature.face;
                mesh.renderMode = mesh.face.index?["FILL", "POINT"]:["POINT"];
                mesh.compressed = (mesh.signature.flags & (2 | 4)); //meco or corto
                mesh.meco = (mesh.signature.flags & 2);
                mesh.corto = (mesh.signature.flags & 4);

				mesh.deepzoom = (mesh.signature.flags & 8);
				if(mesh.deepzoom)
					mesh.baseurl = url.substr(0, url.length -4) + '_files/';


                mesh.requestIndex();
            },
            function() { console.log("Open request error!");},
            function() { console.log("Open request abort!");}
        );
    },

    httpRequest: function(url, start, end, load, error, abort, type) {
        if(!type) type = 'arraybuffer';
        var r = new XMLHttpRequest();
        r.open('GET', url, true);
        r.responseType = type;
		if(end)
			r.setRequestHeader("Range", "bytes=" + start + "-" + (end -1));
        r.onload = function(){
            switch (this.status){
                case 0:
//					console.log("0 response: server unreachable.");//returned in chrome for local files
					error();
					break;
                case 206:
//					console.log("206 response: partial content loaded.");
                    load.bind(this)();
                    break;
                case 200:
//					console.log("200 response: server does not support byte range requests.");
					if(end == 0)
						load.bind(this)();
					else
						error();
					break;
            }
        };
        r.onerror = error;
        r.onabort = abort;
        r.send();
        return r;
    },

    requestIndex: function() {
        var mesh = this;
        // Use indexStart and indexEnd calculated in importHeader2/3
        mesh.httpRequest(this.url,
            mesh.indexStart,
            mesh.indexEnd,
            function() { if(Debug.verbose) console.log("Loading index for " + mesh.url); mesh.handleIndex(this.response); },
            function() { console.log("Index request error!");},
            function() { console.log("Index request abort!");}
        );
    },

    handleIndex: function(buffer) {
        let t = this;
        let view = new DataView(buffer);
        view.offset = 0;

        const n = t.n_nodes;

        t.noffsets  = new Uint32Array(n);
        t.nsizes    = new Uint32Array(n); // Size on disk (v3 explicit, v2 calculated)
        t.nvertices = new Uint32Array(n);
        t.nfaces    = new Uint32Array(n);
        t.nerrors   = new Float32Array(n);
        t.nspheres  = new Float32Array(n*5);
        t.nsize     = new Float32Array(n); // Decompressed size in RAM
        t.nfirstpatch = new Uint32Array(n);

        // Parse nodes: v2=44 bytes, v3=48 bytes per node
        for(let i = 0; i < n; i++) {
            t.noffsets[i] = padding*getUint32(view); //offset
            if(t.v3)
                t.nsizes[i] = getUint32(view);       //explicit size in v3
            t.nvertices[i] = getUint16(view);        //verticesCount
            t.nfaces[i] = getUint16(view);           //facesCount
            t.nerrors[i] = getFloat32(view);
            view.offset += 8;                        //skip cone
            for(let k = 0; k < 5; k++)
                t.nspheres[i*5+k] = getFloat32(view);       //sphere + tight
            t.nfirstpatch[i] = getUint32(view);          //first patch
        }
        
        // For v2, calculate sizes from offsets
        if(!t.v3) {
            for(let i = 0; i < n-1; i++)
                t.nsizes[i] = t.noffsets[i+1] - t.noffsets[i];
        }
        
        t.sink = n -1;

        // Parse patches: v2=12 bytes (3 int32s), v3=16 bytes (4 int32s)
        // Convert to uniform 4-field structure for both versions
        if(t.v3) {
            t.patches = new Int32Array(view.buffer, view.offset, t.n_patches*4);
        } else {
            let tmp = new Int32Array(view.buffer, view.offset, t.n_patches*3);
            t.patches = new Int32Array(t.n_patches*4);
            for(let i = 0; i < t.n_patches; i++) {
                t.patches[i*4+0] = tmp[i*3+0]; // node
                t.patches[i*4+1] = tmp[i*3+1]; // lastTriangle
                t.patches[i*4+2] = tmp[i*3+2]; // texture
                t.patches[i*4+3] = 0;           // material (default to 0 for v2)
            }
        }
        
        t.nroots = t.n_nodes;
        for(let j = 0; j < t.nroots; j++) {
            for(let i = t.nfirstpatch[j]; i < t.nfirstpatch[j+1]; i++) {
                if(t.patches[i*4] < t.nroots)
                    t.nroots = t.patches[i*4];
            }
        }

        view.offset += t.v3 ? t.n_patches*16 : t.n_patches*12;

        // Parse texture groups: v2=68 bytes (offset+skip matrix), v3=8 bytes (offset+size)
        t.textures = new Uint32Array(t.n_textures*2); // [offset, size] pairs
        t.texref = new Uint32Array(t.n_textures);
        
        if(t.v3) {
            // v3: Read offset and size directly
            let tmp = new Uint32Array(view.buffer, view.offset, t.n_textures*2);
            for(let i = 0; i < t.n_textures; i++) {
                t.textures[i*2] = padding*tmp[i*2];     // offset
                t.textures[i*2+1] = tmp[i*2+1];         // size
            }
        } else {
            // v2: Read offset only, calculate size from next offset
            for(let i = 0; i < t.n_textures; i++) {
                t.textures[i*2] = padding*getUint32(view); // offset
                view.offset += 16*4; //skip proj matrix
            }
            // Calculate sizes for v2
            for(let i = 0; i < t.n_textures-1; i++)
                t.textures[i*2+1] = t.textures[(i+1)*2] - t.textures[i*2];
        }

        t.vsize = 12 + (t.vertex.NORMAL?6:0) + (t.vertex.COLOR_0?4:0) + (t.vertex.UV_0?8:0);
        t.fsize = 6;

        //problem: I have no idea how much space a texture is needed in GPU. 10x factor assumed.
        let tmptexsize = new Uint32Array(n-1);
        let tmptexcount = new Uint32Array(n-1);
        for(let i = 0; i < n-1; i++) {
            for(let p = t.nfirstpatch[i]; p != t.nfirstpatch[i+1]; p++) {
                let tex = t.patches[p*4+2]; // Updated to 4-field patches
                tmptexsize[i] += t.textures[tex*2+1]; // Use size from textures array
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
        return { POSITION: e[0], NORMAL: e[1], COLOR_0: e[2], UV_0: e[3], data: e[4] };
    },

    //enum INDEX, NORMAL, COLOR, TEXCOORD, DATA0
    importFace: function(view) {
        const e = this.importElement(view);
        let color = e[2];
        if(color) {
            color.type = 2; //unsigned byte
            color.glType = attrGlMap[2];
        }
        return { index: e[0], NORMAL: e[1], COLOR_0: e[2], UV_0: e[3], data: e[4] };
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
    },

    importHeader2: function(view) {
        let h = {};
        h.version = 2;
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
        let h = {};
        let jsonLength = getUint32(view);
        this.headerSize = jsonLength + 12;
        if(this.headerSize > view.buffer.byteLength) {
            console.log("JSON header size exceeds buffer, may need to re-request.");
            return null;
        }
        
        // Parse JSON header (skip magic[4] + version[4] + jsonLength[4] = 12 bytes)
        let str = String.fromCharCode.apply(null, new Uint8Array(view.buffer, 12, jsonLength));
        h = JSON.parse(str);
        h.version = 3;
        
        // Store materials if present
        if(h.materials) {
            this.materials = h.materials;
        }
        
        // Calculate index byte range for v3
        this.indexStart = jsonLength + 12;
        this.indexEnd = this.indexStart + h.n_nodes*48 + h.n_patches*16 + h.n_textures*8;
        
        return h;
    },
    //OVERRIDE THESE METHOS

    //assemble node and geometry
    createNode: function(id) {},

    createNodeGeometry: function(id, data) {},
    deleteNodeGeometry: function(id) {},

    createTexture: function(id, image, texindex) {},
    deleteTexture: function(id) {},
};


export { Mesh }
