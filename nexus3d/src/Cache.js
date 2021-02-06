
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
    }

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
        }
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
	    mesh.georeq[id] = mesh.httpRequest(
		    mesh.noffsets[id],
		    mesh.noffsets[id+1],
		    function() {
                            delete mesh.texreq[id];
                t.loadNodeGeometry(this, mesh, id);
            },
		    function() {
                            delete mesh.texreq[id];
			    if(this.debug.verbose) console.log("Geometry request error!");
			    t.recoverNode(mesh, id, 0);
		    },
		    function() {
                            delete mesh.texreq[id];
			    if(this.debug.verbose) console.log("Geometry request abort!");
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

	    mesh.status[id]++; //pending

	    mesh.texreq[tex] = mesh.httpRequest(
		    mesh.textures[tex],
		    mesh.textures[tex+1],
		    function() { 
                        delete mesh.texreq[tex];
                t.loadNodeTexture(this, mesh, id, tex); 
            },
		    function() {
                        delete mesh.texreq[tex];
		    	if(this.debug.verbose) console.log("Texture request error!");
			    t.recoverNode(mesh, id, 1);
		    },
		    function() {
                        delete mesh.texreq[tex];
		    	if(this.debug.verbose) console.log("Texture request abort!");
		    	t.removeNode(mesh, id);
		    },
		    'blob'
	    );
    },

    recoverNode: function(mesh, id, mode) {
	    if(mesh.status[id] == 0) return;

	    mesh.status[id]--;

        let t = this;

	    if(mesh.reqAttempt[id] > maxReqAttempt) {
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
		    }

        if(typeof createImageBitmap != 'undefined') {
            var isFirefox = typeof InstallTrigger !== 'undefined';
            //firefox does not support options for this call, BUT the image is automatically flipped.
            if(isFirefox) {
                createImageBitmap(blob).then(callback);
            } else {
            createImageBitmap(blob, { imageOrientation: 'flipY' }).then(callback);
            }
            

        } else { //fallback for IOS
            var urlCreator = window.URL || window.webkitURL;
            var img = document.createElement('img');
            img.onerror = function(e) { console.log("Texture loading error!"); };
            img.src = urlCreator.createObjectURL(blob);
    
            img.onload = function() {
                urlCreator.revokeObjectURL(img.src);
                callback(img);
            }
        }
    },

    removeNode: function(mesh, id) {
        this.nodes.get(mesh).delete(id);

        if(mesh.status[id] == 0)
            throw "Was already removed!";

	    mesh.status[id] = 0;
	if (id in mesh.georeq && mesh.georeq[id].readyState != 4) {
            mesh.georeq[id].abort();
            delete mesh.georeq[id];
		    this.pending--;
	    }

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
        if(!this.nodes.has(mesh))
            this.nodes.set(mesh, new Set());
        this.nodes.get(mesh).add(id);

	    mesh.status[id]--;
        if(mesh.status[id] != 1) throw "A ready node should have status ==1"
		    mesh.reqAttempt[id] = 0;
            this.pending--;
            mesh.createNode(id);
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
                        worst = { id: id, frame: frame, error: error, mesh: mesh }
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
export { Cache }

