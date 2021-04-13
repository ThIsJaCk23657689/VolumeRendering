#version 330 core
out vec4 FragColor;

struct Material {
	vec4 diffuse;

	sampler2D diffuse_texture;

	bool enableDiffuseTexture;
};

in VS_OUT {
	vec3 NaviePos;
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
} fs_in;

uniform bool useDiffuseTexture;

uniform Material material;

void main() {
	vec4 texel_diffuse = vec4(0.0);

	if (useDiffuseTexture && material.enableDiffuseTexture) {
		// �p�G���}����ܧ��� �B �Ӫ��馳����K�Ϯ� => ��Ϥ�����
		texel_diffuse = texture(material.diffuse_texture, fs_in.TexCoords);
	} else {
		// �¦��
		texel_diffuse = material.diffuse;
	}

	// �h�z��
	if (texel_diffuse.a < 0.1) {
		discard;
	}

	FragColor = texel_diffuse;
}