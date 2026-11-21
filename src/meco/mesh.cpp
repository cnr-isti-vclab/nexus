#include <assert.h>
#include "mesh.h"

void Mesh::from(CMesh &mesh, bool use_ormals, bool use_colors, bool use_uv) {
    coords.resize(mesh.vert.size());
    if(use_normals)
        normals.resize(coords.size());
    if(use_colors)
        colors.resize(coords.size());
    if(use_uv)
        uv.resize(coords.size());

    for(int i = 0; i < mesh.vert.size(); i++) {
        CVertex &v = mesh.vert[i];
        coords[i] = v.P();
        if(use_normals)
            normals[i] = v.N();
        if(use_colors)
            colors[i] = v.C();
        if(use_uv)
            uv[i] = v.T();
    }
    faces.resize(mesh.face.size());
    for(int i = 0; i < mesh.face.size(); i++) {
        CFace &f = mesh.face[i];
        for(int k = 0; k < 3; k++)
            faces[i*3 + k] = f.V(k) - &*mesh.face.begin();
    }
}

void Mesh::to(CMesh &mesh) {
    assert(0);
}
