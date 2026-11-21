/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License (http://www.gnu.org/licenses/gpl.txt).
*/
#ifndef NX_RHI_NXSVIEW_H
#define NX_RHI_NXSVIEW_H

#include <QColor>
#include <QPainter>
#include <QTimer>
#include <QWidget>
#include <QtWidgets/qrhiwidget.h>

#include "rhi_renderer.h"
#include "../../nxsview.h"

class QCloseEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

class RhiNxsview : public QRhiWidget, public Nxsview {
	Q_OBJECT

public:
	explicit RhiNxsview(QWidget *parent = nullptr);
	~RhiNxsview() override;

	nx::RhiRenderer renderer;

public slots:
	void checkForUpdate();
	void getSnapshot();

	void lightTrackball(bool on) { Nxsview::lightTrackball(on); }
	void setTriangles(bool on) { Nxsview::setTriangles(on); }
	void setNormals(bool on) { Nxsview::setNormals(on); }
	void setColors(bool on) { Nxsview::setColors(on); }
	void setPatches(bool on) { Nxsview::setPatches(on); }
	void setExtraction(bool on) { Nxsview::setExtraction(on); }
	void setFov(int f) { Nxsview::setFov(f); }
	void setInfo(bool on) { Nxsview::setInfo(on); }
	void setPrimitives(double e) { Nxsview::setPrimitives(e); }
	void setError(double e) { Nxsview::setError(e); }
	void setGpu(double e) { Nxsview::setGpu(e); }
	void setRam(double d) { Nxsview::setRam(d); }
	void setColor();
	void setColorTop() { Nxsview::setColorTop(this); }
	void setColorBottom() { Nxsview::setColorBottom(this); }
	void zoomIn() { Nxsview::zoomIn(); }
	void zoomOut() { Nxsview::zoomOut(); }

protected:
	void initialize(QRhiCommandBuffer *cb) override;
	void render(QRhiCommandBuffer *cb) override;
	void releaseResources() override;
	void paintEvent(QPaintEvent *event) override;
	void requestUpdate() override { update(); }
	nx::Renderer &getRenderer() override { return renderer; }

	void wheelEvent(QWheelEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	bool rendererReady = false;
};

#endif // NX_RHI_NXSVIEW_H
