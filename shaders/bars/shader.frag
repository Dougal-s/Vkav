#version 450

layout(constant_id = 0) const uint audioSize       = 0;
layout(constant_id = 1) const float smoothingLevel = 0;
layout(constant_id = 2) const int width            = 0;
layout(constant_id = 3) const int height           = 0;

layout(binding = 0) uniform sampler1D lAudioSampler;
layout(binding = 1) uniform sampler1D rAudioSampler;

layout(location = 0) out vec4 outColor;

float smoothTexture(in sampler1D s, in float index) {
	const float smoothingFactor = 1.f/(smoothingLevel*smoothingLevel*2.f);
	const float radius = sqrt(-log(0.1f)/smoothingFactor)/audioSize;
	float val = 0.f;
	float sum = 0.f;
	for (float i = index-radius; i < index+radius; i += 1.f/audioSize) {
		float distance = audioSize*(index - i);
		float weight = exp(-distance*distance*smoothingFactor);
		val += texture(s, i).r*weight;
		sum += weight;
	}
	val /= sum;
	return val;
}

const int barWidth = 4;
const int barGap = 2;

const vec4 color = vec4(50/255.0f, 50/255.0f, 52/255.0f, 1.0);

const float amplitude = 2.0;

void main() {
	float x = gl_FragCoord.x - (width/2.0);

	float totalBarSize = barWidth + barGap;
	float center = totalBarSize/2;
	float pos = mod(x, totalBarSize);

	if ( center-barWidth/2.0f <= pos && pos <= center + barWidth/2.0f) {
		float y = 1.0f-gl_FragCoord.y/height;
		float barCenter = x-pos+0.5f;
		if (x < 0.0) {
			float v = smoothTexture(lAudioSampler, -2.0*barCenter/width).r;
			if (y < amplitude*v) {
				outColor = (color * (((height-gl_FragCoord.y) / 40) + 1));
				return;
			}
		} else {
			float v = smoothTexture(rAudioSampler, 2.0*barCenter/width).r;
			if (y < amplitude*v) {
				outColor = (color * (((height-gl_FragCoord.y) / 40) + 1));
				return;
			}
		}
	}

	outColor = vec4(38.45/255.0f, 40.6/255.0f, 41.2/255.0f, 0.0);
}
