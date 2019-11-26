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
	float volume = 0.25*log(max(100*amplitude*(lVolume+rVolume), 1.05f));
	vec2 xy = vec2(gl_FragCoord.x - 0.5 - 0.5*width, gl_FragCoord.y - 0.5 - 0.5*height);
	// distance from centre
	float position = dot(xy,xy) / (width*width+height*height);
	// gaussian curve with width controlled by volume
	float brightness = min(4*volume, 1.f)*exp(-position/(2*volume*volume));

	outColor = texture(backgroundImage, fragTexCoord)*vec4(1,1,1,brightness);
}
