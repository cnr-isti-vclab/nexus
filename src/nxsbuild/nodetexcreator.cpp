#include <vcg/space/rect_packer.h>

#include "nodetexcreator.h"
#include <QDebug>

using namespace nx;
using namespace std;


unsigned int nextPowerOf2 ( unsigned int n ) {
	unsigned count = 0;

	// First n in the below condition
	// is for the case where n is 0
	if (n && !(n & (n - 1)))
		return n;

	while( n != 0) {
		n >>= 1;
		count += 1;
	}

	return 1 << count;
}

class UnionFind {
public:
	std::vector<int> parents;
	void init(int size) {
		parents.resize(size);
		for(int i = 0; i < size; i++)
			parents[i] = i;
	}

	int root(int p) {
		while(p != parents[p])
			p = parents[p] = parents[parents[p]];
		return p;
	}
	void link(int p0, int p1) {
		int r0 = root(p0);
		int r1 = root(p1);
		parents[r1] = r0;
	}
	int compact(std::vector<int> &node_component) { //change numbering of the connected components, return number
		node_component.resize(parents.size());
		std::map<int, int> remap;                   // insert root here and order them.
		for(size_t i = 0; i < parents.size(); i++) {
			int root = i;
			while(root != parents[root])
				root = parents[root];
			parents[i] = root;
			node_component[i] = remap.emplace(root, remap.size()).first->second;
		}
		return remap.size();
	}

};


