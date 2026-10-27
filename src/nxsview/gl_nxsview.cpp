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
#include <iostream>

#include <QWheelEvent>
#include <QFileDialog>
#include <QSurfaceFormat>
#include <QTimer>

#include "gl_nxsview.h"

#include <QPainter>
#include <cmath>

using namespace std;
using namespace nx;

GLNxsview::GLNxsview(QWidget *parent, bool _share):
	QOpenGLWidget(parent), Nxsview(), share(_share) {
	QSurfaceFormat format;
	format.setVersion(2, 1);
	format.setProfile(QSurfaceFormat::CompatibilityProfile);
	setFormat(format);

	setAutoFillBackground(false);
	setFocusPolicy(Qt::StrongFocus);

	setMouseTracking(true);
	//connect(this, SIGNAL(destroyed()), this, SLOT(stopController()));

	QTimer *timer = new QTimer(this);
	timer->start(50);
	QObject::connect(timer, SIGNAL(timeout()), this, SLOT(checkForUpdate()));
}

void GLNxsview::checkForUpdate() {
	Nxsview::checkForUpdate();
}

void GLNxsview::setTriangles(bool on) { Nxsview::setTriangles(on); }
void GLNxsview::setNormals(bool on) { Nxsview::setNormals(on); }
void GLNxsview::setColors(bool on) { Nxsview::setColors(on); }
void GLNxsview::setPatches(bool on) { Nxsview::setPatches(on); }
void GLNxsview::setExtraction(bool on) { Nxsview::setExtraction(on); }
void GLNxsview::setFov(int f) { Nxsview::setFov(f); }
void GLNxsview::setInfo(bool on) { Nxsview::setInfo(on); }
void GLNxsview::setPrimitives(double e) { Nxsview::setPrimitives(e); }
void GLNxsview::setError(double e) { Nxsview::setError(e); }
void GLNxsview::setGpu(double e) { Nxsview::setGpu(e); }
void GLNxsview::setRam(double d) { Nxsview::setRam(d); }
void GLNxsview::lightTrackball(bool on) { Nxsview::lightTrackball(on); }
void GLNxsview::setColor() { Nxsview::setColor(this); }
void GLNxsview::setColorTop() { Nxsview::setColorTop(this); }
void GLNxsview::setColorBottom() { Nxsview::setColorBottom(this); }
void GLNxsview::zoomIn() { Nxsview::zoomIn(); }
void GLNxsview::zoomOut() { Nxsview::zoomOut(); }
void GLNxsview::stopController() { Nxsview::stopController(); }

void GLNxsview::getSnapshot() {
	QImage im = grabFramebuffer();
	QString file = QFileDialog::getSaveFileName(this,
												tr("Select .png filename to save shapshot"),
												"",
												tr("Image (*.png)"));
	if(file.isEmpty()) return;
	im.save(file, "PNG");
}


void GLNxsview::initializeGL() {
	glewInit();

	scene.controller.setWidget(this);
	glClearColor(1, 1, 1, 1);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	//glEnable(GL_COLOR_MATERIAL);

	//glDisable(GL_BLEND);
	glEnable(GL_NORMALIZE);
	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);

	glEnable(GL_LIGHTING);

	glLoadIdentity();
}

