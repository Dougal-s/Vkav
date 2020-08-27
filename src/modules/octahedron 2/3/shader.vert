#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 2) const int width            = 1;
layout(constant_id = 3) const int height           = 1;

layout(constant_id = 11) const float size = 1.f;
layout(constant_id = 12) const float cameraZ = 4.f;
layout(constant_id = 13) const float yPos = -1.f;
layout(constant_id = 14) const float fov = 0.6; // pi/4
layout(constant_id = 15) const float rpm = 6.f;
layout(constant_id = 16) const float verticalFreq = 10;
layout(constant_id = 17) const float verticalDistance = 0.3;
layout(constant_id = 18) const float reactivity = 32.f;

vec3 positions[] = vec3[](
	vec3(0, -2, 0),
	vec3(1, 0, 1),
	vec3(-1, 0, 1),

	vec3(0, -2, 0),
	vec3(-1, 0, 1),
	vec3(-1, 0, -1),

	vec3(0, -2, 0),
	vec3(-1, 0, -1),
	vec3(1, 0, -1),

	vec3(0, -2, 0),
	vec3(1, 0, -1),
	vec3(1, 0, 1),

	vec3(0, 2, 0),
	vec3(-1, 0, 1),
	vec3(1, 0, 1),

	vec3(0, 2, 0),
	vec3(-1, 0, -1),
	vec3(-1, 0, 1),

	vec3(0, 2, 0),
	vec3(1, 0, -1),
	vec3(-1, 0, -1),

	vec3(0, 2, 0),
	vec3(1, 0, 1),
	vec3(1, 0, -1)
); // 3-1 * 2-1

layout(location = 0) out vec2 surfacePos;
layout(location = 1) out vec3 position;
layout(location = 2) out vec3 normal;
layout(location = 3) out vec3 camera;

layout(binding = 0) uniform data {
	float lVolume;
	float rVolume;
	uint time;
};

const float PI = 3.14159265359;

void main() {
	float scale = size + reactivity*(lVolume+rVolume);
	float rotation = 2*PI*time*rpm/(1000*60);

	mat3 rotationMatrix = mat3(
		cos(rotation), 0, -sin(rotation),
		0            , 1, 0             ,
		sin(rotation), 0, cos(rotation)
		);
	mat3 model = rotationMatrix*scale;

	vec3 translate = vec3(0, verticalDistance*sin(verticalFreq*2*PI*time/(60*1000)), 0);

	vec3 view = vec3(0, yPos, cameraZ);
	camera = view;

	float ratio = float(width)/float(height);
	float tanHalf = tan(fov/2.f);
	float far = cameraZ;
	float near = 0;
	mat4 projection = mat4(
			1.f/(ratio*tanHalf), 0          , 0                     , 0 ,
			0                  , 1.f/tanHalf, 0                     , 0 ,
			0                  , 0          , (near+far)/(near-far) , -1,
			0                  , 0          , 2*near*far/(near-far) , 0
		);

	vec3 localPos = model*positions[gl_VertexIndex]+translate;
	position = localPos;
	normal = rotationMatrix*normalize(cross(
		positions[3*(gl_VertexIndex/3)+1]-positions[3*(gl_VertexIndex/3)],
		positions[3*(gl_VertexIndex/3)+2]-positions[3*(gl_VertexIndex/3)]
		));
	gl_Position = projection*vec4(localPos-view, 1.0);

	float x, y;
	if (abs(positions[gl_VertexIndex].x) >= abs(positions[gl_VertexIndex].z)) // right left
		x = sign(positions[gl_VertexIndex].x)*positions[gl_VertexIndex].z;
	else if (abs(positions[gl_VertexIndex].z) >= abs(positions[gl_VertexIndex].x)) //front back
		x = -sign(positions[gl_VertexIndex].z)*positions[gl_VertexIndex].x;

	y = abs(0.5*positions[gl_VertexIndex].y);

	surfacePos = vec2(x, y);
}
