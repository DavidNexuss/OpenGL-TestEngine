#version 330

const int maxLights = 6;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;

uniform float shinness;

in vec3 fragColor;
in vec2 texCord;
in vec3 normalCord;
in vec4 fragPosition;
in mat3 TBN;

out vec4 color;
uniform float time;
uniform vec3 lightPosition[maxLights];
uniform vec3 lightColor[maxLights];
uniform int lightCount;
uniform vec3 viewPos;

vec3 normal = vec3(0.0);

vec3 phong(int i)
{
    vec3 lightV = lightPosition[i] - fragPosition.xyz;
    vec3 lightDir = normalize(lightV);
    vec3 viewDir = normalize(viewPos - fragPosition.xyz);
    vec3 reflectDir = reflect(-lightDir,normal);

    float diff = max(dot(normal,lightDir),0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0),32);

    vec3 diffuse =  (diff + 0.1) * texture(texture0,texCord * 0.2).xyz;
    vec3 specular = (spec * shinness + 0.05) * texture(texture1,texCord * 0.2).xyz;

    float l = length(lightV);
    return (diffuse + specular) * lightColor[i];
}

void main()
{
    normal = texture(texture2,texCord * 0.2).xyz;
    normal = normalize(TBN * (normal * 2.0 - 1.0));
    vec3 v = vec3(0.0);
    for (int i = 0; i < lightCount; i++)
    {
        v += phong(i);
    }

    color = vec4(v,1.0);
}
