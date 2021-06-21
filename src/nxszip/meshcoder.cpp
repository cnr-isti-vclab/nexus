/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/
#include <deque>
#include <QTime>
#include <QTextStream>
#include "meshcoder.h"

using namespace nx;
using namespace vcg;
using namespace std;
using namespace meco;
//side is the edge face.f[side] face.f[side+1]

static int ilog2(uint64_t p) {
	int k = 0;
	while ( p>>=1 ) { ++k; }
	return k;
}

void MeshEncoder::encode() {
	FpuPrecision::store();
	FpuPrecision::setFloat();

	stream.reserve(node.nvert);

	quantize();

	if(sig.face.hasIndex())
		encodeFaces();
	else {
		encodeCoordinates();
	}

	if(sig.vertex.hasNormals())
		encodeNormals();

	if(sig.vertex.hasColors())
		encodeColors();


	if(!sig.face.hasIndex())
		node.nvert = zpoints.size(); //needed because quantizatiojn might remove some vertices in point clouds.

/*
	cout << "Coord bpv: " << coord_size*8/(float)node.nvert << endl;
	cout << "Face bpv: " << face_size*8/(float)node.nvert << endl;
	cout << "Norm bpv: " << normal_size*8/(float)node.nvert << endl;
	cout << "Color bpv: " << color_size*8/(float)node.nvert << endl; */

	FpuPrecision::restore();
}

void MeshEncoder::quantizeCoords() {
	//quantize vertex position and compute min.
	float side = pow(2.0f, (float)coord_q);
	//cout << "Side: " << side << endl;

	//TODO use quadric to quantize for better rate?  probably not worth it, for so few bits.
	qpoints.resize(node.nvert);
	for(unsigned int i = 0; i < node.nvert; i++) {
		Point3f &p = data.coords()[i];
		Point3i &q = qpoints[i];
		for(int k = 0; k < 3; k++) {
			q[k] = (int)floor(p[k]/side + 0.5f);
			if(k == 1)
				q[k] *= 1.0;
			if(i == 0) {
				min[k] = q[k];
				max[k] = q[k]; }
			else {
				if(min[k] > q[k]) min[k] = q[k];
				if(max[k] < q[k]) max[k] = q[k];
			}
		}
	}
	for(unsigned int i = 0; i < node.nvert; i++)
		qpoints[i] -= min;
	Point3i d = max - min;
	coord_bits = 1+std::max(std::max(ilog2(d[0]), ilog2(d[1])), ilog2(d[2]));
	assert(d[0] < (1<<coord_bits));
	assert(d[1] < (1<<coord_bits));
	assert(d[2] < (1<<coord_bits));

	stream.write<int>(min[0]);
	stream.write<int>(min[1]);
	stream.write<int>(min[2]);
	stream.write<char>(coord_q);
	stream.write<char>(coord_bits);

	//same operations as vertex coordinates.
}

void MeshEncoder::quantizeTexCoords() {
	//quantize vertex position and compute min.
	float side = pow(2.0f, (float)tex_q);

	//TODO use quadric to quantize for better rate?  probably not worth it, for so few bits.
	qtexcoords.resize(node.nvert);
	Point2f *texcoords = data.texCoords(sig, node.nvert);
	for(unsigned int i = 0; i < node.nvert; i++) {
		Point2f &p = texcoords[i];
		Point2i &q = qtexcoords[i];
		for(int k = 0; k < 2; k++) {
			//obviously wrong textures moved to 0,0.
			q[k] = (int)floor(p[k]/side + 0.5f);
//			if(q[0] < 0)

			if(i == 0) {
				tmin[k] = q[k];
				tmax[k] = q[k];
			} else {
				if(tmin[k] > q[k]) tmin[k] = q[k];
				if(tmax[k] < q[k]) tmax[k] = q[k];
			}
		}
	}
	for(unsigned int i = 0; i < node.nvert; i++)
		qtexcoords[i] -= tmin;
	Point2i d = tmax - tmin;
	tex_bits = 1+std::max(ilog2(d[0]), ilog2(d[1]));
	assert(d[0] < (1<<tex_bits));
	assert(d[1] < (1<<tex_bits));

	stream.write<int>(tmin[0]);
	stream.write<int>(tmin[1]);
	stream.write<char>(tex_q);
	stream.write<char>(tex_bits);

	//same operations as vertex coordinates.
}

