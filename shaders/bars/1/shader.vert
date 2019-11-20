#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 11) const float limit = 1;

vec2 positions[6] = vec2[](
	vec2(-1.0f, 1-2*limit), // top left
	vec2( 1.0f, 1-2*limit),  // top right
	vec2(-1.0f, 1.0f),  // bottom left

	vec2( 1.0f, 1.0f),   // bottom right
	vec2(-1.0f, 1.0f),  // bottom left
	vec2( 1.0f, 1-2*limit)   // top right
);

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
