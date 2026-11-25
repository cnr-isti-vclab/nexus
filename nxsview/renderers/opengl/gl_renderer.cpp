#include "gl_renderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

#include <QColor>
#include <QDebug>
#include <QVector4D>

#include "../../globalgl.h"
#include "../../nexus.h"

using namespace nx;

namespace {

constexpr auto vertexShader = R"(
#version 430 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 texCoord;

uniform mat4 uMvp;
uniform mat4 uModelView;
uniform mat4 uTextureMatrix;
uniform vec4 uBaseColor;
uniform bool uUseVertexColors;
uniform bool uGenerateTexCoords;

out vec4 vertexColor;
out vec3 vertexNormal;
out vec2 vertexTexCoord;

void main() {
    gl_Position = uMvp * vec4(position, 1.0);
    vertexColor = uUseVertexColors ? color * uBaseColor : uBaseColor;
    vertexNormal = mat3(transpose(inverse(uModelView))) * normal;
    vertexTexCoord = uGenerateTexCoords
        ? (uTextureMatrix * vec4(position, 1.0)).xy
        : texCoord;
}
)";

constexpr auto fragmentShader = R"(
#version 430 core
in vec4 vertexColor;
in vec3 vertexNormal;
in vec2 vertexTexCoord;

uniform sampler2D meshTexture;
uniform vec3 uLightDirection;
uniform bool uUseTexture;
uniform bool uUseLighting;

out vec4 fragColor;

void main() {
    vec4 color = vertexColor;
    if(uUseTexture)
        color *= texture(meshTexture, vertexTexCoord);

    float light = 1.0;
    if(uUseLighting)
        light = 0.2 + 0.8 * max(dot(normalize(vertexNormal), normalize(uLightDirection)), 0.0);

    fragColor = vec4(color.rgb * light, color.a);
}
)";

GLenum vertexAttributeType(const Attribute &attribute) {
	switch(attribute.type) {
	case Attribute::BYTE: return GL_BYTE;
	case Attribute::UNSIGNED_BYTE: return GL_UNSIGNED_BYTE;
	case Attribute::SHORT: return GL_SHORT;
	case Attribute::UNSIGNED_SHORT: return GL_UNSIGNED_SHORT;
	case Attribute::INT: return GL_INT;
	case Attribute::UNSIGNED_INT: return GL_UNSIGNED_INT;
	case Attribute::FLOAT: return GL_FLOAT;
	case Attribute::DOUBLE: return GL_DOUBLE;
	default: return 0;
	}
}

const void *bufferOffset(uint32_t offset) {
	return reinterpret_cast<const void *>(static_cast<std::uintptr_t>(offset));
}

QVector4D rgbaVector(const Rgba8 &color) {
	return QVector4D(color.r / 255.0f, color.g / 255.0f,
	                 color.b / 255.0f, 1.0f);
}

void setAttribute(QOpenGLFunctions_4_3_Core *gl, GLuint index,
                  const Attribute &attribute, GLint minimumComponents,
                  GLsizei stride, uint32_t offset, bool normalized) {
	if(!attribute.isNull() && attribute.number >= minimumComponents) {
		gl->glEnableVertexAttribArray(index);
		gl->glVertexAttribPointer(index, std::min<GLint>(attribute.number, 4),
		                          vertexAttributeType(attribute),
		                          normalized, stride,
		                          bufferOffset(offset));
	} else {
		gl->glDisableVertexAttribArray(index);
	}
}

} // namespace

bool GLRenderer::initialize() {
	if(initialized)
		return true;

	if(!program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader) ||
	   !program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader) ||
	   !program.link()) {
		qWarning() << "Failed to initialize OpenGL core renderer:" << program.log();
		program.removeAllShaders();
		return false;
	}

	currentGLFunctions()->glGenVertexArrays(1, &vertexArray);
	if(!vertexArray) {
		qWarning() << "Failed to create OpenGL core renderer vertex array";
		program.removeAllShaders();
		return false;
	}
	initialized = true;
	return true;
}

void GLRenderer::releaseResources() {
	if(vertexArray) {
		currentGLFunctions()->glDeleteVertexArrays(1, &vertexArray);
		vertexArray = 0;
	}
	program.removeAllShaders();
	initialized = false;
}

void GLRenderer::setViewMatrices(const QMatrix4x4 &projection,
                                     const QMatrix4x4 &modelView,
                                     const QVector3D &light,
                                     const int *viewport) {
	projectionMatrix = projection;
	modelViewMatrix = modelView;
	lightDirection = light;
	std::copy(viewport, viewport + 4, currentViewport);
}

void GLRenderer::getView() {
	Renderer::getView(projectionMatrix.constData(), modelViewMatrix.constData(),
	                  currentViewport);
}

void GLRenderer::beginRender(const RenderOptions &) {
	assert(initialized);
	auto *gl = currentGLFunctions();
	gl->glBindVertexArray(vertexArray);
	program.bind();
	program.setUniformValue("uMvp", projectionMatrix * modelViewMatrix);
	program.setUniformValue("uModelView", modelViewMatrix);
	program.setUniformValue("uLightDirection", lightDirection);
	program.setUniformValue("meshTexture", 0);
}

