#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.f;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 37) const float red = 0.18039215686;
layout(constant_id = 38) const float green = 0.20392156862;
layout(constant_id = 39) const float blue = 0.21176470588;

layout(constant_id = 40) const float lightReactivity = 0.6;
layout(constant_id = 41) const float lightStrength = 75;

layout(constant_id = 42) const float lightPosX = 3;
layout(constant_id = 43) const float lightPosY = 3;
layout(constant_id = 44) const float lightPosZ = 1;

layout(constant_id = 48) const float ambient = 0.3;
layout(constant_id = 49) const float diffuse = 0.9;
layout(constant_id = 50) const float specular = 0.2;

vec3 lightPos = vec3(lightPosX, -lightPosY, lightPosZ);

layout(binding = 0) uniform data {
	float lVolume;
	float rVolume;
};

layout(binding = 4) uniform sampler2D normalMap;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 camera;

layout(location = 0) out vec4 outColor;

#include "../../smoothing/smoothing.glsl"

void main() {
	vec3 normal = vec3(0, -1, 0);
	vec3 fragPosition = position+normal*(texture(normalMap, position.xz/8-0.125).r*2-1);
	// Figure out lighting
	// light properties
	vec3 relativeLightPos = lightPos-fragPosition;
	float lightStrengthAdjusted = lightStrength*(1+lightReactivity*(lVolume+rVolume))/dot(relativeLightPos, relativeLightPos);
	// diffuse lighting
	float cosTheta = clamp(dot(normal, normalize(relativeLightPos)), 0, 1);
	// specular lighting
	vec3 relativeCamerPos = camera-fragPosition;
	vec3 reflectionAngle = reflect(-relativeLightPos, normal);
	float cosAlpha = clamp(dot(reflectionAngle, relativeCamerPos), 0, 1);
	// Combine everything
	float brightness =
		ambient +
		( diffuse*cosTheta + specular*pow(cosAlpha, 5) ) * lightStrengthAdjusted;

	outColor = vec4(brightness*vec3(red, green, blue), texture(normalMap, position.xz/8-0.125).r/2);
}
