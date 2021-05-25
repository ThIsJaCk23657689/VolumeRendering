#version 330 core
layout (location = 0) out vec4 out_frag_color;

uniform sampler2D entry_points_sampler;
uniform sampler2D exit_points_sampler;

uniform sampler3D volume;
uniform sampler1D transfer_function;
uniform vec3 volume_resoultion;

uniform vec2 screen_size;
uniform float sampling_rate;

uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;

#define REF_SAMPLING_INTERVAL 150.0f

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

vec4 RayCasting(vec3 entry_point, vec3 exit_point) {
	vec4 result = vec4(0.0f);
	vec3 ray_direction = exit_point - entry_point;
	float t_end = length(ray_direction);
	float dt = min(t_end, t_end / (sampling_rate * length(ray_direction * volume_resoultion)));
	float sample_count = ceil(t_end / dt);

	ray_direction = normalize(ray_direction);
	float t = 0.5f * dt;
	vec3 sample_pos;

	while (t < t_end) {
		sample_pos = entry_point + t * ray_direction;

		vec4 volume_data = texture(volume, sample_pos);
		vec4 volume_color = texture(transfer_function, volume_data.a);

		volume_color.a = 1.0 - pow(1.0 - volume_color.a, dt * REF_SAMPLING_INTERVAL);

		vec3 temp_color = PhongShading(volume_data.rgb, volume_color.rgb, sample_pos);

		result.rgb += (1.0f - result.a) * volume_color.a * temp_color.rgb;
		result.a += (1.0f - result.a) * volume_color.a;

		if (result.a > 0.99f) {
			break;
		}

		t += dt;
	}

	return result;
}

void main() {
	vec2 tex_coords = vec2(gl_FragCoord.x / screen_size.x, gl_FragCoord.y / screen_size.y);
	vec3 entry_point = texture(entry_points_sampler, tex_coords).rgb;
	vec3 exit_point = texture(exit_points_sampler, tex_coords).rgb;

	vec4 out_color = vec4(0.0f);
	if (entry_point != exit_point) {
		out_color = RayCasting(entry_point, exit_point);
	}

	vec4 bg_color = vec4(0.5f, 0.5f, 0.5f, 1.0f);

	out_frag_color.a = out_color.a + bg_color.a - out_color.a * bg_color.a;
	out_frag_color.rgb = bg_color.a * bg_color.rgb * (1.0f - out_color.a) + out_color.rgb * out_color.a;
}