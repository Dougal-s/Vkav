#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.f;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 11) const float amplitude = 2.f;

layout(constant_id = 12) const float red = 0;
layout(constant_id = 13) const float green = 0;
layout(constant_id = 14) const float blue = 0;

layout(constant_id = 21) const float top = 1;
layout(constant_id = 22) const float bottom = 0.2;

vec3 color = vec3(red, green, blue);

layout(set = 0, binding = 0) uniform data {
	float lVolume;
	float rVolume;
};

layout(set = 0, binding = 1) uniform samplerBuffer lBuffer;
layout(set = 0, binding = 2) uniform samplerBuffer rBuffer;

layout(location = 0) in vec2 position;

layout(location = 0) out vec4 outColor;

#include "../../smoothing/smoothing.glsl"

void main() {
	float v;
	if (position.x < 0.0)
		v = kernelSmoothTexture(lBuffer, smoothingLevel, -2*position.x/width);
	else
		v = kernelSmoothTexture(rBuffer, smoothingLevel, 2*position.x/width);

	float delta = fwidth(v);
	float alpha = 1-smoothstep(amplitude*v-bottom, amplitude*v+top, position.y);
	outColor = vec4(color, alpha);
}
