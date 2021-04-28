#version 330

const int maxLights = 6;

uniform sampler2D texture0;   //Diffuse map
uniform sampler2D texture1;   //Specular map
uniform sampler2D texture2;   //Normal map
uniform samplerCube skybox; //SkyBox 

uniform float shinness;

in vec3 fragColor;
in vec4 fragPosition;
in vec2 texCoord;
in vec3 normalCoord;
in mat3 TBN;

uniform float time;
uniform vec3 lightPosition[maxLights];
uniform vec3 lightColor[maxLights];
uniform int lightCount;
uniform vec3 viewPos;

const float uv_scale = 0.2;

out vec4 color;

void main()
{
    vec3 normalValue = texture(texture2,texCoord * uv_scale).xyz;
    normalValue = normalize(TBN * (normalValue * 2.0 - 1.0));
    
    vec3 diffuseValue = texture(texture0,texCoord * uv_scale).xyz;
    vec3 specularValue = texture(texture1,texCoord * uv_scale).xyz;
        
    vec3 viewDir = normalize(viewPos - fragPosition.xyz);
    vec3 R = reflect(-viewDir,normalValue);

    vec3 v = vec3(0.0);
    v += texture(skybox, R).rgb * (specularValue * 0.7);

    for (int i = 0; i < lightCount; i++)
    {
        vec3 lightDir = normalize(lightPosition[i] - fragPosition.xyz);
        vec3 reflectDir = reflect(-lightDir,normalValue);
    
        float diff = max(dot(normalValue,lightDir),0.0);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0),32);
    
        vec3 diffuse =  (diff + 0.1) * diffuseValue;
        vec3 specular = (spec * shinness + 0.05) * specularValue;
    
        v += (diffuse + specular) * lightColor[i];
    }
    color = vec4(v,1.0);
}
