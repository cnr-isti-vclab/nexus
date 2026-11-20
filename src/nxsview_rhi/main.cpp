/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (http://www.gnu.org/licenses/gpl.txt).
*/
#include <iostream>
#include <cstdint>

#include <QApplication>
#include <QCheckBox>
#include <QCommandLineParser>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "rhi_nxsview.h"

namespace {

class MainWindow : public QWidget {
public:
	explicit MainWindow(QWidget *parent = nullptr): QWidget(parent) {
		setWindowTitle(tr("NxsView - ISTI-CNR"));
		auto *layout = new QHBoxLayout(this);
		area = new RhiNxsview(this);
		area->setMinimumSize(800, 600);
		layout->addWidget(area, 1);

		auto *controls = new QVBoxLayout;
		layout->addLayout(controls);

		auto *viewGroup = new QGroupBox(tr("View:"), this);
		auto *viewLayout = new QVBoxLayout(viewGroup);
		triangles = new QCheckBox(tr("Triangles"), viewGroup);
		normals = new QCheckBox(tr("Normals"), viewGroup);
		colors = new QCheckBox(tr("Colors"), viewGroup);
		patches = new QCheckBox(tr("Patches"), viewGroup);
		light = new QPushButton(tr("Light"), viewGroup);
		light->setCheckable(true);
		snapshot = new QPushButton(tr("Snapshot"), viewGroup);
		info = new QCheckBox(tr("Info"), viewGroup);
		for(QCheckBox *check : {triangles, normals, colors, patches})
			check->setChecked(true);
		patches->setChecked(false);
		viewLayout->addWidget(triangles);
		viewLayout->addWidget(normals);
		viewLayout->addWidget(colors);
		viewLayout->addWidget(patches);
		viewLayout->addWidget(light);
		viewLayout->addWidget(snapshot);

		fov = new QSpinBox(viewGroup);
		fov->setRange(5, 120);
		fov->setSingleStep(10);
		fov->setValue(30);
		auto *viewForm = new QFormLayout;
		viewForm->addRow(tr("Fov"), fov);
		viewLayout->addLayout(viewForm);
		viewLayout->addWidget(info);
		controls->addWidget(viewGroup);

		auto *colorsGroup = new QGroupBox(tr("Colors:"), this);
		auto *colorsLayout = new QVBoxLayout(colorsGroup);
		top = new QPushButton(tr("top"), colorsGroup);
		bottom = new QPushButton(tr("bottom"), colorsGroup);
		meshColor = new QPushButton(tr("mesh"), colorsGroup);
		colorsLayout->addWidget(top);
		colorsLayout->addWidget(bottom);
		colorsLayout->addWidget(meshColor);
		controls->addWidget(colorsGroup);

		auto *parametersGroup = new QGroupBox(tr("Parameters:"), this);
		auto *parametersLayout = new QFormLayout(parametersGroup);
		extract = new QCheckBox(tr("Extract"), parametersGroup);
		extract->setChecked(true);
		mtri = new QDoubleSpinBox(parametersGroup);
		mtri->setDecimals(1);
		mtri->setMaximum(100);
		error = new QDoubleSpinBox(parametersGroup);
		error->setDecimals(1);
		error->setMaximum(100);
		gpu = new QDoubleSpinBox(parametersGroup);
		gpu->setMaximum(24000);
		gpu->setSingleStep(10);
		ram = new QDoubleSpinBox(parametersGroup);
		ram->setMaximum(48000);
		ram->setSingleStep(100);
		parametersLayout->addRow(extract);
		parametersLayout->addRow(tr("MTri"), mtri);
		parametersLayout->addRow(tr("Error"), error);
		parametersLayout->addRow(tr("Gpu"), gpu);
		parametersLayout->addRow(tr("Ram"), ram);
		controls->addWidget(parametersGroup);
		controls->addStretch();

		connect(light, &QPushButton::toggled, area, &RhiNxsview::lightTrackball);
		connect(snapshot, &QPushButton::clicked, area, &RhiNxsview::getSnapshot);
		connect(triangles, &QCheckBox::toggled, area, &RhiNxsview::setTriangles);
		connect(normals, &QCheckBox::toggled, area, &RhiNxsview::setNormals);
		connect(colors, &QCheckBox::toggled, area, &RhiNxsview::setColors);
		connect(patches, &QCheckBox::toggled, area, &RhiNxsview::setPatches);
		connect(fov, &QSpinBox::valueChanged, area, &RhiNxsview::setFov);
		connect(info, &QCheckBox::toggled, area, &RhiNxsview::setInfo);
		connect(extract, &QCheckBox::toggled, area, &RhiNxsview::setExtraction);
		connect(mtri, &QDoubleSpinBox::valueChanged, area, &RhiNxsview::setPrimitives);
		connect(error, &QDoubleSpinBox::valueChanged, area, &RhiNxsview::setError);
		connect(gpu, &QDoubleSpinBox::valueChanged, area, &RhiNxsview::setGpu);
		connect(ram, &QDoubleSpinBox::valueChanged, area, &RhiNxsview::setRam);
		connect(top, &QPushButton::clicked, area, &RhiNxsview::setColorTop);
		connect(bottom, &QPushButton::clicked, area, &RhiNxsview::setColorBottom);
		connect(meshColor, &QPushButton::clicked, area, &RhiNxsview::setColor);
	}

	RhiNxsview *area;
	QCheckBox *triangles;
	QCheckBox *normals;
	QCheckBox *colors;
	QCheckBox *patches;
	QPushButton *light;
	QPushButton *snapshot;
	QCheckBox *info;
	QSpinBox *fov;
	QPushButton *top;
	QPushButton *bottom;
	QPushButton *meshColor;
	QCheckBox *extract;
	QDoubleSpinBox *mtri;
	QDoubleSpinBox *error;
	QDoubleSpinBox *gpu;
	QDoubleSpinBox *ram;
};

} // namespace

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	MainWindow window;

	QCommandLineParser opt;
	opt.setApplicationDescription("ARGS specify a nexus file (specify more to join several nexus)");
	opt.addHelpOption();
	opt.addPositionalArgument("files", "nexus files", "[files...]");

	auto addValue = [&opt](const char *name, const char *shortName,
	                       const char *description, const char *defaultValue) {
		opt.addOption(QCommandLineOption({shortName, name}, description, name, defaultValue));
	};
	addValue("draw", "d", "max number of triangles (millions)", "3");
	addValue("error", "e", "target error in pixels", "3");
	addValue("ram", "m", "max ram used (in Mb)", "500");
	addValue("video", "g", "max video ram used (in Mb)", "250");
	addValue("cache", "c", "max number of items in cache", "100000");
	addValue("fov", "v", "field of view in degrees", "30");
	addValue("height", "H", "height of the window", "600");
	addValue("width", "w", "width of the window", "800");
	addValue("fps", "f", "target frames per second", "0");
	addValue("instances", "i", "number of instances", "1");
	addValue("prefetch", "p", "amount of node prefetching in prioritizer", "100");
	opt.addOption(QCommandLineOption({"a", "autopositioning"}, "distribute models in a grid"));
	opt.process(app);

	const double draw = opt.value("draw").toDouble();
	const double targetError = opt.value("error").toDouble();
	const double ramCapacity = opt.value("ram").toDouble();
	const double gpuCapacity = opt.value("video").toDouble();
	const double cache = opt.value("cache").toDouble();
	const int fov = opt.value("fov").toInt();
	const int width = opt.value("width").toInt();
	const int height = opt.value("height").toInt();
	const double fps = opt.value("fps").toDouble();
	const int instances = opt.value("instances").toInt();

	QStringList inputs = opt.positionalArguments();
	if(inputs.isEmpty()) {
		inputs = QFileDialog::getOpenFileNames(
			&window, "Import Nxs/Nxz", QDir::homePath(),
			"All known files (*.nxs *.nxz);;nxs file (*.nxs);;nxz file (*.nxz)");
		if(inputs.isEmpty()) {
			std::cerr << "No input files specified.\n"
			          << qPrintable(opt.helpText()) << '\n';
			return -1;
		}
	}

	window.area->scene.autopositioning = opt.isSet("autopositioning");
	window.area->scene.load(inputs, instances);
	window.setGeometry(100, 100, width, height);

	if(targetError < 0) {
		std::cerr << "Invalid error: " << targetError << '\n';
		return -1;
	}
	if(ramCapacity < 10) {
		std::cerr << "Ram parameter too low: " << ramCapacity << '\n';
		return -1;
	}
	if(gpuCapacity < 10) {
		std::cerr << "Extr parameter too low: " << gpuCapacity << '\n';
		return -1;
	}
	if(draw < 0.5) {
		std::cerr << "Draw parameter too low: " << draw << '\n';
		return -1;
	}
	if(cache < 10000) {
		std::cerr << "Disk parameter too low: " << cache << '\n';
		return -1;
	}

	nx::Controller &controller = window.area->scene.controller;
	controller.setRam(static_cast<std::uint64_t>(ramCapacity * (1 << 20)));
	window.area->setGpu(gpuCapacity);
	controller.start();

	nx::Renderer &renderer = window.area->renderer;
	renderer.setMaxPrimitives(static_cast<std::uint32_t>(draw * (1 << 20)));
	renderer.setError(static_cast<float>(targetError));
	renderer.setFps(static_cast<float>(fps));

	window.mtri->setValue(draw);
	window.error->setValue(targetError);
	window.ram->setValue(ramCapacity);
	window.gpu->setValue(gpuCapacity);
	window.area->setFov(fov);
	window.fov->setValue(fov);

	window.show();
	try {
		app.exec();
	} catch(const QString &error) {
		std::cout << "Fatal error: " << qPrintable(error) << '\n';
	}
	return 0;
}
