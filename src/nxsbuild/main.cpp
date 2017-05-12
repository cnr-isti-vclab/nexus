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

#include <QtGui>
#include <QVariant>

#include <wrap/system/qgetopt.h>

#include "meshstream.h"
#include "kdtree.h"
#include "../nxsbuild/nexusbuilder.h"
#include "plyloader.h"
#include "objloader.h"


using namespace std;

int main(int argc, char *argv[]) {

	// we create a QCoreApplication just so that QT loads image IO plugins (for jpg and tiff loading)
	QCoreApplication myUselessApp(argc, argv);
	setlocale(LC_ALL, "C");
	QLocale::setDefault(QLocale::C);		      QLocale::setDefault(QLocale::C);

	int node_size = 1<<15;
	int top_node_size = 4096;
	float vertex_quantization = 0.0f;   //optionally quantize vertices position.
	int tex_quality(92);                //default jpg texture quality
	QString decimation("quadric");      //simplification method
	int ram_buffer(2000);               //Mb of ram to use
	float scaling(0.5);                 //simplification ratio
	QString output("");                 //output file


	bool point_cloud = false;
	bool normals = false;
	bool no_normals = false;
	bool colors = false;
	bool no_colors = false;
	bool no_texcoords = false;
	bool compress = false;
	bool useOrigTex = false;

	//BTREE options
	QVariant adaptive(0.333f);

	GetOpt opt(argc, argv);
	opt.allowUnlimitedArguments(true); //able to join several plys
	opt.addOption('o', "output_filename", "filename of the nexus output file", &output);

	//construction options
	opt.addOption('f', "node_faces", "faces per patch, default: 16286", &node_size);
	opt.addOption('t', "top_node_faces", "number of triangles in the top node, default 4096", &top_node_size);
	opt.addOption('d', "decimation", "decimation method: quadric, edgelen. Default: quadric", &decimation);
	opt.addOption('s', "scaling", "scaling between level, default 0.5", &decimation);
	opt.addSwitch('O', "orig_textures", "Use original textures, no repacking", &useOrigTex);

	//btree options
	opt.addOption('a', "adaptive", "split notes adaptively [0...1] default 0.333", &adaptive);

	opt.addOption('v', "vertex_quantization", "approximated to closest power of 2", &vertex_quantization);
	opt.addOption('q', "texture quality", "jpeg quality for texture [0-100] default 92", &tex_quality);

	//format options
	opt.addSwitch('p', "point_cloud", "generate a multiresolution point cloud", &point_cloud);

	opt.addSwitch('N', "normals", "force per vertex normals even in point clouds", &normals);
	opt.addSwitch('n', "no_normals", "do not store per vertex normals", &no_normals);
	opt.addSwitch('C', "colors", "save colors", &colors);
	opt.addSwitch('c', "no_colors", "do not tore per vertex colors", &no_colors);
	opt.addSwitch('u', "no_textures", "do not store per vertex texture coordinates", &no_texcoords);

	opt.addSwitch('z', "compression", "enable geometry driven compression", &compress);

	//other options
	opt.addOption('r', "ram", "max ram used (in Megabytes default 2000) WARNING: not a hard limit, increase at your risk :P", &ram_buffer);
	opt.parse();

	//Check parameters are correct
	QStringList inputs = opt.arguments;
	if(inputs.size() == 0) {
		cerr << "No input files specified.\n";
		cerr << qPrintable(opt.usage()) << "\n";
		return -1;
	}

	if(output == "") output = inputs[0].left(inputs[0].length()-4);
	if(!output.endsWith(".nxs"))
		output += ".nxs";

	if(node_size < 1000 || node_size >= 1<<16) {
		cerr << "Patch size (" << node_size << ") out of bounds [2000-32536]\n";
		return -1;
	}

	try {
		quint64 max_memory = (1<<20)*(uint64_t)ram_buffer/4; //hack 4 is actually an estimate...

		//autodetect point cloud ply.
		{
			if(inputs[0].endsWith(".ply")) {
				PlyLoader autodetect(inputs[0]);
				if(autodetect.nTriangles() == 0)
					point_cloud = true;
			}
		}

		Stream *stream = 0;
		if(point_cloud)
			stream = new StreamCloud("cache_stream");
		else
			stream = new StreamSoup("cache_stream");
		stream->setVertexQuantization(vertex_quantization);
		stream->setMaxMemory(max_memory);
		stream->load(inputs);
		bool has_colors = stream->hasColors();
		bool has_normals = stream->hasNormals();
		bool has_textures = stream->hasTextures();

		if(has_colors) cout << "Mesh with colors\n";
		if(has_normals) cout << "Mesh with normals\n";
		if(has_textures) cout << "Mesh with textures\n";

		quint32 components = 0;
		if(!point_cloud) components |= NexusBuilder::FACES;

		if((!no_normals && (!point_cloud || has_normals)) || normals) {
			components |= NexusBuilder::NORMALS;
			cout << "Normals enabled.\n";
		}
		if((has_colors  && !no_colors ) || colors ) {
			components |= NexusBuilder::COLORS;
			cout << "Colors enabled.\n";
		}
		if(has_textures && !no_texcoords) {
			components |= NexusBuilder::TEXTURES;
			cout << "Textures enabled.\n";
		}

		NexusBuilder builder(components);
		builder.setMaxMemory(max_memory);
		builder.setScaling(scaling);
		builder.useNodeTex = !useOrigTex;
		builder.tex_quality = tex_quality;
		builder.initAtlas(stream->textures);


		KDTree *tree = 0;
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

		delete tree;
		delete stream;


	} catch(QString error) {
		cout << "Fatal error: " << qPrintable(error) << endl;
		return -1;
	} catch(const char *error) {
		cout << "Fatal error: " << error << endl;
		return -1;
	}
	return 0;
}
