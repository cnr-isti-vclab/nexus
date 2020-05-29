/*
Nexus
Copyright (c) 2012-2020, Visual Computing Lab, ISTI - CNR
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/*import * as THREE from './three.module.js'; */

function nocenter() { throw "Centering and in general applying matrix to geometry is unsupported."; }

function NexusObject(url, onLoad, onUpdate, renderer, material) {
	if(onload !== null && typeof(onLoad) == 'object')
		throw "NexusObject constructor has been changed.";

	let gl = renderer.getContext();
	let geometry = new THREE.BufferGeometry();

	geometry.center = nocenter;

/*
function() { 
                var s = 1/instance.mesh.sphere.radius;
                var pos = instance.mesh.sphere.center;
                mesh.position.set(-pos[0]*s, -pos[1]*s, -pos[2]*s);
                mesh.scale.set(s, s, s); 
};
*/

	let positions = new Float32Array(3);
	geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));


	let createMaterial = (!material);

	this.frustumCulled = false;

	var nexus = this;
	var instance = geometry.instance = new Nexus.Instance(gl);
	instance.open(url);
	instance.onLoad = function() {
		let mesh = instance.mesh;
		var c = mesh.sphere.center;
		var center = new THREE.Vector3(c[0], c[1], c[2]);
		var radius = mesh.sphere.radius;

		geometry.boundingSphere = new THREE.Sphere(center, radius);
		geometry.boundingBox = nexus.computeBoundingBox();


		switch(mesh.version) {
		case 2: nexus.createMaterialsV2(createMaterial); break;
		case 3: nexus.createMaterialsV3(createMaterial); break;
		default: throw "Unsupported nexus version: " + instance.version;
		}


		//this seems not to be needed to setup the attributes and shaders
/*
		if(this.mesh.face.index) {
			var indices = new Uint32Array(3);
			geometry.setIndex(new THREE.BufferAttribute( indices, 3) );
		}
*/
		if(onLoad) onLoad(nexus);
	};
	instance.onUpdate = function() { onUpdate(this) };

	THREE.Mesh.call( this, geometry, this.material);

	this.onAfterRender = onAfterRender;
}

function onAfterRender(renderer, scene, camera, geometry, material, group) {
	var gl = renderer.getContext();
	var instance = geometry.instance;
	if(!instance || !instance.isReady) return;
	let s = new THREE.Vector2();
	renderer.getSize(s);
	instance.updateView([0, 0, s.width, s.height], 
	camera.projectionMatrix.elements, 
	this.modelViewMatrix.elements);
	//TODO: this should be one once per material!

	let program = gl.getParameter(gl.CURRENT_PROGRAM);
	let attr = instance.attributes = [];

	["position", "normal", "color", "uv"].forEach(a => {
 		attr[a] = gl.getAttribLocation(program, a);
	});

	["size", "scale"].forEach(a => {
	    attr[a] = gl.getUniformLocation(program, a);
	});

//	each material has a few maps, we need to test more than one material
//  	problem! we are only using a material (is there a way to change material?)
//  	should we gltf brutally?

	let samplers = instance.samplers;

	["map", "bumpMap", "roughnessMap", "normalMap", "specularMap"].forEach((map) => {
		let location = gl.getUniformLocation(program, map); 
		samplers[map] = location === null? null : gl.getUniform(program, location);
	});

	for(let m in instance.mesh.materials) {
		let mat = instance.mesh.materials[m];
		if(!mat.mapping) {
			mat.mapping = [];
			if(mat.pbrMetallicRoughness) {
				let pbr = mat.pbrMetallicRoughness;
				if(pbr.baseColorTexture && samplers.map >= 0)
					mat.mapping[pbr.baseColorTexture.index] = samplers.map;
				if(pbr.metallicRoughnessTexture && samplers.roughnessMap >= 0)
					mat.mapping[pbr.metallicRoughnessTexture.index] = samplers.roughnessMap;
			}
			if(mat.normalTexture)
				mat.mapping[mat.normalTexture.index] = samplers.normalMap;
			if(mat.bumpTexture)
				mat.mapping[mat.bumpTexture.index] = samplers.bumpMap;
			if(mat.specularTexture)
				mat.mapping[mat.specularTexture.index] = samplers.specularMap;

		}
	}


	//hack to detect if threejs using point or triangle shaders
	instance.mode = attr.size ? "POINT" : "FILL";
	if(attr.size != -1) 
		instance.pointsize = material.size;

	//can't find docs or code on how material.scale is computed in threejs.
	if(attr.scale != -1)
		instance.pointscale = 2.0;

	instance.render();
	Nexus.updateCache(gl);
}


