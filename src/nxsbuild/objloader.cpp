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
#include "objloader.h"
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <iostream>



#define RED(c) (c >> 24) 
#define GREEN(c) ((c >> 16) & 0xff)
#define BLUE(c) ((c >> 8) & 0xff)
#define ALPHA(c) (c & 0xff)


void getColor(char *buffer, float *color) {
	char field[12];
	float r, g, b;
	int n = sscanf(buffer, "%s %f %f %f", field, color, color+1, color+2);
	if(n != 4)
		throw QString("Failed parsing material value: ") + buffer;
	color[3] = 1.0f;
}

void getValue(char *buffer, float &value) {
	char field[12];
	int n = sscanf(buffer, "%s %f", field, &value);

	if(n != 2)
		throw QString("Failed parsing material value: ") + buffer;
}


ObjLoader::ObjLoader(QString filename, QString _mtl):
	vertices("cache_plyvertex"),
	n_vertices(0),
	n_triangles(0),
	current_vertex(0) {

	if(!_mtl.isNull()) {
		mtls.push_back(_mtl);
		use_custom_mtl = true;
	}
	file.setFileName(filename);
	if(!file.open(QFile::ReadOnly))
		throw QString("could not open file %1. Error: %2").arg(filename).arg(file.errorString());
}

ObjLoader::~ObjLoader() {
	file.close();
}



void ObjLoader::cacheTextureUV() {

	vtxtuv.reserve(n_vertices * 2);

	char buffer[1024];
	file.seek(0);
	while (1) {
		
		int64_t s = file.readLine(buffer, 1024);
		if (s == -1) {                     //end of file
			break;
		}
		if (s == 0) continue;            //skip empty lines
		if (buffer[0] != 'v' || buffer[1] != 't')            //skip all irrelevant staff
			continue;
		buffer[s] = '\0';               //terminating line, readLine wont do this.


		if (buffer[2] == ' ') {      //skip other properties

			float vt0 = 0.0, vt1 = 0.0;
			int n = sscanf(buffer, "vt %f %f", &vt0, &vt1);
			if (n != 2) throw QString("error parsing vtxt  line: %1").arg(buffer);
			vtxtuv.push_back(vt0);
			vtxtuv.push_back(vt1);


		}//skipping other properties in OBJ
	}
}

/* TODO there might be more than one material! */
void ObjLoader::readMTLs() {
	for(auto mtl: mtls)
		readMTL(mtl);

	if(mtls.size() > 0)
		return;

	//create a basic material when no mtl has been specified.
	//material_map[mtltag] = materials.size() + materialOffset;
	BuildMaterial material;
	material.color[0] = material.color[1] = material.color[2] = 1.0f;
	materials.push_back(material);


	//TODO
	/* Materials and textures will be merged at the end of the loading!
	 * 			if (txtfname.length() > 0) {
					textures_map.insert(mtltag, txtfname);
					bool exists = false;
					for (auto fn : texture_filenames)
						if (fn == txtfname) {
							exists = true;
							break;
						}
					if (!exists)
						texture_filenames.push_back(txtfname);
				} */
}


