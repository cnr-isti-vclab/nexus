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
#include <stdio.h>
#include <iostream>

#include <QStringList>
#include <wrap/system/qgetopt.h>

#include "../common/qtnexusfile.h"
#include "../nxsbuild/nexusbuilder.h"
#include "../common/traversal.h"
#include "extractor.h"

#include <QtGui>

using namespace std;
using namespace nx;

void printInfo(NexusData &nexus);
void printNodes(NexusData& nexus);
void printDag(NexusData& nexus);
void printPatches(NexusData& nexus);
void printTextures(NexusData& nexus);
void checks(NexusData &nexus);
void recomputeError(NexusData &nexus, QString mode);

bool show_dag = false;
bool show_nodes = false;
bool show_patches = false;
bool show_textures = false;

//TODO REMOVE unused textures when resizing nexus.
int main(int argc, char *argv[]) {

	// we create a QCoreApplication just so that QT loads image IO plugins (for jpg and tiff loading)
	QCoreApplication myUselessApp(argc, argv);
	setlocale(LC_ALL, "C");
	QLocale::setDefault(QLocale::C);

	QString input;
	QString output;
	QString ply;
	float coord_step = 0.0f; //approxismate step for quantization
	int position_bits = 0;
	float error_q = 0.1;
	int luma_bits = 6;
	int chroma_bits = 6;
	int alpha_bits = 5;
	int norm_bits = 10;
	float tex_step = 0.25;

	double error(-1.0);
	double max_size(0.0);
	int max_triangles(0.0);
	int max_level(-1);
	QString projection("");
	QString matrix("");
	QString imatrix("");
	QString compresslib("corto");

	bool info = false;
	bool check = false;
	bool compress = false;
	bool drop_level = false;
	QString recompute_error;

	GetOpt opt(argc, argv);
	QString help("ARGS specify a nexus or a ply file");
	opt.setHelp(help);

	opt.allowUnlimitedArguments(true); //able to join several inputs

	//info options
	opt.addSwitch('i', "info", "prints info about the nexus", &info);
	opt.addSwitch('n', "show nodes", "prints info about nodes", &show_nodes);
	opt.addSwitch('q', "show patches", "prints info about patches", &show_patches);
	opt.addSwitch('d', "show dag", "prints info about dag", &show_dag);
//	opt.addSwitch('c', "check", "performs various checks", &check); //DOESN'T WORK, TO CHECK

	//extraction options
	opt.addOption('o', "nexus file", "filename of the nexus output file", &output);
	opt.addOption('p', "ply or stl file", "filename of the ply or stl output file", &ply);

	opt.addOption('s', "size", "size in MegaBytes of the final file [requires -o]", &max_size);
	opt.addOption('e', "error", "remove nodes below this error from the node [requires -o]", &error);
	opt.addOption('t', "triangles", "drop nodes until total number of triangles is < triangles  [requires -o]", &max_triangles);
	opt.addOption('l', "level", "remove nodes above level <level>, (root level is zero)", &max_level);
	opt.addSwitch('L', "last level", "remove nodes from last level [requires -o]", &drop_level);

	//compression and quantization options
	opt.addSwitch('z', "compress", "compress patches", &compress);
	opt.addOption('Z', "compression library", "pick among compression libs [corto, meco (deprecated)], default corto [requires -z]", &compresslib);
	opt.addOption('v', "vertex quantization", "absolute side of quantization grid [requires -z]", &coord_step);
	opt.addOption('V', "vertex bits", "number of bits in vertex coordinates when compressing [requires -z]", &position_bits);
	opt.addOption('Y', "luma bits", "quantization of luma channel, default 6 [requires -z]", &luma_bits);
	opt.addOption('C', "chroma bits", "quantization of chroma channel, default 6 [requires -z]", &chroma_bits);
	opt.addOption('A', "alpha bits", "quantization of alpha channel, default 5 [requires -z]", &alpha_bits);
	opt.addOption('N', "normal bits", "quantization of normals, default 10 [requires -z]", &norm_bits);
	opt.addOption('T', "textures bits", "quantization of textures, default 0.25 [requires -z]", &tex_step);
	opt.addOption('Q', "quantization factor", "quantization as a factor of error, default 0.1 [requires -z]", &error_q);

	//other options
//	opt.addOption('E', "recompute error", "recompute error [average, quadratic, logarithmic, curvature]", &recompute_error); //DOESN'T WORK, TO CHECK
//	opt.addOption('m', "matrix", "multiply by matrix44 in format a:b:c... [requires -o]", &matrix); //DOESN'T WORK, TO CHECK
//	opt.addOption('M', "imatrix", "multiply by inverse of matrix44 in format a:b:c... [requires -o]", &imatrix); //DOESN'T WORK, TO CHECK
//	opt.addOption('P', "project file", "tex taylor project file", &projection); //DOESN'T WORK, TO CHECK

	opt.parse();

	//Check parameters are correct
	QStringList inputs = opt.arguments;
	if (inputs.size() == 0) {
		cerr << "Fatal error: no input files specified" << endl;
		cerr << qPrintable(opt.usage()) << endl;
		return -1;
	}
	else if (inputs.size() > 1) {
		cerr << "Fatal error: too many input files specified" << endl;
		cerr << qPrintable(opt.usage()) << endl;
		return -1;
	}

	NexusData nexus;
	nexus.file = new QTNexusFile();
	bool read_only = true;
	if(!recompute_error.isEmpty())
		read_only = false;

	try {
		if(!nexus.open(inputs[0].toLatin1())) {
			cerr << "Fatal error: could not open file " << qPrintable(inputs[0]) << endl;
			return -1;
		}

		if(info) {
			printInfo(nexus);
			return 0;
		}

		if (show_nodes) {
			printNodes(nexus);
			return 0;
		}

		if (show_dag) {
			printDag(nexus);
			return 0;
		}

		if (show_patches) {
			printPatches(nexus);
			return 0;
		}

		if (show_textures) {
			printTextures(nexus);
			return 0;
		}

		cout << "Reading " << qPrintable(inputs[0].toLatin1()) << endl;

		if(check) {
			checks(nexus);
			return 0;
		}

		if(!recompute_error.isEmpty()) {
			recomputeError(nexus, recompute_error);
			return 0;
		}

		if(compress && output.isEmpty()) {
			output = inputs[0].left(inputs[0].length()-4) + ".nxz";
		}
		if(output.isEmpty() && ply.isEmpty()) return 0;
		if(!output.isEmpty() && !ply.isEmpty())  {
			cerr << "Fatal error: the output can be a ply file or a nexus file, not both" << endl;
			return -1;
		}
		if(output == inputs[0]) {
			cerr << "Fatal error: output and input file must be different" << endl;
			return -1;
		}
		Extractor extractor(&nexus);

		if(max_size != 0.0)
			extractor.selectBySize(max_size*(1<<20));

		if(error != -1)
			extractor.selectByError(error);

		if(max_triangles != 0)
			extractor.selectByTriangles(max_triangles);

		if(max_level >= 0)
			extractor.selectByLevel(max_level);

		if(drop_level)
			extractor.dropLevel();

		if(!ply.isEmpty()) {       //export to ply
			if(ply.endsWith("stl") || ply.endsWith("STL"))
				extractor.saveStl(ply);
			else
				extractor.saveUnifiedPly(ply);
			cout << "Saving to file " << qPrintable(ply) << endl;
		} else if(!output.isEmpty()) { //export to nexus

			bool invert = false;
			if(!imatrix.isEmpty()) {
				matrix = imatrix;
				invert = true;
			}
			if(!matrix.isEmpty()) {
				QStringList sl = matrix.split(":");
				if(sl.size() != 16) {
					cerr << "Wrong matrix: found only " << sl.size() << " elements" << endl;
					exit(-1);
				}
				vcg::Matrix44f m;
				for(int i = 0; i < sl.size(); i++)
					m.V()[i] = sl.at(i).toFloat();
				//if(invert)
				//    m = vcg::Invert(m);

				extractor.setMatrix(m);
			}

			Signature signature = nexus.header.signature;
			if(compress) {
				signature.flags &= ~(Signature::MECO | Signature::CORTO);
				if(compresslib == "meco")
					signature.flags |= Signature::MECO;
				else if(compresslib == "corto")
					signature.flags |= Signature::CORTO;
				else {
					cerr << "Unknown compression method: " << qPrintable(compresslib) << endl;
					exit(-1);
				}
				if(coord_step) {  //global precision, absolute value
					extractor.error_factor = 0.0; //ignore error factor.
					//do nothing
				} else if(position_bits) {
					vcg::Sphere3f &sphere = nexus.header.sphere;
					coord_step = sphere.Radius()/pow(2.0f, position_bits);
					extractor.error_factor = 0.0;

				} else if(error_q) {
					//take node 0:
					uint32_t sink = nexus.header.n_nodes -1;
					coord_step = error_q*nexus.nodes[0].error/2;
					for(unsigned int i = 0; i < sink; i++){
						Node &n = nexus.nodes[i];
						Patch &patch = nexus.patches[n.first_patch];
						if(patch.node != sink)
							continue;
						double e = error_q*n.error/2;
						if(e < coord_step && e > 0)
							coord_step = e; //we are looking at level1 error, need level0 estimate.
					}
					extractor.error_factor = error_q;
				}
				cout << "Vertex quantization step: " << coord_step << endl;
				cout << "Texture quantization step: " << tex_step << endl;
				extractor.coord_q =(int)log2(coord_step);
				extractor.norm_bits = norm_bits;
				extractor.color_bits[0] = luma_bits;
				extractor.color_bits[1] = chroma_bits;
				extractor.color_bits[2] = chroma_bits;
				extractor.color_bits[3] = alpha_bits;
				extractor.tex_step = tex_step; //was (int)log2(tex_step * pow(2, -12));, moved to per node value
				//cout << "Texture step: " << extractor.tex_step << endl;
			}

			cout << "Saving with flag: " << signature.flags;
			if (signature.flags & Signature::MECO) cout << " (compressed with MECO)";
			else if (signature.flags & Signature::CORTO) cout << " (compressed with CORTO)";
			else cout << " (not compressed)";
			cout << endl;

			if(!output.endsWith(".nxs") && !output.endsWith(".nxz")) output += ".nxs";

			extractor.save(output, signature);
			//builder.copy(nexus, out, selection);

			/*        builder.create(out, signature);
		if(q_position.toDouble() != 0.0)
			builder.encoder.geometry.options.precision = q_position.toDouble();
		if(position_bits.toInt() != 0)
			builder.encoder.geometry.options.coord_q = position_bits.toInt();
		if(matrix != "") {
			QStringList sl = matrix.toString().split(":");
			if(sl.size() != 16) {
				cerr << "Wrong matrix: found only " << sl.size() << " elements.\n";
				exit(-1);
			}
			vcg::Matrix44f m;
			for(int i = 0; i < sl.size(); i++)
				m.V()[i] = sl.at(i).toFloat();
			builder.setMatrix(m);
		}
		builder.process(nexus, selection); */
		cout << "Saving to file " << qPrintable(output) << endl;
		}

/*		if(projection != "") {
			//call function to actually project color.
			texProject(NexusBuilder &builder, QString dat_file);
			//for every patch
			Patch loadPatch(quint32 patch);
			//void unloadPatch(quint32 patch, char *mem); (patch.start)

			//for every image
			//check intersection
			//project

		} */
	}
	catch (QString error) {
		cout << "Fatal error: " << qPrintable(error) << endl;
		return -1;
	}
	catch (const char *error) {
		cout << "Fatal error: " << error << endl;
		return -1;
	}

	return 0;
}

