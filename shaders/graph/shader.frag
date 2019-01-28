#version 450
#extension GL_GOOGLE_include_directive : require

layout(constant_id = 0) const uint audioSize       = 0;
layout(constant_id = 1) const float smoothingLevel = 0.f;
layout(constant_id = 2) const int width            = 0;
layout(constant_id = 3) const int height           = 0;

layout(binding = 0) uniform sampler1D lAudioSampler;
layout(binding = 1) uniform sampler1D rAudioSampler;

layout(location = 0) out vec4 outColor;

#define kernel
#include "../smoothing/smoothing.glsl"
#undef kernel

const vec4 color = vec4(50/255.0f, 50/255.0f, 52/255.0f, 1.0);

const float amplitude = 2.0;

void main() {
	float x = 2.f*(gl_FragCoord.x-0.5f)/width - 1.0f;
	float y = 1.0f-gl_FragCoord.y/height;

	if (x < 0.0) {
		float v = smoothTexture(lAudioSampler, -x).r;
		if (amplitude*v-0.001 < y && y < amplitude*v+0.001) {
			outColor = (color * (((height-gl_FragCoord.y) / 40) + 1));;
			return;
		}
	} else {
		float v = smoothTexture(rAudioSampler, x).r;
		if (amplitude*v-0.001 < y && y < amplitude*v+0.001) {
			outColor = (color * (((height-gl_FragCoord.y) / 40) + 1));;
			return;
		}
	}

	outColor = vec4(38.45/255.0f, 40.6/255.0f, 41.2/255.0f, 0.0);
}
