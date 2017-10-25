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
#include <QColorDialog>
#include <QFileDialog>
#include <QTimer>

#include "gl_nxsview.h"

#include <wrap/gl/picking.h>
#include <wrap/gl/space.h>
#include <wrap/qt/gl_label.h>

using namespace std;
using namespace vcg;
using namespace nx;

int current_texture = 0;

void glV(Point3f &p) { glVertex3f(p[0], p[1], p[2]); }

GLNxsview::GLNxsview(QWidget *parent, bool _share):
	QGLWidget(parent), share(_share),
	default_trackball(true), extracting(true), current_viewpoint(-1), current_time(-1), playing(false),
	hasToPick(false), fov(60), info(false) {

	setAutoFillBackground(false);
	setFocusPolicy(Qt::StrongFocus);

	colorBottom = vcg::Color4b(64,64,128,1);
	colorTop    = vcg::Color4b(0,0,0,1);
	color       = vcg::Color4b(200,200,200,1);

	setMouseTracking(true);
	//connect(this, SIGNAL(destroyed()), this, SLOT(stopController()));

	QTimer *timer = new QTimer(this);
	timer->start(50);
	QObject::connect(timer, SIGNAL(timeout()), this, SLOT(checkForUpdate()));
}

void GLNxsview::checkForUpdate() {
	if(scene.controller.hasCacheChanged())
		update();
}

void GLNxsview::setTriangles(bool on) {
	renderer.setMode(Renderer::TRIANGLES, on);
	update();
}

void GLNxsview::setNormals(bool on) {
	renderer.setMode(Renderer::NORMALS, on);
	update();
}

void GLNxsview::setColors(bool on) {
	renderer.setMode(Renderer::COLORS, on);
	update();
}

void GLNxsview::setPatches(bool on) {
	renderer.setMode(Renderer::PATCHES, on);
	update();
}

void GLNxsview::setExtraction(bool on) {
	extracting = on;
	update();
}

void GLNxsview::setFov(int f) {
	fov = f;
	update();
}

void GLNxsview::setInfo(bool on) {
	info = on;
	update();
}

void GLNxsview::setPrimitives(double e) {
	renderer.setMaxPrimitives((int)(e*(1<<20)));
}

void GLNxsview::setError(double e) {
	//extraction.SetExtr((unsigned int)(e*(1<<20)));
	renderer.setError(e); //extraction.target_error = e;
	update();
}

void GLNxsview::setGpu(double d) {
	scene.controller.setGpu(d*(1<<20));
	update();
}

void GLNxsview::setRam(double d) {
	scene.controller.setRam(d*(1<<20));
	update();
}

void GLNxsview::setColor() {
	QColor _color = QColorDialog::getColor(QColor(color[0], color[1], color[2]));
	color = Color4b(_color.red(), _color.green(), _color.blue(), 0);
	update();
}
void GLNxsview::setColorTop() {
	QColor _color = QColorDialog::getColor(QColor(colorTop[0], colorTop[1], colorTop[2]));
	colorTop = Color4b(_color.red(), _color.green(), _color.blue(), 0);
	update();
}
void GLNxsview::setColorBottom() {
	QColor _color = QColorDialog::getColor(QColor(colorBottom[0], colorBottom[1], colorBottom[2]));
	colorBottom = Color4b(_color.red(), _color.green(), _color.blue(), 0);
	update();
}

void GLNxsview::getSnapshot() {
	QImage im = grabFrameBuffer();
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

	trackball.center = Point3f(0, 0, 0);
	trackball.radius = 1;

	glLoadIdentity();
}

