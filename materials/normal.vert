#version 330
in vec3 vertex;
in vec3 color;

const int amount = 1000;
const int maxLights = 6;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformMatrix[amount];
uniform vec3 lightColor[maxLights];
uniform vec3 lightPosition[maxLights];
uniform int lightCount;

out vec3 fragColor;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * transformMatrix[gl_InstanceID] * vec4(vertex,1.0);
    fragColor = color;
}
