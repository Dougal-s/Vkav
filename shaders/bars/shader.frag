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

layout(binding = 0) uniform data {
	float lVolume;
	float rVolume;
	uint time;
};

layout(binding = 1) uniform samplerBuffer lBuffer;
layout(binding = 2) uniform samplerBuffer rBuffer;

layout(binding = 3) uniform sampler2D backgroundImage;

layout(location = 0) out vec4 outColor;

#define kernel
#include "../smoothing/smoothing.glsl"
#undef kernel

const vec4 color = vec4(50, 50, 52, 255)/255.0;

void main() {
	float x = gl_FragCoord.x - (width/2.0);

	float totalBarSize = barWidth + barGap;
	float center = totalBarSize/2;
	float pos = mod(x, totalBarSize);

	if ( center-barWidth/2.0f <= pos && pos <= center + barWidth/2.0f) {
		float y = 1.0f-gl_FragCoord.y/height;
		float barCenter = x-pos+0.5f;
		if (x < 0.0) {
			float v = smoothTexture(lBuffer, -2.0*barCenter/width);
			if (y < amplitude*v) {
				outColor = (color * (((height-gl_FragCoord.y) / 40) + 1));
				return;
			}
		} else {
			float v = smoothTexture(rBuffer, 2.0*barCenter/width);
			if (y < amplitude*v) {
				outColor = (color * (((height-gl_FragCoord.y) / 40) + 1));
				return;
			}
		}
	}

	outColor = vec4(0.f, 0.f, 0.f, 0.f);
}