void printInfo(NexusData &nexus) {
	Header &header = nexus.header;
	cout << "\nTot vertices: " << header.nvert << endl;
	cout << "Tot faces   : " << header.nface << endl;

	cout << "Components  :";
	if(!header.signature.face.hasIndex()) cout << " pointcloud";
	else  cout << " mesh";
	if(header.signature.vertex.hasNormals()) cout << " normals";
	if(header.signature.vertex.hasColors()) cout << " colors";
	if (header.signature.vertex.hasTextures()) cout << " textures";
	cout << endl;

	cout << "Flag        : " << header.signature.flags;
	if(header.signature.flags & Signature::MECO) cout << " (compressed with MECO)";
	else if(header.signature.flags & Signature::CORTO) cout << " (compressed with CORTO)";
	else cout << " (not compressed)";
	cout << endl;

	vcg::Point3f c = header.sphere.Center();
	float r = header.sphere.Radius();
	cout << "Sphere      : c [" << c[0] << "," << c[1] << "," << c[2] << "] r " << r << endl;
	cout << "Nodes       : " << header.n_nodes << endl;
	cout << "Patches     : " << header.n_patches << endl;
	cout << "Textures    : " << header.n_textures << endl;
}

void printNodes(NexusData& nexus) {
	Header& header = nexus.header;
	uint32_t n_nodes = header.n_nodes;
	int last_level_size = 0;
	uint32_t sink = nexus.header.n_nodes - 1;
	if (show_nodes) {
		cout << "\nNode dump:\n";
		double mean = 0;
		for (uint i = 0; i < n_nodes - 1; i++) {
			nx::Node& node = nexus.nodes[i];

			if (nexus.patches[node.first_patch].node == sink)
				last_level_size += node.getEndOffset() - node.getBeginOffset();

			int n = nexus.header.signature.face.hasIndex() ? node.nface : node.nvert;
			//compute primitives
			cout << "Node: " << i << "\t  Error: " << node.error << "\t"
				<< " Sphere r: " << node.sphere.Radius() << "\t"
				<< "Primitives: " << n << "\t"
				<< "Size: " << node.getSize() << "\n";

			mean += node.nface;
		}
		cout << endl;
		mean /= n_nodes;
		double std = 0;
		for (uint i = 0; i < n_nodes; i++)
			std += pow(mean - nexus.nodes[i].nface, 2);
		std /= n_nodes;
		std = sqrt(std);
		cout << "Mean faces: " << mean << " Standard deviation: " << std << endl;
		cout << "Last level size: " << last_level_size / (1024 * 1024) << "MB\n";
	}
}

