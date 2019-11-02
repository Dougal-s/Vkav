#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 fragTexCoord;

vec2 positions[6] = vec2[](
	vec2(-1.0f, -1.0f), // top left
	vec2( 1.0f, -1.0f),  // top right
	vec2(-1.0f,  1.0f),  // bottom left

	vec2( 1.0f,  1.0f),   // bottom right
	vec2(-1.0f,  1.0f),  // bottom left
	vec2( 1.0f, -1.0f)   // top right
);

vec2 texCoords[6] = vec2[](
	vec2(0.0f, 0.0f),
	vec2(1.0f, 0.0f),
	vec2(0.0f, 1.0f),

	vec2(1.0f, 1.0f),
	vec2(0.0f, 1.0f),
	vec2(1.0f, 0.0f)
);

void main() {
	fragTexCoord = texCoords[gl_VertexIndex];
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
