#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.f;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 4) const float amplitude = 1.f;

layout(binding = 0) uniform audioVolume {
	float lVolume;
	float rVolume;
};

layout(binding = 3) uniform sampler2D backgroundImage;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

const vec4 color = vec4(38.45/255.0f, 40.6/255.0f, 41.2/255.0f, 1.0);

void main() {
	float volume = 0.25*log(max(100*amplitude*(lVolume+rVolume), 1.05f));
	float x = gl_FragCoord.x - 0.5 - (width/2.0);
	float y = gl_FragCoord.y - 0.5 - (height/2.0);
	// distance from centre
	float position = (x*x+y*y) / (width*width+height*height);
	// gaussian curve with width controlled by volume
	float brightness = min(4*volume, 1.f)*exp(-position/(2*volume*volume));

	vec4 color = texture(backgroundImage, fragTexCoord);
	color.a = color.a*brightness;
	outColor = color;
}
