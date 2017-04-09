function NexusObject(url, renderer, render, material) {
	var gl = renderer.context;
	var geometry = new THREE.BufferGeometry();
	var positions = new Float32Array(3);


	geometry.addAttribute('position', new THREE.BufferAttribute(positions, 3));
	

	if(!material)
		this.autoMaterial = true;

	THREE.Mesh.call( this, geometry, material);
	this.frustumCulled = false;

	var mesh = this;
	var nexus = this.nexus = new Nexus.Instance(gl);
	nexus.open(url);
	nexus.onLoad = function() {
		var s = 1/nexus.mesh.sphere.radius;
		var pos = nexus.mesh.sphere.center;
		mesh.position.set(-pos[0]*s, -pos[1]*s, -pos[2]*s);
		mesh.scale.set(s, s, s);
		if(mesh.autoMaterial)
			mesh.material = new THREE.MeshLambertMaterial( { color: 0xffffff } );

		if(this.mesh.vertex.normal) {
			var normals = new Float32Array(3);
			geometry.addAttribute( 'normal', new THREE.BufferAttribute(normals, 3));
		}
		if(this.mesh.vertex.color) {
			var colors = new Float32Array(4);
			geometry.addAttribute( 'color', new THREE.BufferAttribute(colors, 4));
			if(mesh.autoMaterial)
				mesh.material = new THREE.MeshLambertMaterial({ vertexColors: THREE.VertexColors });
		}

		if(this.mesh.vertex.texCoord) {
			var uv = new Float32Array(2);
			geometry.addAttribute( 'uv', new THREE.BufferAttribute(uv, 2));
			if(mesh.autoMaterial) {
				var texture = new THREE.DataTexture( new Uint8Array([1, 1, 1]), 1, 1, THREE.RGBFormat );
				texture.needsUpdate = true;
				mesh.material = new THREE.MeshLambertMaterial( { color: 0xffffff, map: texture } );
			}
		}

		if(this.mesh.face.index) {
			var indices = new Uint32Array(3);
			geometry.setIndex(new THREE.BufferAttribute( indices, 3) );
		}
		render();
	};
	nexus.onUpdate = function() { render(); }

	this.onAfterRender = function(renderer, scene, camera, geometry, material, group) { 
		if(!nexus.isReady) return;
		var s = renderer.getSize();
		nexus.updateView([0, 0, s.width, s.height], 
			camera.projectionMatrix.elements, 
			mesh.modelViewMatrix.elements);
		var program = renderer.context.getParameter(gl.CURRENT_PROGRAM);
		nexus.attributes['position'] = renderer.context.getAttribLocation(program, "position");
		nexus.attributes['normal'] = renderer.context.getAttribLocation(program, "normal");
		nexus.attributes['color'] = renderer.context.getAttribLocation(program, "color");
		nexus.attributes['uv'] = renderer.context.getAttribLocation(program, "uv");
		nexus.render();
	}
}

NexusObject.prototype = Object.create(THREE.Mesh.prototype);
