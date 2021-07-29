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
#include <QApplication>
#include <QWidget>
#include <QImageIOPlugin>
#include <QFileDialog>

#include <wrap/system/qgetopt.h>

#include "scene.h"
#include "gl_nxsview.h"
#include "ui_nxsview.h"

#include <iostream>

using namespace std;

class QMyWindow: public QWidget {
public:
	QMyWindow(QWidget *parent): QWidget(parent) {
		ui.setupUi(this);
		connect(ui.light,     SIGNAL(clicked (bool)),        ui.area, SLOT(lightTrackball(bool)));
		connect(ui.snapshot,  SIGNAL(clicked()),             ui.area, SLOT(getSnapshot()));
		connect(ui.triangles, SIGNAL(toggled(bool)),         ui.area, SLOT(setTriangles(bool)));
		connect(ui.normals,   SIGNAL(toggled(bool)),         ui.area, SLOT(setNormals(bool)));
		connect(ui.colors,    SIGNAL(toggled(bool)),         ui.area, SLOT(setColors(bool)));
		connect(ui.patches,   SIGNAL(toggled(bool)),         ui.area, SLOT(setPatches(bool)));
		connect(ui.fov,       SIGNAL(valueChanged(int)),     ui.area, SLOT(setFov(int)));
		connect(ui.info,      SIGNAL(clicked (bool)),        ui.area, SLOT(setInfo(bool)));


		connect(ui.extract,  SIGNAL(toggled(bool)),         ui.area, SLOT(setExtraction(bool)));
		connect(ui.mtri,  SIGNAL(valueChanged(double)),    ui.area, SLOT(setPrimitives(double)));
		connect(ui.error,  SIGNAL(valueChanged(double)),    ui.area, SLOT(setError(double)));
		connect(ui.gpu,     SIGNAL(valueChanged(double)),  ui.area, SLOT(setGpu(double)));
		connect(ui.ram,     SIGNAL(valueChanged(double)),  ui.area, SLOT(setRam(double)));
		connect(ui.top,      SIGNAL(clicked()),             ui.area, SLOT(setColorTop()));
		connect(ui.bottom,   SIGNAL(clicked()),             ui.area, SLOT(setColorBottom()));
		connect(ui.color,    SIGNAL(clicked()),             ui.area, SLOT(setColor()));
	}

	Ui::Form ui;
};


int main(int argc, char *argv[]) {
	QApplication::setAttribute(Qt::AA_X11InitThreads);
	QApplication app(argc, argv);
	QMyWindow *window = new QMyWindow(NULL);

	QVariant draw(3.0), error(3.0f), ram(500.0f), gpu(250.0f), cache(100000),
			fov(30), width(800), height(600), fps(0.0f), instances(1), prefetch(100);

	bool fullscreen;
	bool backface = false;
	bool occlusion = false;
	bool dontshare = false;
	bool autopositioning = false;

	GetOpt opt(argc, argv);
	QString help("ARGS specify a nexus file (specify more ply files or the directory containing them to get a merged output)");
	opt.setHelp(help);

	opt.allowUnlimitedArguments(true); //able to join several nexus

//	GetOpt opt(argc, argv);
//	opt.allowUnlimitedArguments(true); //able to join several plys

	opt.addOption('d', "draw", "max number of triangles", &draw);
	opt.addOption('e', "error", "target error in pixels", &error);
	opt.addOption('m', "ram", "max ram used (in Mb)", &ram);
	opt.addOption('g', "video", "max video ram used (in Mb)", &gpu);
	opt.addOption('c', "cache", "max number of items in cache", &cache);
	opt.addOption('v', "fov", "field of view in degrees", &fov);
	opt.addOption('h', "height", "height of the window", &height);
	opt.addOption('w', "width", "width of the window", &width);

	opt.addSwitch('s', "fullscreen", "fullscreen mode", &fullscreen);
	opt.addSwitch('b', "backface", "backface culling", &backface);
	opt.addSwitch('o', "occlusion", "occlusion culling", &occlusion);
	opt.addSwitch('S', "dontshare", "do not use another thread for textures", &dontshare);

	opt.addOption('f', "fps", "target frames per second", &fps);
	opt.addOption('i', "instances", "number of instances", &instances);
	opt.addSwitch('a', "autopositioning", "distribute models ina a grid", &autopositioning);
	opt.addOption('p', "prefetch", "amount of node prefetching in prioritizer", &prefetch);


//	QString help = "This is just a quick tool to visualize nexus models.";
//	opt.setHelp(help);
	opt.parse();

	Ui::Form &ui = window->ui;
	GLNxsview *area = ui.area;
	if(dontshare)
		area->share = false;

	QStringList inputs = opt.arguments;
	if(inputs.size() == 0) {
		inputs = QFileDialog::getOpenFileNames(
					window, "Import Nxs/Nxz",
					QDir::home().path(),
					"All known files (*.nxs *.nxz);;nxs file (*.nxs);;nxz file (*.nxz)");
		if (inputs.size() == 0){
			cerr << "No input files specified.\n";
			cerr << qPrintable(opt.usage()) << "\n";
			return -1;
		}
	}

	Scene &scene = ui.area->scene;
	scene.autopositioning = autopositioning;
	scene.load(inputs, instances.toInt());

	window->setGeometry(100,100,width.toInt(),height.toInt());
	if(error.toDouble() < 0) {
		cerr << "Invalid error: " << error.toDouble() << endl;
		return -1;
	}

	if(ram.toDouble() < 10) {
		cerr << "Ram parameter too low: " << ram.toDouble() << endl;
		return -1;
	}

	if(gpu.toDouble() < 10) {
		cerr << "Extr parameter too low: " << gpu.toDouble() << endl;
		return -1;
	}

	if(draw.toDouble() < 0.5) {
		cerr << "Draw parameter too low: " << draw.toDouble() << endl;
		return -1;
	}

	if(cache.toDouble() < 10000) {
		cerr << "Disk parameter too low: " << cache.toDouble() << endl;
		return -1;
	}
	nx::Controller &controller = scene.controller;
	controller.setWidget(area);
	controller.setRam((quint64)(ram.toDouble()*(1<<20)));
	controller.setGpu((quint64)(gpu.toDouble()*(1<<20)));
	controller.start();

	nx::Renderer &renderer = ui.area->renderer;
	renderer.setMaxPrimitives((int)(draw.toDouble()*(1<<20)));
	renderer.setError((float)error.toDouble());
	renderer.setFps((float)fps.toDouble());

	ui.mtri->setValue(draw.toDouble());
	ui.error->setValue(error.toDouble());
	ui.ram->setValue(ram.toDouble());
	ui.gpu->setValue(gpu.toDouble());

	area->setFov(fov.toDouble());
	ui.fov->setValue(fov.toDouble());

	window->show();
	try {
		app.exec();
	} catch(QString error) {
		cout << "Fatal error: " << qPrintable(error) << endl;
	}
	return 0;
}