NexusObject.prototype = Object.create(THREE.Mesh.prototype);

NexusObject.prototype.dispose = function() {
	var instance = this.geometry.instance;
	var context = instance.context;
	var mesh = instance.mesh;
	Nexus.flush(context, mesh);
	for( var i = 0; i < context.meshes.length; i++) {
		if ( context.meshes[i] === mesh) {
			context.meshes.splice(i, 1); 
			break;
		}
	}
	this.geometry.instance = null;
	this.geometry.dispose();
}

NexusObject.prototype.georef = function(url) {
	var n = this;
	var obj = new XMLHttpRequest();
	obj.overrideMimeType("application/json");
	obj.open('GET', url, true);
	obj.onreadystatechange = function () {
		if (obj.readyState == 4 && obj.status == "200") {
			this.georef = JSON.parse(obj.responseText);
			var o = this.georef.origin;
			n.position.set(o[0], o[1], o[2]);
		}
	};
	obj.send(null);  
}

NexusObject.prototype.createMaterialsV2 = function(createMaterial) {
	let geometry = this.geometry;
	let mesh = geometry.instance.mesh;

	let options = {};

	if(mesh.vertex.COLOR_0 && mesh.vertex.UV_0) {
		let uv = new Float32Array(2);
		let colors = new Float32Array(4);
		geometry.setAttribute( 'uv', new THREE.BufferAttribute(uv, 2));
		geometry.setAttribute( 'color', new THREE.BufferAttribute(colors, 4));
		if(mesh.material) {
			let texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
			texture.needsUpdate = true;
			mesh.material = new THREE.MeshLambertMaterial( { vertexColors: THREE.VertexColors, map: texture } );
		}
	}

	if(mesh.vertex.COLOR_0) {
		let colors = new Float32Array(4);
		geometry.setAttribute( 'color', new THREE.BufferAttribute(colors, 4));
		options.vertexColors = THREE.VertexColors;
	}

	if(mesh.vertex.UV_0) {
		let uv = new Float32Array(2);
		geometry.setAttribute( 'uv', new THREE.BufferAttribute(uv, 2));
		var texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
		texture.needsUpdate = true;
		options.map = texture;
	}

	if(mesh.vertex.NORMAL) {
		var normals = new Float32Array([1, 1, 1]);
		geometry.setAttribute( 'normal', new THREE.BufferAttribute(normals, 3));
	}

	if(createMaterial)
		this.material = new THREE.MeshStandardMaterial(options);
		
}

NexusObject.prototype.createMaterialsV3 = function(createMaterial) {
	let geometry = this.geometry;
	let mesh = geometry.instance.mesh;


	let m = mesh.materials[0];

	let type = 'standard';
	let options = {};
	if(m.pbrMetallicRoughness) {
		let pbr = m.pbrMetallicRoughness
		if(pbr.baseColorTexture) {
			let texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
			texture.needsUpdate = true;
			options.map = texture;
		}

		if(mesh.vertex.COLOR_0) {
			let colors = new Float32Array(4);
			geometry.setAttribute( 'color', new THREE.BufferAttribute(colors, 4));
			options.vertexColors = THREE.VertexColors;

		} else if(pbr.baseColorFactor)
			options.color = new THREE.Color(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2]);
		else
			options.color =  new THREE.Color(1.0, 1.0, 1.0);


		if(pbr.metallicRoughnessTexture) {
			let texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
			texture.needsUpdate = true;
			options.roughnessMap = options.metalnessMap = texture;
		}
	}

	if(m.bumpTexture) {
		let texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
		texture.needsUpdate = true;
		options.bumpMap = texture;
		options.bumpScale = 0.01;
	}

	if(m.normalTexture) {
		let texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
		texture.needsUpdate = true;
		options.normalMap = texture;
	}

	if(m.glossinessFactor) {
		type = 'phong';
		options.shininess = m.glossinessFactor;
	}
	if(m.specularTexture) {
		type = 'phong';
		let texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
		texture.needsUpdate = true;
		options.specularMap = texture;
//		options.specular = new THREE.Color(1.0, 1.0, 1.0);
	}
	if(m.glossiness) {
		options.shininess = m.glossiness;
	}

	if(mesh.vertex.NORMAL) {
		var normals = new Float32Array([1, 1, 1]);
		geometry.setAttribute( 'normal', new THREE.BufferAttribute(normals, 3));
	} else {
		options.flatShading = true;
	}

