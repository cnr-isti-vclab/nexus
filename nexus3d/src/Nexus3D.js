import {
	Mesh as THREEMesh,
	MeshStandardMaterial,
	MeshPhongMaterial,
	Color,
	Object3D,
	Vector3,
	Vector4,
	Sphere,
	BufferGeometry,
	BufferAttribute,
	DataTexture,
	RGBAFormat,
	LinearSRGBColorSpace
} from 'three'
import { Traversal } from './Traversal.js'
import { Mesh } from './Mesh.js'
import { Cache } from './Cache.js'

class Nexus3D extends THREEMesh {

	constructor(url, renderer, options = {}) {

		super();

		this.patchWebGLRenderer(renderer);

		Object.assign(this, {
			isNXS: true,
			type:'NXS',
			url: url,
			gl: renderer.getContext(),
			XRMode: false, //set to true to avoid flickering due to double eye rendering.
			material: null,
			customMaterial: false, // Track if user provided custom material

			autoUpdate: true,
			mesh: new Mesh(),
			vbo: [],
			ibo: [],
			vao: [],
			textures: [],
			attributes: {},  //here we store the uniform attributes of the shader.

			basemesh: null,  //highest level of the nexus, for picking
		});

		if('material' in options) {
			this.material = options.material;
			this.customMaterial = true;
		} else {
			// Create default material - will be replaced by createMaterialFromMesh() after load
			this.material = new MeshStandardMaterial();
		}
			
		for(let call of ['onLoad', 'onUpdate', 'onProgress']) {
			this['_' + call] = [];
			if(call in options)
				this['_' + call].push(options[call])
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
		Object3D.prototype.copy.call(this, source, false);
		throw new Error("Can't really copy a Nexus3D object.");
		return this;
	}

	
	open(url) {
		let t = this;
		this.mesh.open(url);
		this.mesh.createNode         = (id)           => { };
		this.mesh.createNodeGeometry = (id, geometry) => { t.createNodeGeometry(id, geometry); };
		this.mesh.createTexture      = (id, image, texindex) => { t.createTexture(id, image, texindex); };
		this.mesh.deleteNodeGeometry = (id)           => { t.deleteNodeGeometry(id); };
		this.mesh.deleteTexture      = (id)           => { t.deleteTexture(id); };
		this.mesh.onLoad.push(() => { t.onLoadCallback(); });
		this.mesh.onUpdate.push(() => { 
			for(let callback of t._onUpdate) callback(this); 
			for(let callback of t._onProgress) callback(this, this.mesh.availableNodes, this.mesh.n_nodes); 
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
	createMaterialFromMesh() {
		if(this.customMaterial)
			return; // User provided custom material, don't override

		let m = this.mesh;
		let options = {};

		// V3: Use PBR material properties
		if(m.version === 3 && m.materials && m.materials[0]) {
			let mat = m.materials[0];
			let materialType = 'standard';

			if(mat.pbrMetallicRoughness) {
				let pbr = mat.pbrMetallicRoughness;
				
				// Don't create placeholder textures yet - let them be null
				// They'll be set when actual textures load

				if(m.vertex.COLOR_0) {
					options.vertexColors = true;
				} else if(pbr.baseColorFactor) {
					options.color = new Color(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2]);
				} else {
					options.color = new Color(1.0, 1.0, 1.0);
				}

				// Store which textures should exist for later
				this.expectedTextures = {};
				if(pbr.baseColorTexture && m.vertex.UV_0) {
					this.expectedTextures.map = true;
				}
				if(pbr.metallicRoughnessTexture && m.vertex.UV_0) {
					this.expectedTextures.roughnessMap = true;
					this.expectedTextures.metalnessMap = true;
				}
			}

			if(mat.normalTexture && m.vertex.UV_0) {
				this.expectedTextures = this.expectedTextures || {};
				this.expectedTextures.normalMap = true;
			}

			if(mat.bumpTexture && m.vertex.UV_0) {
				this.expectedTextures = this.expectedTextures || {};
				this.expectedTextures.bumpMap = true;
			}

			if(mat.specularTexture && m.vertex.UV_0) {
				materialType = 'phong';
				this.expectedTextures = this.expectedTextures || {};
				this.expectedTextures.specularMap = true;
			}

			if(mat.glossinessFactor) {
				materialType = 'phong';
				options.shininess = mat.glossinessFactor;
			}

			if(mat.glossiness) {
				options.shininess = mat.glossiness;
			}

			if(m.vertex.NORMAL) {
				// Normal attribute available
			} else {
				options.flatShading = true;
			}

			// Create material based on type
			if(materialType === 'phong') {
				this.material = new MeshPhongMaterial(options);
			} else {
				this.material = new MeshStandardMaterial(options);
			}
		} else {
			// V2: Simple material
			if(m.vertex.COLOR_0) {
				options.vertexColors = true;
			}

			// Store which textures should exist for later
			if(m.vertex.UV_0) {
				this.expectedTextures = { map: true };
			}

			this.material = new MeshStandardMaterial(options);
		}
	}

	updateMaterials() {
		if(!this.material)
			return;

		if(this.material.map && this.material_texture)
			this.material.map = this.material_texture;

if(this.mesh.vertex.COLOR_0)
			this.material.vertexColors = true; 
		this.material.needsUpdate = true; 
	}

	onLoadCallback() {
		const c = this.mesh.sphere.center;
		const center = new Vector3(c[0], c[1], c[2]);
		const radius = this.mesh.sphere.radius;
		this.boundingSphere = new Sphere(center, radius);

		var geometry = new BufferGeometry();

		geometry.setAttribute( 'position', new BufferAttribute(new Float32Array(3), 3));
		
		if(this.mesh.vertex.NORMAL)
			geometry.setAttribute( 'normal', new BufferAttribute(new Float32Array(3), 3));
		if(this.mesh.vertex.COLOR_0)
			geometry.setAttribute( 'color', new BufferAttribute(new Float32Array(4), 4));
		if(this.mesh.vertex.UV_0)
			geometry.setAttribute( 'uv', new BufferAttribute(new Float32Array(2), 2));

		// Create material from mesh data if not provided by user
		this.createMaterialFromMesh();

		if(this.mesh.vertex.UV_0) {
			this.material_texture = new DataTexture( new Uint8Array([1, 1, 1, 1]), 1, 1, RGBAFormat );
			this.material_texture.needsUpdate = true;
		}

		this.updateMaterials();
		this.geometry = geometry;
		
		this.frustumCulled = false;
			
		for(let callback of this._onLoad)
			callback(this);
	}

	renderBufferDirect(renderer, scene, camera, geometry, material, group) {
		let s = new Vector4();
		renderer.getViewport(s);

	   	//object modelview is multiplied by camera during rendering, we need to do it here for visibility computations
		this.modelViewMatrix.multiplyMatrices( camera.matrixWorldInverse, this.matrixWorld );
		if(s.x == 0 || !this.XRMode) { //hack to only traverse on the left eye
			this.traversal.updateView(s, camera.projectionMatrix.elements, this.modelViewMatrix.elements);
			this.instance_errors = this.traversal.traverse(this.mesh, this.cache);
		}
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

		// Detect multiple texture samplers for PBR material support
		// Reset whenever the compiled program changes (e.g. after material.needsUpdate recompile)
		if(!this.samplers || this._lastProgram !== program) {
			this._lastProgram = program;
			this.samplers = {};
			["map", "bumpMap", "roughnessMap", "metalnessMap", "normalMap", "specularMap"].forEach((mapName) => {
				let location = gl.getUniformLocation(program, mapName);
				this.samplers[mapName] = location !== null ? gl.getUniform(program, location) : null;
			});
			// Also reset material mappings so they are rebuilt with the new sampler units
			if(this.mesh.materials)
				for(let mat of this.mesh.materials)
					mat.mapping = null;
		}
		
		// Build material mapping arrays from PBR properties (lazy initialization)
		// Each texture can map to multiple samplers (e.g., metallicRoughnessTexture -> both roughnessMap and metalnessMap)
		if(this.mesh.materials) {
			for(let mat of this.mesh.materials) {
				if(!mat.mapping) {
					mat.mapping = [];
					if(mat.pbrMetallicRoughness) {
						let pbr = mat.pbrMetallicRoughness;
						if(pbr.baseColorTexture && this.samplers.map !== null)
							mat.mapping[pbr.baseColorTexture.index] = [this.samplers.map];
						if(pbr.metallicRoughnessTexture) {
							// Single texture contains both roughness and metalness channels
							let samplers = [];
							if(this.samplers.roughnessMap !== null) samplers.push(this.samplers.roughnessMap);
							if(this.samplers.metalnessMap !== null) samplers.push(this.samplers.metalnessMap);
							if(samplers.length > 0)
								mat.mapping[pbr.metallicRoughnessTexture.index] = samplers;
						}
					}
					if(mat.normalTexture && this.samplers.normalMap !== null)
						mat.mapping[mat.normalTexture.index] = [this.samplers.normalMap];
					if(mat.bumpTexture && this.samplers.bumpMap !== null)
						mat.mapping[mat.bumpTexture.index] = [this.samplers.bumpMap];
					if(mat.specularTexture && this.samplers.specularMap !== null)
						mat.mapping[mat.specularTexture.index] = [this.samplers.specularMap];
				}
			}
		}
	
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
			
		for(let id = 0; id < m.n_nodes; id++) {
//            let err = m.nerrors[id];

			if(!t.selected[id]) continue;

			//check for children: if all are selected, bail out.
			{
				let visible = false;
				let offset = 0;
				let end = 0;
				let last = m.nfirstpatch[id+1]-1;
				for (var p = m.nfirstpatch[id]; p < m.nfirstpatch[id+1]; ++p) {
					var child = m.patches[p*4];
	
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

				if(mesh.vertex.UV_0) {
					if(attr.uv >= 0) {
						gl.vertexAttribPointer(attr.uv, 2, gl.FLOAT, false, 8, offset);
						gl.enableVertexAttribArray(attr.uv);
					}
					offset += nv*8;
				}
				if(mesh.vertex.COLOR_0) {
					if(attr.color >= 0) {
						gl.vertexAttribPointer(attr.color, 4, gl.UNSIGNED_BYTE, true, 4, offset);
						gl.enableVertexAttribArray(attr.color);
					}
					offset += nv*4;
				}
				if(mesh.vertex.NORMAL) {
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
				let child = m.patches[p*4];
	
				if(!t.selected[child]) {
					end = m.patches[p*4+1];
					if(p < last) //we join patches if possible.
						continue;
				}

				if(end > offset) {
					if(m.vertex.UV_0 && attr.uv >= 0) {
						var texgroupid = m.patches[p*4+2];
						if(texgroupid != -1 && this.textures[texgroupid]) { //bind texture
							// Get material from patch and look up definition
							let matIndex = m.patches[p*4 + 3];
							let mat = m.materials[matIndex];
							
							if(mat && mat.mapping) {
								// Bind textures according to material mapping
								// mapping array: index = texture index in group, value = array of sampler units
								mat.mapping.forEach((samplers, texIndex) => {
									if(this.textures[texgroupid][texIndex]) {
										// Bind the same texture to multiple samplers (e.g., metallicRoughnessTexture)
										for(let sampler of samplers) {
											gl.activeTexture(gl.TEXTURE0 + sampler);
											gl.bindTexture(gl.TEXTURE_2D, this.textures[texgroupid][texIndex]);
										}
									}
								});
							} else {
								// Fallback: bind first texture to sampler 0
								let texid = this.textures[texgroupid][0];
								if(texid) {
									let samplerUnit = this.samplers && this.samplers.map !== null ? this.samplers.map : 0;
									gl.activeTexture(gl.TEXTURE0 + samplerUnit);
									gl.bindTexture(gl.TEXTURE_2D, texid);
								}
							}
						}
					}
					let mode = this.material.wireframe ? gl.LINE_STRIP : gl.TRIANGLES;

					gl.drawElements(mode, (end - offset) * 3, gl.UNSIGNED_SHORT, offset * 6);
					rendered += end - offset;
				}
				offset = m.patches[p*4+1];
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
		if(m.vertex.UV_0) {
			var uv = new Float32Array(vertices, off, nv*2);
			uv.set(data.uv);
			off += nv*8;
		}
		if(m.vertex.COLOR_0) {
			var color = new Uint8Array(vertices, off, nv*4);
			color.set(data.color);
			off += nv*4;
		}
		if(m.vertex.NORMAL) {
			var normal = new Int16Array(vertices, off, nv*3);
			normal.set(data.normal);
			off += nv*6;
		}
		
		//needed for approximate picking.
		if(id < this.mesh.nroots) {
			let basegeometry = new BufferGeometry();
			basegeometry.setAttribute( 'position', new BufferAttribute(data.position, 3 ) );
			basegeometry.setAttribute( 'normal', new BufferAttribute(data.normal, 3 ) );
			basegeometry.setIndex(new BufferAttribute( data.index, 1 ) );

			this.basemesh = new THREEMesh(basegeometry, this.material);
			this.basemesh.visible = false;
			this.add(this.basemesh);
		}
		
		var gl = this.gl
		this.vao[id] = {}; //one for each attrib combination in use (casting shadows for example).
		gl.bindVertexArray(null);
		
		var vbo = this.vbo[id] = gl.createBuffer();
		gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
		gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
		var ibo = this.ibo[id] = gl.createBuffer();
		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
		gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
	}

	createTexture(id, image, texindex = 0) {
		let gl = this.gl;
		var flip = gl.getParameter(gl.UNPACK_FLIP_Y_WEBGL);
		gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
		
		// Initialize texture array if needed
		if(!this.textures[id])
			this.textures[id] = [];
		
		let tex = this.textures[id][texindex] = gl.createTexture();
		gl.bindTexture(gl.TEXTURE_2D, tex);

		//TODO some textures might be alpha only! save space
		let internalFormat;
		if (this.material && this.material.map && this.material.map.colorSpace === LinearSRGBColorSpace) {
			internalFormat = gl.RGBA;
		} else if (typeof gl.SRGB8_ALPHA8 !== 'undefined') {
			internalFormat = gl.SRGB8_ALPHA8;
		} else {
			internalFormat = gl.RGBA;
		}
		gl.texImage2D(gl.TEXTURE_2D, 0, internalFormat, gl.RGBA, gl.UNSIGNED_BYTE, image);
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
		
		// Update Three.js material with first texture to trigger shader compilation
		if(!this.materialTexturesSet && texindex === 0 && this.expectedTextures) {
			// Create Three.js DataTexture placeholders to trigger shader compilation
			// with the correct sampler uniforms. Each map type needs a neutral placeholder.
			const whiteTex    = new DataTexture(new Uint8Array([255, 255, 255, 255]), 1, 1, RGBAFormat);
			// Neutral tangent-space normal: (128,128,255) decodes to (0,0,1) — no perturbation.
			const neutralNorm = new DataTexture(new Uint8Array([128, 128, 255, 255]), 1, 1, RGBAFormat);
			whiteTex.needsUpdate    = true;
			neutralNorm.needsUpdate = true;
			
			if(this.expectedTextures.map) {
				this.material.map = whiteTex;
			}
			if(this.expectedTextures.roughnessMap) {
				this.material.roughnessMap = whiteTex;
			}
			if(this.expectedTextures.metalnessMap) {
				this.material.metalnessMap = whiteTex;
			}
			if(this.expectedTextures.normalMap) {
				this.material.normalMap = neutralNorm;
			}
			if(this.expectedTextures.bumpMap) {
				this.material.bumpMap = whiteTex;
			}
			if(this.expectedTextures.specularMap) {
				this.material.specularMap = whiteTex;
			}
			
			this.material.needsUpdate = true; // Force shader recompilation
			this.materialTexturesSet = true;
		}
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

		// Delete all textures in the array (for multi-texture support)
		if(Array.isArray(this.textures[tex])) {
			for(let texid of this.textures[tex]) {
				if(texid)
					this.gl.deleteTexture(texid);
			}
		} else {
			this.gl.deleteTexture(this.textures[tex]);
		}
		this.textures[tex] = null;
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
		throw new Error("Can't convert to json.");
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
		}
		renderer.nexusPatched = true;
	}        

}




export { Nexus3D, Cache };