void ObjLoader::readMTL(QString mtl_path) {
	char buffer[1024];
	
	if (!mtl_path.isNull()) {
		if(!QFileInfo::exists(mtl_path))
			throw QString("Could not find .mtl file: %1").arg(mtl_path);
	} else {
		QString fname = file.fileName();
		QFileInfo info = QFileInfo(fname);
	
		//assuming mtl base file name the same as obj
		mtl_path = info.path() + "/" + info.completeBaseName() + ".mtl";
	}
		
	if (!QFileInfo::exists(mtl_path))
		return;

	QFile f(mtl_path);
	if (!f.open(QFile::ReadOnly))
		return;
	int head_linewas_read = false;
	while (1) {
		int s = 0;
		if (!head_linewas_read) {
			
			s = f.readLine(buffer, 1024);
			if (s == -1)                     //end of file
				break;

			if (s == 0) continue;            //skip empty lines
			if (buffer[0] == '#')            //skip comments
				continue;
			buffer[s] = '\0';
		}
		else {
			head_linewas_read = false;
		}
		
		QString str(buffer);
		//trim whitespaces at start and end and merge spaces
		str = str.simplified();
		if (str.startsWith("newmtl", Qt::CaseInsensitive)){
			BuildMaterial material;

			QString mtltag = str.section(" ", 1);
			QString txtfname;

			do {
				s = f.readLine(buffer, 1024);
				if (s == -1) {
					break; //end of space
				}
				buffer[s] = '\0';
				QString str(buffer);
				str = str.simplified();
				
				if (str.startsWith("newmtl", Qt::CaseInsensitive)){
					head_linewas_read = true;
					break;
				}
				if (str.startsWith("d", Qt::CaseInsensitive)){
					float d = 1.0;
					int n = sscanf(buffer, "d %f", &d);
					if(n == 1)
						uint8_t A = uint8_t(255.0f * d);
					continue;
				}
				if(str.startsWith("Tr", Qt::CaseInsensitive)){
					float tr = 0.0;
					int n = sscanf(buffer, "d %f", &tr);
					if (n == 1)
						uint8_t A = 255 * (1.0f - tr);
					continue;
				}
				if(str.startsWith("Kd", Qt::CaseInsensitive)){
					getColor(buffer, material.color);
					continue;
				}
				if(str.startsWith("Map_Kd", Qt::CaseInsensitive)){
					txtfname = str.mid(7).trimmed();
					txtfname = txtfname.remove(QRegExp("^(\")"));
					txtfname = txtfname.remove(QRegExp("(\")$"));

					material.color_map = material.nmaps++;
					material.textures.push_back(txtfname);
					has_textures = true;
					continue;
				}
				if(str.startsWith("Ka", Qt::CaseInsensitive)){
					getColor(buffer, material.ambient);
					continue;
				}

				if(str.startsWith("Ks", Qt::CaseInsensitive)){
					getColor(buffer, material.specular);
					continue;
				}
				if(str.startsWith("Map_Ks", Qt::CaseInsensitive)){
					txtfname = str.mid(7).trimmed();
					txtfname = txtfname.remove(QRegExp("^(\")"));
					txtfname = txtfname.remove(QRegExp("(\")$"));

					material.specular_map = material.nmaps++;
					material.textures.push_back(txtfname);
					has_textures = true;
					continue;
				}
				if(str.startsWith("Ns", Qt::CaseInsensitive)){
					getValue(buffer, material.glossiness);
					continue;
				}

				//TODO check for bump also str.startsWith("bump", Qt::CaseInsensitive) ||
				if(str.startsWith("map_bump", Qt::CaseInsensitive)) {
					txtfname = str.mid(9).trimmed();
					txtfname = txtfname.remove(QRegExp("^(\")"));
					txtfname = txtfname.remove(QRegExp("(\")$"));

					material.bump_map = material.nmaps++;
					material.textures.push_back(txtfname);
					has_textures = true;
					continue;
				}


			} while (true);

			material_map[mtltag] = materials.size() + materialOffset;
			materials.push_back(material);
		}
	}
}

void ObjLoader::cacheVertices() {
	vertices.setElementsPerBlock(1<<20);
	file.seek(0);
	char buffer[1024];
	int cnt = 0;
	
	while(1) {
		
		int64_t s = file.readLine(buffer, 1024);
		if (s == -1) {                     //end of file
			cout << "Vertices read: " << cnt << endl;
			break;

		}
		if(s == 0) continue;            //skip empty lines
		buffer[s] = '\0';               //terminating line, readLine wont do this.

		if(buffer[0] == 'v') {          //vertex
			if(buffer[1] == ' ') {      //skip other properties

				vertices.resize(n_vertices+1);
				Vertex &vertex = vertices[n_vertices];
				n_vertices++;

				vcg::Point3d p;
				int n = sscanf(buffer, "v %lf %lf %lf", &p[0], &p[1], &p[2]);
				if(n != 3) throw QString("error parsing vertex line %1 while caching").arg(buffer);
				p -= origin;
				box.Add(p);
				
				vertex.v[0] = float(p[0]);
				vertex.v[1] = float(p[1]);
				vertex.v[2] = float(p[2]);

				cnt++;
				if(quantization != 0.0f) {
					quantize(vertex.v[0]);
					quantize(vertex.v[1]);
					quantize(vertex.v[2]);
				}


			}//skipping other properties in OBJ
			continue;

		} else if(buffer[0] == 'm' && strncmp(buffer, "mtllib", 6) == 0) {
			if(!use_custom_mtl) {
				QString fname = file.fileName();
				QFileInfo info = QFileInfo(fname);

				QString mtl = QString(buffer).mid(7).trimmed();
				mtl = mtl.remove(QRegExp("^(\")"));
				mtl = mtl.remove(QRegExp("(\")$"));
				mtl = info.dir().filePath(mtl);
				mtls.push_back(mtl);
			}
		}
	}
}

void ObjLoader::setMaxMemory(quint64 max_memory) {
	vertices.setMaxMemory(max_memory);
}







