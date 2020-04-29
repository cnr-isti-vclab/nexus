#include "nexusbuilder.h"

using namespace nx;

void saveGLTF(NexusBuilder &builder) {

	//we need first to estimate the bin.
	//then write the json
	//then write the bin.


	//skeleton
	QString r = R"(
{
	"scene": 0,
	"scenes": [ { "nodes": [0] } ],
	"meshes": [
		{
		"primitives": [
			{
			"attributes": {
				"TEXCOORD_0": 0,
				"NORMAL": 1,
				"POSITION": 3
			},
			"indices": 4,
			"material": 0
			}
		],
		"extensions": {
			"NXS_meshLOD": {
				"primitives": [

				]

				"nodes" : [
					{
						"error": 3.5 //geometric error
						"center": [1, 2, 3]
						"radius": 3
						"tightRadius": 2
						"children": [
							{ "primitive": 2, "node": 4 }
						]
					}

				"error": []
				"center": []
				"radius": []
				"tightRadius": []
				"

				{
							   "center": [0, 0, 1, 0, 1.0, 2.0],
							   "radius": [0, 0.1],
							   "tightRadius": [1],
							   "firstEdge": [0, 5, 9]
							   "error": [5123, 266, 2466],
						   }],
						   "edges": [
								{
									"primitive": 0
									"node":
							   "primitive": [0, 1, 2, 3, 4, 5, 6, 7, 19],
							   "node": [1, 3, 5, 23]
						   }
						}
					  }
					}
				  ],

	  "accessors": [
		{
		  "bufferView": 0,
		  "componentType": 5126,
		  "count": 406,
		  "type": "VEC2"
		},
		{
		  "bufferView": 1,
		  "componentType": 5126,
		  "count": 406,
		  "type": "VEC3"
		},
		{
		  "bufferView": 2,
		  "componentType": 5126,
		  "count": 406,
		  "type": "VEC4"
		},
		{
		  "bufferView": 3,
		  "componentType": 5126,
		  "count": 406,
		  "type": "VEC3",
		  "max": [
			0.02128091,
			0.06284806,
			0.0138090011
		  ],
		  "min": [
			-0.02128091,
			-4.773855E-05,
			-0.013809
		  ]
		},
		{
		  "bufferView": 4,
		  "componentType": 5123,
		  "count": 2046,
		  "type": "SCALAR"
		}
	  ],
	  "asset": {
		"generator": "glTF Tools for Unity",
		"version": "2.0"
	  },
	  "bufferViews": [
		{
		  "buffer": 0,
		  "byteLength": 3248
		},
		{
		  "buffer": 0,
		  "byteOffset": 3248,
		  "byteLength": 4872
		},
		{
		  "buffer": 0,
		  "byteOffset": 8120,
		  "byteLength": 6496
		},
		{
		  "buffer": 0,
		  "byteOffset": 14616,
		  "byteLength": 4872
		},
		{
		  "buffer": 0,
		  "byteOffset": 19488,
		  "byteLength": 4092
		}
	  ],
	  "buffers": [
		{
		  "uri": "Avocado.bin",
		  "byteLength": 23580
		}
	  ],
	  "extensionsUsed": [
		"KHR_materials_pbrSpecularGlossiness"
	  ],
	  "images": [
		{
		  "uri": "Avocado_baseColor.png"
		},
		{
		  "uri": "Avocado_roughnessMetallic.png"
		},
		{
		  "uri": "Avocado_normal.png"
		},
		{
		  "uri": "Avocado_diffuse.png"
		},
		{
		  "uri": "Avocado_specularGlossiness.png"
		}
	  ],

	  "materials": [
		{
		  "pbrMetallicRoughness": {
			"baseColorTexture": {
			  "index": 0
			},
			"metallicRoughnessTexture": {
			  "index": 1
			}
		  },
		  "normalTexture": {
			"index": 2
		  },
		  "name": "2256_Avocado_d",
		  "extensions": {
			"KHR_materials_pbrSpecularGlossiness": {
			  "diffuseTexture": {
				"index": 3
			  },
			  "specularGlossinessTexture": {
				"index": 4
			  }
			}
		  }
		}
	  ],
	  "nodes": [
		{
		  "mesh": 0,
		  "rotation": [
			0.0,
			1.0,
			0.0,
			0.0
		  ],
		  "name": "Avocado"
		}
	  ],

	  "textures": [
		{
		  "source": 0
		},
		{
		  "source": 1
		},
		{
		  "source": 2
		},
		{
		  "source": 3
		},
		{
		  "source": 4
		}
	  ]
	}

})";
}

