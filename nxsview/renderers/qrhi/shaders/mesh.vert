#version 440

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 texCoord;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    mat4 model;
    mat4 textureMatrix;
    vec4 baseColor;
    vec4 options;
};

layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec3 vertexNormal;
layout(location = 2) out vec2 vertexTexCoord;

void main()
{
    gl_Position = mvp * vec4(position, 1.0);
    vertexColor = options.y > 0.5 ? color * baseColor : baseColor;
    vertexNormal = mat3(model) * normal;
    vertexTexCoord = options.w > 0.5
        ? vec2(dot(textureMatrix[0], vec4(position, 1.0)),
               dot(textureMatrix[1], vec4(position, 1.0)))
        : texCoord;
}
