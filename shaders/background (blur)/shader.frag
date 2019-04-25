#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../smoothing/textureBlur.glsl"

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.f;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 4) const float amplitude = 1.f;
layout(constant_id = 5) const float maxBlur = 0.01f;

layout(binding = 0) uniform audioVolume {
	float lVolume;
	float rVolume;
};

layout(binding = 3) uniform sampler2D backgroundImage;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

const vec4 color = vec4(38.45/255.0f, 40.6/255.0f, 41.2/255.0f, 1.0);

void main() {
	float volume = min(0.01*amplitude*(lVolume+rVolume), maxBlur);

	vec4 color = blurredTexture(backgroundImage, fragTexCoord, volume);
	outColor = color;
}
