#version 330 core
out vec4 FragColor;

in VS_OUT {
	vec3 NaviePos;
	vec3 FragPos;
	vec3 Normal;
} fs_in;
  
uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform float iso_value;
uniform bool is_volume;

void main() {
    
    vec3 norm = normalize(fs_in.Normal);

    vec4 color = vec4(1.0f);
    color = vec4(objectColor, 1.0f);

    // ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    float diff = dot(norm, lightDir);
    if (diff <= 0) {
        diff *= -1;
        norm = -norm;
    }
    vec3 diffuse = diff * lightColor;
    
    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0f);
    vec3 specular = specularStrength * spec * lightColor;  

    vec3 result = (ambient + diffuse + specular) * color.rgb;
    if (!is_volume) {
        result = color.rgb;
    }
    result = clamp(result, 0.0f, 1.0f);
        
    FragColor = vec4(result, color.a);
} 