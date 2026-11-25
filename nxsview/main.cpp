#include <iostream>
#include <memory>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileDialog>
#include <QSurfaceFormat>
#include <QWidget>

#include "nxsview.h"
#include "renderers/opengl/gl_nxsview.h"
#include "renderers/qrhi/rhi_nxsview.h"
#include "renderers/qrhi/qrhi_window.h"
#include "ui_nxsview.h"

class QMyWindow : public QWidget {
public:
	explicit QMyWindow(QWidget *parent): QWidget(parent) {
		ui.setupUi(this);
		connect(ui.light, SIGNAL(clicked(bool)), ui.area, SLOT(lightTrackball(bool)));
		connect(ui.snapshot, SIGNAL(clicked()), ui.area, SLOT(getSnapshot()));
		connect(ui.triangles, SIGNAL(toggled(bool)), ui.area, SLOT(setTriangles(bool)));
		connect(ui.normals, SIGNAL(toggled(bool)), ui.area, SLOT(setNormals(bool)));
		connect(ui.colors, SIGNAL(toggled(bool)), ui.area, SLOT(setColors(bool)));
		connect(ui.patches, SIGNAL(toggled(bool)), ui.area, SLOT(setPatches(bool)));
		connect(ui.fov, SIGNAL(valueChanged(int)), ui.area, SLOT(setFov(int)));
		connect(ui.info, SIGNAL(clicked(bool)), ui.area, SLOT(setInfo(bool)));
		connect(ui.extract, SIGNAL(toggled(bool)), ui.area, SLOT(setExtraction(bool)));
		connect(ui.mtri, SIGNAL(valueChanged(double)), ui.area, SLOT(setPrimitives(double)));
		connect(ui.error, SIGNAL(valueChanged(double)), ui.area, SLOT(setError(double)));
		connect(ui.gpu, SIGNAL(valueChanged(double)), ui.area, SLOT(setGpu(double)));
		connect(ui.ram, SIGNAL(valueChanged(double)), ui.area, SLOT(setRam(double)));
		connect(ui.top, SIGNAL(clicked()), ui.area, SLOT(setColorTop()));
		connect(ui.bottom, SIGNAL(clicked()), ui.area, SLOT(setColorBottom()));
		connect(ui.color, SIGNAL(clicked()), ui.area, SLOT(setColor()));
	}

	Ui::Form ui;
};

int main(int argc, char *argv[]) {
	QSurfaceFormat openGLFormat;
	openGLFormat.setRenderableType(QSurfaceFormat::OpenGL);
	openGLFormat.setVersion(4, 3);
	openGLFormat.setProfile(QSurfaceFormat::CoreProfile);
	openGLFormat.setDepthBufferSize(24);
	openGLFormat.setAlphaBufferSize(8);
	QSurfaceFormat::setDefaultFormat(openGLFormat);

	QApplication app(argc, argv);

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
	addValue("renderer", "r", "renderer backend (opengl or qrhi)", "opengl");
	opt.addOption(QCommandLineOption({"S", "dontshare"}, "do not use another thread for textures"));
	opt.addOption(QCommandLineOption({"a", "autopositioning"}, "distribute models in a grid"));
	opt.process(app);

	const QString rendererName = opt.value("renderer").toLower();
	if(rendererName != "opengl" && rendererName != "qrhi") {
		std::cerr << "Unknown renderer: " << qPrintable(rendererName)
		          << " (choose opengl or qrhi)\n";
		return -1;
	}
	const bool useQrhi = rendererName == "qrhi";

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
			nullptr, "Import Nxs/Nxz", QDir::homePath(),
			"All known files (*.nxs *.nxz);;nxs file (*.nxs);;nxz file (*.nxz)");
		if(inputs.isEmpty()) {
			std::cerr << "No input files specified.\n"
			          << qPrintable(opt.helpText()) << '\n';
			return -1;
		}
	}

	std::unique_ptr<QWidget> window;
	Nxsview *view = nullptr;
	GLNxsview *glView = nullptr;
	QrhiWindow *qrhiWindow = nullptr;
	if(useQrhi) {
		qrhiWindow = new QrhiWindow;
		window.reset(qrhiWindow);
		view = qrhiWindow->area;
	} else {
		auto *glWindow = new QMyWindow(nullptr);
		window.reset(glWindow);
		glView = glWindow->ui.area;
		view = glView;
		if(opt.isSet("dontshare"))
			glView->share = false;
	}

	view->scene.autopositioning = opt.isSet("autopositioning");
	view->scene.load(inputs, instances);
	window->setGeometry(100, 100, width, height);

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

	nx::Controller &controller = view->scene.controller;
	controller.setOpenGLBackend(!useQrhi);
	controller.setRam(static_cast<quint64>(ramCapacity * (1 << 20)));
	controller.setGpu(static_cast<quint64>(gpuCapacity * (1 << 20)));
	controller.start();

	view->setPrimitives(draw);
	view->setError(targetError);
	view->setFov(fov);
	view->setFps(static_cast<float>(fps));

	if(glView) {
		auto &ui = static_cast<QMyWindow *>(window.get())->ui;
		ui.mtri->setValue(draw);
		ui.error->setValue(targetError);
		ui.ram->setValue(ramCapacity);
		ui.gpu->setValue(gpuCapacity);
		ui.fov->setValue(fov);
	} else {
		qrhiWindow->mtri->setValue(draw);
		qrhiWindow->error->setValue(targetError);
		qrhiWindow->ram->setValue(ramCapacity);
		qrhiWindow->gpu->setValue(gpuCapacity);
		qrhiWindow->fov->setValue(fov);
	}

	window->show();
	try {
		app.exec();
	} catch(const QString &error) {
		std::cout << "Fatal error: " << qPrintable(error) << '\n';
	}
	return 0;
}
