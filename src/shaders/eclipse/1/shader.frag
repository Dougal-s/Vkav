#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 11) const int enableBackground = 1;

layout(constant_id = 12) const float red1 = 0;
layout(constant_id = 13) const float green1 = 0;
layout(constant_id = 14) const float blue1 = 0;

layout(constant_id = 15) const float red2 = 0;
layout(constant_id = 16) const float green2 = 0;
layout(constant_id = 17) const float blue2 = 0;

vec3 color1 = vec3(red1, green1, blue1);
vec3 color2 = vec3(red2, green2, blue2);

layout(location = 0) out vec4 outColor;

void main() {
	if (enableBackground == 0) {
		outColor = vec4(0);
		return;
	}

	const vec2 xy = vec2(gl_FragCoord.x - (0.5*width), (0.5*height) - gl_FragCoord.y);
	float distance = 4*dot(xy, xy)/(width*width+height*height);
	outColor = vec4(mix(color1, color2, distance), 1);
}