void MeshEncoder::quantize() {
	quantizeCoords();
	if(sig.vertex.hasTextures())
		quantizeTexCoords();

	if(!sig.face.hasIndex()) {

		//TODO use boundary information for better quantization on the inside.
		zpoints.resize(node.nvert);
		for(int i = 0; i < node.nvert; i++) {
			Point3i q = qpoints[i];
			zpoints[i] = ZPoint(q[0], q[1], q[2], coord_bits, i);
		}

		sort(zpoints.rbegin(), zpoints.rend());//, greater<ZPoint>());

		//we need to remove duplicated vertices and save back ordering
		//for each vertex order contains its new position in zpoints
		int count = 0;

		for(unsigned int i = 1; i < zpoints.size(); i++) {
			if(zpoints[i] != zpoints[count]) {
				count++;

				zpoints[count] = zpoints[i];
			}
		}
		count++;
		zpoints.resize(count);
	}
}

void MeshEncoder::encodeCoordinates() {

	assert(!sig.face.hasIndex());

	vector<uchar> diffs;
	BitStream bitstream(node.nvert/2);

	bitstream.write(zpoints[0].bits, coord_bits*3);

	for(size_t pos = 1; pos < zpoints.size(); pos++) {
		ZPoint &p = zpoints[pos-1]; //previous point
		ZPoint &q = zpoints[pos]; //current point
		uchar d = p.difference(q);
		diffs.push_back(d);
		//we can get away with d diff bits (should be d+1) because the first bit will always be 1 (sorted q> p)
		bitstream.write(q.bits, d); //rmember to add a 1, since
	}

	int start = stream.size();

	bitstream.flush();
	stream.write(bitstream);

	Tunstall tunstall;
	tunstall.compress(stream, &*diffs.begin(), diffs.size());

	coord_size = stream.size() - start;
}


//compact in place faces in data, update patches information, compute topology and encode each patch.
void MeshEncoder::encodeFaces() {

	if(!node.nface) return;

	encoded.resize(node.nvert, -1);
	last.reserve(node.nvert);
	order.reserve(node.nvert);

	uint16_t *triangles = data.faces(sig, node.nvert);

	//remove degenerate faces
	uint32_t start =  0;
	uint32_t count = 0;
	for(size_t p = node.first_patch; p < node.last_patch(); p++) {
		Patch &patch = patches[p];
		uint32_t end = patch.triangle_offset;
		for(uint32_t i = start; i < end; i++) {
			uint16_t *face = triangles + i*3;

			if(face[0] == face[1] || face[0] == face[2] || face[1] == face[2])
				continue;

			uint16_t *dest = triangles + count*3;
			if(count != i) {
				dest[0] = face[0];
				dest[1] = face[1];
				dest[2] = face[2];
			}
			count++;
		}
		patch.triangle_offset = count;
		start = end;
	}
	node.nface = count;


	start =  0;
	//	encodeFaces(0, node.nface);
	for(uint32_t p = node.first_patch; p < node.last_patch(); p++) {
		Patch &patch = patches[p];
		uint end = patch.triangle_offset;
		encodeFaces(start, end);
		start = end;
	}
}

