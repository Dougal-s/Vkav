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

vec3 color = vec3(1.0, 0.5, 0.25);

void main() {
	float x = 2.f*(gl_FragCoord.x-0.5f)/width - 1.0f;
	float intensity = 0.0f;
	if (x < 0)
    	intensity = 10.0*smoothTexture(lAudioSampler, -x).r;
	else
    	intensity = 10.0*smoothTexture(rAudioSampler, x).r;
    outColor = vec4(intensity*color, 1.f);
}