//TODO test these options
//	options.vertexTangents = true;
//	options.wireframe = true;


	if(createMaterial) {
		switch(type) {
			case 'standard': this.material = new THREE.MeshStandardMaterial(options); break;
			case 'phong':    this.material = new THREE.MeshPhongMaterial(options);    break;
			deafault: break;
		}
	}
}


NexusObject.prototype.computeBoundingBox = function() {
	var instance = this.geometry.instance;
	var nexus = instance.mesh;
	if(!nexus.sphere) return;


	var min = new THREE.Vector3( + Infinity, + Infinity, + Infinity );
	var max = new THREE.Vector3( - Infinity, - Infinity, - Infinity );

	var array = new Float32Array(nexus.sink-1);
	//check last level of spheres
	var count = 0;
	for(var i = 0; i < nexus.sink; i++) {
		var patch = nexus.nfirstpatch[i];
		if(nexus.patches[patch*4] != nexus.sink)
			continue;
		var x = nexus.nspheres[i*5];
		var y = nexus.nspheres[i*5+1];
		var z = nexus.nspheres[i*5+2];
		var r = nexus.nspheres[i*5+4]; //tight radius
		if(x-r < min.x) min.x = x-r;
		if(y-r < min.y) min.y = y-r;
		if(z-r < min.z) min.z = z-r;
		if(x-r > max.x) max.x = x+r;
		if(y-r > max.y) max.y = y+r;
		if(z-r > max.z) max.z = z+r;

	}
	return new THREE.Box3(min, max);
}


NexusObject.prototype.raycast = function(raycaster, intersects) {
	if(!this.geometry.instance) return;

	var nexus = this.geometry.instance.mesh;
	if(!nexus.sphere) return;

	var sp = nexus.sphere;
	var c = sp.center;
	var center = new THREE.Vector3(c[0], c[1], c[2]);
	var sphere = new THREE.Sphere(center, sp.radius);
	var m = new THREE.Matrix4();
	m.getInverse(this.matrixWorld);
	var ray = new THREE.Ray();
	ray.copy(raycaster.ray).applyMatrix4(m);

	var point = new THREE.Vector3(0, 0, 0);
	var distance = -1.0;
	var intersect = raycaster.ray.intersectSphere( sphere );
	if(!intersect)
		return;

	if(!nexus.sink || !nexus.basei) {
		//no mesh loaded, we can still use the sphere.
		intersect.applyMatrix4(this.matrixWorld);
		var d = intersect.distanceTo(raycaster.ray.origin);
		if(d < raycaster.near || d > raycaster.far )  
			distance = d;

	} else {

		var vert = nexus.basev;
		var face = nexus.basei;

		for(var j = 0; j < nexus.basei.length; j += 3) {
			var a = face[j];
			var b = face[j+1];
			var c = face[j+2];
			var A = new THREE.Vector3(vert[a*3], vert[a*3+1], vert[a*3+2]);
			var B = new THREE.Vector3(vert[b*3], vert[b*3+1], vert[b*3+2]);
			var C = new THREE.Vector3(vert[c*3], vert[c*3+1], vert[c*3+2]);
			//TODO use material to determine if using doubleface or not!
			var hit  = ray.intersectTriangle( C, B, A, false, point ); 
			if(!hit) continue;

			//check distances in world space
			intersect.applyMatrix4(this.matrixWorld);
			var d = intersect.distanceTo(raycaster.ray.origin);
			if(d < raycaster.near || d > raycaster.far ) continue;
			if(distance == -1.0 || d < distance) {
				distance = d;
				intersect = hit;
			}
		}
	}

	if(distance == -1.0) return;
	intersects.push({ distance: distance, point: intersect, object: this} );
	return;

/* Kept for reference, should we want to implement a raycasting on the higher resolution nodes 

	var distance = -1.0;
	for(var i = 0; i < nexus.sink; i++) {
		var patch = nexus.nfirstpatch[i];
		if(nexus.patches[patch*3] != nexus.sink)
			continue;
		var x = nexus.nspheres[i*5];
		var y = nexus.nspheres[i*5+1];
		var z = nexus.nspheres[i*5+2];
		var r = nexus.nspheres[i*5+4]; //tight radius
		var sphere = new THREE.Sphere(new THREE.Vector3(x, y, z), r);

		if (ray.intersectsSphere( sphere ) != false ) {
			var d = sphere.center.lengthSq();
			if(distance == -1.0 || d < distance)
				distance = d;
		}
	}
	if(distance == -1.0) return;

	intersects.push({ distance: distance, object: this} ); */
}

/*export { NexusObject }; */

