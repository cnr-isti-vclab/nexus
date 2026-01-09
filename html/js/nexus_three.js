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

function nocenter() { throw "Centering and in general applying matrix to geometry is unsupported."; }

class NexusObject extends THREE.Mesh {
constructor(url, onLoad, onUpdate, renderer, material) {

	var geometry = new THREE.BufferGeometry();
	geometry.center = nocenter;
	var positions = new Float32Array(3);
	geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));

	super(geometry, material); //THREE.Mesh.call( this, geometry, material);
	
	if(onload !== null && typeof(onLoad) == 'object')
		throw "NexusObject constructor has been changed.";

	var gl = renderer.getContext();

/*
function() { 
                var s = 1/instance.mesh.sphere.radius;
                var pos = instance.mesh.sphere.center;
                mesh.position.set(-pos[0]*s, -pos[1]*s, -pos[2]*s);
                mesh.scale.set(s, s, s); 
};
*/


	if(!material)
		this.autoMaterial = true;


	this.frustumCulled = false;

	var mesh = this;
	var instance = this.geometry.instance = new Nexus.Instance(gl);
	instance.open(url);
	instance.onLoad = function() {
		var c = instance.mesh.sphere.center;
		var center = new THREE.Vector3(c[0], c[1], c[2]);
		var radius = instance.mesh.sphere.radius;

		geometry.boundingSphere = new THREE.Sphere(center, radius);
		geometry.boundingBox = mesh.computeBoundingBox();

		if(mesh.autoMaterial)
			mesh.material = new THREE.MeshLambertMaterial( { color: 0xffffff } );

		if(this.mesh.vertex.normal) {
			var normals = new Float32Array(3);
			geometry.setAttribute( 'normal', new THREE.BufferAttribute(normals, 3));
		}
		let materialType = this.mesh.vertex.normal? THREE.MeshLambertMaterial : THREE.MeshBasicMaterial;

		if(this.mesh.vertex.color && this.mesh.vertex.texCoord) {
			var uv = new Float32Array(2);
			var colors = new Float32Array(4);
			geometry.setAttribute( 'uv', new THREE.BufferAttribute(uv, 2));
			geometry.setAttribute( 'color', new THREE.BufferAttribute(colors, 4));
			if(mesh.autoMaterial) {
				var texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
				texture.needsUpdate = true;
				mesh.material = new materialType( { vertexColors: THREE.VertexColors, map: texture } );
			}
		}
		else if(this.mesh.vertex.color) {
			var colors = new Float32Array(4);
			geometry.setAttribute( 'color', new THREE.BufferAttribute(colors, 4));
			if(mesh.autoMaterial)
				mesh.material = new materialType({ vertexColors: THREE.VertexColors });
		}
		else if(this.mesh.vertex.texCoord) {
			var uv = new Float32Array(2);
			geometry.setAttribute( 'uv', new THREE.BufferAttribute(uv, 2));
			if(mesh.autoMaterial) {
				var texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
				texture.needsUpdate = true;
				mesh.material = new materialType( { color: 0xffffff, map: texture } );
			}
		}

		//this seems not to be needed to setup the attributes and shaders
/*
		if(this.mesh.face.index) {
			var indices = new Uint32Array(3);
			geometry.setIndex(new THREE.BufferAttribute( indices, 3) );
		}
*/
		if(onLoad) onLoad(mesh);
	};
	instance.onUpdate = function() { onUpdate(this) };

	this.onAfterRender = onAfterRender;
}
}

function onAfterRender(renderer, scene, camera, geometry, material, group) {
	var gl = renderer.getContext();
	var instance = geometry.instance;
	if(!instance || !instance.isReady) return;
	var s = new THREE.Vector2();
	renderer.getSize(s);
	instance.updateView([0, 0, s.width, s.height], 
	camera.projectionMatrix.elements, 
	this.modelViewMatrix.elements);

	var program = gl.getParameter(gl.CURRENT_PROGRAM);


	var attr = instance.attributes;
	attr.position = gl.getAttribLocation(program, "position");
	attr.normal   = gl.getAttribLocation(program, "normal");
	attr.color    = gl.getAttribLocation(program, "color");
	attr.uv       = gl.getAttribLocation(program, "uv");
	attr.size     = gl.getUniformLocation(program, "size");
	attr.scale    = gl.getUniformLocation(program, "scale");
	let map_location = gl.getUniformLocation(program, "map")
	attr.map      = map_location ? gl.getUniform(program, map_location) : null;


	//hack to detect if threejs using point or triangle shaders
	if(instance.mesh.face.index)
		instance.mode = material.isPointsMaterial? "POINT" : "FILL";

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

NexusObject.prototype.flush = function() {
	var instance = this.geometry.instance;
	var context = instance.context;
	var mesh = instance.mesh;
	Nexus.flush(context, mesh);
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
		if(nexus.patches[patch*3] != nexus.sink)
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

	var c = nexus.sphere.center;
	var r = nexus.sphere.radius;
	var center = new THREE.Vector3(c[0], c[1], c[2]);
	var sphere = new THREE.Sphere(center, r);
	sphere.applyMatrix4(this.matrixWorld);
	var m = new THREE.Matrix4().copy(this.matrixWorld).invert();
	var ray = new THREE.Ray();
	ray.copy(raycaster.ray).applyMatrix4(m);

	var point = new THREE.Vector3(0, 0, 0);
	var distance = -1.0;
	var intersect = raycaster.ray.intersectSphere( sphere, point );
	if(!intersect)
		return;

	let face = {};
	if(!nexus.sink || !nexus.basei) {
		return;
		//no mesh loaded, we can still use the sphere.
		intersect.applyMatrix4(this.matrixWorld);
		let d = intersect.distanceTo(raycaster.ray.origin);
		if(d < raycaster.near || d > raycaster.far )  
			distance = d;
	} else {
		let vert = nexus.basev;
		let tri = nexus.basei;

		let A = new THREE.Vector3(0, 0, 0);
		let B = new THREE.Vector3(0, 0, 0);
		let C = new THREE.Vector3(0, 0, 0);
		for(let j = 0; j < nexus.basei.length; j += 3) {
			let a = tri[j];
			let b = tri[j+1];
			let c = tri[j+2];
			A.set(vert[a*3], vert[a*3+1], vert[a*3+2]);
			B.set(vert[b*3], vert[b*3+1], vert[b*3+2]);
			C.set(vert[c*3], vert[c*3+1], vert[c*3+2]);
			//TODO use material to determine if using doubleface or not!
			let hit = ray.intersectTriangle( C, B, A, false, point ); 
			if(!hit) continue;


			//check distances in world space
			hit.applyMatrix4(this.matrixWorld);
			let d = hit.distanceTo(raycaster.ray.origin);
			if(d < raycaster.near || d > raycaster.far ) continue;
			if(distance == -1.0 || d < distance) {
				distance = d;
				intersect = hit.clone();

				face = { a: a, b: b, c: c,
					normal: new THREE.Vector3(),
				};
				THREE.Triangle.getNormal( A, B, C, face.normal );
			}
		}
	}

	if(distance == -1.0) return;
	intersects.push({ distance: distance, point: intersect, face: face, object: this} );
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
