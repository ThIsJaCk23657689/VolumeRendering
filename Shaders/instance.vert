#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTextureCoords;
layout (location = 3) in mat4 instanceMatrix;

out VS_OUT {
	vec3 NaviePos;
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool isCubeMap;

void main() {
	vs_out.NaviePos = aPosition;
	vs_out.FragPos =  vec3(instanceMatrix * vec4(aPosition, 1.0));
	vs_out.Normal = mat3(transpose(inverse(instanceMatrix))) * aNormal;
	vs_out.TexCoords = aTextureCoords;

	if (isCubeMap) {
		// Ã¸»s¤ÑªÅ²°
		mat4 view_new = mat4(mat3(view));
		vec4 pos = projection * view_new * vec4(vs_out.FragPos, 1.0);
		gl_Position = pos.xyww;
	} else {
		gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
	}
}