#include "gl_nxsview.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QCloseEvent>
#include <QOpenGLContext>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVector4D>
#include <QWheelEvent>

#include "../../globalgl.h"

namespace {

constexpr auto backgroundVertexShader = R"(
#version 430 core
out float gradient;
const vec2 positions[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);
void main() {
    vec2 position = positions[gl_VertexID];
    gl_Position = vec4(position, 1.0, 1.0);
    gradient = position.y * 0.5 + 0.5;
}
)";

constexpr auto backgroundFragmentShader = R"(
#version 430 core
in float gradient;
uniform vec3 topColor;
uniform vec3 bottomColor;
out vec4 fragColor;
void main() {
    fragColor = vec4(mix(bottomColor, topColor, gradient), 1.0);
}
)";

constexpr auto lineVertexShader = R"(
#version 430 core
layout(location = 0) in vec3 position;
uniform mat4 mvp;
void main() {
    gl_Position = mvp * vec4(position, 1.0);
}
)";

constexpr auto lineFragmentShader = R"(
#version 430 core
uniform vec3 color;
out vec4 fragColor;
void main() {
    fragColor = vec4(color, 1.0);
}
)";

QMatrix4x4 matrixFromColumnMajor(const float *matrix) {
	return QMatrix4x4(
		matrix[0], matrix[4], matrix[8], matrix[12],
		matrix[1], matrix[5], matrix[9], matrix[13],
		matrix[2], matrix[6], matrix[10], matrix[14],
		matrix[3], matrix[7], matrix[11], matrix[15]);
}

QVector3D colorVector(const nx::Rgba8 &color) {
	return QVector3D(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
}

bool initializeProgram(QOpenGLShaderProgram &program, const char *vertex,
                      const char *fragment) {
	if(!program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex) ||
	   !program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragment) ||
	   !program.link()) {
		qWarning() << "Failed to initialize OpenGL view shader:" << program.log();
		program.removeAllShaders();
		return false;
	}
	return true;
}

} // namespace

GLNxsview::GLNxsview(QWidget *parent, bool _share):
	QOpenGLWidget(parent), Nxsview(), share(_share) {
	QSurfaceFormat format;
	format.setRenderableType(QSurfaceFormat::OpenGL);
	format.setVersion(4, 3);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setDepthBufferSize(24);
	format.setAlphaBufferSize(8);
	setFormat(format);

	setAutoFillBackground(false);
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);

	auto *timer = new QTimer(this);
	timer->start(50);
	QObject::connect(timer, &QTimer::timeout, this, &GLNxsview::checkForUpdate);
}

void GLNxsview::checkForUpdate() {
	Nxsview::checkForUpdate();
}

void GLNxsview::getSnapshot() {
	const QImage image = grabFramebuffer();
	const QString file = QFileDialog::getSaveFileName(
		this, tr("Select .png filename to save snapshot"), QString(), tr("Image (*.png)"));
	if(!file.isEmpty())
		image.save(file, "PNG");
}

GLNxsview::~GLNxsview() {
	cleanupGL();
}

void GLNxsview::initializeGL() {
	glCleaned = false;
	QObject::connect(context(), &QOpenGLContext::aboutToBeDestroyed,
	                 this, &GLNxsview::cleanupGL, Qt::DirectConnection);
	const QSurfaceFormat actualFormat = context()->format();
	if(actualFormat.depthBufferSize() < 24 || actualFormat.alphaBufferSize() < 8)
		qWarning() << "OpenGL context has insufficient depth/alpha buffers:"
		           << actualFormat;

	auto *gl = currentGLFunctions();
	while(gl->glGetError() != GL_NO_ERROR) {}

	if(!renderer.initialize() ||
	   !initializeProgram(backgroundProgram, backgroundVertexShader,
	                      backgroundFragmentShader) ||
	   !initializeProgram(lineProgram, lineVertexShader, lineFragmentShader))
		return;

	gl->glGenVertexArrays(1, &backgroundVertexArray);
	gl->glGenVertexArrays(1, &lineVertexArray);
	gl->glGenBuffers(1, &lineBuffer);
	if(!backgroundVertexArray || !lineVertexArray || !lineBuffer) {
		qWarning() << "Failed to allocate OpenGL view resources";
		return;
	}

	std::array<GLfloat, 96> lineVertices{};
	std::size_t vertex = 0;
	constexpr int lineCount = 3;
	for(int i = 0; i <= lineCount; ++i) {
		for(int j = 0; j <= lineCount; ++j) {
			const GLfloat x = -1.0f + i * 2.0f / lineCount;
			const GLfloat y = -1.0f + j * 2.0f / lineCount;
			lineVertices[vertex++] = x;
			lineVertices[vertex++] = y;
			lineVertices[vertex++] = -2.0f;
			lineVertices[vertex++] = x;
			lineVertices[vertex++] = y;
			lineVertices[vertex++] = 2.0f;
		}
	}
	gl->glBindVertexArray(lineVertexArray);
	gl->glBindBuffer(GL_ARRAY_BUFFER, lineBuffer);
	gl->glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices.data(), GL_STATIC_DRAW);
	gl->glEnableVertexAttribArray(0);
	gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
	gl->glBindVertexArray(0);

	gl->glEnable(GL_DEPTH_TEST);
	glReady = true;
}