void GLNxsview::resizeGL(int w, int h) {
	glViewport(0, 0, (GLint)w, (GLint)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float r = w/(float)h;
	gluPerspective(60, r, 1, 10);

	glMatrixMode(GL_MODELVIEW);
}



void GLNxsview::setView(float znear, float zfar) {

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	GLfloat fAspect = (GLfloat)width()/ (GLfloat)height();
	gluPerspective(fov, fAspect, znear, zfar);

	//compute distance so that unitary sphere is inside view.
	float ratio = 1.0f;
	float objDist = ratio / tanf(vcg::math::ToRad(fov * .5f));

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, objDist,0, 0, 0, 0, 1, 0);
}


void GLNxsview::paintEvent(QPaintEvent * /*event*/) {
	QPainter painter(this);
	painter.beginNativePainting();
	makeCurrent();

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
	glColor(colorTop);    glVertex2f(-1, 1);
	glColor(colorBottom);  glVertex2f(-1,-1);
	glColor(colorTop);  glVertex2f( 1, 1);
	glColor(colorBottom);  glVertex2f( 1,-1);
	glEnd();

	glPopAttrib();
	glPopMatrix(); // restore modelview
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);

	/*end drawing background */

	if(playing && current_time >= 0) {
		current_time += time.restart()/1000.0f;
		int viewpoint = floor(current_time);
		if(viewpoint >= (int)viewpoints.size()-1) {
			playing = false;
		} else {

			vcg::Similarityf r;
			r.tra[0] = r.tra[1] = r.tra[2] = 0.0f;
			r.rot[0] = r.rot[1] = r.rot[2] = r.rot[3] = 0.0f;
			r.sca = 0;
			float tot = 0;
			for(int i = viewpoint - 6; i < viewpoint + 6; i++) {
				if(i < 0 || i >= (int)viewpoints.size()) continue;
				float t = current_time -i;
				float w = exp(-t*t);
				tot += w;
				const vcg::Similarityf &a = viewpoints[i];
				r.rot += a.rot * w;
				r.tra += a.tra * w;
				r.sca += a.sca * w;
			}
			r.rot /= tot;
			r.tra /= tot;
			r.sca /= tot;
			trackball.track = r;
		}
	}

	trackball.GetView();

	glPushMatrix();


	// ============== LIGHT TRACKBALL ==============
	// Apply the trackball for the light direction
	glPushMatrix();
	trackball_light.GetView();
	trackball_light.Apply();

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

	trackball.Apply();
	glColor(color);


	//if(extracting)
	renderer.startFrame();


	//COMPUTE NEAR and FAR
	float znear = 1e20;
	float zfar = 0;
	for(unsigned int i = 0; i < scene.nodes.size(); i++) {
		Scene::Node &node = scene.nodes[i];
		glPushMatrix();
		glMultMatrix(node.transform);
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
	trackball.Apply();

	for(unsigned int i = 0; i < scene.nodes.size(); i++) {
		Scene::Node &node = scene.nodes[i];

		if(!node.nexus->header.signature.vertex.hasNormals())
			glDisable(GL_LIGHTING);


		glPushMatrix();
		glMultMatrix(node.transform);

		renderer.render(node.nexus, extracting);

		glEnable(GL_LIGHTING);

		glPopMatrix();
	}

	if(extracting)
		renderer.endFrame();

	static QTime time;
	static bool first_time = true;
	if(first_time)
		time.start();

	//changed = true; //TODO checkl the last cache for changes (if nothing new in cache, we can assume no change)

	glPopMatrix(); //end of trackball

	if(hasToPick) { // Double click move picked point to center
		Point3f pp;
		hasToPick=false;
		if(Pick<Point3f>(pointToPick[0],pointToPick[1],pp)) {
			//check if intersects
			vcg::Matrix44f m = Inverse(scene.nodes[0].transform);
			//vcg::Point3f p = m * trackball.InverseMatrix() * pp;

			trackball.Translate(-pp);
			trackball.Scale(1.25f);
			QCursor::setPos(mapToGlobal(QPoint(width()/2+2,height()/2+2)));
		}
	}

	if(info) {

		Stats &stats = renderer.stats;

		glLabel::Mode mode;
		mode.qFont.setStyleStrategy(QFont::PreferAntialias);

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
		for(int i = 0; i < text.size(); i++)
			glLabel::render2D(&painter, vcg::Point2f(10, 120 - 15*i), text[i], mode);
	}
	painter.endNativePainting();
	if(playing)
		update();
}

