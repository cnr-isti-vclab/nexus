import { vecMul, matMul, matInv } from './Binary.js'
import { PriorityQueue } from "./PriorityQueue.js"

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
        let r  = spheres[off+3];
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
            error /= 1 -( d/r)*100.0
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

export { Traversal }
