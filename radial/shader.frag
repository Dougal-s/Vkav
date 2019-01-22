#version 450

layout(constant_id = 0) const int audioSize = 0;
layout(constant_id = 1) const int width     = 0;
layout(constant_id = 2) const int height    = 0;

layout(binding = 0) uniform sampler1D lAudioSampler;
layout(binding = 1) uniform sampler1D rAudioSampler;

layout(location = 0) out vec4 outColor;

const int radius = 128;
const int centerLineWidth = 2;
const float barWidth = 3.5;
const int numBars = 180;

const vec4 color = vec4(50/255.0f, 50/255.0f, 52/255.0f, 1.0);

const float amplify = 1000.0;

const float PI = 3.14159265359;

void main() {
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
				v = texture(lAudioSampler, texCoord).r;
			} else {
				v = texture(rAudioSampler, texCoord).r;
			}

			v *= amplify;

			distance -= radius;

			if (distance <= v) {
				outColor = (color * ((distance / 40) + 1));
				return;
			}
		}
	}

	outColor = vec4(38.45/255.0f, 40.6/255.0f, 41.2/255.0f, 0.0);
}
