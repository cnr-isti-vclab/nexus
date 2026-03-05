#include "plyloader.h"
#include "meshloader.h"
#include "mappedmesh.h"
#include <math.h>
using namespace vcg;
using namespace vcg::ply;

#include <iostream>
#include <algorithm>
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
	for(const std::string& raw_comment: pf.comments) {
		const std::string keyword = "TEXTUREFILE";
		if(raw_comment.size() < keyword.size())
			continue;

		std::string prefix = raw_comment.substr(0, keyword.size());
		std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
		if(prefix != keyword)
			continue;

		std::size_t start = keyword.size();
		while(start < raw_comment.size() && (raw_comment[start] == ' ' || raw_comment[start] == '\t' || raw_comment[start] == ':' || raw_comment[start] == '='))
			++start;

		std::string texture_path = raw_comment.substr(start);
		sanitizeTextureFilepath(texture_path);
		if(texture_path.empty())
			continue;

		char interpreted_path[255];
		ply::interpret_texture_name(texture_path.c_str(), filename.c_str(), interpreted_path, 255);

		Material material;
		material.base_color_texture = interpreted_path;
		sanitizeTextureFilepath(material.base_color_texture);
		material.base_color_texture = resolveTexturePath(filename, material.base_color_texture);
		materials.push_back(material);
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

void PlyLoader::load(MappedMesh& mesh, std::vector<Material> &_materials) {
	_materials = this->materials;

	mesh.positions.resize(n_vertices);
	if(has_colors)
		mesh.colors.resize(n_vertices);
	if(has_normals)
		mesh.normals.resize(n_vertices);
	const bool has_wedge_tex_coords = has_textures && !has_vertex_tex_coords;
	if(has_textures) {
		if(has_vertex_tex_coords)
			mesh.texcoords.resize(n_vertices);
		else
			mesh.texcoords.resize(n_triangles*3);
	}
	if(has_wedge_tex_coords)
		mesh.wedges.resize(n_triangles*3);
	else
		mesh.wedges.resize(n_vertices);

	pf.SetCurElement(vertices_element);

	PlyVertex vertex;

	for(size_t i = 0; i < n_vertices; i++) {
		Vector3f &p = mesh.positions[i];
		Wedge *w = nullptr;
		if(!has_wedge_tex_coords) {
			w = &mesh.wedges[i];
			w->p = i;
		}
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
			Vector3f &n = mesh.normals[i];
			n.x = vertex.n[0];
			n.y = vertex.n[1];
			n.z = vertex.n[2];
			if(w)
				w->n = i;
		}
		if(has_colors) {
			Rgba8 &c = mesh.colors[i];
			c.r = vertex.c[0];
			c.g = vertex.c[1];
			c.b = vertex.c[2];
			c.a = vertex.c[3];
		}
		if(has_textures && has_vertex_tex_coords) {
			Vector2f &t = mesh.texcoords[i];
			t.u = vertex.t[0];
			t.v = vertex.t[1];
			if(w)
				w->t = i;
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

			if(has_wedge_tex_coords) {
				const Index wedge_idx = static_cast<Index>(i*3 + k);
				Wedge &w = mesh.wedges[wedge_idx];
				w.p = static_cast<Index>(v);
				if(has_normals)
					w.n = static_cast<Index>(v);
				const Index tex_idx = wedge_idx;
				Vector2f &t = mesh.texcoords[tex_idx];
				t.u = face.t[k*2 + 0];
				t.v = face.t[k*2 + 1];
				w.t = tex_idx;
				tri.w[k] = wedge_idx;
			} else {
				tri.w[k] = static_cast<Index>(v);
			}
		}
		//TODO: if loading more than one model (because split, we need to reintroduce texOffset
		if(!_materials.empty() && face.texNumber >= _materials.size())
			throw std::runtime_error("PLY texNumber out of range of parsed materials.");
		mesh.material_ids[i] = face.texNumber;
	}
	mesh.has_textures = has_textures;
}

}