Trackball::Button QT2VCG(Qt::MouseButton qtbt,  Qt::KeyboardModifiers modifiers) {
	int vcgbt=Trackball::BUTTON_NONE;
	if(qtbt & Qt::LeftButton)           vcgbt |= Trackball::BUTTON_LEFT;
	if(qtbt & Qt::RightButton)          vcgbt |= Trackball::BUTTON_RIGHT;
	if(qtbt & Qt::MidButton)            vcgbt |= Trackball::BUTTON_MIDDLE;
	if(modifiers & Qt::ShiftModifier)   vcgbt |= Trackball::KEY_SHIFT;
	if(modifiers & Qt::ControlModifier) vcgbt |= Trackball::KEY_CTRL;
	if(modifiers & Qt::AltModifier)     vcgbt |= Trackball::KEY_ALT;
	return Trackball::Button(vcgbt);
}

void GLNxsview::wheelEvent(QWheelEvent *e) {
	if(e->delta() > 0)
		trackball.MouseWheel(1);
	else
		trackball.MouseWheel(-1);
	update();
}

void GLNxsview::mouseMoveEvent(QMouseEvent *e) {
	if(default_trackball)
		trackball.MouseMove(e->x(), height() - e->y());
	else
		trackball_light.MouseMove(e->x(), height() - e->y());
	update();
	//e->ignore();
}

void GLNxsview::mousePressEvent(QMouseEvent *e) {
	button =  QT2VCG(e->button(), e->modifiers() );
	if(default_trackball)
		trackball.MouseDown(e->x(),height()-e->y(), button);
	else
		trackball_light.MouseDown(e->x(),height()-e->y(), button );
	update();
	//e->ignore();
}

void GLNxsview::mouseReleaseEvent(QMouseEvent *e) {
	if(default_trackball)
		trackball.MouseUp(e->x(),height()-e->y(), button);
	else
		trackball_light.MouseUp(e->x(),height()-e->y(), button);
	//e->ignore();
}

void GLNxsview::mouseDoubleClickEvent ( QMouseEvent * e ) {
	hasToPick=true;
	pointToPick=Point2i(e->x(),height()-e->y());
	update();
}

