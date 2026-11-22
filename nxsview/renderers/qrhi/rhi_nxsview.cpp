/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (http://www.gnu.org/licenses/gpl.txt).
*/
#include "rhi_nxsview.h"

#include <cmath>
#include <iostream>

#include <QCloseEvent>
#include <QFileDialog>
#include <QImage>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QVector3D>

namespace {
QColor toQColor(const nx::Rgba8 &color) {
	return QColor(color.r, color.g, color.b, 255);
}

QMatrix4x4 matrixFromColumnMajor(const float *matrix) {
	return QMatrix4x4(
		matrix[0], matrix[4], matrix[8], matrix[12],
		matrix[1], matrix[5], matrix[9], matrix[13],
		matrix[2], matrix[6], matrix[10], matrix[14],
		matrix[3], matrix[7], matrix[11], matrix[15]);
}
} // namespace

RhiNxsview::RhiNxsview(QWidget *parent):
	QRhiWidget(parent), Nxsview() {
	renderer.setMeshColor(color);
	setAutoFillBackground(false);
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
	infoLabel = new QLabel(this);
	infoLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	infoLabel->setStyleSheet(
		QStringLiteral("QLabel { color: white; background-color: rgba(0, 0, 0, 160); padding: 4px; }"));
	infoLabel->hide();

	auto *timer = new QTimer(this);
	timer->start(50);
	QObject::connect(timer, &QTimer::timeout, this, &RhiNxsview::checkForUpdate);
}

RhiNxsview::~RhiNxsview() {
	renderer.releaseResources();
}

void RhiNxsview::checkForUpdate() {
	Nxsview::checkForUpdate();
}

void RhiNxsview::getSnapshot() {
	const QImage image = grab().toImage();
	const QString file = QFileDialog::getSaveFileName(
		this, tr("Select .png filename to save snapshot"), QString(), tr("Image (*.png)"));
	if(file.isEmpty())
		return;
	image.save(file, "PNG");
}

void RhiNxsview::setColor() {
	Nxsview::setColor(this);
	renderer.setMeshColor(color);
}

void RhiNxsview::initialize(QRhiCommandBuffer *cb) {
	rendererReady = renderer.initialize(rhi(), cb, renderTarget());
}

void RhiNxsview::releaseResources() {
	renderer.releaseResources();
	rendererReady = false;
}

void RhiNxsview::render(QRhiCommandBuffer *cb) {
	scene.update();
	advanceViewpointAnimation();

	const QSize outputSize = colorTexture()->pixelSize();
	trackball.pixel_size = trackball_light.pixel_size = 2.0f / height();

	const float fovRadians = fov * static_cast<float>(M_PI) / 360.0f;
	const float objectDistance = 1.0f / std::tan(fovRadians);
	QMatrix4x4 cameraView;
	cameraView.lookAt(QVector3D(0, 0, objectDistance), QVector3D(0, 0, 0),
	                  QVector3D(0, 1, 0));
	const QMatrix4x4 trackballMatrix = matrixFromColumnMajor(trackball.matrix().m);

	if(!rendererReady) {
		cb->beginPass(renderTarget(), toQColor(colorBottom), {1.0f, 0});
		cb->endPass();
		return;
	}

	float znear = 1e20f;
	float zfar = 0;
	QMatrix4x4 projection;
	projection.perspective(fov, outputSize.width() / static_cast<float>(outputSize.height()),
	                       0.1f, 1000.0f);
	const int viewport[4] = {0, 0, outputSize.width(), outputSize.height()};
	for(const Scene::Node &node : scene.nodes) {
		const QMatrix4x4 model = matrixFromColumnMajor(node.transform.m);
		const QMatrix4x4 modelView = cameraView * trackballMatrix * model;
		renderer.setViewMatrices(projection, modelView, trackballMatrix * model,
		                         rhi()->clipSpaceCorrMatrix(), viewport);
		renderer.nearFar(node.nexus, znear, zfar);
	}
	if(zfar == 0)
		zfar = 10.0f;
	if(znear == 1e20f)
		znear = 0.1f;
	if(znear < 0.0001f)
		znear = zfar / 1000.0f;

	projection.setToIdentity();
	projection.perspective(fov, outputSize.width() / static_cast<float>(outputSize.height()),
	                       znear, zfar);
	if(!renderer.beginRhiFrame(cb, renderTarget(), outputSize)) {
		cb->beginPass(renderTarget(), toQColor(colorBottom), {1.0f, 0});
		cb->endPass();
		return;
	}
	for(const Scene::Node &node : scene.nodes) {
		const QMatrix4x4 model = matrixFromColumnMajor(node.transform.m);
		const QMatrix4x4 modelTransform = trackballMatrix * model;
		const QMatrix4x4 modelView = cameraView * modelTransform;
		renderer.setViewMatrices(projection, modelView, modelTransform,
		                         rhi()->clipSpaceCorrMatrix(), viewport);
		renderer.render(node.nexus, extracting);
	}
	renderer.finishRhiFrame(toQColor(colorBottom));
	if(extracting)
		renderer.endFrame();

	if(hasToPick)
		hasToPick = false;
	if(playing)
		update();
}

void RhiNxsview::paintEvent(QPaintEvent *event) {
	QRhiWidget::paintEvent(event);
	if(!info) {
		infoLabel->hide();
		return;
	}

	const nx::Stats &stats = renderer.stats;
	QStringList text;
	text << QString("Fps: %1").arg(stats.fps, 0, 'f', 2);
	text << QString("Err: %1").arg(stats.instance_error, 0, 'f', 2);
	text << QString("Drawing: %1 M tri").arg(stats.rendered / static_cast<float>(1 << 20), 0, 'f', 2);
	text << QString("Gpu cache: %1 / %2 Mb")
	            .arg(scene.controller.caches[1]->size() / (1 << 20))
	            .arg(scene.controller.caches[1]->capacity() / (1 << 20));
	text << QString("Ram cache: %1 / %2 Mb")
	            .arg(scene.controller.caches[0]->size() / (1 << 20))
	            .arg(scene.controller.caches[0]->capacity() / (1 << 20));
	text << QString("Patch rendered: %1").arg(stats.patch_rendered);
	text << QString("Node rendered: %1").arg(stats.node_rendered);
	text << QString("Frustum culled: %1").arg(stats.frustum_culled);
	infoLabel->setText(text.join(QLatin1Char('\n')));
	infoLabel->adjustSize();
	infoLabel->move(10, height() - infoLabel->height() - 10);
	infoLabel->show();
	infoLabel->raise();
}

void RhiNxsview::wheelEvent(QWheelEvent *event) {
	handleWheelEvent(event, height());
}

void RhiNxsview::mouseMoveEvent(QMouseEvent *event) {
	handleMouseMoveEvent(event, height());
}

void RhiNxsview::mousePressEvent(QMouseEvent *event) {
	handleMousePressEvent(event, height());
}

void RhiNxsview::mouseReleaseEvent(QMouseEvent *event) {
	handleMouseReleaseEvent(event, height());
}

void RhiNxsview::mouseDoubleClickEvent(QMouseEvent *event) {
	handleMouseDoubleClickEvent(event, height());
}

void RhiNxsview::keyReleaseEvent(QKeyEvent *event) {
	handleKeyReleaseEvent(event);
}

void RhiNxsview::keyPressEvent(QKeyEvent *event) {
	handleKeyPressEvent(event);
}

void RhiNxsview::closeEvent(QCloseEvent *event) {
	handleCloseEvent(event);
}
