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
#ifndef NX_GLNXSVIEW_H
#define NX_GLNXSVIEW_H

#include <GL/glew.h>
#include <iostream>
#include <QOpenGLWidget>
#include <QDebug>

#include "gl_renderer.h"
#include "nxsview.h"

class GLNxsview : public QOpenGLWidget, public Nxsview {
	Q_OBJECT

public:
	bool share; //if true shares context with nexus at initialization
	nx::GLRenderer renderer;

	GLNxsview(QWidget *parent = 0, bool share = true);
	~GLNxsview() {}

public slots:

	void checkForUpdate();
	void getSnapshot();

	// Nxsview is not a QObject, so moc needs these forwarders for the .ui connections.
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
	void setColor() { Nxsview::setColor(this); }
	void setColorTop() { Nxsview::setColorTop(this); }
	void setColorBottom() { Nxsview::setColorBottom(this); }
	void zoomIn() { Nxsview::zoomIn(); }
	void zoomOut() { Nxsview::zoomOut(); }


protected:
	void requestUpdate() override { update(); }
	nx::Renderer &getRenderer() override { return renderer; }
	void initializeGL();
	void resizeGL(int w, int h);
	void setView(float znear, float zfar);
	void paintGL();

	void closeEvent ( QCloseEvent * event );
	void wheelEvent(QWheelEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void mousePressEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);
	void mouseDoubleClickEvent(QMouseEvent* e);
	void keyReleaseEvent(QKeyEvent* e);
	void keyPressEvent(QKeyEvent* e);

private:
};



#endif
