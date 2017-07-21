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

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


/* ISSUES: 
tomaterial for texture colors, normals etc.
interface to know what is inside and use custom materials.
fps external call 
target error and target fps, cache size exposed
target error should be for model? (override global target error)
gl context for multiple contexts
*/

/* updateView done for instance -> should be done for camera.
   follow LOD interface .update ( camera ) and raycast returns a function raycast(raycaster intersects)
	I need access to loading of meshes (readyNode) and loadTexture to create material
	Ho bisogno di tenere lista materiali e lista nodes

  mesh e instance just reuse the geometry part.
.onBeforeRender to update camera (similar to lod?) easier than

use fileloader with range in fileloader.header = { Range: "bytes=start-end }
but interferes with cache! unless we cheat on the url using a ?0-12123 with the range!
*/

/* options accepts 
	all those of Nexus (passed on loading)
	maxCacheSize, 
	targetFps
*/

THREE.NEXUSLoader = function(options) {
	THREE.Loader.call(this);
	this.options = options? options : {};
	this.context = new Nexus.Context();
	if(this.options.onUpdate)
		this.context.onUpdate = this.options.onUpdate;
	
}

THREE.NEXUSLoader.prototype = Object.assign(Object.create(THREE.Loader.prototype), {

constructor: THREE.NEXUSLoader,

load: function(url, options) {
	var o = Object.assign(this.options, options);
	var nexus = new THREE.Nexus(url, this.context, o);
	return nexus;
},

update: function() {
	this.context.update();
},

setTargetFps: function(fps) {
	this.context.targetFps = fps;
},

setMaxCacheSize: function(size) {
	this.context.maxCacheSize = size;
}

});

//accepts: onLoad, onError, onUpdate, targetError, maxTargetError, drawBudget

THREE.Nexus = function(url, context, options) {
	THREE.Group.call( this );

	var t = this;
	t.context = context;
	t.options = options;

	t.geometries = [];
	t.materials = [];
	t.meshes = [];

	t.onLoad = options.onLoad;
	t.onError = options.onError;
	t.onUpdate = options.onUpdate;

	t.frustumCulled = false; //need to traverse and fix cache priority!

	t.extract = true;

	t.model = new Nexus.Model(url, context);
	t.model.onLoad = function() { t.onLoad(t); };
	t.model.onError = function() { t.onError(t); };
	t.model.loadNode = function(node) { t.loadNode(node); }
	t.model.loadTexture = function(img, texid) { t.loadTexture(img, texid); }

	t.waiting = {}; //nodes waiting for material.
};

