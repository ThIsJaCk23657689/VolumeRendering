#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

out VS_OUT {
	vec3 NaviePos;
	vec3 FragPos;
	vec3 Normal;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalModel;

void main() {
	vs_out.NaviePos = aPosition;
	vs_out.FragPos =  vec3(model * vec4(aPosition, 1.0));
	vs_out.Normal = normalModel * aNormal;
	gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}