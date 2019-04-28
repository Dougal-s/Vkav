#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.0;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 4) const int originalRadius = 128;
layout(constant_id = 5) const int centerLineWidth = 2;
layout(constant_id = 6) const float barWidth = 3.5;
layout(constant_id = 7) const int numBars = 180;
layout(constant_id = 8) const float amplitude = 6000.0;

layout(binding = 0) uniform audioVolume {
	float lVolume;
	float rVolume;
};

layout(binding = 1) uniform samplerBuffer lBuffer;
layout(binding = 2) uniform samplerBuffer rBuffer;

layout(binding = 3) uniform sampler2D backgroundImage;

layout(location = 0) out vec4 outColor;

#define kernel
#include "../smoothing/smoothing.glsl"
#undef kernel

const vec4 color = vec4(50/255.0, 50/255.0, 52/255.0, 1.0);

const float PI = 3.14159265359;

void main() {
	int radius = originalRadius+int(amplitude*(lVolume+rVolume)/12);
	float x = gl_FragCoord.x - (width/2.f);
	float y = (height/2.f) - gl_FragCoord.y;

	float angle = atan(y, x);
	float distance = sqrt(x*x + y*y);

	if (radius - centerLineWidth/2.f < distance && distance < radius + centerLineWidth/2.f) {
		outColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	if (distance > radius) {
		const float section = (2.f*PI/numBars);
		const float centerLineAngle = section/2.f;
		const float anglePos = mod(angle, section); // Position inside the section in radians
		const float pos = distance * sin(centerLineAngle - anglePos);
		if (abs(pos) < barWidth/2.f) {
			float idx = angle + PI/2.f;
            float dir = mod(abs(idx), 2.f*PI);
            if (dir > PI)
                idx = -sign(idx) * (2.f*PI - dir);

			float texCoord = int(abs(idx)/section) / float(numBars/2.f);

			float v = 0;
			if (idx > 0) {
				v = smoothTexture(rBuffer, texCoord);
			} else {
				v = smoothTexture(lBuffer, texCoord);
			}

			v *= amplitude;

			distance -= radius;

			if (distance <= v) {
				outColor = (color * ((distance / 40) + 1));
				return;
			}
		}
	}

	outColor = vec4(38.45/255.0f, 40.6/255.0f, 41.2/255.0f, 0.0);
}
