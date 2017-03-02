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
#include <QGLWidget>
#include <QDebug>
#include <QTime>

#include <vcg/space/point2.h>
#include <vcg/space/box3.h>
#include <wrap/gui/trackball.h>

#include "../common/controller.h"
#include "../common/renderer.h"
#include "scene.h"

class GLNxsview : public QGLWidget {
	Q_OBJECT

public:
	nx::Renderer renderer;
	Scene scene;
	vcg::Color4b colorBottom;
	vcg::Color4b colorTop;
	vcg::Color4b color;
	bool share; //if true shares context with nexus at initialization

	GLNxsview(QWidget *parent = 0, bool share = true);
	~GLNxsview() {}

public slots:

	void checkForUpdate();

	void lightTrackball(bool on) { default_trackball = !on; }

	void setTriangles(bool on);
	void setNormals(bool on);
	void setColors(bool on);
	void setPatches(bool on);
	void setExtraction(bool on);
	void setFov(int f);
	void setInfo(bool on);
	void setPrimitives(double e);
	void setError(double e);
	void setGpu(double e);
	void setRam(double d);

	void getSnapshot();

	void setColor();
	void setColorTop();
	void setColorBottom();
	void zoomIn() { trackball.MouseWheel(-1); repaint(); }
	void zoomOut() { trackball.MouseWheel(1); repaint(); }
	void stopController() {
		//sleep(2);
	}

protected:
	vcg::Box3f box;
	vcg::Trackball trackball;
	vcg::Transform saved_trackball;
	vcg::Trackball trackball_light;
	bool default_trackball;
	vcg::Trackball::Button button;
	bool extracting;

	//viewpoints sequence
	std::vector<vcg::Similarityf> viewpoints;
	quint32 current_viewpoint;
	float current_time; //this will be modified by velocity
	QTime time;
	bool playing;

	void init(QString file);
	void initializeGL();
	void resizeGL(int w, int h);
	void setView(float znear, float zfar);
	void computeNearFar(float &znear, float &zfar);
	void paintEvent(QPaintEvent *event);

	void closeEvent ( QCloseEvent * event );
	void wheelEvent(QWheelEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void mousePressEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);
	void mouseDoubleClickEvent(QMouseEvent* e);
	void keyReleaseEvent(QKeyEvent* e);
	void keyPressEvent(QKeyEvent* e);

private:
	bool  hasToPick;  // has to pick during the next redraw.
	float fov;
	bool info;
	vcg::Point2i pointToPick;
};



#endif

