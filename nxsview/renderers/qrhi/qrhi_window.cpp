#include "qrhi_window.h"

#include "rhi_nxsview.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

QrhiWindow::QrhiWindow(QWidget *parent): QWidget(parent) {
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