void MeshEncoder::encodeNormals() {

	int side = 1<<(16 - norm_q);

	vector<uchar> diffs;
	vector<uchar> signs;
	BitStream bitstream(node.nvert/64);

	Point3s *normals = data.normals(sig, node.nvert);

	if(!sig.face.hasIndex()) { //point cloud

		int step = (1<<(16 - norm_q));

		//point cloud: compute differences from previous normal
		for(int k = 0; k < 2; k++) {
			int on = 0; //old color
			for(unsigned int i = 0; i < zpoints.size(); i++) {
				Point3s c = normals[zpoints[i].pos];
				int n = c[k];
				n = n/step;
				//n = (n/(float)step + 0.5);
				//if(n >= max_value) n = max_value-1;
				//if(n <= -max_value) n = -max_value+1;
				int d = n - on;
				encodeDiff(diffs, bitstream, d);
				on = n;
			}
		}
		for(unsigned int i = 0; i < zpoints.size(); i++) {
			Point3s c = normals[zpoints[i].pos];
			bool signbit = c[2] > 0;
			signs.push_back(signbit);
		}

	} else { //mesh

		markBoundary(); //mark boundary points on original vertices.
		vector<Point3s> estimated_normals(node.nvert); //this are computed using the original indexing.
		computeNormals(estimated_normals);

		for(unsigned int i = 0; i < qpoints.size(); i++) {
			int pos = order[i];
			if(!boundary[pos]) continue;
			Point3s &computed = estimated_normals[pos];
			Point3s &original = normals[pos];
			for(int comp = 0; comp < 2; comp++) {
				int d = (int)(original[comp]/side - computed[comp]/side); //act1ual value - predicted
				encodeDiff(diffs, bitstream, d);
			}
			bool signbit = (computed[2]*original[2] < 0);
			signs.push_back(signbit);
		}
	}
	int start = stream.size();

	stream.write<char>(norm_q);
	Tunstall tunstall;
	tunstall.compress(stream, &*diffs.begin(), diffs.size());
	Tunstall tunstall1;
	tunstall1.compress(stream, &*signs.begin(), signs.size());
	bitstream.flush();
	stream.write(bitstream);

	normal_size = stream.size() - start;
}


//how to determine if a vertex is a boundary without topology:
//for each edge a vertex is in, add or subtract the id of the other vertex depending on order
//for internal vertices sum is zero.
//unless we have strange configurations and a lot of sfiga, zero wont happen. //TODO think about this
void MeshEncoder::markBoundary() {
	if(!sig.face.hasIndex()) {
		boundary.resize(node.nvert, true);
		return;
	}
	boundary.resize(node.nvert, false);

	uint16_t *faces = data.faces(sig, node.nvert);
	vector<int> count(node.nvert, 0);
	for(int i = 0; i < node.nface; i++) {
		uint16_t *f = faces + i*3;
		count[f[0]] += (int)f[1] - (int)f[2];
		count[f[1]] += (int)f[2] - (int)f[0];
		count[f[2]] += (int)f[0] - (int)f[1];
	}
	for(int i = 0; i < node.nvert; i++)
		if(count[i] != 0)
			boundary[i] = true;
}


void MeshEncoder::computeNormals(vector<Point3s> &estimated_normals) {

	uint16_t *faces = data.faces(sig, node.nvert);
	vector<Point3i> normals(node.nvert, Point3i(0, 0, 0));

	for(int i = 0; i < node.nface; i++) {
		uint16_t *face = faces + i*3;
		Point3i &p0 = qpoints[face[0]];
		Point3i &p1 = qpoints[face[1]];
		Point3i &p2 = qpoints[face[2]];
		Point3i n = (( p1 - p0) ^ (p2 - p0));
		normals[face[0]] += n;
		normals[face[1]] += n;
		normals[face[2]] += n;
	}
	//normalize
	for(unsigned int i = 0; i < normals.size(); i++) {
		Point3i &n = normals[i];
		float norm = sqrt(float((float)n[0]*n[0] + (float)n[1]*n[1] + (float)n[2]*n[2]));

		for(int k = 0; k < 3; k++)
			estimated_normals[i][k] = (int16_t)(n[k]*32767.0f/norm);
	}
}

