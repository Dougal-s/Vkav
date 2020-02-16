#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 13) const float red = 0;
layout(constant_id = 14) const float green = 0;
layout(constant_id = 15) const float blue = 0;
layout(constant_id = 16) const float alpha = 1;

vec3 color = vec3(red, green, blue);

layout(constant_id = 17) const float lightRed = 1;
layout(constant_id = 18) const float lightGreen = 1;
layout(constant_id = 19) const float lightBlue = 1;
layout(constant_id = 20) const float brightness = 1;

vec3 lightColor = vec3(lightRed, lightGreen, lightBlue);

layout(location = 0) out vec4 outColor;

void main() {
	vec2 position = vec2(0.5*width-gl_FragCoord.x, 0.5*height-gl_FragCoord.y);
	float distanceSq = dot(position, position);
	outColor = vec4(brightness/distanceSq*lightColor+color, alpha);
}
