import * as THREE from 'three'
import { Traversal } from './Traversal.js'
import { Mesh } from './Mesh.js'
import { Cache } from './Cache.js'

function NXS(url, onLoad, onUpdate, material) {

	THREE.Object3D.call( this );

	this.type = 'NXS';

	this.autoUpdate = true;
    this.mesh = null;
    this.url = url;
    this.onLoadCallback = onLoad;
    this.onUpdate = onUpdate || function() {};
    this.debug = false;

    if(this.url)
        this.open(this.url)

}

NXS.prototype = Object.assign( Object.create( THREE.Object3D.prototype ), {

	constructor: NXS,

	isNXS: true,

	copy: function(source) {

		Object3D.prototype.copy.call( this, source, false );
        throw "Can't really copy."
		return this;

    },

    
    open: function(url) {
        let t = this;
        this.mesh = new Mesh(url); 
        this.mesh.createMaterials    = (materials)    => { t.createMaterials(materials) };
        this.mesh.createNode         = (id)           => { t.updateMaterial(id) };
        this.mesh.createNodeGeometry = (id, geometry) => { t.createNodeGeometry(id, geometry); };
        this.mesh.createTexture      = (id, image)    => { t.createTexture(id, image); };
        this.mesh.deleteNodeGeometry = (id)           => { t.deleteNodeGeometry(id); };
        this.mesh.deleteTexture      = (id)           => { t.deleteTexture(id); };
        this.mesh.onLoad = () => { t.onLoad(); }
        this.mesh.onUpdate = () => { t.onUpdate(t.mesh); }
        this.traversal = new Traversal();
        this.cache = new Cache();
        this.materials = [];
        this.textures = {};
        this.nodes = {} //can't use getObjectById, it's slow //TODO use Map!

        this.toDelete = [];

        //this is needed for a onbeforerender callback!
        const geometry = new THREE.BoxBufferGeometry(0.0, 0.0, 0.0);
        const material = new THREE.MeshBasicMaterial();
        let cube = new THREE.Mesh(geometry, material);
        cube.frustumCulled = false;
        cube.onBeforeRender = (renderer, scene, camera, geometry, material, group) => { 
            t.onBeforeRender(renderer, scene, camera, geometry, material, group) }
        cube.onAfterRender = (renderer, scene, camera, geometry, material, group) => { 
            t.onAfterRender(renderer, scene, camera, geometry, material, group) }
        this.add(cube); 

    },

    set material(material) {
        this.material = material;
        this.material.needsUpdate = true;
    },

    set debug(debug) {
        if(this.debug != debug) {
        this.debug = debug;
        this.material.needsUpdate = true;
        }
    },
    
    onLoad: function() {
        const c = this.mesh.sphere.center;
		const center = new THREE.Vector3(c[0], c[1], c[2]);
        const radius = this.mesh.sphere.radius;
        
        this.boundingSphere = new THREE.Sphere(center, radius);
        if(this.material == null || this.material == false)
            this.createMaterial();
        this.onLoadCallback(this);
    },

    onBeforeRender: function(renderer, scene, camera, geometry, material, group) {
        let s = new THREE.Vector2();
        renderer.getSize(s);

        for(let id of this.toDelete)
            this.actuallyDeleteNodeGeometry(id);
        this.toDelete = [];
       	//object modelview is multiplied by camera during rendering, we need to do it here for visibility computations
        this.modelViewMatrix.multiplyMatrices( camera.matrixWorldInverse, this.matrixWorld );
        this.traversal.updateView([0, 0, s.width, s.height], camera.projectionMatrix.elements, this.modelViewMatrix.elements);
        this.traversal.traverse(this.mesh, this.cache);
        this.setVisibility();
    },
    onAfterRender: function(renderer, scene, camera, geometry, material, group) {
        this.cache.update();
    },

    setVisibility: function() {
        if(this.material) {
            if(this.debug != this.cache.debug.nodes) {
                this.debug = this.cache.debug.nodes;
                this.material.needsUpdate = true;
            }
            //threejs increments version when setting neeedsUpdate
            if(this.material.version > 0) {
            this.updateMaterials();
                this.material.version = 0;
                this.onUpdate();
            }
        }

        //set visibile what is visible!
        let t = this.traversal;
        let m = this.mesh;
  
        if(!m.isReady)
            return;
        let rendered = 0;

        for(let id = 0; id < m.nodesCount; id++) {
            if(!(id in this.nodes))
                continue;

            let node = this.nodes[id];
            //let err = m.errors[n]; //careful this is wrong if multiple instances
            let err = m.nerrors[id];

            node.visible = false;


            if(!t.selected[id]) continue;

            var sp = m.nspheres;
            var off = id*5;
            if(!t.isVisible(sp[off], sp[off+1], sp[off+2], sp[off+4])) //tight radius
                continue;
 
            //There is no way to control group visibility, so we remove and recreate them.
            node.geometry.clearGroups();

            var offset = 0;
            var end = 0;
            var last = m.nfirstpatch[id+1]-1;
            for (var p = m.nfirstpatch[id]; p < m.nfirstpatch[id+1]; ++p) {
                var child = m.patches[p*3];
    
                if(!t.selected[child]) {
                    end = m.patches[p*3+1];
                    if(p < last) //we join patches if possible.
                        continue;
                }

                if(end > offset) {
                    node.visible = true;
                    if(this.cache.debug.nodes) {
                        node.material[0].color = new THREE.Color(0, 1, 0);
                        if(child == m.sink) {
                        } else {
                        //node.material[0].color.setHSL(err/50, 0.5, 0.7);
                        let h = id*89.0/244.0;
                        h -= Math.floor(h);
                        node.material[0].color.setHSL(h, 0.6, 0.7);
                        }
                    }
        
                    node.geometry.addGroup(offset*3, (end-offset)*3)
                    rendered += end - offset;
                }
                offset = m.patches[p*3+1];
            }
        }
        this.cache.rendered += rendered;
    },

    //we need to hijack the material when nxs has textures: when we change the material it might have a map or not 
    //and we need to update all the materials!
    //we also need to create an array of materials for groups to work.

    //Nexus has a list of materials and, usually, each node attach his texture.
    createMaterial: function() {
        let options = {}
        if(this.mesh.vertex.texCoord)
            options.map = null;
        if(this.mesh.vertex.color)
           options.vertexColors = THREE.VertexColors;   
        //options.wireframe = true;
        this.material = new THREE.MeshStandardMaterial(options);
        //this.material = [new THREE.MeshPhongMaterial(options)];
        //this.material[0] = new THREE.MeshPhongMaterial( { color: 0xff0000 } );
    },

    updateMaterials: function() {
        for(let id in this.nodes)
            this.updateMaterial(id);
    },

    updateMaterial: function(id) {
        let node = this.nodes[id];
        const m = this.mesh;        
        //debug has each node with a different color, so we need to clone materials       
        if(!m.vertex.texCoord && !this.cache.debug.nodes) {
            this.nodes[id].material = [this.material];
            return;
        }

        let material = this.material.clone();
        if(m.vertex.texCoord) {            
            var texid = m.patches[m.nfirstpatch[id]*3+2];
            if(texid != -1)
                material.map = this.textures[texid];            
        }

        this.nodes[id].material = [material];
    },

    createNodeGeometry: function(id, data) {
        let geometry = new THREE.BufferGeometry()
        geometry.setAttribute( 'position', new THREE.BufferAttribute(data.position, 3 ) );
        if(data.normal) {
            geometry.setAttribute( 'normal', new THREE.BufferAttribute(data.normal, 3, true ) );
        }
        if(data.color)
            geometry.setAttribute( 'color', new THREE.BufferAttribute(data.color, 4, true ) );
        if(data.uv)
            geometry.setAttribute( 'uv', new THREE.BufferAttribute(data.uv, 2 ) );
        geometry.setIndex(new THREE.BufferAttribute( data.index, 1 ) );

        //if material include maps we need a different material for each
        let node = new THREE.Mesh(geometry);
        node.frustumCulled = false;
        geometry.addGroup(0, data.index.length);
        node.visible = false;
        this.nodes[id] = node;
        this.add(node);
    },

    createTexture: function(id, image) {
        this.textures[id] = new THREE.CanvasTexture(image);
    },
    //schedule for removal of this node( might not want to delete it in the middle of something.
    //TODO check if this is really needed!
    deleteNodeGeometry: function(id) {
        this.toDelete.push(id);
    },
    //check for leaking memory.
    actuallyDeleteNodeGeometry(id) {
        this.remove(this.nodes[id]);
        delete this.nodes[id];
    },

    deleteTexture: function(tex) {
        if(!this.textures[tex])
            throw "Deleting missing texture!"
        this.textures[tex].dispose();
        delete this.textures[tex];
    },

    
	toJSON: function ( meta ) {
        throw "Can't"
	},

    raycast: function ( raycaster, intersects ) {

		const levels = this.levels;

		if ( levels.length > 0 ) {

			_v1.setFromMatrixPosition( this.matrixWorld );

			const distance = raycaster.ray.origin.distanceTo( _v1 );

			this.getObjectForDistance( distance ).raycast( raycaster, intersects );

		}

    },
    
} );


export { NXS };