#define REVERSIBLEFAST 1
static Color4b toYCC(Color4b s) {
	Color4b e;

#ifdef REVERSIBLEFAST
	e[0] = s[1];
	e[1] = s[2] - s[1];
	e[2] = s[0] - s[1];
	e[3] = s[3];
	return e;
#endif

	e[0] =              0.299000*s[0] + 0.587000*s[1] + 0.114000*s[2];
	e[1] = 128        - 0.168736*s[0] - 0.331264*s[1] + 0.500000*s[2];
	e[2] = 128        + 0.500000*s[0] - 0.428688*s[1] - 0.081312*s[2];
	e[3] = s[3];
	return e;
}

static Color4b toRGB(Color4b e) {
	Color4b s;
#ifdef REVERSIBLEFAST
	s[2] = e[1] + e[0];
	s[0] = e[2] + e[0];
	s[1] = e[0];
	s[3] = e[3];
	return s;
#endif

	int r = e[0]                        + 1.40200*(e[2] - 128);
	int g = e[0] - 0.34414*(e[1] - 128) - 0.71414*(e[2] - 128);
	int b = e[0] + 1.77200*(e[1] - 128);

	s[0] = std::max(0, std::min(255, r));
	s[1] = std::max(0, std::min(255, g));
	s[2] = std::max(0, std::min(255, b));
	s[3] = e[3];

	return s;
}


void MeshEncoder::encodeColors() {

	Color4b *original_colors = data.colors(sig, node.nvert);
	vector<Color4b> colors;

	BitStream bitstream(node.nvert/2);
	vector<uchar> diffs[4];


	int steps[4];
	for(int k = 0; k < 4; k++)
		steps[k] = (1<<(8 - color_q[k]));

	if(sig.face.hasIndex()) {
		colors.resize(node.nvert);
		for(unsigned int i = 0; i < node.nvert; i++) {
			for(int k = 0; k < 4; k++)
				colors[i][k] = original_colors[i][k]/steps[k]*steps[k];
			colors[i] = toYCC(colors[i]);
		}

		for(int i = 0; i < node.nvert; i++) {
			Color4b &c = colors[order[i]];
			int l = last[i];
			Color4b b;
			if(l < 0)
				b = Color4b(0, 0, 0, 0);
			else
				b = colors[last[i]];

			for(int k = 0; k < 4; k++) {
				int d = c[k]/steps[k] - b[k]/steps[k];
				encodeDiff(diffs[k], bitstream, d);
			}
		}

	} else {

		int on[4] = {0, 0, 0, 0}; //old color
		for(unsigned int i = 0; i < zpoints.size(); i++) {
			for(int k = 0; k < 4; k++) {

				int pos = zpoints[i].pos;
				Color4b c = toYCC(original_colors[pos]);
				int n = c[k];

				n = n/steps[k];

				int d = n - on[k];
				encodeDiff(diffs[k], bitstream, d);
				on[k] = n;
			}
		}
	}

	int start = stream.size();

	for(int k = 0; k < 4; k++)
		stream.write<char>(color_q[k]);

	for(int k = 0; k < 4; k++) {
		Tunstall tunstall;
		tunstall.compress(stream, &*diffs[k].begin(), diffs[k].size());
	}

	bitstream.flush();
	stream.write(bitstream);

	color_size = stream.size() - start;
}

