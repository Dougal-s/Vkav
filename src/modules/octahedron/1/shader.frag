#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.f;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 19) const float outlineWidth = 0.0;

layout(constant_id = 20) const float barWidth = 0.1;
layout(constant_id = 21) const float barGap = 0.015;
layout(constant_id = 22) const float amplitude = 18.f;

layout(constant_id = 23) const float interpolationFactor = 1;

layout(constant_id = 24) const float barRed1 = 0.980;
layout(constant_id = 25) const float barGreen1 = 0.314;
layout(constant_id = 26) const float barBlue1 = 0.792;
layout(constant_id = 27) const float barAlpha1 = 1;

layout(constant_id = 28) const float barRed2 = 0.490;
layout(constant_id = 29) const float barGreen2 = 0.157;
layout(constant_id = 30) const float barBlue2 = 0.396;
layout(constant_id = 31) const float barAlpha2 = 1;

layout(constant_id = 32) const float octahedronRed = 0.196;
layout(constant_id = 33) const float octahedronGreen = 0.0;
layout(constant_id = 34) const float octahedronBlue = 0.204;
layout(constant_id = 35) const float octahedronAlpha = 0.6;

layout(constant_id = 36) const float lightStrength = 100;
layout(constant_id = 37) const float lightReactivity = 0.6;

vec3 barColor1 = vec3(barRed1, barGreen1, barBlue1);
vec3 barColor2 = vec3(barRed2, barGreen2, barBlue2);
vec3 octahedronColor = vec3(octahedronRed, octahedronGreen, octahedronBlue);

layout(binding = 0) uniform data {
	float lVolume;
	float rVolume;
};

layout(binding = 1) uniform samplerBuffer lBuffer;
layout(binding = 2) uniform samplerBuffer rBuffer;

layout(location = 0) in vec2 surfacePos; // x: [-1, 1], y: [0, 1]
layout(location = 1) in vec3 position; // x: [-1, 1], y: [0, 1]

layout(location = 0) out vec4 outColor;

#include "../../smoothing/smoothing.glsl"

void main() {
	vec2 xy = vec2(atan(surfacePos.x/(1-surfacePos.y)), surfacePos.y);

	float distanceFrmSrc = distance(position, vec3(3, -3, 1));
	float brightness = lightStrength*( 1+lightReactivity*(lVolume+rVolume) )
	                   / (distanceFrmSrc*distanceFrmSrc*distanceFrmSrc);

	if (surfacePos.y < 0.25*outlineWidth || 1-abs(surfacePos.x) - surfacePos.y < outlineWidth) {
		outColor = vec4(0,0,0,1);
		return;
	}

	float totalBarWidth = barWidth + barGap;
	float barCenter = 0.5*totalBarWidth;
	float dx = mod(xy.x, totalBarWidth);
	if (abs(abs(dx)-barCenter) <= barWidth/2) {
		float texCoord = xy.x-dx+barCenter;
		float v;
		if (xy.x < 0)
			v = kernelSmoothTexture(lBuffer, smoothingLevel, texCoord);
		else
			v = kernelSmoothTexture(rBuffer, smoothingLevel, texCoord);

		if (xy.y < amplitude*v) {
			float mixAmt = min(interpolationFactor*length(surfacePos), 1);
			outColor = vec4(
				max(brightness, 1)*mix(barColor1, barColor2, mixAmt),
				mix(barAlpha1, barAlpha2, mixAmt)
				);
			return;
		}
	}

	outColor = vec4(brightness*octahedronColor, octahedronAlpha);
}
