#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.0;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 11) const int ringCount        = 10;
layout(constant_id = 12) const float ringWidth      = 20;
layout(constant_id = 13) const float ringGap        = 10;
layout(constant_id = 14) const float rotationSpeed  = 30;
layout(constant_id = 15) const float rotationRatio  = 1;

layout(constant_id = 16) const int originalRadius = 1;
layout(constant_id = 17) const float amplitude = 1;
layout(constant_id = 18) const float radiusSensitivity = 1;

layout(constant_id = 19) const float frequencySensitivity = 10;
layout(constant_id = 20) const int fixedFreq = 0;
layout(constant_id = 21) const int scaleWithRotationRatio = 0;
layout(constant_id = 22) const float frequency = 0;

layout(constant_id = 23) const float red1 = 0.196;
layout(constant_id = 24) const float green1 = 0.196;
layout(constant_id = 25) const float blue1 = 0.204;

layout(constant_id = 26) const float red2 = 0.294;
layout(constant_id = 27) const float green2 = 0.294;
layout(constant_id = 28) const float blue2 = 0.306;

vec3 color1 = vec3(red1, green1, blue1);
vec3 color2 = vec3(red2, green2, blue2);

layout(binding = 0) uniform data {
	float lVolume;
	float rVolume;
	uint time;
};

layout(binding = 1) uniform samplerBuffer lBuffer;
layout(binding = 2) uniform samplerBuffer rBuffer;

layout(location = 0) out vec4 outColor;

#include "../../smoothing/smoothing.glsl"

const float PI = 3.14159265359;

void main() {
	const float radius = min(exp2(amplitude*(lVolume+rVolume))-1, 1.0)*radiusSensitivity+originalRadius;
	const vec2 xy = vec2(gl_FragCoord.x - (0.5*width), (0.5*height) - gl_FragCoord.y);

	float angle = atan(xy.y, xy.x)+PI;
	float distance = length(xy)-radius;

	if (distance < 0) {
		outColor = vec4(0);
		return;
	}

	float totalRingWidth = ringWidth+ringGap;

	if (distance > ringCount*totalRingWidth) {
		outColor = vec4(0);
		return;
	}

	float dx = mod(distance, totalRingWidth)-0.5*totalRingWidth; // distance from the center of the ring
	float delta = fwidth(abs(dx));
	float alpha = 1-smoothstep(ringWidth/2-delta, ringWidth/2+delta, abs(dx));

	int ring = int(distance/totalRingWidth)+1;
	float radiansPerMilisecond = rotationSpeed*2*PI/60000.f;

	float ringFrequency;
	if (fixedFreq == 1)
		ringFrequency = frequency;
	else
		ringFrequency = float(ring-1)/ringCount;

	float offset = frequencySensitivity*(
		kernelSmoothTexture(lBuffer, smoothingLevel, ringFrequency) +
		kernelSmoothTexture(rBuffer, smoothingLevel, ringFrequency)
	);

	float ringAngle;
	if (scaleWithRotationRatio == 1)
		ringAngle = mod((radiansPerMilisecond*time+offset)*rotationRatio*ring, 2*PI);
	else
		ringAngle = mod(radiansPerMilisecond*time*rotationRatio*ring+offset, 2*PI);

	float dTheta = abs(ringAngle-angle)-PI;
	if (abs(dTheta) > PI/2) {
		outColor = vec4(mix(color1, color2, 2*(abs(dTheta)-PI/2)/PI), alpha);
		return;
	}

	outColor = vec4(0);
}