static void buildTopology(vector<McFace> &faces) {
	//create topology;
	vector<McEdge> edges;
	for(unsigned int i = 0; i < faces.size(); i++) {
		McFace &face = faces[i];
		for(int k = 0; k < 3; k++) {
			int kk = k+1;
			if(kk == 3) kk = 0;
			int kkk = kk+1;
			if(kkk == 3) kkk = 0;
			edges.push_back(McEdge(i, k, face.f[kk], face.f[kkk]));
		}
	}
	sort(edges.begin(), edges.end());

	McEdge previous(0xffff, 0xffff, 0xffff, 0xffff);
	for(unsigned int i = 0; i < edges.size(); i++) {
		McEdge &edge = edges[i];
		if(edge.match(previous)) {
			uint16_t &edge_side_face = faces[edge.face].t[edge.side];
			uint16_t &previous_side_face = faces[previous.face].t[previous.side];
			if(edge_side_face == 0xffff && previous_side_face == 0xffff) {
				edge_side_face = previous.face;
				faces[edge.face].i[edge.side] = previous.side;
				previous_side_face = edge.face;
				faces[previous.face].i[previous.side] = edge.side;
			}
		} else
			previous = edge;
	}
}
static int next_(int t) {
	t++;
	if(t == 3) t = 0;
	return t;
}
static int prev_(int t) {
	t--;
	if(t == -1) t = 2;
	return t;
}

