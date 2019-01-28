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

const int radius = 128;
const int centerLineWidth = 2;

const vec4 color = vec4(50/255.0f, 50/255.0f, 52/255.0f, 1.0);

const float amplify = 1000.0;

const float PI = 3.14159265359;

void main() {
	float x = gl_FragCoord.x - (width/2.f);
	float y = gl_FragCoord.y - (height/2.f);

	float angle = atan(y, x);
	float distance = sqrt(x*x + y*y);

	if (radius - centerLineWidth/2.f < distance && distance < radius + centerLineWidth/2.f) {
		outColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	if (distance > radius) {
		float idx = angle - PI/2.f;
		float dir = mod(abs(idx), 2.f*PI);
		if (dir > PI)
			idx = -sign(idx) * (2.f*PI - dir);

		float v = 0;
		if (idx > 0) {
			v = smoothTexture(lAudioSampler, idx/PI).r;
		} else {
			v = smoothTexture(rAudioSampler, -idx/PI).r;
		}

		v *= amplify;

		distance -= radius;

		if (distance <= v) {
			outColor = (color * ((distance / 40) + 1));
			return;
		}
	}

	outColor = vec4(38.45/255.0f, 40.6/255.0f, 41.2/255.0f, 0.0);
}
