#ifndef NX_GL_RENDERER_H
#define NX_GL_RENDERER_H

#include "../../globalgl.h"

#include <QMatrix4x4>
#include <QOpenGLShaderProgram>
#include <QVector3D>

#include "../../renderer.h"

namespace nx {

class GLRenderer : public Renderer {
public:
	using Renderer::getView;

	bool initialize();
	void releaseResources();
	void setViewMatrices(const QMatrix4x4 &projection,
	                     const QMatrix4x4 &modelView,
	                     const QVector3D &lightDirection,
	                     const int *viewport);
	void setMeshColor(Rgba8 color) { color.a = 255; meshColor = color; }
	void getView() override;

protected:
	void beginRender(const RenderOptions &options) override;
	void renderSelected(Nexus *nexus, const RenderOptions &options) override;
	void endRender(const RenderOptions &options) override;

private:
	QOpenGLShaderProgram program;
	QMatrix4x4 projectionMatrix;
	QMatrix4x4 modelViewMatrix;
	QVector3D lightDirection{0.0f, 0.0f, 1.0f};
	int currentViewport[4]{};
	Rgba8 meshColor{255, 255, 255, 255};
	unsigned int vertexArray = 0;
	bool initialized = false;
};

} // namespace nx

#endif // NX_GL_RENDERER_H
