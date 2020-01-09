// This shader was an accident. I have no idea what I did but it looks cool.
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.0;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 4) const int originalRadius = 128;
layout(constant_id = 5) const int centerLineWidth = 2;
layout(constant_id = 6) const float amplitude = 6000.0;

layout(constant_id = 7) const float radiusSensitivity = 1.f;

layout(constant_id = 8) const float red = 0.196;
layout(constant_id = 9) const float green = 0.196;
layout(constant_id = 10) const float blue = 0.204;

vec3 color = vec3(red, green, blue);

layout(binding = 0) uniform data {
	float lVolume;
	float rVolume;
};

layout(binding = 1) uniform samplerBuffer lBuffer;
layout(binding = 2) uniform samplerBuffer rBuffer;

layout(location = 0) out vec4 outColor;

#include "../../smoothing/smoothing.glsl"

const float PI = 3.14159265359;

void main() {
	const float radius = min(exp2(radiusSensitivity*(lVolume+rVolume)), 2.0)*originalRadius;
	const vec2 xy = vec2(gl_FragCoord.x - (0.5*width), (0.5*height) - gl_FragCoord.y);

	float angle = atan(xy.y, xy.x);
	float distance = length(xy)-radius;

	float texCoord = angle+0.5f*PI; // range [-PI/2, 3/2PI]
	if (texCoord > PI)
		texCoord = texCoord-2.f*PI; // range [-PI, PI]
	texCoord = texCoord/PI; // range [-1, 1]

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

	float lineCenter = amplitude*v;

	float lineWidth = max(centerLineWidth, fwidth(lineCenter));

	const float delta = 0.5*lineWidth+fwidth(distance*distance);
	const float alpha = smoothstep(0, 0.5*delta, abs(distance-lineCenter));
	outColor = vec4(color, alpha);
}