TextureGroup NodeTexCreator::process(TMesh &mesh, int level) {
	TextureGroup group;
	float &error = group.error;
	float &pixelXedge = group.pixelXEdge;

	std::vector<vcg::Box2f> boxes;
	std::vector<int> box_texture; //which texture each box belongs;
	std::vector<int> vertex_to_tex(mesh.vert.size(), -1);
	std::vector<int> vertex_to_box;

	UnionFind components;
	components.init(mesh.vert.size());

	for(auto &face: mesh.face) {
		int v[3];
		for(int i = 0; i < 3; i++) {
			v[i] = face.V(i) - &*mesh.vert.begin();


			int &t = vertex_to_tex[v[i]];

			if(t != -1 && t != face.tex) qDebug() << "Missing vertex replication across seams\n";
			t = face.tex;
		}
		components.link(v[0], v[1]);
		components.link(v[0], v[2]);
		assert(components.root(v[1]) == components.root(v[2]));
	}
	int n_boxes = components.compact(vertex_to_box);

	for(auto &face: mesh.face) {
		int v[3];
		for(int i = 0; i < 3; i++) {
			int v = face.V(i) - &*mesh.vert.begin();
			vertex_to_tex[v] = face.tex;
		}
		/*		assert(vertex_to_box[v[0]] == vertex_to_box[v[1]]);
		assert(vertex_to_box[v[0]] == vertex_to_box[v[2]]);
		assert(components.root(v[0]) == components.root(v[2]));
		assert(components.root(v[0]) == components.root(v[1])); */
	}
	//assign all boxes to a tex (and remove boxes where the tex is -1

	//compute boxes
	boxes.resize(n_boxes);
	box_texture.resize(n_boxes, -1);
	for(size_t i = 0; i < mesh.vert.size(); i++) {
		int b = vertex_to_box[i];
		int tex = vertex_to_tex[i];
		if(tex < 0) continue; //vertex not assigned.

		vcg::Box2f &box = boxes[b];
		box_texture[b] = tex;
		auto &t = mesh.vert[i].T().P();
		t[0] = fmod(t[0], 1.0);
		t[1] = fmod(t[1], 1.0);
		//		if(isnan(t[0]) || isnan(t[1]) || t[0] < 0 || t[1] < 0 || t[0] > 1 || t[1] > 1)
		//				cout << "T: " << t[0] << " " << t[1] << endl;
		if(t[0] != 0.0f || t[1] != 0.0f)
			box.Add(t);
	}
	//erase boxes assigned to no texture, and remap vertex_to_box
	int count = 0;
	std::vector<int> remap(mesh.vert.size(), -1);
	for(int i = 0; i < n_boxes; i++) {
		if(box_texture[i] == -1)
			continue;
		boxes[count] = boxes[i];
		box_texture[count] = box_texture[i];
		remap[i] = count++;
	}
	boxes.resize(count);
	box_texture.resize(count);
	for(int &b: vertex_to_box)
		b = remap[b];

	std::vector<vcg::Point2i> sizes(boxes.size());
	std::vector<vcg::Point2i> origins(boxes.size());
	for(size_t b = 0; b < boxes.size(); b++) {
		auto &box = boxes[b];
		int tex = box_texture[b];

		//enlarge 1 pixel
		float w = atlas.width(tex, level); //img->size().width();
		float h = atlas.height(tex, level); //img->size().height();
		float px = 1/(float)w;
		float py = 1/(float)h;
		box.Offset(vcg::Point2f(px, py));
		//snap to higher pix (clamped by 0 and 1 anyway)
		vcg::Point2i &size = sizes[b];
		vcg::Point2i &origin = origins[b];
		origin[0] = std::max(0.0f, floor(box.min[0]/px));
		origin[1] = std::max(0.0f, floor(box.min[1]/py));
		if(origin[0] >= w) origin[0] = w-1;
		if(origin[1] >= h) origin[1] = h-1;

		size[0] = std::min(w, ceil(box.max[0]/px)) - origin[0];
		size[1] = std::min(h, ceil(box.max[1]/py)) - origin[1];
		if(size[0] <= 0) size[0] = 1;
		if(size[1] <= 0) size[1] = 1;

		//		cout << "Box: " << box_texture[b] << " [" << box.min[0] << "  " << box.min[1] << " ] [ " << box.max[0] << "  " << box.max[1] << "]" << std::endl;
		//		cout << "Size: " << size[0] << " - " << size[1] << endl << endl;
		//		getchar();
	}

	//pack boxes;
	std::vector<vcg::Point2i> mapping;
	vcg::Point2i maxSize(1096, 1096);
	vcg::Point2i finalSize;
	bool success = false;
	for(int i = 0; i < 5; i++, maxSize[0]*= 2, maxSize[1]*= 2) {
		if(sizes.size() == 0) { //no texture!
			finalSize = vcg::Point2i(1, 1);
			success = true;
			break;
		}
		bool too_large = false;
		for(auto s: sizes) {
			if(s[0] > maxSize[0] || s[1] > maxSize[1])
				too_large = true;
		}
		if(too_large) { //TODO the packer should simply return false
			continue;
		}
		mapping.clear(); //TODO this should be done inside the packer
		success = vcg::RectPacker<float>::PackInt(sizes, maxSize, mapping, finalSize);
		if(success)
			break;
	}
	if(!success) {
		cerr << "Failed packing: the texture in a single nexus node would be > 16K\n";
		cerr << "Try to reduce the size of the nodes using -t (default is 4096)";
		exit(0);
	}

	if (createPowTwoTex) {
		finalSize[ 0 ] = (int) nextPowerOf2( finalSize[ 0 ] );
		finalSize[ 1 ] = (int) nextPowerOf2( finalSize[ 1 ] );
	}

	//	std::cout << "Boxes: " << boxes.size() << " Final size: " << finalSize[0] << " " << finalSize[1] << std::endl;
	QImage image(finalSize[0], finalSize[1], QImage::Format_RGB32);
	image.fill(QColor(127, 127, 127));
	//copy boxes using mapping

	float pdx = 1/(float)image.width();
	float pdy = 1/(float)image.height();

	for(size_t i = 0; i < mesh.vert.size(); i++) {
		auto &p = mesh.vert[i];
		auto &uv = p.T().P();
		int b = vertex_to_box[i];
		if(b == -1) {
			uv = vcg::Point2f(0.0f, 0.0f);
			continue;
		}
		vcg::Point2i &o = origins[b];
		vcg::Point2i m = mapping[b];

		//QImageReader &img = textures[box_texture[b]];
		int tex = box_texture[b];
		float w = atlas.width(tex, level); //img->size().width();
		float h = atlas.height(tex, level); //img->size().height();
		float px = 1/(float)w;
		float py = 1/(float)h;

		if(uv[0] < 0.0f)
			uv[0] = 0.0f;
		if(uv[1] < 0.0f)
			uv[1] = 0.0f;

		float dx = uv[0]/px - o[0];
		float dy = uv[1]/py - o[1];
		if(dx < 0.0f) dx = 0.0f;
		if(dy < 0.0f) dy = 0.0f;

		uv[0] = (m[0] + dx)*pdx; //how many pixels from the origin
		uv[1] = (m[1] + dy)*pdy; //how many pixels from the origin

		assert(!isnan(uv[0]));
		assert(!isnan(uv[1]));
	}
	//compute error:
	float pdx2 = pdx*pdx;
	error = 0.0;
	pixelXedge = 0.0f;
	for(auto &face: mesh.face) {
		for(int k = 0; k < 3; k++) {
			int j = (k==2)?0:k+1;

			float edge = vcg::SquaredNorm(face.P(k) - face.P(j));
			float pixel = vcg::SquaredNorm(face.V(k)->T().P() - face.V(j)->T().P())/pdx2;
			pixelXedge += pixel;
			if(pixel > 10) pixel = 10;
			if(pixel < 1)
				error += edge;
			else
				error += edge/pixel;
		}
	}
	pixelXedge = sqrt(pixelXedge/mesh.face.size()*3);
	error = sqrt(error/mesh.face.size()*3);

	double areausage = 0.0;
	//compute area waste
	for(int i = 0; i < mesh.face.size(); i++) {
		auto &face = mesh.face[i];
		int b = vertex_to_box[face.V(0) - &(mesh.vert[0])];
		vcg::Point2i &o = origins[b];
		vcg::Point2i m = mapping[b];
		auto V0 = face.V(0)->T().P();
		auto V1 = face.V(1)->T().P();
		auto V2 = face.V(2)->T().P();
		areausage += (V2 - V0)^(V2 - V1)/2;

	}
	//cout << "Area: " << (int)(100*areausage) << "% --- " << (int)areausage*finalSize[0]*finalSize[1] << " vs: " << finalSize[0]*finalSize[1] << "\n";

	{
		//	static int boxid = 0;
		QPainter painter(&image);
		//convert tex coordinates using mapping
		for(int i = 0; i < boxes.size(); i++) {

			/*		vcg::Color4b  color;
		color[2] = ((boxid % 11)*171)%63 + 63;
		//color[1] = 255*log2((i+1))/log2(nexus->header.n_patches);
		color[1] = ((boxid % 7)*57)%127 + 127;
		color[0] = ((boxid % 16)*135)%127 + 127; */

			int source = box_texture[i];
			vcg::Point2i &o = origins[i];
			vcg::Point2i &s = sizes[i];

			QImage rect = atlas.read(source, level, QRect(o[0], o[1], s[0], s[1]));
			painter.drawImage(mapping[i][0], mapping[i][1], rect);

			//		painter.fillRect(mapping[i][0], mapping[i][1], s[0], s[1], QColor(color[0], color[1], color[2]));
			//		boxid++;
		}


		/*	painter.setPen(QColor(255,0,255));
		for(int i = 0; i < mesh.face.size(); i++) {
			auto &face = mesh.face[i];
			int b = vertex_to_box[face.V(0) - &(mesh.vert[0])];
			vcg::Point2i &o = origins[b];
			vcg::Point2i m = mapping[b];

			for(int k = 0; k < 3; k++) {
				int j = (k==2)?0:k+1;
				auto V0 = face.V(k);
				auto V1 = face.V(j);
				float x0 = V0->T().P()[0]/pdx; //how many pixels from the origin
				float y0 = V0->T().P()[1]/pdy; //how many pixels from the origin
				float x1 = V1->T().P()[0]/pdx; //how many pixels from the origin
				float y1 = V1->T().P()[1]/pdy; //how many pixels from the origin
				painter.drawLine(x0, y0, x1, y1);
			}
		}*/
		/*
		for(int i = 0; i < mesh.vert.size(); i++) {
			auto &p = mesh.vert[i];
			int b = vertex_to_box[i];
			vcg::Point2i &o = origins[b];
			vcg::Point2i m = mapping[b];


			float x = p.T().P()[0]/pdx; //how many pixels from the origin
			float y = p.T().P()[1]/pdy; //how many pixels from the origin
			painter.setPen(QColor(255,0,255));
			painter.drawEllipse(x-10, y-10, 20, 20);
			painter.drawPoint(x, y);
		} */

	}

	image = image.mirrored();
	//static int imgcount = 0;
	//image.save(QString("OUT_test_%1.jpg").arg(imgcount++));
	return image;
}
