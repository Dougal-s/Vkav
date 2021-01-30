#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(constant_id = 0) const int audioSize        = 1;
layout(constant_id = 1) const float smoothingLevel = 0.0;
layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 11) const int originalRadius = 128;
layout(constant_id = 12) const float radiusSensitivity = 1.f;
layout(constant_id = 13) const float rotationSensitivity = 1.f;
layout(constant_id = 14) const float rotationFrequency = 1.f;

layout(set = 0, binding = 0) uniform data {
	float lVolume;
	float rVolume;
};

layout(set = 1, binding = 0) uniform sampler2D logo;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

void main() {
	const float radius = min(exp2(radiusSensitivity*(lVolume+rVolume)), 2.0)*originalRadius;
	const float rotation = rotationSensitivity*(lVolume+rVolume);

	const vec2 xy = vec2(gl_FragCoord.x - (0.5*width), (0.5*height) - gl_FragCoord.y);

	float distance = length(xy);

	float delta = fwidth(distance);
	float alpha = 1-smoothstep(radius-delta, radius+delta, distance);

	mat2 rotationMatrix = mat2(cos(rotation), -sin(rotation), sin(rotation), cos(rotation));
	vec2 texCoord = (rotationMatrix*0.5*xy/radius)+vec2(0.5, -0.5);
	vec4 color = texture(logo, texCoord);
	outColor = vec4(color.xyz, color.a*alpha);
}
