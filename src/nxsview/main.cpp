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

#include <QCommandLineParser>

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

	QCommandLineParser opt;
	opt.setApplicationDescription("ARGS specify a nexus file (specify more to join several nexus)");
	opt.addHelpOption();
	opt.addPositionalArgument("files", "nexus files", "[files...]");

	auto addValue = [&opt](const char *name, const char *shortName, const char *desc, const char *def) {
		opt.addOption(QCommandLineOption({shortName, name}, desc, name, def));
	};
	addValue("draw",      "d", "max number of triangles (millions)",    "3");
	addValue("error",     "e", "target error in pixels",                "3");
	addValue("ram",       "m", "max ram used (in Mb)",                  "500");
	addValue("video",     "g", "max video ram used (in Mb)",           "250");
	addValue("cache",     "c", "max number of items in cache",          "100000");
	addValue("fov",       "v", "field of view in degrees",              "30");
	addValue("height",    "H", "height of the window",                  "600");
	addValue("width",     "w", "width of the window",                   "800");
	addValue("fps",       "f", "target frames per second",              "0");
	addValue("instances", "i", "number of instances",                   "1");
	addValue("prefetch",  "p", "amount of node prefetching in prioritizer", "100");
	opt.addOption(QCommandLineOption({"S", "dontshare"}, "do not use another thread for textures"));
	opt.addOption(QCommandLineOption({"a", "autopositioning"}, "distribute models in a grid"));
	opt.process(app);

	QVariant draw(opt.value("draw").toDouble()), error(opt.value("error").toDouble()),
			ram(opt.value("ram").toDouble()), gpu(opt.value("video").toDouble()),
			cache(opt.value("cache").toDouble()), fov(opt.value("fov").toDouble()),
			width(opt.value("width").toInt()), height(opt.value("height").toInt()),
			fps(opt.value("fps").toDouble()), instances(opt.value("instances").toInt());
	bool dontshare = opt.isSet("dontshare");
	bool autopositioning = opt.isSet("autopositioning");

	Ui::Form &ui = window->ui;
	GLNxsview *area = ui.area;
	if(dontshare)
		area->share = false;

	QStringList inputs = opt.positionalArguments();
	if(inputs.size() == 0) {
		inputs = QFileDialog::getOpenFileNames(
					window, "Import Nxs/Nxz",
					QDir::home().path(),
					"All known files (*.nxs *.nxz);;nxs file (*.nxs);;nxz file (*.nxz)");
		if (inputs.size() == 0){
			cerr << "No input files specified.\n";
			cerr << qPrintable(opt.helpText()) << "\n";
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

