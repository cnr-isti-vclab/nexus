import * as THREE from 'three'
import { Traversal } from './Traversal.js'
import { Mesh } from './Mesh.js'
import { Cache } from './Cache.js'

function Nexus3D(url, renderer, options) {

    if(typeof renderer == 'function') 
        throw "Nexus3D constructor has changed: Nexus3D(url, renderer, options) where options include: onLoad, onUpdate, onProgress and material"

	THREE.Object3D.call( this );

	this.type = 'NXS';

    this.url = url;
    this.gl = renderer.getContext();

    this.material = null;
    if('material' in options)
        this.material = options.material;
    if(!this.material) 
        this.material = new THREE.MeshStandardMaterial();
        
    for(let call of ['onLoad', 'onUpdate', 'onProgress']) {
        this[call] = [];
        if(call in options)
            this[call].push(options[call])
    }


    this.autoUpdate = true;
    this.mesh = new Mesh(); 
    
    this.vbo = [];
    this.ibo = [];
    this.textures = [];
    this.attributes = {};  //here we store the uniform attributes of the shader.

    this.basemesh = null;  //highest level of the nexus, for picking


    if(this.url) {
        if(typeof url == 'object') {
            this.nxs = this.url;
            this.nxs.onLoad.push((m) => { 
                this.mesh = this.nxs.mesh;
                this.traversal = this.nxs.traversal;
                this.cache = this.nxs.cache;
                this.vbo = this.nxs.vbo;
                this.ibo = this.nxs.ibo;
                this.textures = this.nxs.textures;
                this.onLoadCallback(this); 
            });
        } else
            this.open(this.url);
    }

}

