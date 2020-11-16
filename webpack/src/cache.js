
var corto;
function loadCorto() {
    corto = new Worker('corto.em.js');
    corto.requests = {};
    corto.count = 0;
    corto.postRequest = function(node) {
        corto.postMessage({ buffer: node.buffer, request:this.count, rgba_colors: true, short_index: true, short_normals: true});
        node.buffer = null;
        this.requests[this.count++] = node;
    }
    corto.onmessage = function(e) {
        var request = e.data.request;
        var node = this.requests[request];
        delete this.requests[request];
        node.model = e.data.model;
        node.cache.readyNode(node.mesh, node.id, node.model);
    };
}

var Debug = { verbose: false };
function powerOf2(n) {
	return n && (n & (n - 1)) === 0;
}

var targetError   = 2.0;    //error won't go lower than this if we reach it
var maxError      = 15;     //error won't go over this even if fps is low
var minFps        = 15;
var maxPending    = 3;
var maxBlocked    = 3;
var maxReqAttempt = 2;
var maxCacheSize  = 512 *(1<<20); 
var drawBudget    = 5*(1<<20);

function Cache() {
    let t = this;
    t.frame = 0;         //keep track of the time

    
    t.maxCacheSize = maxCacheSize;
    t.minFps = minFps;
    t.targetError = targetError;
    t.currentError = targetError;
    t.maxError = maxError;
    t.realError = 0;

    t.pending = 0;
    t.maxPending = 3;
    t.cacheSize = 0;
    t.candidates = [];   //list of nodes to be loaded
    t.nodes = new Map();        //for each mesh a list of node ids.
}