void GLNxsview::resizeGL(int w, int h) {
	glViewport(0, 0, (GLint)w, (GLint)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float r = w/(float)h;
	gluPerspective(60, r, 1, 10);

	glMatrixMode(GL_MODELVIEW);

	trackball.pixel_size = trackball_light.pixel_size = 2.0f/h;
}



void GLNxsview::setView(float znear, float zfar) {

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	GLfloat fAspect = (GLfloat)width()/ (GLfloat)height();
	gluPerspective(fov, fAspect, znear, zfar);

	//compute distance so that unitary sphere is inside view.
	float ratio = 1.0f;
	float objDist = ratio / tanf(fov * (float)M_PI / 360.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, objDist,0, 0, 0, 0, 1, 0);
}


void GLNxsview::paintGL() {
	QPainter painter(this);
	painter.beginNativePainting();

	//glEnable(GL_DEPTH_TEST);
	//initializeGL();
	scene.update();

	setView(0.1f, 1000.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* Drawing background */
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(-1,1,-1,1,-1,1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);

	glBegin(GL_TRIANGLE_STRIP);
	glColor3ub(colorTop.r, colorTop.g, colorTop.b);       glVertex2f(-1, 1);
	glColor3ub(colorBottom.r, colorBottom.g, colorBottom.b); glVertex2f(-1,-1);
	glColor3ub(colorTop.r, colorTop.g, colorTop.b);       glVertex2f( 1, 1);
	glColor3ub(colorBottom.r, colorBottom.g, colorBottom.b); glVertex2f( 1,-1);
	glEnd();

	glPopAttrib();
	glPopMatrix(); // restore modelview
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);

	/*end drawing background */

	advanceViewpointAnimation();

	glPushMatrix();


	// ============== LIGHT TRACKBALL ==============
	// Apply the trackball for the light direction
	glPushMatrix();
	glMultMatrixf(trackball_light.matrix().m);

	static float lightPosF[]={0.0,0.0,1.0,0.0};
	glLightfv(GL_LIGHT0,GL_POSITION,lightPosF);
	static float lightPosB[]={0.0,0.0,-1.0,0.0};
	glLightfv(GL_LIGHT1,GL_POSITION,lightPosB);

	if (!default_trackball) {
		glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
		glColor3f(0.8f,0.8f,0);
		glDisable(GL_LIGHTING);
		const int lineNum=3;
		glBegin(GL_LINES);
		for(int i=0;i<=lineNum;++i)
			for(int j=0;j<=lineNum;++j) {
				glVertex3f(-1.0f+i*2.0/lineNum,-1.0f+j*2.0/lineNum,-2);
				glVertex3f(-1.0f+i*2.0/lineNum,-1.0f+j*2.0/lineNum, 2);
			}
		glEnd();
		glPopAttrib();
	}
	glPopMatrix();
	//draw light trackball?

	glMultMatrixf(trackball.matrix().m);
	glColor3ub(color.r, color.g, color.b);


	//if(extracting)
	renderer.startFrame();


	//COMPUTE NEAR and FAR
	float znear = 1e20;
	float zfar = 0;
	for(unsigned int i = 0; i < scene.nodes.size(); i++) {
		Scene::Node &node = scene.nodes[i];
		glPushMatrix();
		glMultMatrixf(node.transform.m);
		if(extracting)
			renderer.getView();
		renderer.nearFar(node.nexus, znear, zfar);
		glPopMatrix();
	}
	if(zfar == 0)
		zfar = 10.0f;
	if(znear == 1e20f)
		znear = 0.1f;

	if(znear < 0.0001f)
		znear = zfar/1000.0f;

	setView(znear, zfar);
	glMultMatrixf(trackball.matrix().m);

	for(unsigned int i = 0; i < scene.nodes.size(); i++) {
		Scene::Node &node = scene.nodes[i];

		if(!node.nexus->header.signature.vertex.hasNormals())
			glDisable(GL_LIGHTING);


		glPushMatrix();
		glMultMatrixf(node.transform.m);

		renderer.render(node.nexus, extracting);

		glEnable(GL_LIGHTING);

		glPopMatrix();
	}

	if(extracting)
		renderer.endFrame();

	static QElapsedTimer time;
	static bool first_time = true;
	if(first_time)
		time.start();

	//changed = true; //TODO checkl the last cache for changes (if nothing new in cache, we can assume no change)

	glPopMatrix(); //end of trackball

	if(hasToPick) { // Double click move picked point to center
		hasToPick=false;
		GLint viewport[4];
		GLdouble modelview[16], projection[16];
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
		glGetDoublev(GL_PROJECTION_MATRIX, projection);
		GLfloat depth;
		glReadPixels(pickX, pickY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
		GLdouble px, py, pz;
		if(depth < 1.0f && gluUnProject(pickX, pickY, depth, modelview, projection, viewport, &px, &py, &pz) == GL_TRUE) {
			trackball.translate({(float)px, (float)py, (float)pz});
			trackball.scale(1.25f);
			QCursor::setPos(mapToGlobal(QPoint(width()/2+2,height()/2+2)));
		}
	}

	if(info) {

		Stats &stats = renderer.stats;

		QStringList text;
		text << QString("Fps: %1").arg(stats.fps, 0, 'f', 2);
		text << QString("Err: %1").arg(stats.instance_error, 0, 'f', 2);
		text << QString("Drawing: %1 M tri").arg(stats.rendered/(float)(1<<20), 0, 'f', 2);
		text << QString("Gpu cache: %1 / %2 Mb")
				.arg(scene.controller.caches[1]->size()/(1<<20)).arg(scene.controller.caches[1]->capacity()/(1<<20));
		text << QString("Ram cache: %1 / %2 Mb")
				.arg(scene.controller.caches[0]->size()/(1<<20)).arg(scene.controller.caches[0]->capacity()/(1<<20));
		text << QString("Patch rendered: %1").arg(stats.patch_rendered);
		text << QString("Node rendered: %1").arg(stats.node_rendered);
		text << QString("Frustum culled: %1").arg(stats.frustum_culled);
		// text << QString("Cone culled: %1").arg(stats.cone_culled);
		painter.endNativePainting();
		painter.setPen(Qt::white);
		painter.setRenderHint(QPainter::TextAntialiasing);
		for(int i = 0; i < text.size(); i++)
			painter.drawText(10, height() - (120 - 15*i), text[i]);
		painter.beginNativePainting();
	}
	painter.endNativePainting();
	if(playing)
		update();
}

void GLNxsview::wheelEvent(QWheelEvent *e) {
	handleWheelEvent(e, height());
}

void GLNxsview::mouseMoveEvent(QMouseEvent *e) {
	handleMouseMoveEvent(e, height());
}

void GLNxsview::mousePressEvent(QMouseEvent *e) {
	handleMousePressEvent(e, height());
}

void GLNxsview::mouseReleaseEvent(QMouseEvent *e) {
	handleMouseReleaseEvent(e, height());
}

void GLNxsview::mouseDoubleClickEvent ( QMouseEvent * e ) {
	handleMouseDoubleClickEvent(e, height());
}

void GLNxsview::keyReleaseEvent ( QKeyEvent * e ) {
	handleKeyReleaseEvent(e);
}

void GLNxsview::keyPressEvent(QKeyEvent *e) {
	handleKeyPressEvent(e);
}

void GLNxsview::closeEvent ( QCloseEvent * /*event*/ ) {
	handleCloseEvent(nullptr);
}
