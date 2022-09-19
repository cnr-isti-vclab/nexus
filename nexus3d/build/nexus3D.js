(function (global, factory) {
    typeof exports === 'object' && typeof module !== 'undefined' ? factory(exports, require('three')) :
    typeof define === 'function' && define.amd ? define(['exports', 'three'], factory) :
    (global = typeof globalThis !== 'undefined' ? globalThis : global || self, factory(global.Nexus3D = {}, global.THREE));
}(this, (function (exports, THREE) { 'use strict';

    /* UTILITIES */
        
    function getUint64(view) {
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

    function PriorityQueue(max_length) {
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

    function Traversal() {
        let t = this;
        t.maxBlocked    = 30;

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
    }

    Traversal.prototype = {
        updateView: function(viewport, projection, modelView) {
            let t = this;

            for(let i = 0; i < 16; i++) {
                t.projectionMatrix[i] = projection[i];
                t.modelView[i] = modelView[i];
            }
            for(let i = 0; i < 4; i++)
                t.viewport[i] = viewport[i];

            matMul(t.projectionMatrix, t.modelView, t.modelViewProj);
            matInv(t.modelViewProj, t.modelViewProjInv);

            matInv(t.modelView, t.modelViewInv);
            t.viewpoint[0] = t.modelViewInv[12];
            t.viewpoint[1] = t.modelViewInv[13];
            t.viewpoint[2] = t.modelViewInv[14];
            t.viewpoint[3] = 1.0;


            const m = t.modelViewProj;
            const mi = t.modelViewProjInv;
            let p = t.planes;

            //frustum planes Ax + By + Cz + D = 0;
            p[0]  =  m[0] + m[3]; p[1]  =  m[4] + m[7]; p[2]  =  m[8] + m[11];  p[3]  =  m[12] + m[15]; //left
            p[4]  = -m[0] + m[3]; p[5]  = -m[4] + m[7]; p[6]  = -m[8] + m[11];  p[7]  = -m[12] + m[15]; //right
            p[8]  =  m[1] + m[3]; p[9]  =  m[5] + m[7]; p[10] =  m[9] + m[11];  p[11] =  m[13] + m[15]; //bottom
            p[12] = -m[1] + m[3]; p[13] = -m[5] + m[7]; p[14] = -m[9] + m[11];  p[15] = -m[13] + m[15]; //top
            p[16] = -m[2] + m[3]; p[17] = -m[6] + m[7]; p[18] = -m[10] + m[11]; p[19] = -m[14] + m[15]; //near
            p[20] = -m[2] + m[3]; p[21] = -m[6] + m[7]; p[22] = -m[10] + m[11]; p[23] = -m[14] + m[15]; //far

            //normalize planes to get also correct distances
            for(let i = 0; i < 24; i+= 4) {
                let l = Math.sqrt(p[i]*p[i] + p[i+1]*p[i+1] + p[i+2]*p[i+2]);
                p[i] /= l; p[i+1] /= l; p[i+2] /= l; p[i+3] /= l;
            }
            
            //side is M'(1,0,0,1) - M'(-1,0,0,1) and they lie on the planes
            const r3 = mi[3] + mi[15];
            const r0 = (mi[0]  + mi[12 ])/r3;
            const r1 = (mi[1]  + mi[13 ])/r3;
            const r2 = (mi[2]  + mi[14 ])/r3;

            const l3 = -mi[3] + mi[15];
            const l0 = (-mi[0]  + mi[12 ])/l3 - r0;
            const l1 = (-mi[1]  + mi[13 ])/l3 - r1;
            const l2 = (-mi[2]  + mi[14 ])/l3 - r2;

            const side = Math.sqrt(l0*l0 + l1*l1 + l2*l2);

            //center of the scene is M'*(0, 0, 0, 1)
            const c0 = mi[12]/mi[15] - t.viewpoint[0];
            const c1 = mi[13]/mi[15] - t.viewpoint[1];
            const c2 = mi[14]/mi[15] - t.viewpoint[2];
            const dist = Math.sqrt(c0*c0 + c1*c1 + c2*c2);

            const resolution = (2*side/dist)/ t.viewport[2];
            t.currentResolution == resolution ? t.sameResolution = true : t.sameResolution = false;
            t.currentResolution = resolution;
        },

        traverse : function (mesh, cache) {
            let t = this;
            t.mesh = mesh;
    //        if(Debug.extract == true)
     //           return;

            if(!mesh.isReady) return;
        
            const n = mesh.nodesCount;
            t.visited  = new Uint8Array(n);
            t.blocked  = new Uint8Array(n);
            t.selected = new Uint8Array(n);

            t.frame = cache.frame;

            t.instance_errors = new Float32Array(n);

            if(t.frame > mesh.frame) { //clean the errors.
                mesh.errors = new Float32Array(n); 
                mesh.frame = t.frame;
            }

            t.visitQueue = new PriorityQueue(n);
            for(var i = 0; i < mesh.nroots; i++)
                t.insertNode(i);
            
            t.currentError = cache.currentError;
            t.drawSize = 0;
            t.nblocked = 0;

            var requested = 0;
            while(t.visitQueue.size && t.nblocked < t.maxBlocked) {
                var error = t.visitQueue.error[0];
                var id = t.visitQueue.pop();

                //if not loaded and the queue is not full add to the candidates.
                if (mesh.status[id] == 0 && requested < cache.maxPending) {
                    cache.candidates.push({id: id, mesh:mesh, frame:t.frame, error:error});
                    requested++;
                }
                /* we don't want to stop as soon as a node is not availabe, because the nodes are sorted by error, 
                   this could cause a large drop in quality elsewere.
                   we still need to mark all children as blocked, to prevent including them in the cut of the dag. */
                var blocked = t.blocked[id] || !t.expandNode(id, error);
                if (blocked)
                    t.nblocked++;
                else {
                    t.selected[id] = 1;
                    //cache.realError = Math.min(error, cache.realError);
                }
                t.insertChildren(id, blocked);
            }

            //update remaining errors in the cache
            if(cache.nodes.has(mesh)) {
                for(let id of cache.nodes.get(mesh)) {
                    let error = t.nodeError(id);
                    if(t.instance_errors[id] == 0) {
                        t.instance_errors[i] = error;
                        mesh.errors[id] = Math.max(mesh.errors[id], error);
                    }
                }
            }

            t.mesh = null;
            return t.instance_errors;
        },

        insertNode: function (node) {
            let t = this;
            t.visited[node] = 1;

            const error = t.nodeError(node);

            t.instance_errors[node] = error;
            t.mesh.errors[node] = Math.max(error, t.mesh.errors[node]);
            t.mesh.frames[node] = t.frame;

    //        if(node > 0 && error < t.currentError) return;  //2% speed TODO check if needed

            t.visitQueue.push(node, error);
        },

        insertChildren : function (node, block) {
            let t = this;
            for(let i = t.mesh.nfirstpatch[node]; i < t.mesh.nfirstpatch[node+1]; ++i) {
                const child = t.mesh.patches[i*3];
                if (child == t.mesh.sink) return;
                if (block) t.blocked[child] = 1;
                if (!t.visited[child])
                    t.insertNode(child);
            }
        },

        expandNode : function (node, error) {
            let t = this;
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

            const sp = t.mesh.nspheres;
            const off = node*5;
            if(t.isVisible(sp[off], sp[off+1], sp[off+2], sp[off+3])) //expanded radius
                t.drawSize += t.mesh.nvertices[node]*0.8;
                //we are adding half of the new faces. (but we are using the vertices so *2)

            return true;
        },

        nodeError : function (n, tight) {
            let t = this;
            const spheres = t.mesh.nspheres;
            const b = t.viewpoint;
            const off = n*5;
            const cx = spheres[off+0];
            const cy = spheres[off+1];
            const cz = spheres[off+2];
            const r  = spheres[off+3];
            if(tight)
                r = spheres[off+4];
            const d0 = b[0] - cx;
            const d1 = b[1] - cy;
            const d2 = b[2] - cz;
            let dist = Math.sqrt(d0*d0 + d1*d1 + d2*d2) - r;
            if (dist < 0.1)
                dist = 0.1;

            //resolution is how long is a pixel at distance 1.
            let error = t.mesh.nerrors[n]/(t.currentResolution*dist); //in pixels

            //causes flickering due to things popping in and out of visibility, causes a huge resorting.
            /*if (!t.isVisible(cx, cy, cz, spheres[off+4]))
                error /= 100.0; */

            //more stable, at least it's continuous.
            let d = t.distance(cx, cy, cz, spheres[off+4]);
            if(d < -r) {
                error /= 101.0;
            } else if(d < 0) {
                error /= 1 -( d/r)*100.0;
            } 
            return error;
        },

        distance: function(x, y, z, r) {
            const p = this.planes;
            let min_distance = 1e20;
            for (let i = 0; i < 24; i +=4) {
                let d = p[i]*x + p[i+1]*y + p[i+2]*z + p[i+3] + r;
                if(d < min_distance)
                    min_distance = d;
            }
            return min_distance;
        },

        isVisible : function (x, y, z, r) {
            const p = this.planes;
            for (let i = 0; i < 24; i +=4) {
                if(p[i]*x + p[i+1]*y + p[i+2]*z + p[i+3] + r < 0) //plane is ax+by+cz+d = 0; 
                    return false;
            }
            return true;
        }
    };

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
        if(url)
            t.open(url);
    };

    Mesh.prototype = {
        open: function(url) {
            let mesh = this;
            mesh.url = url;
            mesh.httpRequest(url,
                0,
                88,
                function() {
                    console.log("Loading header for " + mesh.url);
                    let view = new DataView(this.response);
                    view.offset = 0;
                    mesh.reqAttempt++;
                    const header = mesh.importHeader(view);
                    if(!header) {
                        console.log("Empty header!");
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
            var end = 88 + mesh.nodesCount*44 + mesh.patchesCount*12 + mesh.texturesCount*68;
            mesh.httpRequest(this.url,
                88,
                end,
                function() { console.log("Loading index for " + mesh.url); mesh.handleIndex(this.response); },
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

    var targetError   = 2.0;    //error won't go lower than this if we reach it
    var maxError      = 15;     //error won't go over this even if fps is low
    var minFps        = 15;
    var maxPending    = 3;
    var maxReqAttempt$1 = 2;
    var maxCacheSize  = 1024 *(1<<20); 

    function _Cache() {
        let t = this;
        t.cortopath = '.';
    	t.frame = 0;         //keep track of the time

        t.maxCacheSize = maxCacheSize;
        t.minFps = minFps;
        t.currentFps = 0;
        t.targetError = targetError;
        t.currentError = targetError;
        t.maxError = maxError;
        t.realError = 0;

        t.pending = 0;
        t.maxPending = 3;
        t.cacheSize = 0;
        t.candidates = [];   //list of nodes to be loaded
        t.nodes = new Map();        //for each mesh a list of node ids.
        
        t.last_frametime = 0;
        t.frametime = 0;
        t.end_frametime = 0;

        t.debug = { 
            verbose : false,  //debug messages
            nodes   : false,  //color each node
            draw    : false,  //final rendering call disabled
            extract : false,  //extraction disabled}
        };

        t.totswapped = 0; //in the last frame.
        t.swaprate = 0;
        t.lastupdate = performance.now();
    }

    _Cache.prototype = {
        getTargetError:  function()      { return this.targetError; },
        getMinFps:       function()      { return this.minFps; },
        setMinFps:       function(fps)   { this.minFps = fps; },
        getMaxCacheSize: function()      { return this.maxCacheSize; },
        setMaxCacheSize: function(size)  { this.maxCacheSize = size; },
        setTargetError:  function(error) { this.targetError = error; },
        
        loadCorto: function() {
            let corto = new Worker(this.cortopath + '/corto.em.js');
            corto.requests = {};
            corto.count = 0;
            corto.postRequest = function(node) {
                corto.postMessage({ buffer: node.buffer, request:this.count, rgba_colors: true, short_index: true, short_normals: true});
                node.buffer = null;
                this.requests[this.count++] = node;
            };
            corto.onmessage = function(e) {
                var request = e.data.request;
                var node = this.requests[request];
                delete this.requests[request];
                node.model = e.data.model;
                node.cache.readyGeometryNode(node.mesh, node.id, node.model);
            };
            this.corto = corto;
        },

        
        beginFrame: function(fps) { //each context has a separate frame count.
            let c = this;
            c.frametime = performance.now();
            let elapsed =  c.frametime - c.last_frametime;
            c.last_frametime = c.frametime;
            if(elapsed < 500)
                c.currentFps = 0.9*c.currentFps + 0.1*(1000/elapsed);

            fps = c.currentFps;

    	    c.frame++;
    	    c.candidates = [];
    	    if(fps && c.minFps) {
    		    c.currentFps = fps;
    		    const r = c.minFps/fps;
    		    if(r > 1.1)
    			    c.currentError *= 1.05;
    		    if(r < 0.9)
    			    c.currentError *= 0.95;

    		    c.currentError = Math.max(c.targetError, Math.min(c.maxError, c.currentError));

    	    } else
    		    c.currentError = c.targetError;

    	    c.rendered = 0;
    	    c.realError = 1e20;
            c.totswapped = 0;
        },

        endFrame: function() {
    	    this.update();
        },



        requestNode: function(mesh, id) {
    	    mesh.status[id] = 2; 

    	    
    	    this.cacheSize += mesh.nsize[id];
    	    mesh.reqAttempt[id] = 0;

    		if(!this.nodes.has(mesh))
    			this.nodes.set(mesh, new Set());
    		this.nodes.get(mesh).add(id);

    	    this.requestNodeGeometry(mesh, id);
    	    this.requestNodeTexture(mesh, id);
        },

        requestNodeGeometry: function(mesh, id) {
    		this.pending++;
    	    mesh.status[id]++;
    		let url = mesh.deepzoom ? mesh.baseurl + id + ".nxn" : mesh.url;
    		let start = mesh.deepzoom ? 0 : mesh.noffsets[id];
    		let end = mesh.deepzoom ? 0 : mesh.noffsets[id+1];
    	    let request = mesh.georeq[id] = mesh.httpRequest(url,
    		    start,
    		    end, 
    		    ()=>{
                    delete mesh.georeq[id];
                    this.loadNodeGeometry(request, mesh, id);
    				this.pending--;
                },
    		    ()=>{
                    delete mesh.georeq[id];
    			    if(this.debug.verbose) console.log("Geometry request error!");
    			    this.recoverNode(mesh, id, 0);
    				this.pending--;
    		    },
    		    ()=>{
                    delete mesh.georeq[id];
    			    if(this.debug.verbose) console.log("Geometry request abort!");
    			    this.removeNode(mesh, id);
    				this.pending--;
    		    },
    		    'arraybuffer'
            );
        },

        requestNodeTexture: function(mesh, id) {

    	    if(!mesh.vertex.texCoord) return;

    	    let tex = mesh.patches[mesh.nfirstpatch[id]*3+2];
    	    mesh.texref[tex]++;
    	    mesh.status[id]++;

    		let url = mesh.deepzoom ? mesh.baseurl + tex + ".jpg" : mesh.url;
    		let start = mesh.deepzoom ? 0 : mesh.textures[tex];
    		let end = mesh.deepzoom ? 0 : mesh.textures[tex+1];
    	    let request = mesh.texreq[tex] = mesh.httpRequest(url,
    		    start,
    		    end,
    		    ()=>{ 
                    delete mesh.texreq[tex];
                    this.loadNodeTexture(request, mesh, id, tex); 

                },
    		    ()=>{
                    delete mesh.texreq[tex];
    		    	if(this.debug.verbose) console.log("Texture request error!");
    			    this.recoverNode(mesh, id, 1);
    		    },
    		    ()=>{
                    delete mesh.texreq[tex];
    		    	if(this.debug.verbose) console.log("Texture request abort!");
    		    	this.removeNode(mesh, id);
    		    },
    		    'blob'
    	    );
        },

        recoverNode: function(mesh, id, mode) {
    	    if(mesh.status[id] == 0) return;

    	    mesh.status[id]--;

            let t = this;

    	    if(mesh.reqAttempt[id] > maxReqAttempt$1) {
    		    if(this.debug.verbose) console.log("Max request limit for " + m.url + " node: " + n);
    		    t.removeNode(mesh, id);
    		    return;
    	    }

    	    mesh.reqAttempt[id]++;

    	    switch (mode){
    		case 0:
    		    t.requestNodeGeometry(mesh, id);
    		    if(this.debug.verbose) console.log("Recovering geometry for " + m.url + " node: " + n);
    		    break;
    		case 1:
    			t.requestNodeTexture(mesh, id);
    			if(this.debug.verbose) console.log("Recovering texture for " + m.url + " node: " + n);
    			break;
    	    }
        },

        loadNodeGeometry: function(request, mesh, id) {
    	    if(mesh.status[id] == 0) return;
            
    	    if(!mesh.compressed)
    		    this.readyGeometryNode(mesh, id, request.response);
    	    else {
                if(!this.corto) this.loadCorto();
    		    this.corto.postRequest( { mesh:mesh, id:id, buffer: request.response, cache: this });
            }
        },


        loadNodeTexture: function(request, mesh, id, texid) {
            if(mesh.status[id] == 0) {
                throw "Should not load texture twice";
            }

    	    let blob = request.response;
            let callback = (img) => {
                if(mesh.status[id] == 0) //call was aborted.
                    return;
                mesh.createTexture(texid, img);

    		    mesh.status[id]--;

                if(mesh.status[id] == 2)
                    this.readyNode(mesh, id);
    		    };

            if(typeof createImageBitmap != 'undefined') {
    			try {
    				createImageBitmap(blob, { imageOrientation: 'flipY' }).then(callback);
    			} catch(error) { //old version of firefox do not support options (but flip anyway)
    				createImageBitmap(blob).then(callback);
    			}

            } else { //fallback for IOS
                var urlCreator = window.URL || window.webkitURL;
                var img = document.createElement('img');
                img.onerror = function(e) { console.log("Texture loading error!"); };
                img.src = urlCreator.createObjectURL(blob);
        
                img.onload = function() {
                    urlCreator.revokeObjectURL(img.src);
                    callback(img);
                };
            }
        },

        removeNode: function(mesh, id) {
            this.nodes.get(mesh).delete(id);

            if(mesh.status[id] == 0) {
    			if(this.debug.verbose)
    				console.log("Double remove due to abort.");
                return; //TODO
    		}

    	    mesh.status[id] = 0;
    	    if (id in mesh.georeq && mesh.georeq[id].readyState != 4) {
                mesh.georeq[id].abort();
                delete mesh.georeq[id];
    		    //this.pending--;
    	    }
            mesh.availableNodes--;
            this.cacheSize -= mesh.nsize[id];
            mesh.deleteNodeGeometry(id);

    	    if(!mesh.vertex.texCoord) return;

    	    const tex = mesh.patches[mesh.nfirstpatch[id]*3+2]; //TODO assuming one texture per node

    	    if (tex in mesh.texreq && mesh.texreq[tex].readyState != 4) {
                mesh.texreq[tex].abort();
                delete mesh.texreq[tex];
            }

    	    mesh.texref[tex]--;
        	if(mesh.texref[tex] == 0) {
                mesh.deleteTexture(tex);
            }
        },

        readyGeometryNode: function(mesh, id, buffer) {
            if(mesh.status[id] == 0) //call was aborted
                return;
            const nv = mesh.nvertices[id];
            const nf = mesh.nfaces[id];
    	    let geometry = {};

    	    
    	    if(!mesh.corto) {
    		    geometry.index  = new Uint16Array(buffer, nv*mesh.vsize,  nf*3);
                geometry.position =  new Float32Array(buffer, 0, nv*3);

    		    var off = nv*12;
    		    if(mesh.vertex.texCoord) {
                    geometry.uv = new Float32Array(buffer, off, nv*2);
                    off += nv*8;
                }
                if(mesh.vertex.normal) {
    				geometry.normal = new Int16Array(buffer, off, nv*3);
                    off += nv*6;
    			}

                if(mesh.vertex.color) {
                    geometry.color = new Uint8Array(buffer, off, nv*4);
                    off += nv*4;
    			}

    		
    	    } else {
                geometry = buffer;
    	    }

    	    //if(nf == 0)
        	//	scramble(nv, v, no, co);

            mesh.createNodeGeometry(id, geometry);
    	    mesh.status[id]--;

    	    if(mesh.status[id] == 2) {
                this.readyNode(mesh, id);
    	    }
        },

        //the node is finished, add to cache, and update counters
        readyNode: function(mesh, id) {

    	    mesh.status[id]--;
            if(mesh.status[id] != 1) throw "A ready node should have status ==1"

    		
    		mesh.reqAttempt[id] = 0;
            //this.pending--;
            mesh.createNode(id);
    		mesh.availableNodes++;

            for(let callback of mesh.onUpdate)
                callback();
            this.update();
        },

        flush: function(mesh) {
            if(!this.nodes.has(mesh)) return;

            for(let id of this.nodes.get(mesh))
                this.removeNode(mesh, id);
            this.nodes.delete(mesh);
        }, 

        update: function() {
            if(this.pending >= maxPending)
                return;

            //the best candidate has the highest error
            let best = null;
            for(let c of this.candidates) {
                if(c.mesh.status[c.id] == 0 && (!best || c.error > best.error)) 
                    best = c;
    	    }
            if(!best) return;

    	// record amount of data transfer per second.
        /*
            let now = performance.now();
            if(Math.floor(now/1000) > Math.floor(this.lastupdate/1000)) { //new second
                this.swaprate = (this.totswapped/1000)/(now - this.lastupdate); //transfer in mb/s
                if(this.debug.verbose)
                console.log("Memory loaded in GPU: ", this.swaprate);
                this.totswapped = 0;
                this.lastupdate =  now;
            }
        */
            
                
            //make room for new nodes!
            
    	    while(this.cacheSize > this.maxCacheSize) {
                let worst = null;
                
                //find node with smallest error in cache and remove it if worse than the best candidate.
                for(let [mesh, ids] of this.nodes) {
                    for(let id of ids) {
                    //we need to recompute the errors for the cache, as if not traversed doesn't get updated.
                        let error = mesh.errors[id];
                        let frame = mesh.frames[id];
                        if( !worst || error < worst.error) {
                            worst = { id: id, frame: frame, error: error, mesh: mesh };
                        }
                    }
                }
                if(!worst || worst.error >= best.error*0.9) {
                    //(worst.frame + 30 >= best.frame && )) //dont' remove if  the best candidate is not good enogh
                    return;
                }
    		    this.removeNode(worst.mesh, worst.id);
    	    }
            this.totswapped += best.mesh.nsize[best.id];
            this.candidates = this.candidates.filter(e => e.mesh == best.mesh && e.id == best.id);
        	this.requestNode(best.mesh, best.id);
    	    this.update();  //try again.
        }
    };

    let Cache = new _Cache;

    class Nexus3D extends THREE.Mesh {

    	constructor(url, renderer, options) {

    		super();
    		if(typeof renderer == 'function') 
    			throw "Nexus3D constructor has changed: Nexus3D(url, renderer, options) where options include: onLoad, onUpdate, onProgress and material"

    		this.patchWebGLRenderer(renderer);

    		Object.assign(this, {
    			isNXS: true,
    			type:'NXS',
    			url: url,
    			gl: renderer.getContext(),
    			material: null,

    			autoUpdate: true,
    			mesh: new Mesh(),
    			vbo: [],
    			ibo: [],
    			vao: [],
    			textures: [],
    			attributes: {},  //here we store the uniform attributes of the shader.

    			basemesh: null,  //highest level of the nexus, for picking
    		});

    		if('material' in options)
    			this.material = options.material;
    		if(!this.material) 
    			this.material = new THREE.MeshStandardMaterial();
    			
    		for(let call of ['onLoad', 'onUpdate', 'onProgress']) {
    			this['_' + call] = [];
    			if(call in options)
    				this['_' + call].push(options[call]);
    		}

    		if(this.url) {
    			if(typeof url == 'object') {
    				this.nxs = this.url;
    				this.nxs.onLoad.push((m) => { 
    					this.mesh = this.nxs.mesh;
    					this.traversal = this.nxs.traversal;
    					this.cache = this.nxs.cache;
    					this.vbo = this.nxs.vbo;
    					this.ibo = this.nxs.ibo;
    					this.vao = this.nxs.vao;
    					this.textures = this.nxs.textures;
    					this.onLoadCallback(this); 
    				});
    			} else
    				this.open(this.url);
    		}
    	}

    	copy(source) {
    		Object3D.prototype.copy.call( this, source, false );
    		throw "Can't really copy."
    	}

    	
    	open(url) {
    		let t = this;
    		this.mesh.open(url);
    		this.mesh.createNode         = (id)           => { };
    		this.mesh.createNodeGeometry = (id, geometry) => { t.createNodeGeometry(id, geometry); };
    		this.mesh.createTexture      = (id, image)    => { t.createTexture(id, image); };
    		this.mesh.deleteNodeGeometry = (id)           => { t.deleteNodeGeometry(id); };
    		this.mesh.deleteTexture      = (id)           => { t.deleteTexture(id); };
    		this.mesh.onLoad.push(() => { t.onLoadCallback(); });
    		this.mesh.onUpdate.push(() => { 
    			for(let callback of t._onUpdate) callback(this); 
    			for(let callback of t._onProgress) callback(this, this.mesh.availableNodes, this.mesh.nodesCount); 
    		});

    		this.traversal = new Traversal();
    		this.cache = Cache; //new Cache();
    		this.textures = {};        
    	}

    	set onLoad(callback) {
    		this._onLoad.push(callback);
    	}

    	set onUpdate(callback) {
    		this._onUpdate.push(callback);
    	}

    	set onProgress(callback) {
    		this._onProgress.push(callback);
    	}

    	//TODO this is not really needed, we might just conform to THREEJS standard of updating.
    	/*set material(material) {
    		this.material = material;
    		this.material.needsUpdate = true;
    	}*/
    	
    	updateMaterials() {
    		if(this.material.map !== false && this.mesh.vertex.texCoord)
    			this.material.map = this.material_texture;

    		if(this.mesh.vertex.color)
    			this.material.vertexColors = THREE.VertexColors; 
    		this.material.needsUpdate = true; 
    	}

    	onLoadCallback() {
    		const c = this.mesh.sphere.center;
    		const center = new THREE.Vector3(c[0], c[1], c[2]);
    		const radius = this.mesh.sphere.radius;
    		this.boundingSphere = new THREE.Sphere(center, radius);

    		var geometry = new THREE.BufferGeometry();

    		geometry.setAttribute( 'position', new THREE.BufferAttribute(new Float32Array(3), 3));
    		
    		if(this.mesh.vertex.normal)
    			geometry.setAttribute( 'normal', new THREE.BufferAttribute(new Float32Array(3), 3));
    		if(this.mesh.vertex.color)
    			geometry.setAttribute( 'color', new THREE.BufferAttribute(new Float32Array(4), 4));
    		if(this.mesh.vertex.texCoord)
    			geometry.setAttribute( 'uv', new THREE.BufferAttribute(new Float32Array(2), 2));

    		if(this.mesh.vertex.texCoord) {
    			this.material_texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
    			this.material_texture.needsUpdate = true;
    		}

    		this.updateMaterials();
    		this.geometry = geometry;
    		
    		this.frustumCulled = false;
    			
    		for(let callback of this._onLoad)
    			callback(this);
    	}

    	renderBufferDirect(renderer, scene, camera, geometry, material, group) {
    		let s = new THREE.Vector2();
    		renderer.getSize(s);

    	   	//object modelview is multiplied by camera during rendering, we need to do it here for visibility computations
    		this.modelViewMatrix.multiplyMatrices( camera.matrixWorldInverse, this.matrixWorld );
    		this.traversal.updateView([0, 0, s.width, s.height], camera.projectionMatrix.elements, this.modelViewMatrix.elements);
    		this.instance_errors = this.traversal.traverse(this.mesh, this.cache);
    		
    		//threejs increments version when setting neeedsUpdate
    		/*if(this.material.version > 0) {
    			this.updateMaterials();
    			this.material.version = 0;
    			for(let callback of this.onUpdate) 
    				callback(this); 
    		}*/
    		let gl = this.gl;
    		let program = gl.getParameter(gl.CURRENT_PROGRAM);

    		//TODO these calls could be cached saving attrs per each material.
    		let attr = this.attributes;
    		attr.position = gl.getAttribLocation(program, "position");
    		attr.normal   = gl.getAttribLocation(program, "normal");
    		attr.color    = gl.getAttribLocation(program, "color");
    		attr.uv       = gl.getAttribLocation(program, "uv");
    		attr.size     = gl.getUniformLocation(program, "size");
    		attr.scale    = gl.getUniformLocation(program, "scale");

    		let map_location = gl.getUniformLocation(program, "map"); 
    		attr.map      = map_location ? gl.getUniform(program, map_location) : null;
    		
    	
    		//hack to detect if threejs using point or triangle shaders
    		//instance.mode = attr.size ? "POINT" : "FILL";
    		//if(attr.size != -1) 
    		//    instance.pointsize = material.size;
    	
    		//can't find docs or code on how material.scale is computed in threejs.
    		//if(attr.scale != -1)
    		//    instance.pointscale = 2.0;

    		this.setVisibility();
    	}

    	setVisibility() {
    		//set visibile what is visible!
    		let t = this.traversal;
    		let m = this.mesh;
    	
    		if(!m.isReady)
    			return;
    		let rendered = 0;
    		
    		let attr = this.attributes;
    		let mesh = this.mesh;
    		let gl = this.gl;
    		let gl2 = gl instanceof WebGL2RenderingContext;
    		let state = attr.uv + 10*attr.color + 100*attr.normal;
    			
    		for(let id = 0; id < m.nodesCount; id++) {
    //            let err = m.nerrors[id];

    			if(!t.selected[id]) continue;

    			//check for children: if all are selected, bail out.
    			{
    				let visible = false;
    				let last = m.nfirstpatch[id+1]-1;
    				for (var p = m.nfirstpatch[id]; p < m.nfirstpatch[id+1]; ++p) {
    					var child = m.patches[p*3];
    	
    					if(!t.selected[child]) {
    						visible = true;
    						break;
    					}
    				}
    				if(!visible) continue;
    			}

    			var sp = m.nspheres;
    			var off = id*5;
    			if(!t.isVisible(sp[off], sp[off+1], sp[off+2], sp[off+4])) //tight radius
    				continue;
    	
    			let doBind = true;

    			if(gl2) {
    				if(state in this.vao[id])
    					doBind = false;
    				else
    					this.vao[id][state] = gl.createVertexArray();
    					
    			   	gl.bindVertexArray(this.vao[id][state]);
    			}
    			
    			if(doBind) { 
    				gl.bindBuffer(gl.ARRAY_BUFFER, this.vbo[id]);
    			   	gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.ibo[id]);
    	
    				gl.vertexAttribPointer(attr.position, 3, gl.FLOAT, false, 12, 0);
    				gl.enableVertexAttribArray(attr.position);
    	
    				let nv = this.mesh.nvertices[id];
    				let offset = nv*12;
    	
    				if(mesh.vertex.texCoord) {
    					if(attr.uv >= 0) {
    						gl.vertexAttribPointer(attr.uv, 2, gl.FLOAT, false, 8, offset);
    						gl.enableVertexAttribArray(attr.uv);
    					}
    					offset += nv*8;
    				}
    				if(mesh.vertex.color) {
    					if(attr.color >= 0) {
    						gl.vertexAttribPointer(attr.color, 4, gl.UNSIGNED_BYTE, true, 4, offset);
    						gl.enableVertexAttribArray(attr.color);
    					}
    					offset += nv*4;
    				}
    				if(mesh.vertex.normal) {
    					if(attr.normal >= 0) {
    						gl.vertexAttribPointer(attr.normal, 3, gl.SHORT, true, 6, offset);
    						gl.enableVertexAttribArray(attr.normal);
    					}
    				}

    				if(this.cache.debug.nodes) {
    					gl.disableVertexAttribArray(attr.color);

    					var error = this.instance_errors[id]; //this.mesh.errors[id];
    					var palette = [
    						[1, 1, 1, 1], //white
    						[1, 1, 1, 1], //white
    						[0, 1, 0, 1], //green
    						[0, 1, 1, 1], //cyan
    						[1, 1, 0, 1], //yellow
    						[1, 0, 1, 1], //magenta
    						[1, 0, 0, 1]  //red
    					];
    					let w = Math.min(5.99, Math.max(0, Math.log2(error)/2));
    					let low = Math.floor(w);
    					w -= low;
    					let color = [];
    					for( let k = 0; k < 4; k++)
    						color[k] = palette[low][k]*(1-w) + palette[low+1][k]*w;
    					
    					gl.vertexAttrib4fv(attr.color, color);
    				}
    			}
    			this.cache.realError = Math.min(this.mesh.errors[id], this.cache.realError);
    			
    			let offset = 0;
    			let end = 0;
    			let last = m.nfirstpatch[id+1]-1;
    			for (let p = m.nfirstpatch[id]; p < m.nfirstpatch[id+1]; ++p) {
    				let child = m.patches[p*3];
    	
    				if(!t.selected[child]) {
    					end = m.patches[p*3+1];
    					if(p < last) //we join patches if possible.
    						continue;
    				}

    				if(end > offset) {
    					if(m.vertex.texCoord && attr.uv >= 0) {
    						var tex = m.patches[p*3+2];
    						if(tex != -1) { //bind texture
    							var texid = this.textures[tex];
    							gl.activeTexture(gl.TEXTURE0 + attr.map);
    							gl.bindTexture(gl.TEXTURE_2D, texid);
    						}
    					}
    					let mode = this.material.wireframe ? gl.LINE_STRIP : gl.TRIANGLES;

    					gl.drawElements(mode, (end - offset) * 3, gl.UNSIGNED_SHORT, offset * 6);
    					rendered += end - offset;
    				}
    				offset = m.patches[p*3+1];
    			}
    		}
    		this.cache.rendered += rendered;
    	}

    	createNodeGeometry(id, data) {
    		let m = this.mesh;
    		var nv = m.nvertices[id];
    		var nf = m.nfaces[id];
    		let indices  = data.index;
    		let vertices = new ArrayBuffer(nv*m.vsize);
    		var position = new Float32Array(vertices, 0, nv*3);
    		position.set(data.position);
    		var off = nv*12;
    		if(m.vertex.texCoord) {
    			var uv = new Float32Array(vertices, off, nv*2);
    			uv.set(data.uv);
    			off += nv*8;
    		}
    		if(m.vertex.color) {
    			var color = new Uint8Array(vertices, off, nv*4);
    			color.set(data.color);
    			off += nv*4;
    		}
    		if(m.vertex.normal) {
    			var normal = new Int16Array(vertices, off, nv*3);
    			normal.set(data.normal);
    			off += nv*6;
    		}
    		
    		//needed for approximate picking.
    		if(id < this.mesh.nroots) {
    			let basegeometry = new THREE.BufferGeometry();
    			basegeometry.setAttribute( 'position', new THREE.BufferAttribute(data.position, 3 ) );
    			basegeometry.setAttribute( 'normal', new THREE.BufferAttribute(data.normal, 3 ) );
    			basegeometry.setIndex(new THREE.BufferAttribute( data.index, 1 ) );

    			this.basemesh = new THREE.Mesh(basegeometry, this.material);
    			this.basemesh.visible = false;
    			this.add(this.basemesh);
    		}
    		
    		var gl = this.gl;
    		this.vao[id] = {}; //one for each attrib combination in use (casting shadows for example).
    		gl.bindVertexArray(null);
    		
    		var vbo = this.vbo[id] = gl.createBuffer();
    		gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    		gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
    		var ibo = this.ibo[id] = gl.createBuffer();
    		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
    		gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
    	}

    	createTexture(id, image) {
    		let gl = this.gl;
    		var flip = gl.getParameter(gl.UNPACK_FLIP_Y_WEBGL);
    		gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
    		let tex = this.textures[id] = gl.createTexture();
    		gl.bindTexture(gl.TEXTURE_2D, tex);

    		//TODO some textures might be alpha only! save space
    		var s = gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB, gl.RGB, gl.UNSIGNED_BYTE, image);
    		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);

    		function powerOf2(n) { return n && (n & (n - 1)) === 0; }
    		if(!(gl instanceof WebGLRenderingContext) || (powerOf2(image.width) && powerOf2(image.height))) {
    			gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST_MIPMAP_LINEAR);
    			gl.generateMipmap(gl.TEXTURE_2D);
    		} else {
    			gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    		}

    		gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, flip);   
    	}
    	//schedule for removal of this node( might not want to delete it in the middle of something.
    	//TODO check if this is really needed!
    	deleteNodeGeometry(id) {
    		this.gl.deleteBuffer(this.vbo[id]);
    		this.gl.deleteBuffer(this.ibo[id]);

    		this.vbo[id] = this.ibo[id] = null;
    		if(this.vao[id]) //node might have been loaded and unloaded before a rendering
    			for(const [state, vao] of Object.entries(this.vao[id]))
    				this.gl.deleteVertexArray(vao);
    		this.vao[id] = null;
    	}

    	deleteTexture(tex) {
    		if(!this.textures[tex])
    			return;
    		//	throw "Deleting missing texture!"

    		this.gl.deleteTexture(this.textures[tex]);
    		this.textures[tex] = 0;
    	}

    	flush() {
    		this.cache.flush(this.mesh);
    	}

    	dispose() {
    		this.flush();
    		for(let child of this.children)
    			child.geometry.dispose();
    	}

    	toJSON(meta) {
    		throw "Can't";
    	}

    	patchWebGLRenderer(renderer) {
    		if(renderer.nexusPatched) return;
    		let f = renderer.renderBufferDirect;
    		renderer.renderBufferDirect = ( camera, scene, geometry, material, object, group) => { 
    			f( camera, scene, geometry, material, object, group );
    			if ( object.renderBufferDirect)
    				object.renderBufferDirect(renderer, scene, camera, geometry, material, group);
    		};
    		renderer.originalRender = renderer.render;
    		renderer.render = (scene, camera) => {
    			Cache.beginFrame(30);
    			renderer.originalRender(scene, camera);
    			Cache.endFrame();
    		};
    		renderer.nexusPatched = true;
    	}        

    }

    exports.Cache = Cache;
    exports.Nexus3D = Nexus3D;

    Object.defineProperty(exports, '__esModule', { value: true });

})));
