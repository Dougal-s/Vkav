#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.f;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 4) const int barWidth = 4;
layout(constant_id = 5) const int barGap = 2;
layout(constant_id = 6) const float amplitude = 2.f;

layout(constant_id = 7) const float brightnessSensitivity = 1.f;

layout(constant_id = 8) const float red = 0.196;
layout(constant_id = 9) const float green = 0.196;
layout(constant_id = 10) const float blue = 0.204;

vec3 color = vec3(red, green, blue);

layout(binding = 0) uniform data {
	float lVolume;
	float rVolume;
};

layout(binding = 1) uniform samplerBuffer lBuffer;
layout(binding = 2) uniform samplerBuffer rBuffer;

layout(location = 0) in vec2 position;

layout(location = 0) out vec4 outColor;

#include "../../smoothing/smoothing.glsl"

void main() {
	float brightness = exp2(20.f*brightnessSensitivity*(lVolume+rVolume));

	float totalBarSize = barWidth + barGap;
	float center = 0.5*totalBarSize;
	float pos = mod(position.x, totalBarSize);

	if (2*abs(pos-center) <= barWidth) {
		float barCenter = position.x-pos+0.5f;
		float v = 0.f;
		if (position.x < 0.0)
			v = kernelSmoothTexture(lBuffer, smoothingLevel, -2.0*barCenter/width);
		else
			v = kernelSmoothTexture(rBuffer, smoothingLevel, 2.0*barCenter/width);

		if (position.y < amplitude*v) {
			outColor = vec4(color * brightness * (height*position.y/40 + 1), 1.f);
			return;
		}
	}

	outColor = vec4(0);
}
