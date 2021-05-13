#version 330 core
out vec4 FragColor;

in VS_OUT {
	vec3 FragPos;
    vec3 TexCoord;
} fs_in;
  
uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;

uniform float iso_value;
uniform bool enable_transfer_function;
uniform bool is_volume;

uniform vec3 volume_resolution;
uniform sampler3D volume_texture;
uniform sampler1D transfer_function;

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
    vec3 ray_direction = normalize(fs_in.FragPos - viewPos);
    float accurate_opacities = 0.0f;
    vec3 color = vec3(0.0f);

    vec3 TexCoord = fs_in.TexCoord;
    vec3 CurrentPosition = fs_in.FragPos;

    while (true) {
        vec4 data = texture(volume_texture, TexCoord);
        vec4 tf_color = texture(transfer_function, data.a);
        
        vec3 normal = data.rgb;
        if (tf_color.a != 0.0f) {
            // 如果查出來的透明度不是0的話，就計算光照
            vec3 temp_color = PhongShading(normal, tf_color.rgb, CurrentPosition) * tf_color.a;
            color = color + (1 - accurate_opacities) * temp_color;
            accurate_opacities = accurate_opacities + (1 - accurate_opacities) * tf_color.a;
            if (accurate_opacities > 0.995) {
                break;
            }
        }

        // 實際的位置也改變，相對材質座標也要改變
        CurrentPosition = CurrentPosition + ray_direction;

        vec3 ray_direction_in_texture = vec3(1.0f);
        ray_direction_in_texture.x = ray_direction.x / volume_resolution.x;
        ray_direction_in_texture.y = ray_direction.y / volume_resolution.y;
        ray_direction_in_texture.z = ray_direction.z / volume_resolution.z;

        TexCoord = TexCoord + ray_direction_in_texture;
        if (CurrentPosition.x < 0 || CurrentPosition.x > volume_resolution.x) {
            break;
        }
        if (CurrentPosition.y < 0 || CurrentPosition.y > volume_resolution.y) {
            break;
        }
        if (CurrentPosition.z < 0 || CurrentPosition.z > volume_resolution.z) {
            break;
        }
    }

    vec3 BackgroundColor = vec3(0.0f, 0.0f, 0.0f);
    if (accurate_opacities < 0.995) {
        color += BackgroundColor * (1 - accurate_opacities);
    }

    FragColor = vec4(color, accurate_opacities);
} 