void GLNxsview::keyReleaseEvent ( QKeyEvent * e ) {
	e->ignore();

	//if(e->key()==Qt::Key_Control) trackball.MouseUp(0,0, QT2VCG(Qt::NoButton, Qt::ControlModifier ) );
	//if(e->key()==Qt::Key_Shift) trackball.MouseUp(0,0, QT2VCG(Qt::NoButton, Qt::ShiftModifier ) );
	//if(e->key()==Qt::Key_Alt) trackball.MouseUp(0,0, QT2VCG(Qt::NoButton, Qt::AltModifier ) );

	if(e->modifiers() == Qt::ControlModifier) {
		return;
	}
	if(e->modifiers() == Qt::ShiftModifier) {
		return;
	}
	if(e->modifiers() == Qt::AltModifier) {
		return;
	}

	//ignore multiple modifiers
	if(e->modifiers() & (Qt::ControlModifier | Qt::ControlModifier | Qt::AltModifier))
		return;

	switch(e->key()) {
	case Qt::Key_H:
		cout << "Keyboard shortcuts:\n"
			 << "h : print this help sheet\n"
			 << "r : reset trackball\n"
			 << "p : print trackball\n"
			 << "s : save trackball\n"
			 << "l : load trackball\n\n"

			 << "[ctrl+s] : save camera path\n"
			 << "[ctrl+l] : load camera path\n\n"

			 << "[enter] : insert viewpoint (after current viewpoint)\n"
			 << "[canc] : delete current viewpoint\n"
			 << "[home] : go to first viewpoint\n"
			 << "[PgDn] : go to next viewpoint\n"
			 << "[PgUp] : go to previous viewpoint\n"
			 << "[end] : go to last viewpoint\n\n"

			 << "[space] : play\n"
			 << "[space] : pause\n"
			 << "[ctrl+r] : record (starts from beginning)\n";
		break;
	case Qt::Key_S: //save trackball
		saved_trackball = trackball;
		break;
	case Qt::Key_L: //save trackball
		trackball.track = saved_trackball.track;
	case Qt::Key_R: //reset trackball
		trackball.Reset();

	case Qt::Key_P: {
		Matrix44f m = trackball.Matrix();
		m *= scene.nodes[0].transform;
		m.transposeInPlace();
		for(int i = 0; i < 16; i++) {
			cout << m.V()[i];
			if(i != 15) cout << ", ";
		}
		cout << endl;
	}
		break;


	case Qt::Key_Return:
		current_viewpoint++;
		viewpoints.resize(viewpoints.size()+1);
		for(unsigned int i = viewpoints.size()-1; i > current_viewpoint; i--) {
			viewpoints[i] = viewpoints[i-1];
		}
		viewpoints[current_viewpoint] = trackball.track;
		cout << "Viewpoint created: " << current_viewpoint << endl;
		break;

	case Qt::Key_Home:
		if(viewpoints.size()) {
			current_time = current_viewpoint = 0;

			trackball.track = viewpoints[0];
			cout << "Viewpoint: " << current_viewpoint << endl;
		}
		break;

	case Qt::Key_End:
		if(viewpoints.size()) {
			current_time = current_viewpoint = viewpoints.size()-1;
			trackball.track = viewpoints[current_viewpoint];
			cout << "Viewpoint: " << current_viewpoint << endl;
		}

		break;

	case Qt::Key_PageDown:
		if(current_viewpoint < viewpoints.size()-1) {
			current_viewpoint++;
			current_time = current_viewpoint;
			trackball.track = viewpoints[current_viewpoint];
			cout << "Viewpoint: " << current_viewpoint << endl;
		}
		break;

	case Qt::Key_PageUp:
		if(current_viewpoint > 0) {
			current_viewpoint--;
			current_time = current_viewpoint;
			trackball.track = viewpoints[current_viewpoint];
			cout << "Viewpoint: " << current_viewpoint << endl;
		}
		break;

	case Qt::Key_Delete:
		if(viewpoints.size()) {

			//last viewpoint
			if(current_viewpoint == viewpoints.size()-1) {
				current_viewpoint--;
				current_time = current_viewpoint;
				viewpoints.pop_back();
			} else {
				for(unsigned int i = current_viewpoint; i < viewpoints.size()-1; i++)
					viewpoints[i] = viewpoints[i+1];
				viewpoints.resize(viewpoints.size()-1);
			}
		}
		cout << "Viewpoint: " << current_viewpoint << endl;
		break;

	case Qt::Key_Space:
		playing = !playing;
		if(playing) time.start();
		break;
	}
	update();
}

void GLNxsview::keyPressEvent ( QKeyEvent * e )
{
	current_texture++;
	if(current_texture == 9)
		current_texture = 0;

	e->ignore();

	//if(e->key()==Qt::Key_Control) trackball.ButtonDown(QT2VCG(Qt::NoButton, Qt::ControlModifier ) );
	//if(e->key()==Qt::Key_Shift) trackball.ButtonDown(QT2VCG(Qt::NoButton, Qt::ShiftModifier ) );
	//if(e->key()==Qt::Key_Alt) trackball.ButtonDown(QT2VCG(Qt::NoButton, Qt::AltModifier ) );

}

void GLNxsview::closeEvent ( QCloseEvent * /*event*/ ) {
	//scene.controller.finish();
}