Nexus3D.prototype = Object.assign( Object.create( THREE.Object3D.prototype ), {

	constructor: Nexus3D,

	isNXS: true,

	copy: function(source) {

		Object3D.prototype.copy.call( this, source, false );
        throw "Can't really copy."
		return this;

    },

    
    open: function(url) {
        let t = this;
        this.mesh.open(url);
        this.mesh.createNode         = (id)           => { };
        this.mesh.createNodeGeometry = (id, geometry) => { t.createNodeGeometry(id, geometry); };
        this.mesh.createTexture      = (id, image)    => { t.createTexture(id, image); };
        this.mesh.deleteNodeGeometry = (id)           => { t.deleteNodeGeometry(id); };
        this.mesh.deleteTexture      = (id)           => { t.deleteTexture(id); };
        this.mesh.onLoad.push(() => { t.onLoadCallback(); });
        this.mesh.onUpdate.push(() => { for(let callback of t.onUpdate) callback(this); });

        this.traversal = new Traversal();
        this.cache = Cache; //new Cache();
        this.textures = {};        
    },

    set onLoad(callback) {
        this.onLoad.push(callback);
    },

    set onUpdate(callback) {
        this.onLoad.push(callback);
    },

    set onProgress(callback) {
        this.onProgress.push(callback);
    },

    set material(material) {
        this.cube.material = this.material = material;
        this.material.needsUpdate = true;
    },
    
        //we need to hijack the material when nxs has textures: when we change the material it might have a map or not 
    //and we need to update all the materials!
    //we also need to create an array of materials for groups to work.

    //Nexus has a list of materials and, usually, each node attach his texture.

    updateMaterials: function() {
        if(this.material.map !== false && this.mesh.vertex.texCoord)
            this.material.map = this.cube_texture;

        if(this.mesh.vertex.color)
            this.material.vertexColors = THREE.VertexColors; 
        this.material.needsUpdate = true; 
    },

    onLoadCallback: function() {
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

        this.cube_texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
        this.cube_texture.needsUpdate = true;

        this.updateMaterials();

        let cube = new THREE.Mesh(geometry, this.material);
        cube.frustumCulled = false;
        cube.onBeforeRender = (renderer, scene, camera, geometry, material, group) => { 
            this.onBeforeRender(renderer, scene, camera, geometry, material, group) }
        cube.onAfterRender = (renderer, scene, camera, geometry, material, group) => { 
            this.onAfterRender(renderer, scene, camera, geometry, material, group) }

        this.add(cube); 
            
        for(let callback of this.onLoad)
            callback(this);
    },

    onBeforeRender: function(renderer, scene, camera, geometry, material, group) {  
    },
    onAfterRender: function(renderer, scene, camera, geometry, material, group) {
        let s = new THREE.Vector2();
        renderer.getSize(s);

       	//object modelview is multiplied by camera during rendering, we need to do it here for visibility computations
        this.modelViewMatrix.multiplyMatrices( camera.matrixWorldInverse, this.matrixWorld );
        this.traversal.updateView([0, 0, s.width, s.height], camera.projectionMatrix.elements, this.modelViewMatrix.elements);
        this.instance_errors = this.traversal.traverse(this.mesh, this.cache);



        //threejs increments version when setting neeedsUpdate
        if(this.material.version > 0) {
            this.updateMaterials();
            this.material.version = 0;
            for(let callback of this.onUpdate) callback(this); 

        let gl = this.gl;
        var program = gl.getParameter(gl.CURRENT_PROGRAM);

        var attr = this.attributes;
        attr.position = gl.getAttribLocation(program, "position");
        attr.normal   = gl.getAttribLocation(program, "normal");
        attr.color    = gl.getAttribLocation(program, "color");
        attr.uv       = gl.getAttribLocation(program, "uv");
        attr.size     = gl.getUniformLocation(program, "size");
        attr.scale    = gl.getUniformLocation(program, "scale");
        let map_location = gl.getUniformLocation(program, "map"); 
        attr.map      = map_location ? gl.getUniform(program, map_location) : null;
        }
    
    
        //hack to detect if threejs using point or triangle shaders
        //instance.mode = attr.size ? "POINT" : "FILL";
        //if(attr.size != -1) 
        //    instance.pointsize = material.size;
    
        //can't find docs or code on how material.scale is computed in threejs.
        //if(attr.scale != -1)
        //    instance.pointscale = 2.0;

        this.setVisibility();
    },

    setVisibility: function() {
        //set visibile what is visible!
        let t = this.traversal;
        let m = this.mesh;
    
        if(!m.isReady)
            return;
        let rendered = 0;

        for(let id = 0; id < m.nodesCount; id++) {
//            let err = m.nerrors[id];

            if(!t.selected[id]) continue;

            //check for children: if all are selected, bail out.
            {
                let visible = false;
                let offset = 0;
                let end = 0;
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
    
            let attr = this.attributes;
            let mesh = this.mesh;
            let gl = this.gl;
            //gl.bindVertexArray(null);
            gl.bindBuffer(gl.ARRAY_BUFFER, this.vbo[id]);
            //if(t.mode != "POINT")
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
            this.cache.realError = Math.min(this.mesh.errors[id], this.cache.realError);
            
             offset = 0;
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
					gl.drawElements(gl.TRIANGLES, (end - offset) * 3, gl.UNSIGNED_SHORT, offset * 6);
                    rendered += end - offset;
                }
                offset = m.patches[p*3+1];
            }
        }
        this.cache.rendered += rendered;
    },



    createNodeGeometry: function(id, data) {
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
        
        var gl = this.gl
        var vbo = this.vbo[id] = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
        gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
        var ibo = this.ibo[id] = gl.createBuffer();
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
        gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
    },

    createTexture: function(id, image) {
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
    },
    //schedule for removal of this node( might not want to delete it in the middle of something.
    //TODO check if this is really needed!
    deleteNodeGeometry: function(id) {
        this.gl.deleteBuffer(this.vbo[id]);
	    this.gl.deleteBuffer(this.ibo[id]);
        this.vbo[id] = this.ibo[id] = null;
    },

    deleteTexture: function(tex) {
        if(!this.textures[tex])
            throw "Deleting missing texture!"

        this.gl.deleteTexture(this.textures[tex]);
        this.textures[tex] = 0;
    },

	flush: function() {
		this.cache.flush(this.mesh);
	},

	dispose: function() {
		this.flush();
		for(let child of this.children)
			child.geometry.dispose();
	},

	toJSON: function ( meta ) {
		throw "Can't";
	},

} );


export { Nexus3D, Cache };
