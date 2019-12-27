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

layout(location = 0) out vec4 outColor;

#include "../../smoothing/smoothing.glsl"

void main() {
	float brightness = exp2(20.f*brightnessSensitivity*(lVolume+rVolume));

	float x = gl_FragCoord.x - (0.5*width);

	float totalBarSize = barWidth + barGap;
	float center = totalBarSize/2;
	float pos = mod(x, totalBarSize);

	if (abs(pos-center) <= 0.5*barWidth) {
		float y = 1.0f-gl_FragCoord.y/height;
		float barCenter = x-pos+0.5f;
		float v = 0.f;
		if (x < 0.0)
			v = kernelSmoothTexture(lBuffer, -2.0*barCenter/width);
		else
			v = kernelSmoothTexture(rBuffer, 2.0*barCenter/width);

		if (y < amplitude*v) {
			outColor = vec4(color * brightness * (((height-gl_FragCoord.y) / 40) + 1), 1.f);
			return;
		}
	}

	outColor = vec4(0);
}