void MeshEncoder::encodeFaces(int start, int end) {
	int useless = 0;

	vector<McFace> faces(end - start);
	uint16_t *triangles = data.faces(sig, node.nvert);
	for(int i = start; i < end; i++) {
		uint16_t * f = triangles + i*3;
		faces[i - start] = McFace(f[0], f[1], f[2]);
		assert(f[0] != f[1] && f[1] != f[2] && f[2] != f[0]);
	}

	buildTopology(faces);

	vector<uchar> clers;
	vector<uchar> diffs;
	vector<uchar> tdiffs;
	BitStream bitstream(2*faces.size());

	unsigned int current = 0;          //keep track of connected component start

	vector<int> delayed;
	deque<int> faceorder;
	vector<CEdge> front;

	reorder.resize(node.nvert);

	vector<bool> visited(faces.size(), false);
	unsigned int totfaces = faces.size();

	bool hasTextures = sig.vertex.hasTextures();
	//	vector<int> test_faces;
	int counting = 0;
	while(totfaces > 0) {
		if(!faceorder.size() && !delayed.size()) {

			while(current != faces.size()) {   //find first triangle non visited
				if(!visited[current]) break;
				current++;
			}
			if(current == faces.size()) break; //no more faces to encode exiting

			//encode first face: 3 vertices indexes, and add edges
			unsigned int current_edge = front.size();
			McFace &face = faces[current];
			Point3i estimated(0, 0, 0);
			Point2i texestimated(0, 0);
			int last_index = -1;
			for(int k = 0; k < 3; k++) {
				int index = face.f[k];
				last.push_back(last_index);

				last_index = index;
				encodeVertex(index, estimated, texestimated, bitstream, diffs, tdiffs);
				Point3i &p = qpoints[index];
				estimated = p;
				if(hasTextures) {
					Point2i &pt = qtexcoords[index];
					texestimated = pt;
				}
				faceorder.push_back(front.size());
				front.push_back(CEdge(current, k, current_edge + prev_(k), current_edge + next_(k)));
			}
			counting++;
			visited[current] = true;
			current++;
			totfaces--;
			continue;
		}
		int c;
		if(faceorder.size()) {
			c = faceorder.front();
			faceorder.pop_front();
		} else {
			c = delayed.back();
			delayed.pop_back();
		}
		CEdge &e = front[c];
		if(e.deleted) continue;
		e.deleted = true;

		//opposite face is the triangle we are encoding
		uint16_t opposite_face = faces[e.face].t[e.side];
		int opposite_side = faces[e.face].i[e.side];

		if(opposite_face == 0xffff || visited[opposite_face]) { //boundary edge or glue
			clers.push_back(BOUNDARY);
			continue;
		}

		assert(opposite_face < faces.size());
		McFace &face = faces[opposite_face];
		//		test_faces.push_back(face.f[0]);
		//		test_faces.push_back(face.f[1]);
		//		test_faces.push_back(face.f[2]);


		int k2 = opposite_side;
		int k0 = next_(k2);
		int k1 = next_(k0);

		//check for closure on previous or next edge
		int eprev = e.prev;
		int enext = e.next;
		assert(enext < front.size());
		assert(eprev < front.size());
		CEdge &previous_edge = front[eprev];
		CEdge &next_edge = front[enext];

		int first_edge = front.size();
		bool close_left = (faces[previous_edge.face].t[previous_edge.side] == opposite_face);
		bool close_right = (faces[next_edge.face].t[next_edge.side] == opposite_face);

		if(close_left && close_right) {
			clers.push_back(END);
			previous_edge.deleted = true;
			next_edge.deleted = true;
			front[previous_edge.prev].next = next_edge.next;
			front[next_edge.next].prev = previous_edge.prev;

		} else if(close_left) {
			clers.push_back(LEFT);
			previous_edge.deleted = true;
			front[previous_edge.prev].next = first_edge;
			front[enext].prev = first_edge;
			faceorder.push_front(front.size());
			front.push_back(CEdge(opposite_face, k1, previous_edge.prev, enext));

		} else if(close_right) {
			clers.push_back(RIGHT);
			next_edge.deleted = true;
			front[next_edge.next].prev = first_edge;
			front[eprev].next = first_edge;
			faceorder.push_front(front.size());
			front.push_back(CEdge(opposite_face, k0, eprev, next_edge.next));

		} else {
			int v0 = face.f[k0];
			int v1 = face.f[k1];
			int opposite = face.f[k2];

			if(encoded[opposite] != -1 && faceorder.size()) { //split, but we can still delay it.
				e.deleted = false; //undelete it.
				delayed.push_back(c);
				clers.push_back(DELAY);
				continue;
			}

			clers.push_back(VERTEX);
			//compute how much would it take to save vertex information:
			//we need to estimate opposite direction using v0 + v1 -
			int v2 = faces[e.face].f[e.side];

			Point3i p0 = qpoints[v0];
			Point3i p1 = qpoints[v1];
			Point3i p2 = qpoints[v2];

			Point3i predicted = p0 + p1 - p2;
			Point2i texpredicted(0, 0);
			if(hasTextures)
				texpredicted = qtexcoords[v0] + qtexcoords[v1] - qtexcoords[v2];


			if(encoded[opposite] == -1) {//only first time
				last.push_back(v0);
			} else
				useless++;  //we encoutered it already.

			encodeVertex(opposite, predicted, texpredicted, bitstream, diffs, tdiffs);


			previous_edge.next = first_edge;
			next_edge.prev = first_edge + 1;
			faceorder.push_front(front.size());
			front.push_back(CEdge(opposite_face, k0, eprev, first_edge+1));
			faceorder.push_back(front.size());
			front.push_back(CEdge(opposite_face, k1, first_edge, enext));
		}

		counting++;
		assert(!visited[opposite_face]);
		visited[opposite_face] = true;
		totfaces--;
	}
	/*	vector<int> freq(6, 0);
	for(int i: clers)
		freq[i]++;
	cout << "Clers.size: " << clers.size() << " face.size: " << faces.size() << endl;

	for(int i: freq)
		cout << "I: " << 100.0*i/clers.size() << endl;

	//save obj
	QFile file("test.obj");
	file.open(QFile::WriteOnly);
	QTextStream tstream(&file);
	for(Point3i p: qpoints)
		tstream << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";

	for(int i = 0; i < test_faces.size(); i += 3) {
		int *f = &test_faces[i];
		tstream << "f " << (f[0]+1) << " " << (f[1] +1) << " " << (f[2]+1) << "\n";
	}
*/



	int stream_start = stream.size();
	Tunstall tunstall;
	tunstall.compress(stream, &*clers.begin(), clers.size());

	face_size += stream.size() - stream_start;
	stream_start = stream.size();

	Tunstall tunstall1;
	tunstall1.compress(stream, &*diffs.begin(), diffs.size());
	if(sig.vertex.hasTextures()) {
		int bits =0;
		for(auto &n: tdiffs) {
			bits += 2*n;
//			cout << "N: " << (int)n << endl;
		}
		Tunstall tunstall2;
		int sbits = 8*tunstall2.compress(stream, &*tdiffs.begin(), tdiffs.size());


/*		cout << "diff stream: " << sbits/(float)tdiffs.size() << endl;
		cout << " tdiffs: " << tdiffs.size()/(float)faces.size() << endl;
		cout << "stream bits: " << bits/(float)faces.size() << endl;
				bits += sbits;
		cout << "Bit per face: " << bits/(float)faces.size() << endl;
		cout << endl; */
	}

	bitstream.flush();
	stream.write(bitstream);

	coord_size += stream.size() - stream_start;
}