void printDag(NexusData& nexus) {
	Header& header = nexus.header;
	uint32_t n_nodes = header.n_nodes;
	if (show_dag) {
		cout << "\nDag dump: \n";
		for (uint i = 0; i < n_nodes - 1; i++) {
			nx::Node& node = nexus.nodes[i];
			cout << "Node: " << i << "\t";
			for (uint k = node.first_patch; k < node.last_patch(); k++)
				cout << "[" << nexus.patches[k].node << "] ";
			cout << "\n";
		}
	}
}

void printPatches(NexusData& nexus) {
	if (show_patches) {
		cout << "\nPatch dump:\n";
		for (uint i = 0; i < nexus.header.n_patches; i++) {
			nx::Patch& patch = nexus.patches[i];
			cout << "Patch: " << i << "\t Offset: " << patch.triangle_offset << "\t Texture: " << patch.texture << "\n";
		}
	}
}

void printTextures(NexusData& nexus) {
	if (show_textures) {
			cout << "\nTexture dump: \n";
			for (uint i = 0; i < nexus.header.n_textures; i++) {
				nx::Texture& texture = nexus.textures[i];
				cout << "Texture: " << i << "\t Offset: " << texture.getBeginOffset() << " \t size: " << texture.getSize() << "\n";
			}
		}
}

