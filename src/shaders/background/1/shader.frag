#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 2) const int width  = 1;
layout(constant_id = 3) const int height = 1;

layout(constant_id = 4) const float amplitude = 1.f;

layout(binding = 0) uniform audioVolume {
	float lVolume;
	float rVolume;
};

layout(binding = 3) uniform sampler2D backgroundImage;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	float volume = exp2(amplitude*(lVolume+rVolume))-1;
	vec4 rgba = texture(backgroundImage, fragTexCoord);
    float gray = dot(rgba.rgb, vec3(0.2989, 0.5870, 0.1140));
	rgba.rgb = -gray*volume+rgba.rgb*(1+volume);
	outColor = rgba;
}
