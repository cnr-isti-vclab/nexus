#include "plyloader.h"
#include "meshloader.h"
#include "mesh.h"
#include <math.h>
using namespace vcg;
using namespace vcg::ply;

#include <iostream>
using namespace std;

namespace fs = std::filesystem;

namespace nx {
//Ugly ply stuff from vcg.

struct PlyFace {
	uint32_t f[3];
	float t[6];
	uint32_t texNumber;
	unsigned char n;
};

struct PlyVertex {
	double dv[3];
	float v[3];
	float t[2]; //texture
	float n[3];
	unsigned char c[4]; //colors
};

//TODO add uv?
PropDescriptor plyprop1[15]= {
	{"vertex", "x",     T_FLOAT, T_FLOAT, offsetof(PlyVertex,v[0]),0,0,0,0,0,0},
	{"vertex", "y",     T_FLOAT, T_FLOAT, offsetof(PlyVertex,v[1]),0,0,0,0,0,0},
	{"vertex", "z",     T_FLOAT, T_FLOAT, offsetof(PlyVertex,v[2]),0,0,0,0,0,0},
	{"vertex", "red"  , T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[0]),0,0,0,0,0,0},
	{"vertex", "green", T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[1]),0,0,0,0,0,0},
	{"vertex", "blue" , T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[2]),0,0,0,0,0,0},
	{"vertex", "alpha", T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[3]),0,0,0,0,0,0},
	{"vertex", "nx",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, n[0]),0,0,0,0,0,0},
	{"vertex", "ny",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, n[1]),0,0,0,0,0,0},
	{"vertex", "nz",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, n[2]),0,0,0,0,0,0},
	{"vertex", "diffuse_red",   T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[0]),0,0,0,0,0,0},
	{"vertex", "diffuse_green", T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[1]),0,0,0,0,0,0},
	{"vertex", "diffuse_blue" , T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[2]),0,0,0,0,0,0},
	{"vertex", "s",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, t[0]),0,0,0,0,0,0},
	{"vertex", "t",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, t[1]),0,0,0,0,0,0}
};
PropDescriptor doublecoords[3] = {
	{"vertex", "x",     T_DOUBLE, T_DOUBLE, offsetof(PlyVertex,dv[0]),0,0,0,0,0,0},
	{"vertex", "y",     T_DOUBLE, T_DOUBLE, offsetof(PlyVertex,dv[1]),0,0,0,0,0,0},
	{"vertex", "z",     T_DOUBLE, T_DOUBLE, offsetof(PlyVertex,dv[2]),0,0,0,0,0,0}
};