Cache.prototype = {
    getTargetError:  function()      { return this.targetError; },
    getMinFps:       function()      { return this.minFps; },
    setMinFps:       function(fps)   { this.minFps = fps; },
    getMaxCacheSize: function()      { return this.maxCacheSize; },
    setMaxCacheSize: function(size)  { this.maxCacheSize = size; },
    setTargetError:  function(error) { this.targetError = error; },
    
    beginFrame: function(fps) { //each context has a separate frame count.
        let c = this;
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
	    c.realError = 0;
    },

    endFrame: function() {
	    this.update();
    },



    requestNode: function(mesh, id) {
	    mesh.status[id] = 2; //pending

	    this.pending++;
	    this.cacheSize += mesh.nsize[id];
	    mesh.reqAttempt[id] = 0;

	    this.requestNodeGeometry(mesh, id);
	    this.requestNodeTexture(mesh, id);
    },

    requestNodeGeometry: function(mesh, id) {
        let t = this;

	    mesh.status[id]++; //pending
	    mesh.georeq = mesh.httpRequest(
		    mesh.noffsets[id],
		    mesh.noffsets[id+1],
		    function() {
                t.loadNodeGeometry(this, mesh, id);
            },
		    function() {
			    if(Debug.verbose) console.log("Geometry request error!");
			    t.recoverNode(mesh, id, 0);
		    },
		    function() {
			    if(Debug.verbose) console.log("Geometry request abort!");
			    t.removeNode(mesh, id);
		    },
		    'arraybuffer'
        );
    },

    requestNodeTexture: function(mesh, id) {
        let t = this;
	    
	    if(!mesh.vertex.texCoord) return;

	    let tex = mesh.patches[mesh.nfirstpatch[id]*3+2];
	    mesh.texref[tex]++;
	    //if(m.texids[tex])
		//    return;

	    mesh.status[id]++; //pending

	    mesh.texreq = mesh.httpRequest(
		    mesh.textures[tex],
		    mesh.textures[tex+1],
		    function() { 
                t.loadNodeTexture(this, mesh, id, tex); 
            },
		    function() {
		    	if(Debug.verbose) console.log("Texture request error!");
			    t.recoverNode(mesh, id, 1);
		    },
		    function() {
		    	if(Debug.verbose) console.log("Texture request abort!");
		    	t.removeNode(mesh, id);
		    },
		    'blob'
	    );
    },

    recoverNode: function(mesh, id, mode) {
	    if(mesh.status[id] == 0) return;

	    mesh.status[id]--;

	    if(mesh.reqAttempt[id] > maxReqAttempt) {
		    if(Debug.verbose) console.log("Max request limit for " + m.url + " node: " + n);
		    t.removeNode(mesh, id);
		    return;
	    }

	    mesh.reqAttempt[id]++;

	    switch (mode){
		case 0:
		    t.requestNodeGeometry(mesh, id);
		    if(Debug.verbose) console.log("Recovering geometry for " + m.url + " node: " + n);
		    break;
		case 1:
			t.requestNodeTexture(mesh, id);
			if(Debug.verbose) console.log("Recovering texture for " + m.url + " node: " + n);
			break;
	    }
    },

    loadNodeGeometry: function(request, mesh, id) {
	    if(mesh.status[id] == 0) return;
        
	    if(!mesh.compressed)
		    this.readyNode(mesh, id, request.response);
	    else {
            if(!corto) loadCorto();
		    corto.postRequest( { mesh:mesh, id:id, buffer: request.response, cache: this });
        }
    },


    loadNodeTexture: function(request, mesh, id, texid) {
        if(mesh.status[id] == 0) 
            return;

	    let blob = request.response;

        
        //The image deconding is done here! not in a thread.
        createImageBitmap(blob, { imageOrientation: 'flipY' }).then((img) => {
            mesh.createTexture(texid, img);

		    mesh.status[id]--;

		    if(mesh.status[id] == 2) {
			    mesh.status[id]--; //ready
			    mesh.reqAttempt[id] = 0;
                this.pending--;
                mesh.createNode(id);
			    mesh.onUpdate();
			    this.update();
		    }
        });
    },

    removeNode: function(mesh, id) {
	    if(mesh.status[id] == 0) return;

	    mesh.status[id] = 0;

	    if (mesh.georeq.readyState != 4) {
		    mesh.georeq.abort();
		    this.pending--;
	    }

        this.cacheSize -= mesh.nsize[id];
        mesh.deleteNodeGeometry(id);

	    if(!mesh.vertex.texCoord) return;
	    if (mesh.texreq && mesh.texreq.readyState != 4) mesh.texreq.abort();
	    const tex = mesh.patches[mesh.nfirstpatch[id]*3+2]; //TODO assuming one texture per node
	    mesh.texref[tex]--;

    	if(mesh.texref[tex] == 0) {
            mesh.deleteTexture(tex);
        }
        this.nodes.set(mesh, this.nodes.get(mesh).filter(i => i == id));
    },

    readyNode: function(mesh, id, buffer) {
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
        if(!this.nodes.has(mesh))
            this.nodes.set(mesh, []);
        this.nodes.get(mesh).push(id);

	    mesh.status[id]--;

	    if(mesh.status[id] == 2) {
		    mesh.status[id]--; //ready
		    mesh.reqAttempt[id] = 0;
            this.pending--;
            mesh.createNode(id);
		    mesh.onUpdate();
            this.update();
	    }
    },

    flush: function(mesh) {
        for(let id of this.nodes.get(mesh))
            mesh.remove(id)
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

        //make room for new nodes!
	    while(this.cacheSize > this.maxCacheSize) {
            let worst = null;
            
            //find node with smallest error in cache and remove it if worse than the best candidate.
            for(let [mesh, ids] of this.nodes) {
                for(let i = 0; i < ids.length; i++) {
                    let id = ids[i];
                //we need to recompute the errors for the cache, as if not traversed doesn't get updated.
                    let error = mesh.errors[id];
                    let frame = mesh.frames[id];
                    if( !worst || frame < worst.frame || error < worst.error) {
                        worst = { id: id, index: i, frame: frame, error: error, mesh: mesh }
                    }
                }
            }
            if(!worst || worst.error >= best.error*0.9)
                //(worst.frame + 30 >= best.frame && )) //dont' remove if  the best candidate is not good enogh
                return;
            //we have added some histeresys: we swap only if the best is a bit better.

            this.nodes.get(worst.mesh).splice(worst.index, 1);
		    this.removeNode(worst.mesh, worst.id);
	    }
        this.candidates = this.candidates.filter(e => e.mesh == best.mesh && e.id == best.id);
    	this.requestNode(best.mesh, best.id);
	    this.update();  //try again.
    }
};

export { Cache }

