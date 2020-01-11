#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../../smoothing/textureBlur.glsl"

layout(constant_id = 2) const int width  = 1;
layout(constant_id = 3) const int height = 1;

layout(constant_id = 4) const float amplitude = 1.f;

layout(constant_id = 5) const float saturation = 20.f;
layout(constant_id = 6) const float brightnessSensitivity = 1.f;
layout(constant_id = 7) const float blur = 15.0;

layout(constant_id = 8) const float maxBlur = 0.01;

layout(binding = 0) uniform audioVolume {
	float lVolume;
	float rVolume;
};

layout(binding = 3) uniform sampler2D backgroundImage;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	float volume = amplitude*(lVolume+rVolume);
	float value = exp2(saturation*volume)-1;
	float blurAmount = min(0.01*blur*volume, maxBlur);
	float brightness = min(exp2(brightnessSensitivity*volume), 2);

	// blur
	vec4 rgba = blurredTexture(backgroundImage, fragTexCoord, blurAmount);
	// saturate
    float gray = dot(rgba.rgb, vec3(0.2989, 0.5870, 0.1140));
	rgba.rgb = -gray*value+rgba.rgb*(1+value);

	outColor = brightness*rgba;
}