void GLNxsview::cleanupGL() {
	if(!context() || glCleaned)
		return;
	glCleaned = true;
	QObject::disconnect(context(), &QOpenGLContext::aboutToBeDestroyed,
	                    this, &GLNxsview::cleanupGL);
	makeCurrent();
	auto *gl = currentGLFunctions();
	renderer.releaseResources();
	backgroundProgram.removeAllShaders();
	lineProgram.removeAllShaders();
	if(lineBuffer)
		gl->glDeleteBuffers(1, &lineBuffer);
	if(lineVertexArray)
		gl->glDeleteVertexArrays(1, &lineVertexArray);
	if(backgroundVertexArray)
		gl->glDeleteVertexArrays(1, &backgroundVertexArray);
	lineBuffer = lineVertexArray = backgroundVertexArray = 0;
	glReady = false;
	doneCurrent();
}

void GLNxsview::resizeGL(int w, int h) {
	currentGLFunctions()->glViewport(0, 0, w, h);
	if(h > 0)
		trackball.pixel_size = trackball_light.pixel_size = 2.0f / h;
}

void GLNxsview::paintGL() {
	auto *gl = currentGLFunctions();
	if(!glReady) {
		gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		return;
	}

	scene.update();
	advanceViewpointAnimation();

	GLint viewport[4]{};
	gl->glGetIntegerv(GL_VIEWPORT, viewport);
	if(viewport[2] <= 0 || viewport[3] <= 0)
		return;

	gl->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	gl->glDisable(GL_SCISSOR_TEST);
	gl->glDisable(GL_BLEND);
	gl->glDisable(GL_STENCIL_TEST);
	gl->glDisable(GL_CULL_FACE);
	gl->glDisable(GL_RASTERIZER_DISCARD);
	gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	gl->glDepthMask(GL_TRUE);
	gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	gl->glDisable(GL_DEPTH_TEST);
	gl->glDepthMask(GL_FALSE);
	gl->glBindVertexArray(backgroundVertexArray);
	backgroundProgram.bind();
	backgroundProgram.setUniformValue("topColor", colorVector(colorTop));
	backgroundProgram.setUniformValue("bottomColor", colorVector(colorBottom));
	gl->glDrawArrays(GL_TRIANGLES, 0, 3);
	backgroundProgram.release();
	gl->glDepthMask(GL_TRUE);
	gl->glEnable(GL_DEPTH_TEST);
	gl->glClear(GL_DEPTH_BUFFER_BIT);

	const QSize outputSize(viewport[2], viewport[3]);
	const float aspect = outputSize.width() / static_cast<float>(outputSize.height());
	const float fovRadians = fov * static_cast<float>(M_PI) / 360.0f;
	const float objectDistance = 1.0f / std::tan(fovRadians);
	QMatrix4x4 cameraView;
	cameraView.lookAt(QVector3D(0, 0, objectDistance), QVector3D(0, 0, 0),
	                  QVector3D(0, 1, 0));
	const QMatrix4x4 trackballMatrix = matrixFromColumnMajor(trackball.matrix().m);
	const QMatrix4x4 lightMatrix = matrixFromColumnMajor(trackball_light.matrix().m);
	const QVector3D lightDirection =
		(cameraView * lightMatrix * QVector4D(0, 0, 1, 0)).toVector3D();

	QMatrix4x4 projection;
	projection.perspective(fov, aspect, 0.1f, 1000.0f);

	renderer.setMeshColor(color);
	renderer.startFrame();
	float znear = 1e20f;
	float zfar = 0;
	for(const Scene::Node &node : scene.nodes) {
		const QMatrix4x4 model = matrixFromColumnMajor(node.transform.m);
		const QMatrix4x4 modelView = cameraView * trackballMatrix * model;
		renderer.setViewMatrices(projection, modelView, lightDirection, viewport);
		if(extracting)
			renderer.getView();
		renderer.nearFar(node.nexus, znear, zfar);
	}
	if(zfar == 0)
		zfar = 10.0f;
	if(znear == 1e20f)
		znear = 0.1f;
	if(znear < 0.0001f)
		znear = zfar / 1000.0f;

	projection.setToIdentity();
	projection.perspective(fov, aspect, znear, zfar);

	if(!default_trackball) {
		gl->glBindVertexArray(lineVertexArray);
		lineProgram.bind();
		lineProgram.setUniformValue("mvp", projection * cameraView * lightMatrix);
		lineProgram.setUniformValue("color", QVector3D(0.8f, 0.8f, 0.0f));
		gl->glDrawArrays(GL_LINES, 0, 32);
		lineProgram.release();
	}

	for(const Scene::Node &node : scene.nodes) {
		const QMatrix4x4 model = matrixFromColumnMajor(node.transform.m);
		const QMatrix4x4 modelView = cameraView * trackballMatrix * model;
		renderer.setViewMatrices(projection, modelView, lightDirection, viewport);
		renderer.render(node.nexus, extracting);
	}
	if(extracting)
		renderer.endFrame();

	if(hasToPick) {
		hasToPick = false;
		const qreal dpr = devicePixelRatioF();
		const int pixelX = std::clamp(static_cast<int>(pickX * dpr), 0, viewport[2] - 1);
		const int pixelY = std::clamp(static_cast<int>(pickY * dpr), 0, viewport[3] - 1);
		GLfloat depth = 1.0f;
		gl->glReadPixels(pixelX, pixelY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
		bool invertible = false;
		const QMatrix4x4 inverseViewProjection =
			(projection * cameraView * trackballMatrix).inverted(&invertible);
		if(depth < 1.0f && invertible) {
			const QVector4D clip(
				2.0f * (pixelX + 0.5f) / viewport[2] - 1.0f,
				2.0f * (pixelY + 0.5f) / viewport[3] - 1.0f,
				depth * 2.0f - 1.0f, 1.0f);
			QVector4D world = inverseViewProjection * clip;
			if(!qFuzzyIsNull(world.w())) {
				world /= world.w();
				trackball.translate({world.x(), world.y(), world.z()});
				trackball.scale(1.25f);
				QCursor::setPos(mapToGlobal(QPoint(width() / 2 + 2, height() / 2 + 2)));
			}
		}
	}

	gl->glBindVertexArray(0);
	if(info) {
		QPainter painter(this);
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
		painter.setPen(Qt::white);
		painter.setRenderHint(QPainter::TextAntialiasing);
		for(int i = 0; i < text.size(); ++i)
			painter.drawText(10, height() - (120 - 15 * i), text[i]);
	}
	if(playing)
		update();
}

void GLNxsview::wheelEvent(QWheelEvent *event) {
	handleWheelEvent(event, height());
}

void GLNxsview::mouseMoveEvent(QMouseEvent *event) {
	handleMouseMoveEvent(event, height());
}

void GLNxsview::mousePressEvent(QMouseEvent *event) {
	handleMousePressEvent(event, height());
}

void GLNxsview::mouseReleaseEvent(QMouseEvent *event) {
	handleMouseReleaseEvent(event, height());
}

void GLNxsview::mouseDoubleClickEvent(QMouseEvent *event) {
	handleMouseDoubleClickEvent(event, height());
}

void GLNxsview::keyReleaseEvent(QKeyEvent *event) {
	handleKeyReleaseEvent(event);
}

void GLNxsview::keyPressEvent(QKeyEvent *event) {
	handleKeyPressEvent(event);
}

void GLNxsview::closeEvent(QCloseEvent *) {
	handleCloseEvent(nullptr);
}
