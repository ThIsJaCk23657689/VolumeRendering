#version 330 core
out vec4 FragColor;

struct Material {
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	vec4 emission;
	float shininess;

	sampler2D diffuse_texture;
	sampler2D specular_texture;
	sampler2D emission_texture;

	bool enableDiffuseTexture;
    bool enableSpecularTexture;
	bool enableEmission;
    bool enableEmissionTexture;
};

struct Light {
	vec3 position;
	vec3 direction;
	
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;

	float constant;
	float linear;
	float quadratic;

	float cutoff;
	float outerCutoff;
	float exponent;

	bool enable;
	int caster;
};

struct Fog {
	int mode;
	int depthType;
	float density;
	float f_start;
	float f_end;
	bool enable;
	vec4 color;
};

// 0 Direction Light; 1 ~ 5 Point Light; 6 Spot Light;
#define NUM_LIGHTS 7

in VS_OUT {
	vec3 NaviePos;
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
} fs_in;

uniform vec3 viewPos;
uniform bool useLighting;
uniform bool useDiffuseTexture;

uniform Material material;

void main() {
	vec3 norm = normalize(fs_in.Normal);
	vec3 viewDir = normalize(viewPos - fs_in.FragPos);

	vec4 texel_ambient = vec4(0.0);
	vec4 texel_diffuse = vec4(0.0);
	vec4 texel_specular = vec4(0.0);

    if (material.enableDiffuseTexture) {
        FragColor = texture(material.diffuse_texture, fs_in.TexCoords);
    } else {
        FragColor = material.diffuse;
		// FragColor = vec4(0.0, 0.0, 1.0, 1.0);
    }  
} 