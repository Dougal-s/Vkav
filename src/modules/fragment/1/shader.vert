#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 1) const float smoothingLevel = 0.f;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;
layout(constant_id = 4) const int vertexCount      = 1;

layout(constant_id = 11) const float radius = 0.3;
layout(constant_id = 12) const float reactivity = 1000;

layout(binding = 0) uniform data {
	float lVolume;
	float rVolume;
	uint time;
};

layout(binding = 1) uniform samplerBuffer lBuffer;
layout(binding = 2) uniform samplerBuffer rBuffer;


const float PI = 3.14159265359;

#include "../../smoothing/smoothing.glsl"

void main() {
	const int triangleCount = vertexCount/3;
	const float triangleAngle = 2.f*PI/triangleCount;
	const int triangle = gl_VertexIndex/3;
	const int vertex = gl_VertexIndex%3;

	float theta = (triangle+vertex/2.f)*triangleAngle;
	if (theta > PI) theta = -2*PI+theta;

	float texCoord = 2.f*(triangle+0.5)/triangleCount;
	float v;
	if (2*triangle >= triangleCount) v = kernelSmoothTexture(rBuffer, smoothingLevel, texCoord);
	else v = kernelSmoothTexture(lBuffer, smoothingLevel, texCoord);

	mat4 transposeMat = mat4(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		radius*reactivity*v*cos(theta-PI/2)/width, -radius*reactivity*v*sin(theta-PI/2)/height, 0, 1
		);

	mat4 scaleMat = mat4(
		1.f/width, 0         , 0, 0,
		0        , 1.f/height, 0, 0,
		0        , 0         , 1, 0,
		0        , 0         , 0, 1
		);

	if (vertex == 1) {
		gl_Position = transposeMat*vec4(0,0,0,1);
		return;
	}

	gl_Position = transposeMat*scaleMat*vec4(radius*cos(theta-PI/2), -radius*sin(theta-PI/2), 0.0, 1.0);
}