quint32 ObjLoader::getTriangles(quint32 size, Triangle *faces) {

	if (n_triangles == 0) {
		cacheVertices();
		cacheTextureUV();
		readMTLs();
	}

	char buffer[1024];
	file.seek(current_tri_pos);
	
	quint32 count = 0;
	qint64 cpos = current_tri_pos;

	while (count < size) {
		cpos = file.pos();
		qint64 s = file.readLine(buffer, 1024);
		if (s == -1) {                     //end of file
			cpos = file.pos();
			break;
		}

		if(buffer[0] == 'u') {
			QString str = QString(buffer).simplified().section(" ", 1);
			if(!material_map.count(str))
				throw QString("Could not find material: " + str);

			current_material_id = material_map[str];
			continue;
		}

		if (buffer[0] != 'f')
			continue;

		//int res = sscanf(buffer, "f %s %s %s %s", st[0], st[1], st[2], st[3]);

		QString str = QString(buffer).simplified();

		QRegExp rx("[ ]");// match a space
		QStringList list = str.split(rx, QString::SkipEmptyParts);
		list.removeFirst(); //'f'
		if (list.last().startsWith('\n') || list.last().startsWith('\r'))
			list.removeLast();
		if (list.last().startsWith('#'))
			list.removeLast();

		//qDebug() << list;

		int valence = list.size();

		if (count + (valence - 2) >= size)
			break;

		if (valence >= 3) {
			int* face_ = new int[valence];
			int* normal_ = new int[valence];
			int* vtxt_ = new int[valence];
			for (int i = 0; i < valence; i++) {
				normal_[i] = -1;
				vtxt_[i] = -1;
			}

			int* face1 = new int[(valence - 2) * 3];
			int* normal1 = new int[(valence - 2) * 3];
			int* vtxt1 = new int[(valence - 2) * 3];

			for (int w = 0; w < valence; w++) {
				int n = list[w].length();
				int rr[3]; rr[0] = rr[1] = rr[2] = 0;
				int rri = 0;
				for (int i = 0; i < n; i++) {
					char c = (char)list[w][i].cell();
					if (c == '/') {
						if (rri < 3) rri++;
					}
					else
						rr[rri] = rr[rri] * 10 + (c - '0');
				}
				face_[w] = rr[0] - 1;
				vtxt_[w] = rr[1] - 1;
				normal_[w] = rr[2] - 1;
			}

			for (int j = 0; j < valence - 2; j++) {

				face1[j * 3 + 0] = face_[0];
				normal1[j * 3 + 0] = normal_[0];
				vtxt1[j * 3 + 0] = vtxt_[0];

				face1[j * 3 + 1] = face_[j + 1];
				normal1[j * 3 + 1] = normal_[j + 1];
				vtxt1[j * 3 + 1] = vtxt_[j + 1];

				face1[j * 3 + 2] = face_[j + 2];
				normal1[j * 3 + 2] = normal_[j + 2];
				vtxt1[j * 3 + 2] = vtxt_[j + 2];
			}

			for (int m = 0; m <= valence - 3; m++) {

				Triangle &current = faces[count];

				for (int k = 0; k < 3; k++) {
					current.vertices[k] = vertices[face1[m * 3 + k]];
					/*for (int j = 0; normal1[m * 3 + k] >= 0 && j < 3; j++)
						current.vertices[k].n[j] = vnormals[normal1[m * 3 + k] * 3 + j];*/
					if (vtxt1[m * 3 + k] >= 0)
						for (int j = 0; j < 2; j++)
							current.vertices[k].t[j] = vtxtuv[vtxt1[m * 3 + k] * 2 + j];
				}
				current.tex = current_material_id;
				if (has_colors && current_color) {
					current.vertices[0].c[0] = RED(current_color);
					current.vertices[0].c[1] = GREEN(current_color);
					current.vertices[0].c[2] = BLUE(current_color);
					current.vertices[0].c[3] = ALPHA(current_color);

					current.vertices[1].c[0] = RED(current_color);
					current.vertices[1].c[1] = GREEN(current_color);
					current.vertices[1].c[2] = BLUE(current_color);
					current.vertices[1].c[3] = ALPHA(current_color);

					current.vertices[2].c[0] = RED(current_color);
					current.vertices[2].c[1] = GREEN(current_color);
					current.vertices[2].c[2] = BLUE(current_color);
					current.vertices[2].c[3] = ALPHA(current_color);
				}
				current.node = 0;
				if (current.isDegenerate()) {
					continue;
				}
				else {
					count++;
					n_triangles++;
				}
			}
			delete[] face_;
			delete[] normal_;
			delete[] vtxt_;
			delete[] face1;
			delete[] normal1;
			delete[] vtxt1;
			cpos = file.pos();

		}
		else {
			throw QString("could not parse face: %1").arg(buffer);
		}
	}


	current_tri_pos = cpos;

	if (count == 0)
		cout << "faces read: " << n_triangles << endl;

	return count;
}


/*
 * Seeks all verteces in whole file
 */
quint32 ObjLoader::getVertices(quint32 size, Splat *vertices) {
	char buffer[1024];
	
	quint32 count = 0;
	while(count < size) {
		int s = file.readLine(buffer, 1024);
		if(s == -1)                     //end of file
			return count;
		
		if(buffer[0] != 'v')            //skip comments, faces, etc
			continue;

		buffer[s] = '\0';               //terminating line, readLine wont do this.

		if(buffer[1] != ' ')
			continue; //skip other vertex properties{        //vertex coordinates

		Splat &vertex = vertices[count];
		
		vcg::Point3d p;
		int n = sscanf(buffer, "v %lf %lf %lf", &p[0], &p[1], &p[2]);
		if(n != 3) throw QString("error parsing vertex line %1").arg(buffer);
		
		p -= origin;
		box.Add(p);

		vertex.v[0] = (float)p[0];
		vertex.v[1] = (float)p[1];
		vertex.v[2] = (float)p[2];
	
		if(quantization) {
			quantize(vertex.v[0]);
			quantize(vertex.v[1]);
			quantize(vertex.v[2]);
		}
		n_vertices++;
		current_vertex++;
		count++;
	}
	return count;
}