int needed(int v) {
	int n = 1;
	while(1) {
		if(v >= 0) {
			if(v < (1<<(n-1)))
				break;
		} else
			if(v + (1<<(n-1)) >= 0)
				break;
		n++;
	}
	return n;
}

void MeshEncoder::encodeVertex(int target, const Point3i &predicted, const Point2i &texpredicted, BitStream &bitstream,
							   std::vector<::uchar> &diffs, std::vector<::uchar> &tdiffs) {


	static int count = 0;
	count++;
	//compute how much would it take to save vertex information:
	//we need to estimate opposite direction using v0 + v1 -

	assert(target < node.nvert);
	if(encoded[target] != -1) { //write index of target.
		diffs.push_back(0);
		uint64_t bits = encoded[target];
		bitstream.write(bits, 16);
		return;
	}
	assert(order.size() < node.nvert);
	encoded[target] = order.size();
	reorder[target] = order.size();
	order.push_back(target);

	{
		Point3i d = qpoints[target] - predicted; //this is the estimated point.
		int diff = 0;
		for(int k = 0; k < 3; k++) {
			int n = needed(d[k]);
			if(n > diff)
				diff = n;
		}
		assert(diff > 0);
		for(int k = 0; k < 3; k++)
			d[k] += 1<<(diff-1); //bring it into the positive.

		//int diff = 1+std::max(ilog2(d[0]), std::max(ilog2(d[1]), ilog2(d[2])));

		diffs.push_back(diff);
		bitstream.write(d[0], diff);
		bitstream.write(d[1], diff);
		bitstream.write(d[2], diff);
	}

	if(sig.vertex.hasTextures()) {
		Point2i dt = qtexcoords[target] - texpredicted; //this is the estimated point


		int tdiff = 0;
		for(int k = 0; k < 2; k++) {
			int n = needed(dt[k]);
			if(n > tdiff)
				tdiff = n;
			if(tdiff >= 22) {
				cerr << "Target: " << target << " Size: " << qtexcoords.size() << endl;
				cerr << "Texture precision required cannot be bigger than 2^-21.\n"
					<< "Tex: " << qtexcoords[target][0] << " " << qtexcoords[target][1] << "\n"
					<< "Predicted: " << texpredicted[0] << " " << texpredicted[1] << "\n"
					<< "Dt: " << dt[0] << " " << dt[1] << endl;
				cerr << "Tex q: " << tex_q << " tex bits " << tex_bits << endl;
			}
		}

		assert(tdiff > 0);
		for(int k = 0; k < 2; k++)
			dt[k] += 1<<(tdiff-1);

		//int diff = 1+std::max(ilog2(d[0]), std::max(ilog2(d[1]), ilog2(d[2])));

		tdiffs.push_back(tdiff);
		bitstream.write(dt[0], tdiff);
		bitstream.write(dt[1], tdiff);
	}
}

//val cam be zero.
void MeshEncoder::encodeDiff(vector<::uchar> &diffs, BitStream &stream, int val) {
	val = Tunstall::toUint(val)+1;
	int ret = ilog2(val);
	diffs.push_back(ret);
	if(ret > 0)
		stream.write(val, ret);
}