void recomputeError(NexusData &nexus, QString error_method) {
	enum Method { AVERAGE, QUADRATIC, LOGARITHMIC, CURVATURE };
	Method method;

	if(error_method == "average") {
		method = AVERAGE;
	} else if(error_method == "quadratic") {
		method = QUADRATIC;
	} else if(error_method == "logarithmic") {
		method = LOGARITHMIC;
	} else if(error_method == "curvature") {
		method = CURVATURE;
	} else {
		cerr << "Not a valid error method: " << qPrintable(error_method) << endl;
		exit(0);
	}

	float min_error = 1e20;

	for(uint i = 0; i < nexus.header.n_nodes-1; i++) {
		nx::Node &node = nexus.nodes[i];
		float error = 0;
		int count = 0;
		nexus.loadRam(i);

		NodeData &data = nexus.nodedata[i];
		vcg::Point3f *coords = data.coords();
		uint16_t *faces = data.faces(nexus.header.signature, node.nvert);

		switch(method) {

		case AVERAGE:
			for(int i = 0; i < node.nface; i++) {
				for(int k = 0; k < 3; k++) {
					int v0 = faces[i*3 + k];
					int v1 = faces[i*3 + ((k+1)%3)];
					//this computes average
					float err = (coords[v0] - coords[v1]).SquaredNorm();
					error += sqrt(err);
					count++;
				}
			}
			break;

		case QUADRATIC:
			for(int i = 0; i < node.nface; i++) {
				for(int k = 0; k < 3; k++) {
					int v0 = faces[i*3 + k];
					int v1 = faces[i*3 + ((k+1)%3)];
					error +=(coords[v0] - coords[v1]).SquaredNorm();
					count++;
				}
			}
			break;

		case LOGARITHMIC:
			for(int i = 0; i < node.nface; i++) {
				for(int k = 0; k < 3; k++) {
					int v0 = faces[i*3 + k];
					int v1 = faces[i*3 + ((k+1)%3)];

					float err = (coords[v0] - coords[v1]).SquaredNorm();
					error += log(err); //this is actually 2*error because of the missing square root
					count++;
				}
			}
			break;

		case CURVATURE:
			assert(0);
			break;
		}


		nexus.dropRam(i);
		if(count > 0) {
			switch(method) {
			case AVERAGE: error /= count; break;
			case QUADRATIC: error = sqrt(error/count); break;
			case LOGARITHMIC: error = exp(error/(2*count)); //2 accounts for the square roots
			case CURVATURE: break;
			}
		}
		if(error > 0 && error < min_error)
			min_error = error;
		node.error = error;
	}

	nexus.nodes[nexus.header.n_nodes - 1].error = min_error;

	nexus.file->seek(sizeof(Header));
	nexus.file->write((char *)nexus.nodes, sizeof(nx::Node)* nexus.header.n_nodes);
	//fseek(nexus.file, sizeof(Header), SEEK_SET);
	//fwrite(nexus.nodes, sizeof(nx::Node), nexus.header.n_nodes, nexus.file);
}

