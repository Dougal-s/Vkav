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

layout(constant_id = 23) const float red = 0.196;
layout(constant_id = 24) const float green = 0.196;
layout(constant_id = 25) const float blue = 0.204;

vec3 color = vec3(red, green, blue);

layout(set = 0, binding = 0) uniform data {
	float lVolume;
	float rVolume;
	uint time;
};

layout(set = 0, binding = 1) uniform samplerBuffer lBuffer;
layout(set = 0, binding = 2) uniform samplerBuffer rBuffer;

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

	float dTheta = ringAngle-angle;
	if (dTheta > PI)
		dTheta -= 2*PI;
	else if (dTheta < -PI)
		dTheta += 2*PI;

	float dx = mod(distance, totalRingWidth)-0.5*totalRingWidth; // distance from the center of the ring
	if (abs(dTheta) < PI/2) {
		float width = ringWidth*(1-(dTheta+PI/2)/PI);
		float delta = fwidth(abs(dx));
		float alpha = 1-smoothstep(width/2-delta, width/2+delta, abs(dx));
		outColor = vec4(color, (1-(dTheta+PI/2)/PI)*alpha);
		return;
	}

	if (-dTheta-PI/2 > 0) {
		float ringDistance = distance+radius-dx;
		float ringEndAngle = ringAngle-PI/2;
		vec2 ringEndPos = ringDistance*vec2(cos(ringEndAngle), sin(ringEndAngle));

		vec2 difference = ringEndPos-xy;

		/*
		 * min(100) is a crude way of preventing wierd effects
		 * around the edges of the ring where ringEndPos changes
		 */
		float delta = min(fwidth(dot(difference, difference)), 100);
		float semicircleRadius = ringWidth*ringWidth/4;
		float alpha = 1-smoothstep(semicircleRadius-delta, semicircleRadius+delta, dot(difference, difference));

		outColor = vec4(color, alpha);
		return;
	}

	outColor = vec4(0);
}
