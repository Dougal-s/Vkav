#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 12) const float cameraZ = 4.f;
layout(constant_id = 14) const float fov = 0.6; // pi/4

layout(constant_id = 36) const float floorY = -1.f;

vec3 positions[] = vec3[](
	vec3(-1000, 0, 1000),
	vec3(-1000, 0, -1000),
	vec3(1000, 0, -1000),

	vec3(-1000, 0, 1000),
	vec3(1000, 0, -1000),
	vec3(1000, 0, 1000),

	// extra vertices that are not required
	vec3(0), vec3(0), vec3(0),
	vec3(0), vec3(0), vec3(0),
	vec3(0), vec3(0), vec3(0),
	vec3(0), vec3(0), vec3(0),
	vec3(0), vec3(0), vec3(0),
	vec3(0), vec3(0), vec3(0)
); // 3-1 * 2-1

layout(location = 0) out vec3 position;
layout(location = 1) out vec3 camera;

const float PI = 3.14159265359;

void main() {
	vec3 translate = vec3(0, floorY, 0);
	vec3 view = vec3(0, 0, cameraZ);
	camera = view;

	float ratio = float(width)/float(height);
	float tanHalf = tan(fov/2.f);
	float far = 1000;
	float near = 0;
	mat4 projection = mat4(
			1.f/(ratio*tanHalf), 0          , 0                     , 0 ,
			0                  , 1.f/tanHalf, 0                     , 0 ,
			0                  , 0          , (near+far)/(near-far) , -1,
			0                  , 0          , 2*near*far/(near-far) , 0
		);

	vec3 localPos = positions[gl_VertexIndex]-translate;
	position = localPos;
	gl_Position = projection*vec4(localPos-view, 1.0);
}
