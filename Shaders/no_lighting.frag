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
		// 如果有開啟顯示材質 且 該物體有材質貼圖時 => 對圖片取樣
		texel_diffuse = texture(material.diffuse_texture, fs_in.TexCoords);
	} else {
		// 純色填滿
		texel_diffuse = material.diffuse;
	}

	// 去透明
	if (texel_diffuse.a < 0.1) {
		discard;
	}

	FragColor = texel_diffuse;
}