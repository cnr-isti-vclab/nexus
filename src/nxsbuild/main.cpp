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
#include <iostream>
#include <iomanip>

#include <QtGui>
#include <QVariant>

#include <wrap/system/qgetopt.h>

#include "meshstream.h"
#include "kdtree.h"
#include "../nxsbuild/nexusbuilder.h"
#include "plyloader.h"
#include "objloader.h"
#include "tsploader.h"

using namespace std;

int main(int argc, char *argv[]) {

	// we create a QCoreApplication just so that QT loads image IO plugins (for jpg and tiff loading)
	QCoreApplication myUselessApp(argc, argv);
	setlocale(LC_ALL, "C");
	QLocale::setDefault(QLocale::C);

	int node_size = 1<<15;
	int top_node_size = 4096;
	float vertex_quantization = 0.0f;   //optionally quantize vertices position.
	int tex_quality(92);                //default jpg texture quality
	QString decimation("quadric");      //simplification method
	int ram_buffer(2000);               //Mb of ram to use
	float scaling(0.5);                 //simplification ratio
	int skiplevels = 0;
	QString output("");                 //output file
	QString mtl;
	QString translate;
	bool center = false;


	bool point_cloud = false;
	bool normals = false;
	bool no_normals = false;
	bool colors = false;
	bool no_colors = false;
	bool no_texcoords = false;
	bool useOrigTex = false;
	bool create_pow_two_tex = false;

	//BTREE options
	QVariant adaptive(0.333f);

	GetOpt opt(argc, argv);
	QString help("ARGS specify a ply file (specify more ply files or the directory containing them to get a merged output)");
	opt.setHelp(help);

	opt.allowUnlimitedArguments(true); //able to join several plys

	//extraction options
	opt.addOption('o', "output filename", "filename of the nexus output file", &output);

	//construction options
	opt.addOption('f', "node faces", "number of faces per patch, default 32768", &node_size);
	opt.addOption('t', "top node faces", "number of triangles in the top node, default 4096", &top_node_size);
	opt.addOption('d', "decimation", "decimation method [quadric, edgelen], default quadric", &decimation);
	opt.addOption('s', "scaling", "decimation factor between levels, default 0.5", &decimation);
	opt.addOption('S', "skiplevels", "decimation skipped for n levels, default 0", &skiplevels);
	opt.addSwitch('O', "original textures", "use original textures, no repacking", &useOrigTex);
	opt.addOption('m', "mtl file", "mtl for a single obj file", &mtl);
	opt.addOption('k', "pow 2 textures", "create textures to be power of 2", &create_pow_two_tex);

	//btree options
	opt.addOption('a', "adaptive", "split nodes adaptively [0-1], default 0.333", &adaptive);

	opt.addOption('v', "vertex quantization", "vertex quantization grid size (might be approximated)", &vertex_quantization);
	opt.addOption('q', "texture quality", "texture quality [0-100], default 92", &tex_quality);

	//format options
	opt.addSwitch('p', "point cloud", "generate a multiresolution point cloud", &point_cloud);

	opt.addSwitch('N', "normals", "force per vertex normals, even in point clouds", &normals);
	opt.addSwitch('n', "no normals", "do not store per vertex normals", &no_normals);
	opt.addSwitch('C', "colors", "save colors", &colors);
	opt.addSwitch('c', "no colors", "do not store per vertex colors", &no_colors);
	opt.addSwitch('u', "no textures", "do not store per vertex texture coordinates", &no_texcoords);
	

	//other options
	opt.addOption('r', "ram", "max ram used (in MegaBytes), default 2000 (WARNING: not a hard limit, increase at your risk)", &ram_buffer);
	opt.addOption('T', "origin", "new origin for the model X:Y:Z", &translate);
	opt.addSwitch('G', "center", "set origin in the bounding box center", &center);
	opt.parse();

	//Check parameters are correct
	QStringList inputs = opt.arguments;
	if(inputs.size() == 0) {
		cerr << "No input files specified" << endl;
		cerr << qPrintable(opt.usage()) << endl;
		return -1;
	}

	if(inputs.size() == 1) { //check for directory parameter
		QDir dir(inputs[0]);
		if(dir.exists()) {
			QStringList filters;
			filters << "*.ply" << "*.obj" << "*.stl" << "*.tsp"; //append your xml filter. You can add other filters here
			inputs = dir.entryList(filters, QDir::Files);
			if(inputs.size() == 0) {
				cerr << "Empty directory" << endl;
				return -1;
			}
			for(QString &s: inputs)
				s = dir.filePath(s);
		}
	}

	if(output == "") output = inputs[0].left(inputs[0].length()-4);
	if(!output.endsWith(".nxs"))
		output += ".nxs";

	if(node_size < 1000 || node_size >= 1<<16) {
		cerr << "Patch size (" << node_size << ") out of bounds [1000-65536]" << endl;
		return -1;
	}
	
	vcg::Point3d origin(0, 0, 0);
	if(!translate.isEmpty()) {
		QStringList p = translate.split(':');
		if(p.size() != 3) {
			cerr << "Malformed translate parameter, expecting X:Y:Z" << endl;
			return -1;
		}

		bool ok = false;
		for(int i = 0; i < 3; i++) {
			origin[i] = p[i].toDouble(&ok);
			if(!ok) {
				cerr << "Malformed translate parameter, expecting X:Y:Z" << endl;
				return -1;
			}
		}
		if(center) {
			cerr << "Can't specify both --center (-t) and --translate (-T)" << endl;
			return -1;
		}
	}

	Stream *stream = 0;
	KDTree *tree = 0;
	int returncode = 0;
	try {
		quint64 max_memory = (1<<20)*(uint64_t)ram_buffer/4; //hack 4 is actually an estimate...

		//autodetect point cloud ply
		if(inputs[0].endsWith(".ply")) {
			PlyLoader autodetect(inputs[0]);
			if(autodetect.nTriangles() == 0)
				point_cloud = true;
		}

		string input = "mesh";

		if (point_cloud) {
			input = "pointcloud";
			stream = new StreamCloud("cache_stream");
		}
		else
			stream = new StreamSoup("cache_stream");

		stream->setVertexQuantization(vertex_quantization);
		stream->setMaxMemory(max_memory);
		if(center) {
			vcg::Box3d box = stream->getBox(inputs);
			vcg::Point3d m = box.min;
			vcg::Point3d M = box.max;
			cout << setprecision(12) << "Box: " << m[0] << " " << m[1] << " " << m[2] << "  --- " << M[0] << " " << M[1] << " " << M[2] << endl;
			stream->origin = box.Center();
		} else
			stream->origin = origin;
		
		vcg::Point3d &o = stream->origin;
		if(o[0] != 0.0 || o[1] != 0.0 || o[2] != 0.0) {
			int lastPoint = output.lastIndexOf(".");
			QString ref = output.left(lastPoint) + ".js";
			QFile file(ref);
			if(!file.open(QFile::ReadWrite)) {
				cerr << "Could not save reference file: " << qPrintable(ref) << endl;
				return -1;
			}
			QTextStream stream(&file);
			stream.setRealNumberPrecision(12);
			stream << "{ \"origin\": [" << o[0] << ", " << o[1] << ", " << o[2] << "] }\n";
		}
		//TODO: actually the stream will store textures or normals or colors even if not needed
		stream->load(inputs, mtl);


		bool has_colors = stream->hasColors();
		bool has_normals = stream->hasNormals();
		bool has_textures = stream->hasTextures();

		cout << "Components: " << input;
		if(has_normals) cout << " normals";
		if(has_colors) cout << " colors";
		if(has_textures) cout << " textures";
		cout << "\n";

		quint32 components = 0;
		if(!point_cloud) components |= NexusBuilder::FACES;

		if((!no_normals && (!point_cloud || has_normals)) || normals) {
			components |= NexusBuilder::NORMALS;
			cout << "Normals enabled\n";
		}
		if((has_colors  && !no_colors ) || colors ) {
			components |= NexusBuilder::COLORS;
			cout << "Colors enabled\n";
		}
		if(has_textures && !no_texcoords) {
			components |= NexusBuilder::TEXTURES;
			cout << "Textures enabled\n";
		}
		
		//WORKAROUND to save loading textures not needed
		if(!(components & NexusBuilder::TEXTURES)) {
			stream->textures.clear();
		}

		NexusBuilder builder(components);
		builder.skipSimplifyLevels = skiplevels;
		builder.setMaxMemory(max_memory);
		builder.setScaling(scaling);
		builder.useNodeTex = !useOrigTex;
		builder.createPowTwoTex = create_pow_two_tex;
		builder.tex_quality = tex_quality;
		bool success = builder.initAtlas(stream->textures);
		if(!success) {
			cerr << "Exiting" << endl;
			return 1;
		}


		if(point_cloud)
			tree = new KDTreeCloud("cache_tree", adaptive.toFloat());
		else
			tree = new KDTreeSoup("cache_tree", adaptive.toFloat());

		tree->setMaxMemory((1<<20)*(uint64_t)ram_buffer/2);
		KDTreeSoup *treesoup = dynamic_cast<KDTreeSoup *>(tree);
		if(treesoup)
			treesoup->setTrianglesPerBlock(node_size);

		KDTreeCloud *treecloud = dynamic_cast<KDTreeCloud *>(tree);
		if(treecloud)
			treecloud->setTrianglesPerBlock(node_size);

		builder.create(tree, stream,  top_node_size);
		builder.save(output);

	} catch(QString error) {
		cerr << "Fatal error: " << qPrintable(error) << endl;
		returncode = 1;
		
	} catch(const char *error) {
		cerr << "Fatal error: " << error << endl;
		returncode = 1;
	}

	if(tree)   delete tree;
	if(stream) delete stream;
	
	return returncode;
}
