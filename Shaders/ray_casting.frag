#version 330 core
out vec4 FragColor;

in VS_OUT {
	vec3 FragPos;
    vec3 TexCoord;
} fs_in;

uniform sampler3D volume;
uniform sampler1D transfer_function;
uniform vec3 volume_resolution;

uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;
uniform vec3 backgroundColor;
uniform float sample_rate;

uniform bool useLighting;
uniform bool useNormalColor;

vec3 PhongShading(vec3 normal, vec3 color, vec3 position) {    
    // ambient
    float ambientStrength = 0.1f;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - position);
    float diff = dot(norm, lightDir);
    if (diff <= 0) {
        diff *= -1;
        norm = -norm;
    }
    vec3 diffuse = diff * lightColor;
    
    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - position);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0f);
    vec3 specular = specularStrength * spec * lightColor; 

    vec3 result = clamp(vec3(ambient + diffuse + specular) * color, 0.0f, 1.0f);
    return result;
}

void main() {
    vec4 result = vec4(0.0f);
    vec3 ray_direction = normalize(fs_in.FragPos - viewPos);
    
    vec3 sample_pos = fs_in.TexCoord;
    vec3 current_pos = fs_in.FragPos;

    while (true) {
        vec4 volume_data = texture(volume, sample_pos);
        vec4 volume_color = texture(transfer_function, volume_data.a);
        if (useNormalColor) {
            volume_color.r = volume_data.r;
            volume_color.g = volume_data.g;
            volume_color.b = volume_data.b;
        }

        vec3 temp_color = vec3(0.0f);
        if (useLighting) {
            temp_color = PhongShading(volume_data.rgb, volume_color.rgb, current_pos);
        } else {
            temp_color = volume_color.rgb;
        }

         result.rgb += (1.0f - result.a) * volume_color.a * temp_color.rgb;
		 result.a += (1.0f - result.a) * volume_color.a;
            
         if (result.a > 0.99f) {
            break;
         }

        // 實際的位置也改變，相對材質座標也要改變
        current_pos = current_pos + ray_direction * sample_rate;

        vec3 ray_direction_in_texture = vec3(1.0f);
        ray_direction_in_texture.x = ray_direction.x / volume_resolution.x;
        ray_direction_in_texture.y = ray_direction.y / volume_resolution.y;
        ray_direction_in_texture.z = ray_direction.z / volume_resolution.z;

        sample_pos = sample_pos + ray_direction_in_texture * sample_rate;
        if (current_pos.x < -volume_resolution.x / 2.0f || current_pos.x > volume_resolution.x / 2.0f) {
            break;
        }
        if (current_pos.y < -volume_resolution.y / 2.0f || current_pos.y > volume_resolution.y / 2.0f) {
            break;
        }
        if (current_pos.z < -volume_resolution.z / 2.0f || current_pos.z > volume_resolution.z / 2.0f) {
            break;
        }
    }

    vec4 bg_color = vec4(backgroundColor, 1.0f);

    FragColor.a = result.a + bg_color.a - result.a * bg_color.a;
	FragColor.rgb = bg_color.a * bg_color.rgb * (1.0f - result.a) + result.rgb * result.a;
} 