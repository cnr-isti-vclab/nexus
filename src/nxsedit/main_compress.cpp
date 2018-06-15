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

#include "../nxsbuild/nexusbuilder.h"
#include "../common/traversal.h"
#include "extractor.h"

#include <QtGui>

using namespace std;
using namespace nx;

//TODO REMOVE unused textures when resizing nexus.
int main(int argc, char *argv[]) {

	// we create a QCoreApplication just so that QT loads image IO plugins (for jpg and tiff loading)
	QCoreApplication myUselessApp(argc, argv);
	setlocale(LC_ALL, "C");
	QLocale::setDefault(QLocale::C);

	QString input;
	QString output;
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
	QString projection("");
	QString matrix("");
	QString imatrix("");
	QString compresslib("corto");

	bool compress = true;

	GetOpt opt(argc, argv);
	QString help("ARGS specify a nexus file");
	opt.setHelp(help);

	opt.allowUnlimitedArguments(true); //able to join several inputs

	//extraction options
	opt.addOption('o', "nexus file", "filename of the nexus output file", &output);

	//compression and quantization options
	opt.addOption('Z', "compression library", "pick among compression libs [corto, meco], default corto", &compresslib);
	opt.addOption('v', "vertex quantization", "absolute side of quantization grid, default 0.0 (uses quantization factor, instead)", &coord_step);
	opt.addOption('V', "vertex bits", "number of bits in vertex coordinates when compressing default, 0 (uses quantization factor, instead)", &position_bits);
	opt.addOption('Q', "quantization factor", "quantization as a factor of error, default 0.1", &error_q);
	opt.addOption('Y', "luma bits", "quantization of luma channel, default 6", &luma_bits);
	opt.addOption('C', "chroma bits", "quantization of chroma channel, default 6", &chroma_bits);
	opt.addOption('A', "alha bits", "quantization of alpha channel, default 5", &alpha_bits);
	opt.addOption('N', "normal bits", "quantization of normals, default 10", &norm_bits);
	opt.addOption('T', "textures bits", "quantization of textures, default 0.25", &tex_step);

	opt.parse();

	//Check parameters are correct
	QStringList inputs = opt.arguments;
	if (inputs.size() == 0) {
		cerr << "No input files specified" << endl;
		cerr << qPrintable(opt.usage()) << endl;
		return -1;
	}
	else if (inputs.size() > 1) {
		cerr << "Too many input files specified" << endl;
		cerr << qPrintable(opt.usage()) << endl;
		return -1;
	}

	NexusData nexus;
	bool read_only = true;

	try {
		if(!nexus.open(inputs[0].toLatin1())) {
			cerr << "Could not open file " << qPrintable(inputs[0]) << endl;
			return 0;
		}

		cout << "Reading " << qPrintable(inputs[0].toLatin1()) << endl;

		if(compress && output.isEmpty()) {
			output = inputs[0].left(inputs[0].length()-4) + ".nxz";
		}

		if(output == inputs[0]) {
			cerr << "Output and input file must be different" << endl;
			return -1;
		}
		Extractor extractor(&nexus);

	  if(!output.isEmpty()) { //export to nexus

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

		cout << "Saving to file " << qPrintable(output) << endl;
		}

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



