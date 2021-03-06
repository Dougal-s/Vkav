#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 18) const float limit = 1;

layout(constant_id = 19) const float boxWidth = 1;
layout(constant_id = 20) const float boxHeight = 0.5;

// degrees
layout(constant_id = 21) const float rotation = 0;

layout(constant_id = 22) const float xOffset = 0;
layout(constant_id = 23) const float yOffset = 0.5;

vec2 positions[6] = vec2[](
	vec2(-1.0f, -1.0f), // top left
	vec2( 1.0f, -1.0f),  // top right
	vec2(-1.0f,  1.0f),  // bottom left

	vec2( 1.0f,  1.0f),   // bottom right
	vec2(-1.0f,  1.0f),  // bottom left
	vec2( 1.0f, -1.0f)   // top right
);

layout(location = 0) out vec2 position;

layout(location = 1) out vec2 screenDimensions;

void main() {
	mat2 transform = mat2(cos(rotation), sin(rotation), -sin(rotation), cos(rotation))
	               * mat2(boxWidth, 0, 0, limit*boxHeight);

	vec2 boxSize = vec2(boxWidth, boxHeight);
	vec2 rotationXY = vec2(cos(rotation), sin(rotation));
	vec2 screen = vec2(width, height);

	gl_Position = vec4(transform*positions[gl_VertexIndex] + vec2(xOffset, yOffset) + (1-limit)*rotationXY.yx*vec2(boxHeight, boxHeight), 0.0, 1.0);
	position = 0.5*vec2(dot(screen, rotationXY), -limit)*boxSize*(vec2(0, -1)+positions[gl_VertexIndex]);
	screenDimensions = vec2(dot(screen, rotationXY), dot(screen, rotationXY.yx))*boxSize;
}