THREE.Nexus.prototype = Object.assign(Object.create(THREE.Group.prototype), {

constructor: THREE.Nexus,

setTargetError: function(error) {
	this.options.targetError = error;
},

setDrawBudget: function(budget) {
	this.options.drawBudget = budget;
},

loadNode: function(node) {
	var t= this;
	var n = node.id;
	var nv = node.nvert;
	var nf = node.nface;
	var m = node.model;

	//color = 0xffffff;
	var rcolor;
	if(Nexus.Debug.nodes)
		rcolor = new THREE.Color((n*200 %255)/255.0, (n*140 %255)/255.0,(n*90 %255)/255.0);
	else
		rcolor = 0xffffff;

	var geometry = new THREE.BufferGeometry();

	var model = {};

	if(!node.patch) {

		model.position = new Float32Array(node.buffer, off, nv*3);
		var off = nv*12;
		if(m.vertex.texCoord)
			model.uv = new Float32Array(node.buffer, off, nv*2), off += nv*8;

		if(m.vertex.normal)
			model.normal = new Int16Array(node.buffer, off, nv*3), off += nv*6; 

		if(m.vertex.color) {
			model.color = new Uint8Array(node.buffer, off, nv*4), off += nv*4;
			var color = new Uint8Array(nv*3);
			for(var i = 0, j = 0; i < color.length; i+= 3, j += 4) {
				color[i] = model.color[j];
				color[i+1] = model.color[j+1];
				color[i+2] = model.color[j+2];
			}
			
			model.color = color;
		}

		model.index  = new Uint16Array(node.buffer, nv*m.vsize,  nf*3);
	} else
		model = node.patch;

	geometry.setIndex( new THREE.BufferAttribute( model.index, 1 ) );

	geometry.addAttribute( 'position', new THREE.BufferAttribute(model.position, 3));

	if(model.normal)
		geometry.addAttribute( 'normal', new THREE.BufferAttribute(model.normal, 3));

	if(model.uv)
		geometry.addAttribute( 'uv', new THREE.BufferAttribute(model.uv, 2));

	if(model.color)
		geometry.addAttribute( 'color', new THREE.BufferAttribute(model.color, 3, true), false);

	var materialoptions = { color: rcolor, shininess: 0 };

	if(model.color)
		materialoptions.vertexColors = THREE.VertexColors;

	if(model.uv) {
		var m = node.model;
		var p = m.nfirstpatch[n];
		var texid = m.patches[p*3+2];
		if(!t.materials[texid]) {
			if(!t.waiting[texid])
				t.waiting[texid] = [];

			t.waiting[texid].push(n);
			material = [new THREE.MeshPhongMaterial(materialoptions)];
		} else
			material = t.materials[texid]; 

	} else
		material = [new THREE.MeshPhongMaterial(materialoptions)];


	var sp = m.nspheres;
	var off = n*5;
	geometry.boundingSphere = new THREE.Sphere(new THREE.Vector3(sp[off],sp[off+1],sp[off+2]), sp[off+3]); 

	t.geometries[n] = geometry;

	var mesh = t.meshes[n] = new THREE.Mesh(geometry, material);
	mesh.visible = false;
	mesh.frustumCulled = false;
	t.add(mesh);
},

loadTexture: function(img, texid) {
	var t = this;
	var tex = new THREE.Texture(img);
	tex.generateMipmaps = false;
	tex.minFilter = THREE.LinearFilter;
	tex.needsUpdate = true;

	var rcolor;
	if(Nexus.Debug.nodes)
		rcolor = new THREE.Color((texid*200 %255)/255.0, (texid*140 %255)/255.0,(texid*90 %255)/255.0);
	else
		rcolor = 0xffffff;

	t.materials[texid] = [new THREE.MeshPhongMaterial({ color:rcolor, map: tex, shininess: 0})];
	
	var w = t.waiting[texid];
	if(!w) return;
	w.forEach(function(n) {
		t.meshes[n].material = t.materials[texid];
	});
	delete t.waiting[texid];
},

dispose: function() {
	var t = this;
	t.model.dispose();
	t.meshes.forEach(function(m) { m.dispose(); });
	t.geometries.forEach(function(m) { m.dispose(); });
	t.materials.forEach(function(m) { m.dispose(); }); //TODO check for disposing of textures?
},

center: function() {
	var m = this.model;
	var s = 1/m.sphere.radius;
	var pos = m.sphere.center;
	this.position.set(-pos[0]*s, -pos[1]*s, -pos[2]*s);
	this.scale.set(s, s, s);
},

update: function(renderer, camera) { 
	var t = this;
	var m = t.model;


	var s = renderer.getSize();
	var viewport = [0, 0, s.width, s.height];

	var modelView = new THREE.Matrix4();
	modelView.multiplyMatrices(camera.matrixWorldInverse, t.matrixWorld);

	m.updateView(viewport, camera.projectionMatrix.elements, modelView.elements);
	
	var targetError = m.currentTargetError();
	var drawBudget = t.drawBudget? t.drawBudget : t.context.drawBudget;


	m.traversal(targetError, drawBudget);

	var rendered = 0;
	//set visible hidden.
	for(var n = 0; n < m.nodesCount; n++) {
		var mesh = t.meshes[n];
		if(!mesh) continue;

		if(!m.selected[n]) {
			mesh.visible = false;
			continue;
		}
		var skip = true;
		for(var p = m.nfirstpatch[n]; p < m.nfirstpatch[n+1]; p++) {
			var child = m.patches[p*3];
			if(!m.selected[child]) {
				skip = false;
				break;
			}
		}
		if(skip) {
			mesh.visible = false;
			continue;
		}
		//no need to check for visibility.

		mesh.visible = true;
		mesh.geometry.clearGroups();
		var offset = 0;
		var end = 0;
		var last = m.nfirstpatch[n+1]-1;
		for (var p = m.nfirstpatch[n]; p < m.nfirstpatch[n+1]; ++p) {
			var child = m.patches[p*3];

			if(!m.selected[child]) { 
				end = m.patches[p*3+1];
				if(p < last) //if textures we do not join. TODO: should actually check for same texture of last one. 
					continue;
			} 
			if(end > offset) {
				mesh.geometry.addGroup(offset*3, (end - offset)*3, 0); //add material!
//				gl.drawElements(gl.TRIANGLES, (end - offset) * 3, gl.UNSIGNED_SHORT, offset * 6);
				rendered += end - offset;
			}
			offset = m.patches[p*3+1];
		} 
	}
//	console.log(rendered);
}

});
