#version 440

layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexTexCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    mat4 model;
    mat4 textureMatrix;
    vec4 baseColor;
    vec4 options;
};

layout(binding = 1) uniform sampler2D meshTexture;

void main()
{
    vec4 color = vertexColor;
    if(options.x > 0.5)
        color *= texture(meshTexture, vertexTexCoord);

    float light = 1.0;
    if(options.z > 0.5)
        light = 0.2 + 0.8 * max(dot(normalize(vertexNormal), normalize(vec3(0.35, 0.55, 1.0))), 0.0);

    fragColor = vec4(color.rgb * light, color.a);
}