void GLRenderer::renderSelected(Nexus *nexus, const RenderOptions &options) {
	auto *gl = currentGLFunctions();
	const Signature &signature = nexus->header.signature;
	const VertexElement &vertexFormat = signature.vertex;
	const Attribute &coordinate = vertexFormat.attributes[VertexElement::COORD];
	const Attribute &normal = vertexFormat.attributes[VertexElement::NORM];
	const Attribute &color = vertexFormat.attributes[VertexElement::COLOR];
	const Attribute &texCoord = vertexFormat.attributes[VertexElement::TEX];
	const GLsizei stride = vertexFormat.vertexSize();

	program.setUniformValue("uModelView", modelViewMatrix);
	program.setUniformValue("uUseVertexColors", options.draw_colors);
	program.setUniformValue("uUseLighting", options.draw_normals);

	for(uint32_t nodeIndex = 0; nodeIndex <= last_node; ++nodeIndex) {
		if(!selected[nodeIndex])
			continue;

		Node &node = nexus->nodes[nodeIndex];
		if(signature.face.hasIndex() && skipNode(nodeIndex))
			continue;

		++stats.node_rendered;
		const Sphere sphere = node.tightSphere();
		if(frustum_culling && metric.frustum.isOutside(sphere.Center(), sphere.Radius())) {
			++stats.frustum_culled;
			continue;
		}
		if(cone_culling && node.cone.Backface(sphere, metric.frustum.viewPoint()))
			continue;

		NodeData &data = nexus->nodedata[nodeIndex];
		assert(data.memory);
		if(!data.vbo)
			nexus->loadGpu(nodeIndex);
		if(!data.vbo)
			continue;

		gl->glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
		setAttribute(gl, 0, coordinate, 3, stride,
		             vertexFormat.componentOffset(VertexElement::COORD), false);
		setAttribute(gl, 1, options.draw_normals ? normal : Attribute{}, 3, stride,
		             vertexFormat.componentOffset(VertexElement::NORM), true);
		setAttribute(gl, 2, options.draw_colors ? color : Attribute{}, 3, stride,
		             vertexFormat.componentOffset(VertexElement::COLOR), true);
		setAttribute(gl, 3, options.draw_texcoords ? texCoord : Attribute{}, 2, stride,
		             vertexFormat.componentOffset(VertexElement::TEX), true);

		program.setUniformValue("uMvp", projectionMatrix * modelViewMatrix);
		program.setUniformValue("uModelView", modelViewMatrix);
		program.setUniformValue("uUseVertexColors", options.draw_colors);
		program.setUniformValue("uUseLighting", options.draw_normals);
		program.setUniformValue("uGenerateTexCoords", false);
		program.setUniformValue("uUseTexture", false);

		if(!options.draw_triangles) {
			const Rgba8 pointColor = options.draw_colors
				? Rgba8{255, 255, 255, 255}
				: meshColor;
			program.setUniformValue("uBaseColor", rgbaVector(pointColor));
			gl->glDrawArrays(GL_POINTS, 0, node.nvert);
			++stats.patch_rendered;
			stats.instance_rendered += node.nvert;
			continue;
		}

		gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.fbo);
		uint32_t offset = 0;
		uint32_t end = 0;
		const uint32_t lastPatch = node.last_patch() - 1;
		for(uint32_t patchIndex = node.first_patch; patchIndex < node.last_patch(); ++patchIndex) {
			Patch &patch = nexus->patches[patchIndex];
			if(!selected[patch.node]) {
				end = patch.triangle_offset;
				if(patchIndex != lastPatch)
					continue;
			}
			if(end > offset) {
				Rgba8 patchColor = options.draw_colors
					? Rgba8{255, 255, 255, 255}
					: meshColor;
				if(mode & PATCHES) {
					patchColor = Rgba8{
						static_cast<uint8_t>(((nodeIndex % 16) * 135) % 127 + 127),
						static_cast<uint8_t>(((nodeIndex % 7) * 57) % 127 + 127),
						static_cast<uint8_t>(((nodeIndex % 11) * 171) % 63 + 63),
						255
					};
				}
				program.setUniformValue("uBaseColor", rgbaVector(patchColor));

				const bool useTexture = options.draw_textures && patch.texture != 0xffffffff;
				program.setUniformValue("uUseTexture", useTexture);
				program.setUniformValue("uGenerateTexCoords",
				                        useTexture && !options.draw_texcoords);
				if(useTexture) {
					const TextureData &textureData = nexus->texturedata[patch.texture];
					const Texture &texture = nexus->textures[patch.texture];
					gl->glActiveTexture(GL_TEXTURE0);
					gl->glBindTexture(GL_TEXTURE_2D, textureData.tex);
					program.setUniformValue("uTextureMatrix", QMatrix4x4(texture.matrix));
				}

				gl->glDrawElements(GL_TRIANGLES, static_cast<GLsizei>((end - offset) * 3),
				                   GL_UNSIGNED_SHORT,
				                   bufferOffset(offset * 3 * sizeof(uint16_t)));
				++stats.patch_rendered;
				stats.instance_rendered += end - offset;
			}
			offset = patch.triangle_offset;
		}
		gl->glBindTexture(GL_TEXTURE_2D, 0);
	}
}

void GLRenderer::endRender(const RenderOptions &) {
	auto *gl = currentGLFunctions();
	gl->glBindTexture(GL_TEXTURE_2D, 0);
	gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
	gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	gl->glBindVertexArray(0);
	program.release();
}