PropDescriptor vindices[2]=	{
	{"face", "vertex_indices",T_INT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor vindices_uint[1]=	{
	{"face", "vertex_indices",T_UINT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor vindices_ushort[1] = {
	{"face", "vertex_indices",T_USHORT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor vindex[1]=	{
	{"face", "vertex_index",T_INT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor vindex_uint[1]=	{
	{"face", "vertex_index",T_UINT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor vindex_ushort[1]=	{
	{"face", "vertex_index",T_USHORT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};



PropDescriptor plyprop4[1]=	{
	{"face", "texcoord",T_FLOAT,T_FLOAT,offsetof(PlyFace,t[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor plyprop5[1]=	{
	{"face", "texnumber",T_INT,T_INT,offsetof(PlyFace, texNumber), 0,0,0,0,0,0}
};

PlyLoader::PlyLoader(const std::string& filename):
	vertices_element(-1),
	faces_element(-1),
	n_vertices(0),
	n_triangles(0) {

	int val = pf.Open(filename.c_str(), PlyFile::MODE_READ);
	if(val == -1) {
		int error = pf.GetError();
		throw std::runtime_error("could not open file " + filename + ". Error: " + std::to_string(error));

	}
	init();
	for(std::string &comment: pf.comments) {
		std::string TFILE = "TEXTUREFILE";


		std::string start = comment.substr(0,TFILE.length());
		for(char &c: comment)
			c = ::toupper(c);

		if( TFILE == start ) {
			std::string bufstr,bufclean;
			bufstr = comment.substr(TFILE.length()+1);
			int n = static_cast<int>(bufstr.length());
			for(int i = 0; i < n; i++)
				if(bufstr[i]!='\t' && bufstr[i]>=32 && bufstr[i]<=126 )	bufclean.push_back(bufstr[i]);

			char buf2[255];
			ply::interpret_texture_name( bufclean.c_str(),filename.c_str(), buf2, 255);
			Material material;
			material.base_color_texture = buf2;
			sanitizeTextureFilepath(material.base_color_texture);
			resolveTexturePath(filename, material.base_color_texture);
			materials.push_back(material);
		}
	}
}

PlyLoader::~PlyLoader() {
	pf.Destroy();
}

void PlyLoader::init() {
	bool has_faces = false;
	for(unsigned int i = 0; i < pf.elements.size(); i++) {

		if(!strcmp(pf.ElemName(i),"vertex")) {
			n_vertices = pf.ElemNumber(i);
			vertices_element = i;

		} else if( !strcmp(pf.ElemName(i),"face") ) {
			n_triangles = pf.ElemNumber(i);
			if (n_triangles) {
				faces_element = i;
				has_faces = true;
			}
		}
	}

	//testing for required vertex fields.
	if(pf.AddToRead(plyprop1[0])==-1 ||
			pf.AddToRead(plyprop1[1])==-1 ||
			pf.AddToRead(plyprop1[2])==-1) {
		
		if(pf.AddToRead(doublecoords[0])==-1 ||
				pf.AddToRead(doublecoords[1])==-1 ||
				pf.AddToRead(doublecoords[2])==-1) {
			throw std::runtime_error("ply file is missing xyz coords");
		} else {
			double_coords = true;
		}
	}

	//these calls will silently fail if no color is present
	int error = pf.AddToRead(plyprop1[3]);
	pf.AddToRead(plyprop1[4]);
	pf.AddToRead(plyprop1[5]);
	pf.AddToRead(plyprop1[6]);

	if(error ==  vcg::ply::E_NOERROR)
		has_colors = true;
	else {
		error = pf.AddToRead(plyprop1[10]);
		pf.AddToRead(plyprop1[11]);
		pf.AddToRead(plyprop1[12]);

		if(error ==  vcg::ply::E_NOERROR)
			has_colors = true;
	}

	error = pf.AddToRead(plyprop1[13]);
	pf.AddToRead(plyprop1[14]);

	if(error ==  vcg::ply::E_NOERROR) {
		has_textures = true;
		has_vertex_tex_coords = true;
	}

	//these calls will fail silently if no normal is present
	if(!has_faces) { //skip normals for triangle mesh
		error = pf.AddToRead(plyprop1[7]);
		pf.AddToRead(plyprop1[8]);
		pf.AddToRead(plyprop1[9]);
		if(error == vcg::ply::E_NOERROR)
			has_normals = true;
	}

	pf.AddToRead(vindex[0]);
	pf.AddToRead(vindex_uint[0]);
	pf.AddToRead(vindex_ushort[0]);
	pf.AddToRead(vindices[0]);
	pf.AddToRead(vindices_uint[0]);
	pf.AddToRead(vindices_ushort[0]);

	//these calls will fail silently if no texture is present
	if (pf.AddToRead(plyprop4[0]) == vcg::ply::E_NOERROR)
		has_textures = true;

	if (pf.AddToRead(plyprop5[0]) == vcg::ply::E_NOERROR) { }

}

void PlyLoader::load(MeshFiles& mesh) {
	mesh.positions.resize(n_vertices);
	if(has_colors)
		mesh.colors.resize(n_vertices);
	mesh.wedges.resize(n_vertices);

	pf.SetCurElement(vertices_element);

	PlyVertex vertex;

	for(size_t i = 0; i < n_vertices; i++) {
		Vector3f &p = mesh.positions[i];
		Wedge &w = mesh.wedges[i];
		w.p = i;
		pf.Read((void *)&vertex);

		if(double_coords) {
			p.x = (float)(vertex.dv[0] - origin.x)*scale.x;
			p.y = (float)(vertex.dv[1] - origin.y)*scale.y;
			p.z = (float)(vertex.dv[2] - origin.z)*scale.z;
		} else {
			p.x = (float)(vertex.v[0] - origin.x)*scale.x;
			p.y = (float)(vertex.v[1] - origin.y)*scale.y;
			p.z = (float)(vertex.v[2] - origin.z)*scale.z;
		}
		if(has_normals) {
			w.n.x = vertex.n[0];
			w.n.y = vertex.n[1];
			w.n.z = vertex.n[2];
		}
		if(has_colors) {
			Rgba8 &c = mesh.colors[i];
			c.r = vertex.c[0];
			c.g = vertex.c[1];
			c.b = vertex.c[2];
			c.a = vertex.c[3];
		}
		if(has_textures) {
			w.t.u = vertex.t[0];
			w.t.v = vertex.t[1];
		}
	}

	pf.SetCurElement(faces_element);
	if(faces_element == -1)
		throw std::string("ply has no triangles!");

	mesh.triangles.resize(n_triangles);
	mesh.material_ids.resize(n_triangles);
	PlyFace face;
	face.texNumber = 0;
	for(size_t i = 0; i < n_triangles; i++) {

		pf.Read((void *) &face);
		Triangle &tri = mesh.triangles[i];

		for(int k = 0; k < 3; k++) {
			int v = face.f[k];
			if(v < 0 || v >= n_vertices)
				throw std::runtime_error("Bad index in triangle list.");

			tri.w[k] = v;
		}
		//TODO: if loading more than one model (because split, we need to reintroduce texOffset
		mesh.material_ids[i] = face.texNumber;
	}
}

}
