#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.0;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 11) const int originalRadius = 128;
layout(constant_id = 12) const int centerLineWidth = 2;
layout(constant_id = 13) const float barWidth = 3.5;
layout(constant_id = 14) const int numBars = 180;
layout(constant_id = 15) const float amplitude = 6000.0;

layout(constant_id = 16) const float brightnessSensitivity = 1.f;
layout(constant_id = 17) const float radiusSensitivity = 1.f;

layout(constant_id = 18) const float red = 0.196;
layout(constant_id = 19) const float green = 0.196;
layout(constant_id = 20) const float blue = 0.204;

layout(constant_id = 21) const float limit = 0.5;

vec3 color = vec3(red, green, blue);

layout(set = 0, binding = 0) uniform data {
	float lVolume;
	float rVolume;
};

layout(set = 0, binding = 1) uniform samplerBuffer lBuffer;
layout(set = 0, binding = 2) uniform samplerBuffer rBuffer;

layout(location = 0) out vec4 outColor;

#include "../../smoothing/smoothing.glsl"

const float PI = 3.14159265359;

void main() {
	const float brightness = exp2(20.f*brightnessSensitivity*(lVolume+rVolume));
	const float radius = min(exp2(radiusSensitivity*(lVolume+rVolume)), 2.0)*originalRadius;
	const vec2 xy = vec2(gl_FragCoord.x - (0.5*width), (0.5*height) - gl_FragCoord.y);

	float angle = atan(xy.y, xy.x);
	float distance = length(xy);

	if (abs(distance-radius) < 0.5*centerLineWidth) {
		outColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	if (distance > amplitude*limit+radius) {
		outColor = vec4(0);
		return;
	}

	if (distance > radius) {
		const float section = (2.0*PI/numBars);
		const float centerLineAngle = 0.5*section;
		const float anglePos = mod(angle, section);
		const float pos = distance * sin(centerLineAngle - anglePos);
		const float delta = fwidth(pos*pos);
		const float alpha = 1.0-smoothstep(0.5*barWidth-delta, 0.5*barWidth+delta, pos*pos);
		if (alpha > 0.0) {
			float idx = angle + 0.5*PI;
            float dir = mod(abs(idx), 2.f*PI);
            if (dir > PI)
                idx = -sign(idx) * (2.f*PI - dir);

			float texCoord = 2.0*floor(idx/section) / numBars;

			const float trebleMixPoint = 0.95;
			const float bassMixPoint = 0.05;

			float v = 0;
			if (abs(texCoord) > trebleMixPoint)
				v = mix(
						kernelSmoothTexture(lBuffer, smoothingLevel, texCoord),
						kernelSmoothTexture(rBuffer, smoothingLevel, texCoord),
						0.5+sign(texCoord)*0.5f/(1-trebleMixPoint)*(1.f-abs(texCoord))
					);
			else if (abs(texCoord) < bassMixPoint)
				v = mix(
						kernelSmoothTexture(lBuffer, smoothingLevel, texCoord),
						kernelSmoothTexture(rBuffer, smoothingLevel, texCoord),
						0.5+sign(texCoord)*0.5f/bassMixPoint*abs(texCoord)
					);
			else if (texCoord < 0)
				v = kernelSmoothTexture(lBuffer, smoothingLevel, texCoord);
			else
				v = kernelSmoothTexture(rBuffer, smoothingLevel, texCoord);

			distance -= radius;

			if (distance <= amplitude*v) {
				outColor = vec4(color * brightness * ((distance / 40) + 1), alpha);
				return;
			}
		}
	}

	outColor = vec4(0);
}