void checks(NexusData &nexus) {
	uint32_t n_nodes = nexus.header.n_nodes;
	uint32_t n_patches = nexus.header.n_patches;
	uint32_t n_textures = nexus.header.n_textures;

	for(uint i = 0; i < n_nodes-1; i++) {
		Node &node = nexus.nodes[i];
		if(node.first_patch >= n_patches) {
			cout << "Node " << i << " of " << n_nodes << " First patch " << node.first_patch << " of " << n_patches << endl;
			exit(-1);
		}
		assert(node.offset * (quint64)NEXUS_PADDING < nexus.file->size());
	}
	assert(nexus.nodes[n_nodes-1].first_patch = n_patches);

	for(uint i = 0; i < n_patches-1; i++) {
		Patch &patch = nexus.patches[i];
		assert(patch.node < n_nodes);
		if(patch.texture == 0xffffffff) continue;
		if(patch.texture >= n_textures) {
			cout << "Patch " << i << " of " << n_patches << " Texture " << patch.texture << " of " << n_textures << endl;
			exit(-1);
		}
	}
	if(n_textures)
		assert(nexus.patches[n_patches-1].texture = n_textures);

	for(uint i = 0; i < n_textures-1; i++) {
		Texture &texture = nexus.textures[i];
		if(texture.offset * (quint64)NEXUS_PADDING >= nexus.file->size()) {
			cout << "Texture " << i << " offset " << texture.offset*NEXUS_PADDING << " file size " << nexus.file->size() << endl;
		}
	}
	if(nexus.textures[n_textures-1].offset*NEXUS_PADDING != nexus.file->size()) {
		cout << "Last texture " << nexus.textures[n_textures-1].offset*NEXUS_PADDING <<
				"File size " << nexus.file->size() << endl;
	}
	return;
}

//check bounding spheres
void checkSpheres(NexusData &nexus) {

	uint32_t n_nodes = nexus.header.n_nodes;
	for(int i = n_nodes -2; i >= 0; i--) {
		Node &node = nexus.nodes[i];
		vcg::Sphere3f sphere = node.tightSphere();
		nexus.loadRam(i);
		NodeData &data = nexus.nodedata[i];

		vcg::Point3f *coords = data.coords();
		float new_radius = sphere.Radius();
		for(int k = 0; k < node.nvert; k++) {
			vcg::Point3f &p = coords[k];
			float dist = (p - sphere.Center()).Norm();
			if(dist > new_radius)
				new_radius = dist;
		}
		if(node.tight_radius != new_radius) {
			if(node.tight_radius*2 < new_radius) {
				cout << "Offset " << node.offset*NEXUS_PADDING << endl;
			}
			cout << "Node " << i << " radius " << node.tight_radius << " -> " << new_radius << endl;
		}
		node.tight_radius = new_radius;
		nexus.dropRam(i);
	}

	//write back nodes.
	nexus.file->seek(sizeof(Header));
	nexus.file->write((char *)nexus.nodes, sizeof(nx::Node)* nexus.header.n_nodes);
	//fseek(nexus.file, sizeof(Header), SEEK_SET);
	//fwrite(nexus.nodes, sizeof(Node), n_nodes, nexus.file);
}
