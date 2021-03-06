#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.f;

layout(constant_id = 11) const int barWidth = 4;
layout(constant_id = 12) const int barGap = 2;
layout(constant_id = 13) const float amplitude = 2.f;

layout(constant_id = 14) const float brightnessSensitivity = 1.f;

layout(constant_id = 15) const float red = 0.196;
layout(constant_id = 16) const float green = 0.196;
layout(constant_id = 17) const float blue = 0.204;

vec3 color = vec3(red, green, blue);

layout(set = 0, binding = 0) uniform data {
	float lVolume;
	float rVolume;
};

layout(set = 0, binding = 1) uniform samplerBuffer lBuffer;
layout(set = 0, binding = 2) uniform samplerBuffer rBuffer;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 screen;

layout(location = 0) out vec4 outColor;

#include "../../smoothing/smoothing.glsl"

void main() {
	float brightness = exp2(10.f*brightnessSensitivity*(lVolume+rVolume));

	float totalBarSize = barWidth + barGap;
	float center = 0.5*totalBarSize;
	float pos = mod(position.x, totalBarSize);

	if (2*abs(pos-center) <= barWidth) {
		float barCenter = position.x-pos+0.5f;
		float v = 0.f;

		const float mixThreshold = 0.03;

		float texCoord = 2.0*barCenter/screen.x;

		if (abs(texCoord) < mixThreshold)
			v = mix(
					kernelSmoothTexture(lBuffer, smoothingLevel, texCoord),
					kernelSmoothTexture(rBuffer, smoothingLevel, texCoord),
					0.5*(texCoord+mixThreshold)/mixThreshold
				);
		else if (position.x < 0.0)
			v = kernelSmoothTexture(lBuffer, smoothingLevel, texCoord);
		else
			v = kernelSmoothTexture(rBuffer, smoothingLevel, texCoord);

		if (position.y < amplitude*v) {
			outColor = vec4(color * brightness * (screen.y*position.y/40 + 1), 1.f);
			return;
		}
	}

	outColor = vec4(0);
}
