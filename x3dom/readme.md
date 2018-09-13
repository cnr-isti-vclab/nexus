
#glTF ideas

Nodes to define the hyerarchy? (we don't need the hyararchy!


	"nodes": [
		{
			"mesh": 11
			children[....]
		},

Buffer and bufferviews: we don't need a bin for each object.
we could use the nxs just renamed bin :P

{
	buffers:[
	{
		"byteLength": 35884 //without uri this is a  glb
	}],

	"bufferViews": [
		{
			"buffer": 0,
			"byteLength": 25272,
			"byteOffset": 0,
			"target": 34963
		},
		{
			"buffer": 0,
			"byteLength": 76768,
			"byteOffset": 25272,
			"byteStride": 32,
			"target": 34962
		}
	]
}

Accessors to define the data types:

"accessors": [
		{
			"bufferView": 0,
			"byteOffset": 0,
			"componentType": 5123,
			"count": 12636,
			"max": [
				4212
			],
			"min": [
				0
			],
			"type": "SCALAR"
		},
		}



"meshes": [
		{
			"primitives": [
				{
					"attributes": {
						"NORMAL": 23,
						"POSITION": 22,
						"TANGENT": 24,
						"TEXCOORD_0": 25
					},
					"indices": 21,
					"material": 3,
					"mode": 4
				}
			]
		}
	]



"textures": [
		{
			"sampler": 0,
			"source": 2
		}
	]


 "images": [
		{
			"bufferView": 14,
			"mimeType": "image/jpeg"
		}
	]


"samplers": [
		{
			"magFilter": 9729,
			"minFilter": 9987,
			"wrapS": 10497,
			"wrapT": 10497
		}
	]
